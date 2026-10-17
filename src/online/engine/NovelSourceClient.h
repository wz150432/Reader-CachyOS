#pragma once
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <memory>
class NovelSourceClient : public QObject {
  Q_OBJECT
public:
  explicit NovelSourceClient(QObject *parent = nullptr);
  void submit(QJsonObject);
  void cancel(const QString &);
signals:
  void completed(QJsonObject);

private:
  struct Job;
  QNetworkAccessManager network;
  QHash<QString, std::shared_ptr<Job>> jobs;
  QHash<QString, qint64> last;
  void fetch(std::shared_ptr<Job>, QUrl, int = 0, int = 0);
  void finish(std::shared_ptr<Job>, QString = {});
  void parse(std::shared_ptr<Job>, QByteArray, QUrl, QString = {});
};
