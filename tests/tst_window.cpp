#include <QtTest>
#include <QApplication>
#include <QDockWidget>
#include <QMenuBar>
#include <QWheelEvent>
#include <QTemporaryDir>
#include "app/MainWindow.h"
#include "app/ReadingView.h"

using namespace reader;

class TestWindow : public QObject
{
    Q_OBJECT
private slots:
    void hideAndShow();
    void translucentBackgroundEnabled();
    void fullscreenToggle();
    void alwaysOnTopToggle();
    void hideBorderToggle();
    void autopageReflectsSettings();
    void altHShortcutHidesWindow();
    void dualButtonPressHidesWindow();
    void noMinimumSizeLimit();
    void shrinksBelowOldLimit();
    void fullTransparencyMakesChromeTransparent();
};

void TestWindow::hideAndShow()
{
    MainWindow w;
    w.show();
    w.hide();
    QVERIFY(!w.isVisible());
    w.show();
    QVERIFY(w.isVisible());
}

void TestWindow::translucentBackgroundEnabled()
{
    MainWindow w;
    QVERIFY(w.testAttribute(Qt::WA_TranslucentBackground));
}

void TestWindow::fullscreenToggle()
{
    MainWindow w;
    w.show();
    w.toggleFullscreen();
    QVERIFY(w.isFullScreen());
    w.toggleFullscreen();
    QVERIFY(!w.isFullScreen());
}

void TestWindow::alwaysOnTopToggle()
{
    MainWindow w;
    w.show();
    w.toggleAlwaysOnTop();
    QVERIFY(!!(w.windowFlags() & Qt::WindowStaysOnTopHint));
    w.toggleAlwaysOnTop();
    QVERIFY(!(w.windowFlags() & Qt::WindowStaysOnTopHint));
}

void TestWindow::hideBorderToggle()
{
    MainWindow w;
    w.show();
    w.toggleHideBorder();
    QVERIFY(!w.menuBar()->isVisible());
    auto *view = qobject_cast<ReadingView *>(w.centralWidget());
    QVERIFY(view);
    QVERIFY(!view->pageIndicatorVisible());
    w.toggleHideBorder();
    QVERIFY(w.menuBar()->isVisible());
    QVERIFY(view->pageIndicatorVisible());
}

void TestWindow::autopageReflectsSettings()
{
    MainWindow w;
    w.show();
    w.toggleAutoPage();
    QVERIFY(w.autoPageActive());
    w.toggleAutoPage();
    QVERIFY(!w.autoPageActive());
}

void TestWindow::altHShortcutHidesWindow()
{
    MainWindow w;
    w.show();
    QTest::keyClick(&w, Qt::Key_H, Qt::AltModifier);
    QVERIFY(!w.isVisible());
    w.show();
    QVERIFY(w.isVisible());
}

void TestWindow::dualButtonPressHidesWindow()
{
    MainWindow w;
    w.show();
    auto *view = qobject_cast<ReadingView *>(w.centralWidget());
    QVERIFY(view);
    QTest::mousePress(view, Qt::LeftButton);
    QTest::mousePress(view, Qt::RightButton);
    QVERIFY(!w.isVisible());
    w.show();
}

void TestWindow::noMinimumSizeLimit()
{
    MainWindow w;
    QCOMPARE(w.minimumWidth(), 0);
    QCOMPARE(w.minimumHeight(), 0);
}

void TestWindow::shrinksBelowOldLimit()
{
    MainWindow w;
    w.show();
    w.resize(240, 180);
    QTest::qWait(30);
    QVERIFY(w.width() < 480);
    QVERIFY(w.height() < 320);
}

void TestWindow::fullTransparencyMakesChromeTransparent()
{
    MainWindow w;
    w.show();
    auto *view = qobject_cast<ReadingView *>(w.centralWidget());
    QVERIFY(view);
    QWheelEvent wheel(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, 120),
                      Qt::NoButton, Qt::ControlModifier | Qt::ShiftModifier,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(view, &wheel);
    QVERIFY(w.menuBar()->styleSheet().contains(QStringLiteral("transparent")));
    auto *dock = w.findChild<QDockWidget *>(QStringLiteral("tocDock"));
    QVERIFY(dock);
    QVERIFY(dock->styleSheet().contains(QStringLiteral("transparent")));
}

int main(int argc, char *argv[])
{
    QTemporaryDir tmp;
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
    QApplication app(argc, argv);
    TestWindow tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_window.moc"
