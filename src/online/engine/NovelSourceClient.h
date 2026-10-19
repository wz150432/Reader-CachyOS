#pragma once
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <memory>
class QNetworkCookieJar;
class NovelSourceClient : public QObject {
  Q_OBJECT
public:
  explicit NovelSourceClient(QObject *parent = nullptr);
  static bool validEnvelope(const QJsonObject &);
  void submit(QJsonObject);
  void cancel(const QString &);
signals:
  void completed(QJsonObject);

private:
  struct Job;
  QNetworkAccessManager network;
  QHash<QString, std::shared_ptr<Job>> jobs;
  QHash<QString, qint64> last;
  QHash<QByteArray, QNetworkCookieJar *> cookieJars;
  void fetch(std::shared_ptr<Job>, QUrl, int = 0, int = 0, bool = false);
  void finish(std::shared_ptr<Job>, QString = {});
  void parse(std::shared_ptr<Job>, QByteArray, QUrl, QString = {});
};
