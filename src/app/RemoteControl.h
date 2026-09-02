#pragma once
#include <QObject>
#include <QString>

class QLocalServer;

namespace reader {

class RemoteControl : public QObject
{
    Q_OBJECT
public:
    explicit RemoteControl(QObject *parent = nullptr);
    static bool sendCommand(const QString &command, QString *error = nullptr);
    static QString controlSocketPath();

signals:
    void commandReceived(const QString &command);

private slots:
    void onNewConnection();

private:
    QLocalServer *m_server;
};

}
