#include "app/RemoteControl.h"
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

namespace reader {

RemoteControl::RemoteControl(QObject *parent)
    : QObject(parent)
{
    const QString path = controlSocketPath();
    m_server = nullptr;
    QLocalSocket probe;
    probe.connectToServer(path);
    if (probe.waitForConnected(100)) {
        probe.disconnectFromServer();
        return;
    }
    QLocalServer::removeServer(path);
    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection,
            this, &RemoteControl::onNewConnection);
    m_server->listen(path);
}

QString RemoteControl::controlSocketPath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (base.isEmpty())
        base = QDir::tempPath();
    return QDir(base).filePath(QStringLiteral("reader-control.sock"));
}

void RemoteControl::onNewConnection()
{
    if (!m_server)
        return;
    while (m_server->hasPendingConnections()) {
        QLocalSocket *socket = m_server->nextPendingConnection();
        socket->setParent(this);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            const QString command = QString::fromUtf8(socket->readAll()).trimmed();
            if (!command.isEmpty())
                emit commandReceived(command);
            socket->disconnectFromServer();
            socket->deleteLater();
        });
    }
}

bool RemoteControl::sendCommand(const QString &command, QString *error)
{
    QLocalSocket socket;
    socket.connectToServer(controlSocketPath());
    if (!socket.waitForConnected(500)) {
        if (error)
            *error = socket.errorString();
        return false;
    }
    socket.write(command.toUtf8());
    socket.flush();
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();
    return true;
}

}
