#include <QtTest>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QJsonDocument>
#include "core/NiriClient.h"

using namespace reader;
class TestNiriClient : public QObject
{
    Q_OBJECT
private slots:
    void reloadReportsActualResult_data()
    {
        QTest::addColumn<QByteArray>("reply");
        QTest::addColumn<bool>("success");
        QTest::newRow("handled") << QByteArray("{\"Ok\":\"Handled\"}\n") << true;
        QTest::newRow("rejected") << QByteArray("{\"Err\":\"invalid config\"}\n") << false;
        QTest::newRow("malformed") << QByteArray("invalid\n") << false;
        QTest::newRow("timeout") << QByteArray() << false;
        QTest::newRow("disconnect") << QByteArray("disconnect") << false;
    }

    void reloadReportsActualResult()
    {
        QFETCH(QByteArray, reply);
        QFETCH(bool, success);
        QTemporaryDir dir;
        QLocalServer server;
        const auto previous = qgetenv("NIRI_SOCKET");
        const auto restore = qScopeGuard([&] {
            if (previous.isNull()) qunsetenv("NIRI_SOCKET");
            else qputenv("NIRI_SOCKET", previous);
        });
        const auto path = dir.filePath("niri.sock");
        QVERIFY(server.listen(path));
        qputenv("NIRI_SOCKET", path.toUtf8());
        NiriClient client;
        int completions = 0;
        bool result = false;
        client.reloadConfig([&](bool ok) { ++completions; result = ok; });
        QCOMPARE(completions, 0);
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->canReadLine());
        QVERIFY(QJsonDocument::fromJson(peer->readLine()).object()
                    .value("Action").toObject().contains("LoadConfigFile"));
        QCOMPARE(completions, 0);
        if (reply == "disconnect") peer->disconnectFromServer();
        else if (!reply.isEmpty()) {
            peer->write(reply.left(4));
            QTest::qWait(10);
            QCOMPARE(completions, 0);
            peer->write(reply.mid(4));
        }
        QTRY_COMPARE_WITH_TIMEOUT(completions, 1, 1500);
        QCOMPARE(result, success);
        QTest::qWait(20);
        QCOMPARE(completions, 1);
    }

    void tracksEventsAndDisconnectsWhenStopped()
    {
        QTemporaryDir dir;
        QLocalServer server;
        const auto previous = qgetenv("NIRI_SOCKET");
        const auto restore = qScopeGuard([&] {
            if (previous.isNull()) qunsetenv("NIRI_SOCKET");
            else qputenv("NIRI_SOCKET", previous);
        });
        const auto path = dir.filePath("niri.sock");
        QVERIFY(server.listen(path));
        qputenv("NIRI_SOCKET", path.toUtf8());
        NiriClient client;
        client.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->canReadLine());
        QCOMPARE(peer->readLine(), QByteArray("\"EventStream\"\n"));
        const QByteArray snapshot = QStringLiteral("{\"WindowsChanged\":{\"windows\":[{\"id\":7,\"pid\":%1,\"app_id\":\"reader\",\"is_floating\":true,\"layout\":{\"tile_pos_in_workspace_view\":[123,234]}}]}}\n")
                                        .arg(QCoreApplication::applicationPid()).toUtf8();
        peer->write(snapshot.left(20));
        QTest::qWait(10);
        QVERIFY(!client.window());
        peer->write(snapshot.mid(20));
        QTRY_VERIFY(client.window().has_value());
        QCOMPARE(client.window()->position, QPoint(123, 234));
        peer->write("{\"WindowLayoutsChanged\":{\"changes\":[[8,{\"tile_pos_in_workspace_view\":[9,9]}],[7,{\"tile_pos_in_workspace_view\":[200,300]}]]}}\n");
        QTRY_COMPARE(client.window()->position, QPoint(200, 300));
        QVERIFY(client.moveWindow(7, QPoint(40, 50)));
        QTRY_VERIFY(server.hasPendingConnections());
        auto *action = server.nextPendingConnection();
        QTRY_VERIFY(action->canReadLine());
        const auto command = QJsonDocument::fromJson(action->readLine()).object();
        QCOMPARE(command.value("Action").toObject().value("MoveFloatingWindow").toObject().value("x").toObject().value("SetFixed").toInt(), 40);
        action->write("{\"Ok\":\"Handled\"}\n");
        client.stop();
        QVERIFY(!client.window());
        QTRY_COMPARE(peer->state(), QLocalSocket::UnconnectedState);
        client.start();
        QTRY_VERIFY(server.hasPendingConnections());
        peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->canReadLine());
        peer->readLine();
        peer->write(snapshot);
        QTRY_VERIFY(client.window());
        peer->write("{\"WindowClosed\":{\"id\":7}}\n");
        QTRY_VERIFY(!client.window());
    }
};
QTEST_GUILESS_MAIN(TestNiriClient)
#include "tst_niriclient.moc"
