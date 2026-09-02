#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include "app/MainWindow.h"

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
    QVERIFY(!!(w.windowFlags() & Qt::FramelessWindowHint));
    w.toggleHideBorder();
    QVERIFY(!(w.windowFlags() & Qt::FramelessWindowHint));
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
