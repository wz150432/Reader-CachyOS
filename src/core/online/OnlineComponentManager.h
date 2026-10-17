#pragma once
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <atomic>
#include <memory>
class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryFile;
class QThread;
namespace reader {
class OnlineComponentManager : public QObject {
  Q_OBJECT
public:
  explicit OnlineComponentManager(QString root, QString appVersion,
                                  QJsonObject metadata = {},
                                  QObject *parent = nullptr);
  ~OnlineComponentManager() override;
  bool isInstalled() const;
  bool isEnabled() const;
  bool isBusy() const { return m_busy; }
  QString executablePath() const;
  QString rootPath() const { return m_root; }
  QJsonObject metadata() const { return m_metadata; }
  QJsonObject installedMetadata() const;
  bool downloadAvailable() const;
  bool validateMetadata(const QJsonObject &, QString *error = nullptr) const;
  bool importPackage(const QString &archive, const QJsonObject &,
                     QString *error = nullptr);
  bool download(QString *error = nullptr);
  bool setEnabled(bool, QString *error = nullptr);
  bool uninstall(QString *error = nullptr);
  static QJsonObject bundledMetadata();
public slots:
  void cancel();
signals:
  void changed();
  void progress(qint64 received, qint64 total);
  void finished(bool success, const QString &error);
  void stopping();

private:
  void startDownload(const QUrl &, int redirects);
  void extract(const QString &, const QJsonObject &);
  void verifyHelper(const QString &, const QJsonObject &);
  void complete(bool, const QString &);
  bool writeRecord(const QJsonObject &, QString *);
  bool safeRoot() const;
  QString m_root, m_appVersion, m_stage;
  QJsonObject m_metadata, m_candidate;
  QNetworkAccessManager *m_network;
  QPointer<QNetworkReply> m_reply;
  std::unique_ptr<QTemporaryFile> m_download;
  QThread *m_worker = nullptr;
  std::shared_ptr<std::atomic_bool> m_cancel;
  QProcess m_probe;
  QTimer m_timeout;
  QByteArray m_hello;
  bool m_busy = false;
  bool m_probing = false;
  quint64 m_generation = 0;
};
} // namespace reader
