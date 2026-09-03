#include <QtTest>
#include <QApplication>
#include <QAction>
#include <QDateTime>
#include <QTemporaryDir>
#include <QFile>
#include <QMenu>
#include <QLineEdit>
#include <QToolBar>
#include "app/ReadingView.h"
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
    void escapeClosesSearchBarAndClearsHighlight();
    void progressPercentReflectsCurrentBookPosition();
    void removeRecentMenuDeletesSingleBook();
    void mouseLeaveHideHotkeyToggles();
    void editModeCtrlEToggles();
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

void TestMainWindow2::escapeClosesSearchBarAndClearsHighlight()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString path = makeTxt(dir, QStringLiteral("search_book.txt"));
    w.openBook(path);
    auto *bar = w.findChild<QToolBar *>(QStringLiteral("searchBar"));
    QVERIFY(bar);
    auto *edit = w.findChild<QLineEdit *>(QStringLiteral("searchEdit"));
    QVERIFY(edit);
    QTest::keyClick(&w, Qt::Key_F, Qt::ControlModifier);
    QVERIFY(bar->isVisible());
    auto *view = qobject_cast<ReadingView *>(w.centralWidget());
    QVERIFY(view);
    edit->setText(QStringLiteral("正文内容"));
    QVERIFY(view->findNext(edit->text()));
    QVERIFY(view->currentMatchStart() >= 0);
    edit->setFocus();
    QTest::keyClick(edit, Qt::Key_Escape);
    QVERIFY(!bar->isVisible());
    QVERIFY(view->currentMatchStart() < 0);
}

void TestMainWindow2::progressPercentReflectsCurrentBookPosition()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString path = makeTxt(dir, QStringLiteral("progress_book.txt"));
    w.openBook(path);
    auto *view = qobject_cast<ReadingView *>(w.centralWidget());
    QVERIFY(view);
    QCOMPARE(w.currentProgressPercent(), 0);
    view->goToChapter(4);
    QVERIFY(w.currentProgressPercent() > 0);
}

void TestMainWindow2::removeRecentMenuDeletesSingleBook()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString first = makeTxt(dir, QStringLiteral("recent_a.txt"));
    const QString second = makeTxt(dir, QStringLiteral("recent_b.txt"));
    w.openBook(first);
    QTest::qWait(30);
    w.openBook(second);
    QTest::qWait(30);

    QMenu *deleteMenu = w.findChild<QMenu *>(QStringLiteral("deleteRecentMenu"));
    QVERIFY(deleteMenu);
    bool found = false;
    for (QAction *action : deleteMenu->actions()) {
        if (action->toolTip() == first) {
            action->trigger();
            found = true;
            break;
        }
    }
    QVERIFY(found);
    QTest::qWait(30);

    Cache c(Cache::defaultCacheFilePath());
    c.load();
    const QStringList recent = c.recentFiles();
    QVERIFY(recent.contains(second));
    QVERIFY(!recent.contains(first));
}

void TestMainWindow2::mouseLeaveHideHotkeyToggles()
{
    MainWindow w;
    w.show();
    QVERIFY(!w.mouseLeaveHideEnabled());
    QTest::keyClick(&w, Qt::Key_P, Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
    QVERIFY(!w.mouseLeaveHideEnabled());
    QVERIFY(!w.mouseLeaveHideActive());
    QTest::keyClick(&w, Qt::Key_P, Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
    QVERIFY(!w.mouseLeaveHideEnabled());
    QVERIFY(!w.mouseLeaveHideActive());
}

void TestMainWindow2::editModeCtrlEToggles()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString path = makeTxt(dir, QStringLiteral("edit_book.txt"));
    w.openBook(path);
    QVERIFY(!w.editModeActive());
    QTest::keyClick(&w, Qt::Key_E, Qt::ControlModifier);
    QVERIFY(w.editModeActive());
    QTest::keyClick(&w, Qt::Key_E, Qt::ControlModifier);
    QVERIFY(!w.editModeActive());
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
