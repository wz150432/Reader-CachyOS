#include <QtTest>
#include "core/NiriWindow.h"

using namespace reader;

class TestNiriWindow : public QObject
{
    Q_OBJECT
private slots:
    void findsOwnFloatingWindow();
    void rejectsOtherWindows();
};

void TestNiriWindow::findsOwnFloatingWindow()
{
    const QByteArray json = R"([{"id":11,"app_id":"reader.desktop","pid":42,"is_floating":true,"layout":{"tile_pos_in_workspace_view":[123.5,234.5]}},{"id":12,"app_id":"reader.desktop","pid":43,"is_floating":true,"layout":{"tile_pos_in_workspace_view":[9,9]}}])";
    const auto window = findNiriReaderWindow(json, 42);
    QVERIFY(window.has_value());
    QCOMPARE(window->id, quint64(11));
    QCOMPARE(window->position, QPoint(124, 235));
}

void TestNiriWindow::rejectsOtherWindows()
{
    const QByteArray json = R"([{"id":11,"app_id":"reader.desktop","pid":43,"is_floating":true,"layout":{"tile_pos_in_workspace_view":[1,2]}},{"id":12,"app_id":"reader.desktop","pid":42,"is_floating":false,"layout":{"tile_pos_in_workspace_view":null}},{"id":13,"app_id":"other","pid":42,"is_floating":true,"layout":{"tile_pos_in_workspace_view":[3,4]}}])";
    QVERIFY(!findNiriReaderWindow(json, 42).has_value());
    QVERIFY(!findNiriReaderWindow(QByteArrayLiteral("not-json"), 42).has_value());
}

QTEST_APPLESS_MAIN(TestNiriWindow)
#include "tst_niriwindow.moc"
