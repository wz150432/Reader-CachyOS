#include "app/online/BookSourceDialog.h"
#include "app/online/OnlineComponentDialog.h"
#include "app/online/OnlineLibraryDialog.h"
#include "core/Cache.h"
#include "core/online/OnlineBook.h"
#include "core/online/OnlineComponentManager.h"
#include "core/online/OnlineEngineBridge.h"
#include <QComboBox>
#include <QDir>
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

private slots:
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
    auto direct = d.findChild<QLineEdit *>("directUrl");
    direct->setText("https://example.org/book");
    d.findChild<QPushButton *>("importUrl")->click();
    auto rows = d.findChild<QListWidget *>("onlineToc");
    QTRY_COMPARE(rows->count(), 2);
    bridge.setExecutablePath({});
    direct->setText("https://example.org/another-book");
    d.findChild<QPushButton *>("importUrl")->click();
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
    auto direct = d.findChild<QLineEdit *>("directUrl");
    direct->setText("https://example.org/book");
    d.findChild<QPushButton *>("importUrl")->click();
    auto rows = d.findChild<QListWidget *>("onlineToc");
    QTRY_COMPARE(rows->count(), 2);
    direct->setText("https://example.org/slow/book-b");
    d.findChild<QPushButton *>("importUrl")->click();
    rows->setFocus();
    QTest::keyClick(rows, Qt::Key_Return);
    QCOMPARE(opened.size(), 0);
    QPushButton *cancel = nullptr;
    for (auto button : d.findChildren<QPushButton *>())
      if (button->text() == QStringLiteral("取消请求"))
        cancel = button;
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
    d.findChild<QListWidget *>("cachedBooks")->setCurrentRow(0);
    d.findChild<QPushButton *>("refreshOnlineToc")->click();
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
  }

  void captureActualWidgets() {
    QString output = qEnvironmentVariable("READER_ONLINE_SCREENSHOTS");
    if (output.isEmpty())
      QSKIP("Screenshot artifact capture is opt-in");
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
    library.findChild<QLineEdit *>("directUrl")
        ->setText("https://example.org/book");
    library.findChild<QPushButton *>("importUrl")->click();
    QVERIFY(library.grab().save(output + "/online-library-loading.png"));
    QTRY_COMPARE(library.findChild<QListWidget *>("onlineToc")->count(), 2);
    QVERIFY(library.grab().save(output + "/online-library-toc.png"));
    bridge.setExecutablePath({});
    library.findChild<QPushButton *>("importUrl")->click();
    QVERIFY(library.grab().save(output + "/online-library-error.png"));
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
    QCOMPARE(store.sources().size(), 1);
    name->setText("测试书源");
    QTest::mouseClick(d.findChild<QPushButton *>("saveSource"), Qt::LeftButton);
    QTRY_COMPARE(store.sources().size(), 2);
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
    d.findChild<QLineEdit *>("directUrl")->setText("https://example.org/lost");
    d.findChild<QPushButton *>("importUrl")->click();
    QTRY_COMPARE(d.findChild<QListWidget *>("onlineToc")->count(), 1);
    d.findChild<QPushButton *>("openOnlineBook")->click();
    QCOMPARE(opened.size(), 1);
    auto unchanged = Book::create(book->filePath());
    QCOMPARE(unchanged->chapters().size(), 2);
    QCOMPARE(unchanged->chapters()[0].title, QString("Original One"));
    auto cached = d.findChild<QListWidget *>("cachedBooks");
    cached->setCurrentRow(0);
    d.findChild<QPushButton *>("refreshOnlineToc")->click();
    auto status = d.findChild<QLabel *>("libraryStatus");
    QTRY_VERIFY(status->text().contains("保留旧目录"));
    QCOMPARE(refreshed.size(), 0);
    unchanged = Book::create(book->filePath());
    QCOMPARE(unchanged->chapters().size(), 2);
    cache.load();
    QCOMPARE(cache.progress(book->filePath())->pageIndex, 3);
  }
  void libraryDirectAndChoices() {
    QTemporaryDir dir;
    BookSourceStore store(dir.path() + "/sources.json");
    store.load();
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    OnlineLibraryDialog d(&store, &bridge, dir.path() + "/books");
    QSignalSpy open(&d, &OnlineLibraryDialog::openRequested);
    d.show();
    auto input = d.findChild<QLineEdit *>("directUrl");
    QVERIFY(input);
    input->setText("https://example.org/versions");
    QTest::mouseClick(d.findChild<QPushButton *>("importUrl"), Qt::LeftButton);
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
