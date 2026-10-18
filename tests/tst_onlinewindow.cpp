#include "app/MainWindow.h"
#include "app/ReadingView.h"
#include "app/online/OnlineComponentDialog.h"
#include "app/online/OnlineLibraryDialog.h"
#include "core/online/OnlineBook.h"
#include "core/online/OnlineComponentManager.h"
#include <QAction>
#include <QApplication>
#include <QCryptographicHash>
#include <QMenu>
#include <QPushButton>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
#include <archive.h>
#include <archive_entry.h>
using namespace reader;
class OnlineWindowTest : public QObject {
  Q_OBJECT
private:
  bool install(OnlineComponentManager *manager, const QString &folder) {
    QFile helper(QCoreApplication::applicationDirPath() + "/online_fixture");
    if (!helper.open(QIODevice::ReadOnly))
      return false;
    auto data = helper.readAll();
    QString path = folder + "/helper.tar";
    auto a = archive_write_new();
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, QFile::encodeName(path));
    auto e = archive_entry_new();
    archive_entry_set_pathname(e, "reader-online");
    archive_entry_set_filetype(e, AE_IFREG);
    archive_entry_set_perm(e, 0755);
    archive_entry_set_size(e, data.size());
    archive_write_header(a, e);
    archive_write_data(a, data.constData(), data.size());
    archive_entry_free(e);
    archive_write_close(a);
    archive_write_free(a);
    QFile package(path);
    if (!package.open(QIODevice::ReadOnly))
      return false;
    auto bytes = package.readAll();
    QJsonObject metadata{
        {"version", "0.1.0"},
        {"protocol", 1},
        {"appVersion", "0.1.0"},
        {"platform", "linux"},
        {"arch", "x86_64"},
        {"entrypoint", "reader-online"},
        {"url", "https://github.com/wz150432/Reader-CachyOS/releases/download/"
                "v0.1.0/online.tar"},
        {"size", bytes.size()},
        {"unpackedSize", data.size()},
        {"sha256", QString::fromLatin1(QCryptographicHash::hash(
                                           bytes, QCryptographicHash::Sha256)
                                           .toHex())}};
    return manager->importPackage(path, metadata);
  }
private slots:
  void bookmarksRequireDisplayedCommittedChapter() {
    QTemporaryDir dir;
    auto book = OnlineBook::createManifest(dir.path(), BookSource::preset(),
        "https://example.org/bookmark", "Bookmark",
        QJsonArray{QJsonObject{{"title", "First"}, {"url", "https://example.org/first"}},
                   QJsonObject{{"title", "Saved"}, {"url", "https://example.org/saved"}}});
    QVERIFY(book);
    Cache cache;
    cache.load();
    cache.upsertProgress({book->filePath(), 1, 3, 123, 12.5});
    cache.save();
    MainWindow window;
    window.openBook(book->filePath());
    auto view = window.findChild<ReadingView *>("readingView");
    QCOMPARE(view->pendingChapter(), 1);
    QVERIFY(!view->committedChapterReady());
    window.addBookmarkForCurrentBook();
    cache.load();
    QVERIFY(cache.bookmarks(book->filePath()).isEmpty());
    QVERIFY(book->saveChapter(0, "Displayed cached text"));
    view->goToChapter(0);
    QVERIFY(view->committedChapterReady());
    window.addBookmarkForCurrentBook();
    cache.load();
    QCOMPARE(cache.bookmarks(book->filePath()).size(), 1);
    QCOMPARE(cache.bookmarks(book->filePath()).first().chapterIndex, 0);
    view->goToChapter(1);
    QCOMPARE(view->pendingChapter(), 1);
    QVERIFY(view->committedChapterReady());
    window.addBookmarkForCurrentBook();
    cache.load();
    for (const auto &mark : cache.bookmarks(book->filePath()))
      QCOMPARE(mark.chapterIndex, 0);
  }
  void installedLibraryAndAsyncResume() {
    QTemporaryDir dir;
    auto book = OnlineBook::createManifest(
        dir.path() + "/books", BookSource::preset(),
        "https://example.org/resume", "Resume",
        QJsonArray{QJsonObject{{"title", "Cached"},
                               {"url", "https://example.org/cached"}},
                   QJsonObject{{"title", "Missing"},
                               {"url", "https://example.org/long/slow/one"}}});
    QVERIFY(book);
    QVERIFY(book->saveChapter(0, "Cached content"));
    QVERIFY(book->saveChapter(
        1, QString(
               "Paragraph with enough text to paginate through the reader.\n\n")
               .repeated(1500)));
    Settings settings;
    settings.load();
    ReadingView reference;
    reference.setSettings(settings.display);
    reference.resize(960, 720);
    reference.setBook(book);
    reference.goToChapter(1);
    reference.goToPage(1);
    reference.lineDown();
    const qreal savedPixel = reference.pixelOffset();
    QVERIFY(savedPixel > 0);
    QCOMPARE(reference.currentPage(), 1);
    QVERIFY(QFile::remove(book->chapterFilePath(1)));
    Cache cache;
    cache.load();
    cache.upsertProgress({book->filePath(), 1, 1, 123, savedPixel});
    cache.save();
    MainWindow window;
    auto manager = window.findChild<OnlineComponentManager *>();
    QVERIFY(manager);
    auto cleanup = qScopeGuard([&] {
      if (manager->isInstalled())
        manager->uninstall();
    });
    QSignalSpy done(manager, &OnlineComponentManager::finished);
    QVERIFY(install(manager, dir.path()));
    QTRY_COMPARE(done.size(), 1);
    QVERIFY(done[0][0].toBool());
    window.findChild<QAction *>("actOnlineReading")->trigger();
    auto library = window.findChild<OnlineLibraryDialog *>();
    QVERIFY(library);
    QVERIFY(library->isVisible());
    window.openBook(book->filePath());
    auto view = window.findChild<ReadingView *>("readingView");
    QCOMPARE(view->pendingChapter(), 1);
    QTRY_COMPARE(view->pendingChapter(), -1);
    QCOMPARE(view->currentChapter(), 1);
    QCOMPARE(view->currentPage(), 1);
    QCOMPARE(view->pixelOffset(), savedPixel);
    window.quitApplication();
    cache.load();
    QCOMPARE(cache.progress(book->filePath())->chapterIndex, 1);
    QCOMPARE(cache.progress(book->filePath())->pageIndex, 1);
    QCOMPARE(cache.progress(book->filePath())->pixelOffset, savedPixel);
    window.openBook(book->filePath());
    view->goToChapter(0);
    library->close();
    QVERIFY(QFile::remove(book->chapterFilePath(1)));
    {
      MainWindow pending;
      pending.openBook(book->filePath());
      auto pendingView = pending.findChild<ReadingView *>("readingView");
      pendingView->goToChapter(1);
      QCOMPARE(pendingView->pendingChapter(), 1);
      QTest::qWait(10);
    }
    QVERIFY(manager->uninstall());
  }

  void onlineMenuCancel() {
    MainWindow w;
    w.show();
    auto menu = w.findChild<QMenu *>("openMenu");
    QVERIFY(menu);
    QMetaObject::invokeMethod(menu, "aboutToShow", Qt::DirectConnection);
    auto action = w.findChild<QAction *>("actOnlineReading");
    QVERIFY(action);
    action->trigger();
    auto dialog = w.findChild<OnlineComponentDialog *>();
    QVERIFY(dialog);
    QVERIFY(dialog->isVisible());
    auto close = dialog->findChild<QPushButton *>("closeComponent");
    QVERIFY(close);
    QTest::mouseClick(close, Qt::LeftButton);
    QVERIFY(!dialog->isVisible());
  }
  void deliberateCachedNavigationReplacesPendingResume() {
    QTemporaryDir dir;
    auto b = OnlineBook::createManifest(
        dir.path(), BookSource::preset(), "https://example.org/switch",
        "Switch",
        QJsonArray{QJsonObject{{"title", "Cached"},
                               {"url", "https://example.org/cached"}},
                   QJsonObject{{"title", "Missing"},
                               {"url", "https://example.org/missing"}}});
    QVERIFY(b);
    QVERIFY(b->saveChapter(0, "cached content"));
    Cache c;
    c.load();
    c.upsertProgress({b->filePath(), 1, 7, 123, 19.5});
    c.save();
    {
      MainWindow w;
      w.openBook(b->filePath());
      auto view = w.findChild<ReadingView *>("readingView");
      QCOMPARE(view->pendingChapter(), 1);
      view->goToChapter(0);
      w.quitApplication();
    }
    c.load();
    auto saved = c.progress(b->filePath());
    QVERIFY(saved);
    QCOMPARE(saved->chapterIndex, 0);
    QCOMPARE(saved->pageIndex, 0);
  }
  void missingResumePreservesProgress() {
    QTemporaryDir dir;
    QJsonArray chapters;
    for (int i = 0; i < 3; ++i)
      chapters.append(
          QJsonObject{{"title", QString::number(i)},
                      {"url", QString("https://example.org/%1").arg(i)}});
    auto b = OnlineBook::createManifest(dir.path(), BookSource::preset(),
                                        "https://example.org/book",
                                        "Online title", chapters);
    QVERIFY(b);
    QVERIFY(b->saveChapter(0, "Cached text"));
    Cache c;
    c.load();
    c.upsertProgress({b->filePath(), 2, 7, 123, 19.5});
    c.save();
    {
      MainWindow w;
      w.openBook(b->filePath());
      auto view = w.findChild<ReadingView *>("readingView");
      QCOMPARE(view->pendingChapter(), 2);
      QTest::qWait(400);
      w.quitApplication();
    }
    c.load();
    auto saved = c.progress(b->filePath());
    QVERIFY(saved);
    QCOMPARE(saved->chapterIndex, 2);
    QCOMPARE(saved->pageIndex, 7);
    QCOMPARE(saved->pixelOffset, 19.5);
  }
};
int main(int argc, char **argv) {
  QTemporaryDir dir;
  qputenv("XDG_DATA_HOME", dir.path().toUtf8());
  qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
  QApplication app(argc, argv);
  OnlineWindowTest t;
  return QTest::qExec(&t, argc, argv);
}
#include "tst_onlinewindow.moc"
