#include <QtTest>
#include <QApplication>
#include <QAction>
#include <QDateTime>
#include <QTemporaryDir>
#include <QFile>
#include <QMenu>
#include "app/MainWindow.h"
#include "core/Cache.h"

using namespace reader;

class TestMainWindow2 : public QObject
{
    Q_OBJECT
private slots:
    void addBookmarkPersists();
    void resetSettingsRestoresDefaults();
    void clearRecentMenuClearsRecentList();
    void openMenuShowsRecentAndNewBook();
    void remoteToggleHidesAndRestores();
    void openLastReadRestoresRecentBook();
    void ctrlOShowsOpenMenu();
};

static QString makeTxt(const QTemporaryDir &dir, const QString &name)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    QString text;
    for (int i = 0; i < 10; ++i)
        text += QStringLiteral("第%1章 章节\n正文内容\n").arg(i + 1);
    f.write(text.toUtf8());
    f.close();
    return path;
}

void TestMainWindow2::addBookmarkPersists()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString path = makeTxt(dir, QStringLiteral("book.txt"));
    w.openBook(path);
    w.addBookmarkForCurrentBook();
    QTest::qWait(30);
    Cache c(Cache::defaultCacheFilePath());
    c.load();
    const QVector<Bookmark> marks = c.bookmarks(path);
    QCOMPARE(marks.size(), 1);
    QCOMPARE(marks.at(0).chapterIndex, w.currentChapter());
}

void TestMainWindow2::resetSettingsRestoresDefaults()
{
    QTemporaryDir dir;
    Settings seed(Settings::defaultConfigFilePath());
    seed.load();
    seed.chapterRegex = QStringLiteral("^自定义章节$");
    seed.save();
    MainWindow w;
    w.show();
    w.resetSettings();
    QTest::qWait(30);
    Settings s(Settings::defaultConfigFilePath());
    s.load();
    QCOMPARE(s.keyset.shortcut(KeyAction::Search), QKeySequence(QStringLiteral("Ctrl+F")));
    QCOMPARE(s.display.bgColor, QColor(Qt::white));
    QVERIFY(s.chapterRegex.isEmpty());
}

void TestMainWindow2::clearRecentMenuClearsRecentList()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString path = makeTxt(dir, QStringLiteral("recent.txt"));
    w.openBook(path);
    QTest::qWait(30);
    Cache c(Cache::defaultCacheFilePath());
    c.load();
    QVERIFY(!c.recentFiles().isEmpty());

    QAction *clear = w.findChild<QAction *>(QStringLiteral("actClearRecent"));
    QVERIFY(clear);
    QVERIFY(clear->isEnabled());
    clear->trigger();
    QTest::qWait(30);

    Cache d(Cache::defaultCacheFilePath());
    d.load();
    QVERIFY(d.recentFiles().isEmpty());
}

void TestMainWindow2::openMenuShowsRecentAndNewBook()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString path = makeTxt(dir, QStringLiteral("recent_book.txt"));
    w.openBook(path);
    QTest::qWait(30);
    QMenu *open = w.findChild<QMenu *>(QStringLiteral("openMenu"));
    QVERIFY(open);
    bool hasRecent = false;
    bool hasNew = false;
    for (QAction *action : open->actions()) {
        if (action->toolTip() == path)
            hasRecent = true;
        if (action->objectName() == QStringLiteral("actOpenNew"))
            hasNew = true;
    }
    QVERIFY(hasRecent);
    QVERIFY(hasNew);
}

void TestMainWindow2::remoteToggleHidesAndRestores()
{
    QTemporaryDir dir;
    Settings seed(Settings::defaultConfigFilePath());
    seed.load();
    seed.behavior.globalHidePopup = false;
    seed.save();
    MainWindow w;
    w.show();
    w.handleRemoteCommand(QStringLiteral("toggle-hide"));
    QVERIFY(!w.isVisible());
    w.handleRemoteCommand(QStringLiteral("toggle-hide"));
    QVERIFY(w.isVisible());
}

void TestMainWindow2::openLastReadRestoresRecentBook()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString path = makeTxt(dir, QStringLiteral("last_book.txt"));
    w.openBook(path);
    QTest::qWait(30);
    Cache c(Cache::defaultCacheFilePath());
    c.load();
    QVERIFY(!c.recentFiles().isEmpty());

    Cache seed(Cache::defaultCacheFilePath());
    seed.load();
    seed.clearRecent();
    seed.upsertProgress({path, 1, 0, QDateTime::currentSecsSinceEpoch()});
    seed.save();

    MainWindow resume;
    resume.openLastRead();
    QVERIFY(resume.tocItemCount() > 0);
    QCOMPARE(resume.currentChapter(), 1);
    QVERIFY(resume.currentBookTitle().contains(QStringLiteral("第2章")));
}

void TestMainWindow2::ctrlOShowsOpenMenu()
{
    MainWindow w;
    w.show();
    QTest::keyClick(&w, Qt::Key_O, Qt::ControlModifier);
    QMenu *open = w.findChild<QMenu *>(QStringLiteral("openMenu"));
    QVERIFY(open);
    QVERIFY(open->isVisible());
    open->close();
}

int main(int argc, char *argv[])
{
    QTemporaryDir tmp;
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
    QApplication app(argc, argv);
    TestMainWindow2 tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_mainwindow2.moc"
