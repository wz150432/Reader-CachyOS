#pragma once
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QTimer>
namespace reader {
class OnlineEngineBridge : public QObject {
  Q_OBJECT
public:
  explicit OnlineEngineBridge(QObject *parent = nullptr);
  ~OnlineEngineBridge() override;
  void setExecutablePath(const QString &path);
  QString executablePath() const { return m_path; }
  void setTimeoutMs(int ms) { m_timeoutMs = qMax(1, ms); }
  QString request(const QString &op, const QJsonObject &fields,
                  const QString &session);
  void cancel(const QString &id);
  void stop();
signals:
  void ready();
  void result(const QString &id, const QString &session,
              const QJsonObject &value);
  void error(const QString &id, const QString &session, const QString &message);

private:
  struct Pending {
    QJsonObject message;
    QTimer *timer = nullptr;
    bool sent = false;
  };
  void start();
  void flush();
  void receive();
  void failAll(const QString &message);
  void finishStopping();
  void send(const QJsonObject &);
  QProcess m_process;
  QString m_path;
  QByteArray m_buffer;
  QMap<QString, Pending> m_pending;
  quint64 m_id = 0;
  bool m_ready = false;
  // The dying child must finish before queued requests can start a new one.
  bool m_stopping = false;
  // Zero selects bounded per-operation budgets after a separate 5s hello wait.
  // Explicit overrides retain an end-to-end deadline for tests/callers.
  int m_timeoutMs = 0;
};
} // namespace reader
