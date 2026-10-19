#include "app/online/BookSourceDialog.h"
#include "app/online/OnlineComponentDialog.h"
#include "app/online/OnlineLibraryDialog.h"
#include "core/Cache.h"
#include "core/online/OnlineBook.h"
#include "core/online/OnlineComponentManager.h"
#include "core/online/OnlineEngineBridge.h"
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>
using namespace reader;
class DialogTest : public QObject {
  Q_OBJECT

private:
  bool selectBook(OnlineLibraryDialog &dialog, const QString &keyword) {
    dialog.findChild<QLineEdit *>("onlineKeyword")->setText(keyword);
    dialog.findChild<QPushButton *>("searchOnline")->click();
    auto results = dialog.findChild<QListWidget *>("onlineResults");
    if (!QTest::qWaitFor([&] { return results->count() > 0; }, 2000))
      return false;
    return QMetaObject::invokeMethod(results, "itemActivated", Qt::DirectConnection,
        Q_ARG(QListWidgetItem *, results->currentItem()));
  }
private slots:
  void allSourcesPreserveOriginAndIsolateFailures() {
    QTemporaryDir dir;
    BookSourceStore store(dir.filePath("sources.json"));
    QVERIFY(store.load());
    auto a = BookSource::preset().copy("aggregate-a", "Source A");
    auto b = BookSource::preset().copy("aggregate-b", "Source B");
    auto failed = BookSource::preset().copy("aggregate-error", "Broken").toJson();
    failed["fixtureError"] = "HTTP 503";
    auto disabled = BookSource::preset().copy("aggregate-disabled", "Disabled").toJson();
    disabled["enabled"] = false;
    QVERIFY(store.upsert(a));
    QVERIFY(store.upsert(b));
    QVERIFY(store.upsert(BookSource(failed)));
    QVERIFY(store.upsert(BookSource(disabled)));
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() + "/online_fixture");
    OnlineLibraryDialog dialog(&store, &bridge, dir.filePath("books"));
    auto sources = dialog.findChild<QComboBox *>("librarySources");
    QCOMPARE(sources->currentText(), QStringLiteral("全部书源"));
    dialog.show();
    dialog.findChild<QLineEdit *>("onlineKeyword")->setText("federated");
    auto search = dialog.findChild<QPushButton *>("searchOnline");
    search->click();
    auto results = dialog.findChild<QListWidget *>("onlineResults");
    QTRY_COMPARE(search->text(), QStringLiteral("搜索"));
    QCOMPARE(results->count(), BookSource::presets().size() + 2);
    QVERIFY(dialog.findChild<QLabel *>("libraryStatus")->text().contains("1"));
    int row = -1;
    for (int i = 0; i < results->count(); ++i) {
      QVERIFY(!results->item(i)->toolTip().contains("Disabled"));
      if (results->item(i)->toolTip().contains("Source B")) row = i;
    }
    QVERIFY(row >= 0);
    results->setCurrentRow(row);
    QVERIFY(QMetaObject::invokeMethod(results, "itemActivated", Qt::DirectConnection,
        Q_ARG(QListWidgetItem *, results->currentItem())));
    QTRY_COMPARE(dialog.findChild<QListWidget *>("onlineToc")->count(), 2);
    QSignalSpy opened(&dialog, &OnlineLibraryDialog::openRequested);
    dialog.findChild<QPushButton *>("openOnlineBook")->click();
    QCOMPARE(opened.size(), 1);
    auto book = std::dynamic_pointer_cast<OnlineBook>(Book::create(opened[0][0].toString()));
    QVERIFY(book);
    QCOMPARE(book->source().id(), QStringLiteral("aggregate-b"));
    QCOMPARE(book->title(), QStringLiteral("aggregate-b"));
    bridge.stop();
  }
  void cancelAllSourcesDiscardsActiveAndQueuedReplies() {
    QTemporaryDir dir;
    BookSourceStore store(dir.filePath("sources.json"));
    QVERIFY(store.load());
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() + "/online_fixture");
    OnlineLibraryDialog dialog(&store, &bridge, dir.filePath("books"));
    dialog.show();
    dialog.findChild<QLineEdit *>("onlineKeyword")->setText("slow");
    auto search = dialog.findChild<QPushButton *>("searchOnline");
    search->click();
    QCOMPARE(search->text(), QStringLiteral("取消"));
    QTest::qWait(30);
    search->click();
    QTest::qWait(900);
    QCOMPARE(dialog.findChild<QListWidget *>("onlineResults")->count(), 0);
    QCOMPARE(search->text(), QStringLiteral("搜索"));
    bridge.stop();
  }
  void searchResultsKeepAuthorSeparateAndClickable() {
    QTemporaryDir dir;
    BookSourceStore store(dir.filePath("sources.json"));
    QVERIFY(store.load());
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() + "/online_fixture");
    OnlineLibraryDialog dialog(&store, &bridge, dir.filePath("books"));
    dialog.findChild<QComboBox *>("librarySources")->setCurrentIndex(1);
    dialog.show();
    dialog.findChild<QLineEdit *>("onlineKeyword")->setText("Novel");
    dialog.findChild<QPushButton *>("searchOnline")->click();
    auto results = dialog.findChild<QListWidget *>("onlineResults");
    QTRY_COMPARE(results->count(), 1);
    QCOMPARE(results->item(0)->text(), QStringLiteral("Fixture Book"));
    QVERIFY(results->item(0)->data(Qt::AccessibleTextRole).toString().contains("Writer"));
    const auto row = results->visualItemRect(results->item(0));
    QTest::mouseClick(results->viewport(), Qt::LeftButton, Qt::NoModifier,
                     QPoint(results->viewport()->width() - 30, row.center().y()));
    QTRY_COMPARE(dialog.findChild<QListWidget *>("onlineToc")->count(), 2);
  }
  void compactSearchCanCancelAndDiscardLateResults() {
    QTemporaryDir dir;
    BookSourceStore store(dir.filePath("sources.json"));
    QVERIFY(store.load());
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() + "/online_fixture");
    OnlineLibraryDialog dialog(&store, &bridge, dir.filePath("books"));
    dialog.findChild<QComboBox *>("librarySources")->setCurrentIndex(1);
    dialog.show();
    QVERIFY(!dialog.findChild<QLineEdit *>("directUrl"));
    QVERIFY(!dialog.findChild<QListWidget *>("cachedBooks"));
    QVERIFY(dialog.width() <= 560);
    QVERIFY(dialog.height() <= 420);
    auto input = dialog.findChild<QLineEdit *>("onlineKeyword");
    auto search = dialog.findChild<QPushButton *>("searchOnline");
    QVERIFY(input && search);
    input->setText("slow");
    search->click();
    QCOMPARE(search->text(), QStringLiteral("取消"));
    search->click();
    QCOMPARE(search->text(), QStringLiteral("搜索"));
    QTest::qWait(350);
    QCOMPARE(dialog.findChild<QListWidget *>("onlineResults")->count(), 0);
    input->setText("Novel");
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_COMPARE(dialog.findChild<QListWidget *>("onlineResults")->count(), 1);
    QCOMPARE(search->text(), QStringLiteral("搜索"));
    bridge.stop();
  }
  void existingChaptersTakePrecedenceOverSupplementaryVersions() {
    QTemporaryDir dir;
    BookSourceStore store(dir.filePath("sources.json"));
    QVERIFY(store.load());
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() + "/online_fixture");
    OnlineLibraryDialog dialog(&store, &bridge, dir.filePath("books"));
    dialog.findChild<QComboBox *>("librarySources")->setCurrentIndex(1);
    dialog.show();
    QVERIFY(selectBook(dialog, "mixed"));
    QTRY_COMPARE(dialog.findChild<QListWidget *>("onlineToc")->count(), 2);
    QCOMPARE(dialog.findChild<QPushButton *>("openOnlineBook")->text(), QStringLiteral("开始阅读"));
  }
  void changingSourceClearsResultsAndDisablesSearchAfterReopening() {
    QTemporaryDir dir;
    BookSourceStore store(dir.filePath("sources.json"));
    QVERIFY(store.load());
    auto disabled = BookSource::preset().copy("disabled", "Disabled").toJson();
    disabled["enabled"] = false;
    QVERIFY(store.upsert(BookSource(disabled)));
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() + "/online_fixture");
    OnlineLibraryDialog dialog(&store, &bridge, dir.filePath("books"));
    dialog.findChild<QComboBox *>("librarySources")->setCurrentIndex(1);
    dialog.show();
    dialog.findChild<QLineEdit *>("onlineKeyword")->setText("Novel");
    auto search = dialog.findChild<QPushButton *>("searchOnline");
    search->click();
    auto results = dialog.findChild<QListWidget *>("onlineResults");
    QTRY_VERIFY(results->count() > 0);
    auto sources = dialog.findChild<QComboBox *>("librarySources");
    sources->setCurrentIndex(sources->findData("disabled"));
    dialog.hide();
    dialog.show();
    QCOMPARE(results->count(), 0);
    QVERIFY(!search->isEnabled());
  }
  void libraryShowsSafeEngineFailure() {
    QTemporaryDir dir;
    BookSourceStore store(dir.filePath("sources.json"));
    QVERIFY(store.load());
    auto source = BookSource::preset().copy("blocked", "Blocked").toJson();
    source["fixtureError"] = "HTTP 403";
    QVERIFY(store.upsert(BookSource(source)));
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() + "/online_fixture");
    OnlineLibraryDialog dialog(&store, &bridge, dir.filePath("books"));
    dialog.findChild<QComboBox *>("librarySources")->setCurrentIndex(1);
    auto sources = dialog.findChild<QComboBox *>("librarySources");
    QVERIFY(sources);
    sources->setCurrentIndex(sources->findData("blocked"));
    dialog.findChild<QLineEdit *>("onlineKeyword")->setText("Novel");
    dialog.findChild<QPushButton *>("searchOnline")->click();
    auto status = dialog.findChild<QLabel *>("libraryStatus");
    QTRY_VERIFY(status->text().contains("HTTP 403"));
    QSignalSpy errors(&bridge, &OnlineEngineBridge::error);
    bridge.setExecutablePath({});
    bridge.request("validate", {}, "teardown");
    QTRY_COMPARE(errors.size(), 1);
  }
  void newSourceDoesNotCopyHiddenCredentials() {
    QTemporaryDir dir;
    BookSourceStore store(dir.path() + "/sources.json");
    QVERIFY(store.load());
    OnlineEngineBridge bridge;
    BookSourceDialog d(&store, &bridge);
    d.show();
    d.findChild<QPushButton *>("copySource")->click();
    d.findChild<QLineEdit *>("cookie")->setText("private-cookie");
    d.findChild<QLineEdit *>("contentXPath")->setText("//private-rule");
    d.findChild<QPlainTextEdit *>("headers")->setPlainText(
        "{\"Authorization\":\"private-token\"}");
    QPushButton *add = nullptr;
    for (auto button : d.findChildren<QPushButton *>())
      if (button->text() == QStringLiteral("新建"))
        add = button;
    QVERIFY(add);
    add->click();
    QVERIFY(d.findChild<QLineEdit *>("cookie")->text().isEmpty());
    QVERIFY(d.findChild<QLineEdit *>("contentXPath")->text().isEmpty());
    QCOMPARE(
        QJsonDocument::fromJson(
            d.findChild<QPlainTextEdit *>("headers")->toPlainText().toUtf8())
            .object(),
        QJsonObject{});
  }
  void failedTocCannotActivatePreviousRows() {
    QTemporaryDir dir;
    BookSourceStore store(dir.path() + "/sources.json");
    store.load();
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    OnlineLibraryDialog d(&store, &bridge, dir.path() + "/books");
    QSignalSpy opened(&d, &OnlineLibraryDialog::openRequested);
    d.show();
    QVERIFY(selectBook(d, "book"));
    auto rows = d.findChild<QListWidget *>("onlineToc");
    QTRY_COMPARE(rows->count(), 2);
    bridge.setExecutablePath({});
    d.findChild<QLineEdit *>("onlineKeyword")->setText("Another book");
    d.findChild<QPushButton *>("searchOnline")->click();
    QVERIFY(!d.findChild<QPushButton *>("openOnlineBook")->isEnabled());
    rows->setFocus();
    QTest::keyClick(rows, Qt::Key_Return);
    QCOMPARE(opened.size(), 0);
  }
  void pendingAndCancelledTocCannotActivatePreviousRows() {
    QTemporaryDir dir;
    BookSourceStore store(dir.path() + "/sources.json");
    store.load();
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    OnlineLibraryDialog d(&store, &bridge, dir.path() + "/books");
    QSignalSpy opened(&d, &OnlineLibraryDialog::openRequested);
    d.show();
    QVERIFY(selectBook(d, "book"));
    auto rows = d.findChild<QListWidget *>("onlineToc");
    QTRY_COMPARE(rows->count(), 2);
    QVERIFY(selectBook(d, "slow-book"));
    rows->setFocus();
    QTest::keyClick(rows, Qt::Key_Return);
    QCOMPARE(opened.size(), 0);
    auto cancel = d.findChild<QPushButton *>("searchOnline");
    QCOMPARE(cancel->text(), QStringLiteral("取消"));
    QVERIFY(cancel);
    cancel->click();
    QTest::keyClick(rows, Qt::Key_Return);
    QCOMPARE(opened.size(), 0);
    QCOMPARE(rows->count(), 0);
  }
  void refreshKeepsRemovedBookmarkIdentity() {
    QTemporaryDir dir;
    BookSourceStore store(dir.path() + "/sources.json");
    store.load();
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    auto book = OnlineBook::createManifest(
        dir.path() + "/books", BookSource::preset(), "https://example.org/lost",
        "Original",
        QJsonArray{QJsonObject{{"title", "One"},
                               {"url", "https://example.org/chapter-1"}},
                   QJsonObject{{"title", "Two"},
                               {"url", "https://example.org/chapter-2"}}});
    QVERIFY(book);
    Cache cache;
    cache.load();
    cache.upsertProgress({book->filePath(), 1, 3, 123, 12.5});
    cache.addBookmark({book->filePath(), 0, 2, "saved first", 456});
    cache.save();
    OnlineLibraryDialog d(&store, &bridge, dir.path() + "/books");
    QSignalSpy refreshed(&d, &OnlineLibraryDialog::tocRefreshed);
    d.show();
    d.refreshBook(book->filePath());
    auto status = d.findChild<QLabel *>("libraryStatus");
    QTRY_VERIFY(!status->text().contains("正在获取目录"));
    QCOMPARE(refreshed.size(), 0);
    auto preserved = Book::create(book->filePath());
    QVERIFY(preserved);
    QCOMPARE(preserved->chapters().size(), 2);
    cache.load();
    QCOMPARE(cache.bookmarks(book->filePath())[0].chapterIndex, 0);
    QCOMPARE(cache.progress(book->filePath())->chapterIndex, 1);
  }
  void validationFeedbackOnlyAllowsKnownFieldNames() {
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    QSignalSpy errors(&bridge, &OnlineEngineBridge::error);
    bridge.request("validate",
                   {{"source", QJsonObject{{"id", "validate-safe"}}}}, "form");
    QTRY_COMPARE(errors.size(), 1);
    QCOMPARE(errors[0][2].toString(), QString("Invalid XPath: contentXPath"));
    for (auto id : {"validate-secret", "validate-unknown"}) {
      bridge.request("validate", {{"source", QJsonObject{{"id", id}}}}, "form");
      QTRY_COMPARE(errors.size(), 2);
      QCOMPARE(errors[1][2].toString(), QString("Online request failed"));
      errors.removeLast();
    }
    bridge.setExecutablePath({});
    bridge.request("validate", {}, "teardown");
    QTRY_COMPARE(errors.size(), 2);
  }

  void captureActualWidgets() {
    QString output = qEnvironmentVariable("READER_ONLINE_SCREENSHOTS");
    if (output.isEmpty())
      QSKIP("Screenshot artifact capture is opt-in");
    QApplication::setStyle("Fusion");
    QDir().mkpath(output);
    QTemporaryDir dir;
    BookSourceStore store(dir.path() + "/sources.json");
    store.load();
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    OnlineComponentManager m(dir.path() + "/component", "0.1.0");
    OnlineComponentDialog component(&m);
    component.show();
    QTest::qWait(30);
    QVERIFY(
        component.grab().save(output + "/online-component-unavailable.png"));
    component.hide();
    BookSourceDialog source(&store, &bridge);
    source.show();
    QTest::qWait(30);
    QVERIFY(source.grab().save(output + "/online-source-basic.png"));
    source.findChild<QTabWidget *>()->setCurrentIndex(2);
    QTest::qWait(20);
    QVERIFY(source.grab().save(output + "/online-source-rules.png"));
    source.hide();
    OnlineLibraryDialog library(&store, &bridge, dir.path() + "/books");
    library.show();
    QTest::qWait(30);
    QVERIFY(library.grab().save(output + "/online-library-empty.png"));
    library.findChild<QLineEdit *>("onlineKeyword")->setText("layout");
    library.findChild<QPushButton *>("searchOnline")->click();
    QTRY_COMPARE(library.findChild<QListWidget *>("onlineResults")->count(), BookSource::presets().size() * 3);
    QVERIFY(library.grab().save(output + "/online-library-results.png"));
    auto sources = library.findChild<QComboBox *>("librarySources");
    sources->showPopup();
    QTest::qWait(30);
    QVERIFY(sources->view()->window()->grab().save(output + "/online-source-options.png"));
    sources->hidePopup();
    QVERIFY(selectBook(library, "book"));
    QVERIFY(library.grab().save(output + "/online-library-loading.png"));
    QTRY_COMPARE(library.findChild<QListWidget *>("onlineToc")->count(), 2);
    QVERIFY(library.grab().save(output + "/online-library-toc.png"));
    bridge.setExecutablePath({});
    library.findChild<QPushButton *>("backToOnlineResults")->click();
    library.findChild<QLineEdit *>("onlineKeyword")->setText("Another book");
    library.findChild<QPushButton *>("searchOnline")->click();
    QVERIFY(library.grab().save(output + "/online-library-error.png"));
  }

  void realDownloadClosesInstaller() {
    if (!qEnvironmentVariableIsSet("READER_VERIFY_DOWNLOAD"))
      QSKIP("Real component download is opt-in");
    QTemporaryDir dir;
    OnlineComponentManager manager(dir.filePath("component"), READER_APP_VERSION,
        OnlineComponentManager::bundledMetadata());
    QVERIFY(manager.downloadAvailable());
    OnlineComponentDialog dialog(&manager);
    dialog.show();
    QVERIFY(!manager.isBusy());
    QVERIFY(!manager.isInstalled());
    QSignalSpy done(&manager, &OnlineComponentManager::finished);
    QSignalSpy ready(&dialog, &OnlineComponentDialog::ready);
    dialog.findChild<QPushButton *>("downloadComponent")->click();
    QVERIFY(manager.isBusy());
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 60000);
    QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
    QCOMPARE(ready.size(), 1);
    QVERIFY(manager.isEnabled());
    QVERIFY(QFileInfo(manager.executablePath()).isExecutable());
    QVERIFY(!dialog.isVisible());
  }
  void absentDownloadNoConsent() {
    QTemporaryDir dir;
    OnlineComponentManager m(dir.path() + "/component", "0.1.0");
    OnlineComponentDialog d(&m);
    d.show();
    auto button = d.findChild<QPushButton *>("downloadComponent");
    QVERIFY(button);
    QVERIFY(!button->isEnabled());
    auto close = d.findChild<QPushButton *>("closeComponent");
    QVERIFY(close);
    QTest::mouseClick(close, Qt::LeftButton);
    QVERIFY(!m.isBusy());
    QVERIFY(!m.isInstalled());
  }
  void sourceValidationAndPreview() {
    QTemporaryDir dir;
    BookSourceStore store(dir.path() + "/sources.json");
    QVERIFY(store.load());
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    BookSourceDialog d(&store, &bridge);
    d.show();
    auto copy = d.findChild<QPushButton *>("copySource");
    QVERIFY(copy);
    QTest::mouseClick(copy, Qt::LeftButton);
    auto name = d.findChild<QLineEdit *>("name");
    QVERIFY(name);
    name->clear();
    QTest::mouseClick(d.findChild<QPushButton *>("saveSource"), Qt::LeftButton);
    QCOMPARE(store.sources().size(), BookSource::presets().size());
    name->setText("测试书源");
    QTest::mouseClick(d.findChild<QPushButton *>("saveSource"), Qt::LeftButton);
    QTRY_COMPARE(store.sources().size(), BookSource::presets().size() + 1);
    auto input = d.findChild<QLineEdit *>("testInput");
    QVERIFY(input);
    input->setText("https://example.org/book");
    QTest::mouseClick(d.findChild<QPushButton *>("testToc"), Qt::LeftButton);
    auto rows = d.findChild<QListWidget *>("testItems");
    QTRY_COMPARE(rows->count(), 2);
    rows->setCurrentRow(1);
    QTest::mouseClick(d.findChild<QPushButton *>("testChapter"),
                      Qt::LeftButton);
    auto preview = d.findChild<QPlainTextEdit *>("testPreview");
    QVERIFY(preview);
    QTRY_VERIFY(
        preview->toPlainText().contains("https://example.org/chapter-2"));
  }

  void reopeningKeepsTocAndLostRefreshPreservesManifest() {
    QTemporaryDir dir;
    BookSourceStore store(dir.path() + "/sources.json");
    store.load();
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    auto book = OnlineBook::createManifest(
        dir.path() + "/books", BookSource::preset(), "https://example.org/lost",
        "Original title",
        QJsonArray{QJsonObject{{"title", "Original One"},
                               {"url", "https://example.org/chapter-1"}},
                   QJsonObject{{"title", "Original Two"},
                               {"url", "https://example.org/chapter-2"}}});
    QVERIFY(book);
    Cache cache;
    cache.load();
    cache.upsertProgress({book->filePath(), 0, 3, 123, 12.5});
    cache.save();
    OnlineLibraryDialog d(&store, &bridge, dir.path() + "/books");
    QSignalSpy opened(&d, &OnlineLibraryDialog::openRequested);
    QSignalSpy refreshed(&d, &OnlineLibraryDialog::tocRefreshed);
    d.show();
    QVERIFY(selectBook(d, "lost"));
    QTRY_COMPARE(d.findChild<QListWidget *>("onlineToc")->count(), 1);
    d.findChild<QPushButton *>("openOnlineBook")->click();
    QCOMPARE(opened.size(), 1);
    auto unchanged = Book::create(book->filePath());
    QCOMPARE(unchanged->chapters().size(), 2);
    QCOMPARE(unchanged->chapters()[0].title, QString("Original One"));
    d.show();
    d.refreshBook(book->filePath());
    auto status = d.findChild<QLabel *>("libraryStatus");
    QTRY_VERIFY(status->text().contains("保留旧目录"));
    QCOMPARE(refreshed.size(), 0);
    unchanged = Book::create(book->filePath());
    QCOMPARE(unchanged->chapters().size(), 2);
    cache.load();
    QCOMPARE(cache.progress(book->filePath())->pageIndex, 3);
  }
  void librarySearchAndChoices() {
    QTemporaryDir dir;
    BookSourceStore store(dir.path() + "/sources.json");
    store.load();
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    OnlineLibraryDialog d(&store, &bridge, dir.path() + "/books");
    QSignalSpy open(&d, &OnlineLibraryDialog::openRequested);
    d.show();
    QVERIFY(selectBook(d, "versions"));
    auto toc = d.findChild<QListWidget *>("onlineToc");
    QTRY_COMPARE(toc->count(), 1);
    QTest::mouseClick(d.findChild<QPushButton *>("openOnlineBook"),
                      Qt::LeftButton);
    QTRY_COMPARE(toc->count(), 2);
    QTest::mouseClick(d.findChild<QPushButton *>("openOnlineBook"),
                      Qt::LeftButton);
    QCOMPARE(open.size(), 1);
    QVERIFY(QFileInfo::exists(open[0][0].toString()));
  }
};
int main(int argc, char **argv) {
  QTemporaryDir dir;
  qputenv("XDG_DATA_HOME", dir.path().toUtf8());
  qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
  QApplication app(argc, argv);
  DialogTest t;
  return QTest::qExec(&t, argc, argv);
}
#include "tst_onlinedialogs.moc"
