#include <QtTest>
#include <QApplication>
#include <QAction>
#include <QDateTime>
#include <QTemporaryDir>
#include <QFile>
#include <QMenu>
#include <QLineEdit>
#include <QToolBar>
#include <QMenuBar>
#include <QMessageBox>
#include <QDialog>
#include <QLabel>
#include <QAbstractButton>
#include <QPushButton>
#include <QProcess>
#include <QWidgetAction>
#include <QTreeWidget>
#include <QTimer>
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
    void openMenuHasNoLimit();
    void tocFillsReadingAreaAndToggles();
    void mouseLeaveHideHotkeyToggles();
    void editModeCtrlEToggles();
    void aboutActionOpensProjectDialog();
    void remoteShortcutDoesNotLaunchReaderWithoutInstance();
};

void TestMainWindow2::remoteShortcutDoesNotLaunchReaderWithoutInstance()
{
    QTemporaryDir runtimeDir;
    QVERIFY(runtimeDir.isValid());
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtimeDir.path());
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    process.setProcessEnvironment(environment);
    process.start(QCoreApplication::applicationDirPath() + QStringLiteral("/reader"),
                  {QStringLiteral("--toggle-hide")});
    QVERIFY(process.waitForStarted());
    const bool finished = process.waitForFinished(1500);
    if (!finished) {
        process.kill();
        process.waitForFinished();
    }
    QVERIFY2(finished, "--toggle-hide must not launch a new Reader when no instance is running");
    QCOMPARE(process.exitCode(), 1);
}

void TestMainWindow2::aboutActionOpensProjectDialog()
{
    MainWindow w;
    QAction *about = nullptr;
    for (QAction *action : w.menuBar()->actions()) {
        if (action->text() == QStringLiteral("关于"))
            about = action;
        QVERIFY(action->text() != QStringLiteral("帮助"));
    }
    QVERIFY(about);
    QVERIFY(!about->menu());

    bool sawDialog = false;
    QTimer::singleShot(0, &w, [&] {
        auto *dialog = w.findChild<QDialog *>(QStringLiteral("aboutDialog"));
        if (!dialog)
            return;
        auto *link = dialog->findChild<QLabel *>(QStringLiteral("projectLink"));
        auto *icon = dialog->findChild<QLabel *>(QStringLiteral("aboutIcon"));
        sawDialog = link && link->text().contains(QStringLiteral("https://github.com/wz150432/Reader-CachyOS"))
            && icon && !icon->pixmap(Qt::ReturnByValue).isNull();
        const QString screenshotPath = qEnvironmentVariable("READER_ABOUT_SCREENSHOT");
        if (!screenshotPath.isEmpty())
            dialog->grab().save(screenshotPath);
        dialog->reject();
    });
    about->trigger();
    QVERIFY(sawDialog);
}

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
    QCOMPARE(w.menuBar()->actions().first()->menu(), open);
    QVERIFY(w.findChild<QAction *>(QStringLiteral("actClearRecent")));
    QVERIFY(!w.findChild<QAction *>(QStringLiteral("actQuit")));
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

    QMenu *open = w.findChild<QMenu *>(QStringLiteral("openMenu"));
    QVERIFY(open);
    QVERIFY(!w.findChild<QMenu *>(QStringLiteral("deleteRecentMenu")));
    auto rightClick = [&](const QString &path, QMessageBox::StandardButton answer) {
        open->popup(w.mapToGlobal(QPoint(20, 20)));
        QTest::qWait(20);
        QAction *book = nullptr;
        for (QAction *action : open->actions())
            if (action->toolTip() == path) book = action;
        QVERIFY(book);
        QSignalSpy opened(book, &QAction::triggered);
        QTest::mouseMove(open, open->actionGeometry(book).center());
        open->setActiveAction(book);
        QTest::mousePress(open, Qt::RightButton, Qt::NoModifier, open->actionGeometry(book).center());
        QVERIFY(open->isVisible());
        QTest::mouseRelease(open, Qt::RightButton, Qt::NoModifier, open->actionGeometry(book).center());
        QTest::qWait(20);
        QVERIFY(open->isVisible());
        QCOMPARE(opened.count(), 0);
        QVERIFY(!QApplication::activeModalWidget());
        auto *confirmation = open->findChild<QWidgetAction *>(QStringLiteral("deleteConfirmation"));
        QVERIFY(confirmation);
        const int bookIndex = open->actions().indexOf(book);
        QCOMPARE(open->actions().at(bookIndex + 1), confirmation);
        auto *button = confirmation->defaultWidget()->findChild<QPushButton *>(
            answer == QMessageBox::Yes ? QStringLiteral("confirmDelete") : QStringLiteral("cancelDelete"));
        QVERIFY(button);
        QTest::mouseClick(button, Qt::LeftButton);
        QTest::qWait(20);
        QVERIFY(open->isVisible());
    };
    rightClick(first, QMessageBox::No);
    Cache c(Cache::defaultCacheFilePath());
    c.load();
    QVERIFY(c.recentFiles().contains(first));
    QAction *firstBook = nullptr;
    for (QAction *action : open->actions())
        if (action->toolTip() == first) firstBook = action;
    QVERIFY(firstBook);
    QSignalSpy leftOpened(firstBook, &QAction::triggered);
    QTest::mouseMove(open, open->actionGeometry(firstBook).center());
    open->setActiveAction(firstBook);
    QTest::mouseClick(open, Qt::LeftButton, Qt::NoModifier, open->actionGeometry(firstBook).center());
    QVERIFY(!open->isVisible());
    QCOMPARE(leftOpened.count(), 1);
    rightClick(first, QMessageBox::Yes);
    c.load();
    QVERIFY(c.recentFiles().contains(second));
    QVERIFY(!c.recentFiles().contains(first));
    QVERIFY(QFile::exists(first));
    rightClick(second, QMessageBox::Yes);
    open->close();
    w.close();
    c.load();
    QVERIFY(!c.recentFiles().contains(second));
    QVERIFY(QFile::exists(second));
}

void TestMainWindow2::openMenuHasNoLimit()
{
    QTemporaryDir dir;
    Cache seed(Cache::defaultCacheFilePath());
    seed.clearRecent();
    QStringList paths;
    for (int i = 0; i < 100; ++i) {
        const QString path = makeTxt(dir, QStringLiteral("book%1.txt").arg(i));
        paths.append(path);
        seed.upsertProgress({path, 0, 0, i});
    }
    seed.save();
    MainWindow w;
    auto *open = w.findChild<QMenu *>(QStringLiteral("openMenu"));
    QVERIFY(open);
    int count = 0;
    for (QAction *action : open->actions()) {
        QVERIFY(!action->text().contains(QStringLiteral("最近阅读")));
        if (paths.contains(action->toolTip())) ++count;
    }
    QCOMPARE(count, 100);
    w.show();
    open->popup(w.mapToGlobal(QPoint(20, 20)));
    QTest::qWait(20);
    const int firstColumn = open->actionGeometry(open->actions().first()).x();
    for (QAction *action : open->actions())
        if (paths.contains(action->toolTip()))
            QCOMPARE(open->actionGeometry(action).x(), firstColumn);
    open->close();
}

void TestMainWindow2::tocFillsReadingAreaAndToggles()
{
    QTemporaryDir dir;
    MainWindow w;
    w.openBook(makeTxt(dir, QStringLiteral("toc.txt")));
    w.show();
    QTest::qWait(20);
    QAction *toggle = nullptr;
    for (QAction *action : w.menuBar()->actions())
        if (action->text() == QStringLiteral("目录")) toggle = action;
    QVERIFY(toggle);
    QVERIFY(!toggle->menu());
    auto *toc = w.findChild<QTreeWidget *>();
    QVERIFY(toc);
    QVERIFY(!toc->isVisible());
    toggle->trigger();
    QVERIFY(toc->isVisible());
    QCOMPARE(toc->size(), w.centralWidget()->size());
    w.resize(800, 600);
    QTest::qWait(20);
    QCOMPARE(toc->size(), w.centralWidget()->size());
    toggle->trigger();
    QVERIFY(!toc->isVisible());
    toggle->trigger();
    auto *chapter = toc->topLevelItem(2);
    QVERIFY(chapter);
    QTest::mouseClick(toc->viewport(), Qt::LeftButton, Qt::NoModifier, toc->visualItemRect(chapter).center());
    QCOMPARE(w.currentChapter(), 2);
    QVERIFY(!toc->isVisible());
    toggle->trigger();
    QTest::keyClick(toc, Qt::Key_Down);
    QCOMPARE(toc->currentItem(), toc->topLevelItem(3));
    QCOMPARE(w.currentChapter(), 2);
    QTest::keyClick(toc, Qt::Key_Return);
    QCOMPARE(w.currentChapter(), 3);
    QVERIFY(!toc->isVisible());
    toggle->trigger();
    QTest::keyClick(toc, Qt::Key_Escape);
    QVERIFY(!toc->isVisible());
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
