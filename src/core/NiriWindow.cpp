#include "core/NiriWindow.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QtMath>

namespace reader {

std::optional<NiriReaderWindow> findNiriReaderWindow(const QByteArray &windowsJson,
                                                     qint64 pid)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(windowsJson, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray())
        return std::nullopt;
    for (const QJsonValue &value : document.array()) {
        const QJsonObject window = value.toObject();
        const QString appId = window.value(QStringLiteral("app_id")).toString();
        if (window.value(QStringLiteral("pid")).toInteger() != pid
            || (appId != QStringLiteral("reader") && appId != QStringLiteral("reader.desktop"))
            || !window.value(QStringLiteral("is_floating")).toBool())
            continue;
        const QJsonArray pos = window.value(QStringLiteral("layout")).toObject()
                                   .value(QStringLiteral("tile_pos_in_workspace_view")).toArray();
        if (pos.size() != 2 || !pos.at(0).isDouble() || !pos.at(1).isDouble())
            continue;
        return NiriReaderWindow{
            quint64(window.value(QStringLiteral("id")).toInteger()),
            QPoint(qRound(pos.at(0).toDouble()), qRound(pos.at(1).toDouble()))};
    }
    return std::nullopt;
}

std::optional<NiriReaderWindow> currentNiriReaderWindow()
{
    if (qEnvironmentVariableIsEmpty("NIRI_SOCKET"))
        return std::nullopt;
    QProcess process;
    process.start(QStringLiteral("niri"), {QStringLiteral("msg"), QStringLiteral("-j"),
                                             QStringLiteral("windows")});
    if (!process.waitForStarted(300) || !process.waitForFinished(700)) {
        process.kill();
        process.waitForFinished();
        return std::nullopt;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return std::nullopt;
    return findNiriReaderWindow(process.readAllStandardOutput(), QCoreApplication::applicationPid());
}

bool moveNiriReaderWindow(quint64 id, const QPoint &position)
{
    QProcess process;
    process.start(QStringLiteral("niri"), {QStringLiteral("msg"), QStringLiteral("action"),
                                             QStringLiteral("move-floating-window"),
                                             QStringLiteral("--id"), QString::number(id),
                                             QStringLiteral("--x"), QString::number(position.x()),
                                             QStringLiteral("--y"), QString::number(position.y())});
    if (!process.waitForStarted(300) || !process.waitForFinished(700)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool reloadNiriConfig()
{
    QProcess process;
    process.start(QStringLiteral("niri"), {QStringLiteral("msg"), QStringLiteral("action"),
                                             QStringLiteral("load-config-file")});
    if (!process.waitForStarted(300) || !process.waitForFinished(700)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

}
