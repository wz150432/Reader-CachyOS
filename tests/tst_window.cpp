#include <QtTest>
#include <QApplication>
#include <QTreeWidget>
#include <QMenuBar>
#include <QWheelEvent>
#include <QTemporaryDir>
#include <QSlider>
#include <QTimer>
#include "app/SettingsDialog.h"
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
    void fullTransparencyRequiresHiddenMenu();
    void displayDialogAlphaMinimumFollowsMenu();
    void savedZeroAlphaClampedOnStartup();
    void tocHiddenByDefault();
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
    w.toggleHideBorder();
    QVERIFY(w.menuBar()->isVisible());
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

void TestWindow::fullTransparencyRequiresHiddenMenu()
{
    MainWindow w;
    w.show();
    auto *view = qobject_cast<ReadingView *>(w.centralWidget());
    QVERIFY(view);
    const auto setTransparent = [view] {
        QWheelEvent wheel(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, 120),
                          Qt::NoButton, Qt::ControlModifier | Qt::ShiftModifier,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(view, &wheel);
    };
    const auto savedAlpha = [] {
        Settings saved;
        saved.load();
        return saved.display.windowAlpha;
    };
    setTransparent();
    QCOMPARE(savedAlpha(), 1);
    QVERIFY(!w.menuBar()->styleSheet().contains(QStringLiteral("transparent")));
    QTest::keyClick(&w, Qt::Key_F12);
    QVERIFY(w.menuBar()->isHidden());
    setTransparent();
    QCOMPARE(savedAlpha(), 0);
    QTest::keyClick(&w, Qt::Key_F12);
    QVERIFY(!w.menuBar()->isHidden());
    QCOMPARE(savedAlpha(), 1);
    setTransparent();
    QCOMPARE(savedAlpha(), 1);
}

void TestWindow::savedZeroAlphaClampedOnStartup()
{
    Settings saved;
    saved.load();
    saved.display.windowAlpha = 0;
    saved.save();
    MainWindow w;
    w.show();
    QVERIFY(!w.menuBar()->isHidden());
    saved.load();
    QCOMPARE(saved.display.windowAlpha, 1);
    auto *view = qobject_cast<ReadingView *>(w.centralWidget());
    QVERIFY(view);
    QWheelEvent down(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, -120),
                     Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(view, &down);
    saved.load();
    QCOMPARE(saved.display.windowAlpha, 11);
}

void TestWindow::displayDialogAlphaMinimumFollowsMenu()
{
    MainWindow w;
    w.show();
    QAction *display = nullptr;
    for (auto *action : w.findChildren<QAction *>()) {
        if (action->text() == QStringLiteral("显示设置"))
            display = action;
    }
    QVERIFY(display);
    for (int minimum : {1, 0}) {
        QTimer::singleShot(0, &w, [&w, minimum] {
            auto *dialog = w.findChild<SettingsDialog *>();
            QVERIFY(dialog);
            auto *slider = dialog->findChild<QSlider *>();
            QVERIFY(slider);
            const int actual = slider->minimum();
            dialog->reject();
            QCOMPARE(actual, minimum);
        });
        display->trigger();
        w.toggleHideBorder();
    }
}

void TestWindow::tocHiddenByDefault()
{
    MainWindow w;
    w.show();
    auto *toc = w.findChild<QTreeWidget *>(QStringLiteral("tocView"));
    QVERIFY(toc);
    QVERIFY(!toc->isVisible());
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
