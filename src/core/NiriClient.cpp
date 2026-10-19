#include "core/NiriClient.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <memory>

namespace reader {
NiriClient::NiriClient(QObject *parent)
    : QObject(parent), m_socketPath(qEnvironmentVariable("NIRI_SOCKET"))
{
    connect(&m_events, &QLocalSocket::connected, this, [this] {
        m_events.write("\"EventStream\"\n");
    });
    connect(&m_events, &QLocalSocket::readyRead, this, &NiriClient::readEvents);
    connect(&m_events, &QLocalSocket::disconnected, this, [this] { m_window.reset(); });
}

void NiriClient::start()
{
    if (!m_socketPath.isEmpty() && m_events.state() == QLocalSocket::UnconnectedState) {
        m_window.reset();
        m_events.connectToServer(m_socketPath);
    }
}

void NiriClient::stop()
{
    m_events.abort();
    m_window.reset();
}

void NiriClient::applyWindow(const QJsonObject &object)
{
    const auto found = findNiriReaderWindow(
        QJsonDocument(QJsonArray{object}).toJson(QJsonDocument::Compact),
        QCoreApplication::applicationPid());
    if (found) m_window = found;
    else if (m_window && object.value("id").toInteger() == qint64(m_window->id))
        m_window.reset();
}

void NiriClient::readEvents()
{
    while (m_events.canReadLine()) {
        const QByteArray line = m_events.readLine(4 * 1024 * 1024);
        const QJsonObject event = QJsonDocument::fromJson(line).object();
        if (event.contains("WindowsChanged")) {
            m_window.reset();
            for (const auto &value : event.value("WindowsChanged").toObject().value("windows").toArray())
                applyWindow(value.toObject());
        } else if (event.contains("WindowOpenedOrChanged")) {
            applyWindow(event.value("WindowOpenedOrChanged").toObject().value("window").toObject());
        } else if (event.contains("WindowClosed")) {
            if (m_window && event.value("WindowClosed").toObject().value("id").toInteger() == qint64(m_window->id))
                m_window.reset();
        } else if (event.contains("WindowLayoutsChanged") && m_window) {
            for (const auto &value : event.value("WindowLayoutsChanged").toObject().value("changes").toArray()) {
                const QJsonArray change = value.toArray();
                if (change.size() != 2 || change[0].toInteger() != qint64(m_window->id)) continue;
                const QJsonArray position = change[1].toObject().value("tile_pos_in_workspace_view").toArray();
                if (position.size() == 2 && position[0].isDouble() && position[1].isDouble())
                    m_window->position = QPoint(qRound(position[0].toDouble()), qRound(position[1].toDouble()));
            }
        }
    }
    // A malformed peer must not grow an unbounded unterminated buffer.
    if (m_events.bytesAvailable() > 4 * 1024 * 1024) stop();
}

bool NiriClient::sendAction(const QJsonObject &action, std::function<void(bool)> completed)
{
    if (m_socketPath.isEmpty()) {
        if (completed) completed(false);
        return false;
    }
    auto *socket = new QLocalSocket(this);
    const QByteArray request = QJsonDocument(QJsonObject{{"Action", action}})
                                   .toJson(QJsonDocument::Compact) + '\n';
    connect(socket, &QLocalSocket::connected, socket, [socket, request] { socket->write(request); });
    const auto finished = std::make_shared<bool>(false);
    const auto finish = [socket, completed, finished](bool success) {
        if (*finished) return;
        *finished = true;
        if (completed) completed(success);
        socket->deleteLater();
    };
    connect(socket, &QLocalSocket::readyRead, socket, [socket, finish] {
        if (socket->canReadLine()) {
            const auto reply = QJsonDocument::fromJson(socket->readLine(65536)).object();
            finish(reply.value("Ok").toString() == QStringLiteral("Handled"));
        } else if (socket->bytesAvailable() >= 65536) {
            finish(false);
        }
    });
    connect(socket, &QLocalSocket::errorOccurred, socket, [finish] { finish(false); });
    connect(socket, &QLocalSocket::disconnected, socket, [finish] { finish(false); });
    QTimer::singleShot(500, socket, [finish] { finish(false); });
    socket->connectToServer(m_socketPath);
    return true;
}

bool NiriClient::moveWindow(quint64 id, const QPoint &position)
{
    return sendAction({{"MoveFloatingWindow", QJsonObject{
        {"id", qint64(id)}, {"x", QJsonObject{{"SetFixed", position.x()}}},
        {"y", QJsonObject{{"SetFixed", position.y()}}}}}});
}

void NiriClient::reloadConfig(std::function<void(bool)> completed)
{
    sendAction({{"LoadConfigFile", QJsonObject{{"path", QJsonValue::Null}}}}, std::move(completed));
}
}
