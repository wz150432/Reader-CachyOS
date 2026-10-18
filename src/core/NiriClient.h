#pragma once
#include "core/NiriWindow.h"
#include <QObject>
#include <QLocalSocket>
#include <functional>

namespace reader {
// Event-driven Niri connection. No processes, polling or synchronous waits.
class NiriClient final : public QObject
{
    Q_OBJECT
public:
    explicit NiriClient(QObject *parent = nullptr);
    void start();
    void stop();
    std::optional<NiriReaderWindow> window() const { return m_window; }
    bool moveWindow(quint64 id, const QPoint &position);
    void reloadConfig(std::function<void(bool)> completed);

private:
    void readEvents();
    void applyWindow(const QJsonObject &window);
    bool sendAction(const QJsonObject &action, std::function<void(bool)> completed = {});
    QLocalSocket m_events;
    QString m_socketPath;
    std::optional<NiriReaderWindow> m_window;
};
}
