#include "OnlineComponentManager.h"
#include "OnlineMetadata.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QThread>
#include <QUuid>
#include <archive.h>
#include <archive_entry.h>
namespace reader {
namespace {
constexpr qint64 MaxPacked = 128 * 1024 * 1024, MaxUnpacked = 512 * 1024 * 1024;
bool fail(QString *e, const QString &message) {
  if (e)
    *e = message;
  return false;
}
QJsonObject readRecord(const QString &root) {
  if (QFileInfo(root + "/active.json").isSymLink())
    return {};
  QFile f(root + "/active.json");
  if (f.size() > 16384 || !f.open(QIODevice::ReadOnly))
    return {};
  return QJsonDocument::fromJson(f.readAll()).object();
}
bool noLinks(QString path) {
  for (QFileInfo f(path);; f = QFileInfo(f.absolutePath())) {
    if (f.isSymLink())
      return false;
    if (f.absoluteFilePath() == f.absolutePath())
      return true;
  }
}
bool official(const QUrl &u) {
  return u.isValid() && u.scheme() == "https" && u.host() == "github.com" &&
         u.port(-1) == -1 && u.userInfo().isEmpty() && u.query().isEmpty() &&
         u.fragment().isEmpty() &&
         u.path().startsWith("/wz150432/Reader-CachyOS/releases/download/") &&
         !u.path().contains("/../");
}
QString unpack(const QString &path, const QString &stage, const QJsonObject &j,
               const std::shared_ptr<std::atomic_bool> &cancel) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly) || f.size() != j["size"].toInteger())
    return "压缩包大小不匹配";
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!f.atEnd()) {
    if (*cancel)
      return "已取消安装";
    hash.addData(f.read(1024 * 1024));
  }
  if (hash.result().toHex() != j["sha256"].toString().toLatin1())
    return "压缩包 SHA-256 校验失败";
  auto a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_tar(a);
  archive_read_support_format_zip(a);
  QString error;
  if (archive_read_open_filename(a, QFile::encodeName(path).constData(),
                                 65536) != ARCHIVE_OK)
    error = "无法打开组件压缩包";
  archive_entry *entry = nullptr;
  qint64 total = 0;
  int count = 0;
  QSet<QString> names;
  while (error.isEmpty()) {
    int status = archive_read_next_header(a, &entry);
    if (status == ARCHIVE_EOF)
      break;
    if (status != ARCHIVE_OK) {
      error = "组件压缩包损坏";
      break;
    }
    QString name = QString::fromUtf8(archive_entry_pathname(entry));
    auto parts = name.split('/');
    while (!parts.isEmpty() && parts.last().isEmpty())
      parts.removeLast();
    bool safe =
        !name.isEmpty() && !QDir::isAbsolutePath(name) && !name.contains('\\');
    for (auto &p : parts)
      if (p == ".." || p == "." || p.isEmpty())
        safe = false;
    auto type = archive_entry_filetype(entry);
    qint64 size = archive_entry_size(entry);
    if (*cancel) {
      error = "已取消安装";
      break;
    }
    if (!safe || ++count > 256 || names.contains(name) ||
        archive_entry_symlink(entry) || archive_entry_hardlink(entry) ||
        (type != AE_IFREG && type != AE_IFDIR) || size < 0 ||
        size > MaxUnpacked || total + size > j["unpackedSize"].toInteger()) {
      error = "组件压缩包含不安全路径、链接或超出限制";
      break;
    }
    names.insert(name);
    QString out = stage + "/" + name;
    if (type == AE_IFDIR) {
      if (!QDir().mkpath(out))
        error = "无法创建组件目录";
      continue;
    }
    if (!QDir().mkpath(QFileInfo(out).absolutePath())) {
      error = "无法创建组件目录";
      break;
    }
    QFile target(out);
    if (!target.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
      error = "无法写入组件";
      break;
    }
    char buffer[65536];
    qint64 written = 0;
    for (;;) {
      auto n = archive_read_data(a, buffer, sizeof(buffer));
      if (n == 0)
        break;
      if (n < 0 || *cancel || written + n > size ||
          target.write(buffer, n) != n) {
        error = "组件解压失败或已取消";
        break;
      }
      written += n;
    }
    if (written != size)
      error = "组件文件大小不匹配";
    total += written;
    target.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                          (name == "reader-online"
                               ? QFileDevice::ExeOwner
                               : QFileDevice::Permissions{}));
  }
  archive_read_close(a);
  archive_read_free(a);
  if (error.isEmpty() && (total != j["unpackedSize"].toInteger() ||
                          !QFileInfo(stage + "/reader-online").isFile()))
    error = "组件入口或解压大小不匹配";
  return error;
}
} // namespace
OnlineComponentManager::OnlineComponentManager(QString root, QString version,
                                               QJsonObject metadata, QObject *p)
    : QObject(p), m_root(QDir(root).absolutePath()), m_appVersion(version),
      m_metadata(metadata), m_network(new QNetworkAccessManager(this)) {
  m_timeout.setSingleShot(true);
  m_timeout.setInterval(20000);
  m_probe.setProcessChannelMode(QProcess::SeparateChannels);
  m_probe.setStandardErrorFile(QProcess::nullDevice());
  connect(&m_timeout, &QTimer::timeout, this,
          [this] { complete(false, "组件握手超时"); });
  connect(&m_probe, &QProcess::started, this, [this] {
    if (!m_busy || !m_probing)
      return;
    m_probe.write("{\"protocol\":1,\"id\":\"install\",\"session\":\"install\","
                  "\"op\":\"hello\"}\n");
  });
  connect(&m_probe, &QProcess::readyReadStandardOutput, this, [this] {
    if (!m_busy || !m_probing) {
      m_probe.readAllStandardOutput();
      return;
    }
    m_hello += m_probe.readAllStandardOutput();
    if (m_hello.size() > 4096) {
      complete(false, "组件握手响应过大");
      return;
    }
    if (!m_hello.contains('\n'))
      return;
    auto j =
        QJsonDocument::fromJson(m_hello.left(m_hello.indexOf('\n'))).object();
    auto r = j["result"].toObject();
    if (j["protocol"].toInt() != 1 || j["id"] != "install" ||
        j["session"] != "install" || !j["ok"].toBool() ||
        r["protocol"].toInt() != 1 || r["version"] != m_candidate["version"]) {
      complete(false, "组件版本或协议握手不匹配");
      return;
    }
    QString active = "v" + m_candidate["version"].toString() + "-" +
                     QUuid::createUuid().toString(QUuid::Id128);
    if (!safeRoot() || !QDir().rename(m_stage, m_root + "/" + active)) {
      complete(false, "无法激活组件");
      return;
    }
    m_stage = m_root + "/" + active;
    QString error;
    if (!writeRecord({{"directory", active},
                      {"enabled", true},
                      {"metadata", m_candidate}},
                     &error)) {
      complete(false, error);
      return;
    }
    m_stage.clear();
    complete(true, {});
  });
  connect(&m_probe, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError) {
            if (m_busy && m_probing)
              complete(false, "组件无法运行");
          });
  connect(&m_probe, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this](int, QProcess::ExitStatus) {
            if (m_busy && m_probing)
              complete(false, "组件提前退出");
          });
}
OnlineComponentManager::~OnlineComponentManager() {
  ++m_generation;
  if (m_cancel)
    *m_cancel = true;
  if (m_reply) {
    m_reply->disconnect(this);
    m_reply->abort();
  }
  if (m_worker) {
    m_worker->wait();
    delete m_worker;
  }
  m_probe.disconnect(this);
  m_probe.kill();
  if (!m_stage.isEmpty() && safeRoot())
    QDir(m_stage).removeRecursively();
}
QJsonObject OnlineComponentManager::bundledMetadata() {
  return QJsonDocument::fromJson(
             QByteArray(reinterpret_cast<const char *>(onlineMetadata),
                        sizeof(onlineMetadata) - 1))
      .object();
}
bool OnlineComponentManager::safeRoot() const { return noLinks(m_root); }
bool OnlineComponentManager::validateMetadata(const QJsonObject &j,
                                              QString *e) const {
  if (!QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+(?:-[A-Za-z0-9.-]+)?$")
           .match(j["version"].toString())
           .hasMatch() ||
      j["protocol"].toDouble() != 1 ||
      j["appVersion"].toString() != m_appVersion || j["platform"] != "linux" ||
      j["arch"] != "x86_64" || j["entrypoint"] != "reader-online")
    return fail(e, "组件版本、平台或协议不兼容");
  for (auto key : {"size", "unpackedSize"}) {
    auto n = j[key].toDouble(-1);
    if (n <= 0 || n != j[key].toInteger() ||
        n > (QString(key) == "size" ? MaxPacked : MaxUnpacked))
      return fail(e, "组件大小超出限制");
  }
  if (!QRegularExpression("^[a-f0-9]{64}$")
           .match(j["sha256"].toString())
           .hasMatch() ||
      (!j["url"].isString() ||
       (!j["url"].toString().isEmpty() && !official(QUrl(j["url"].toString())))))
    return fail(e, "组件校验或官方 HTTPS 下载地址无效");
  return true;
}
QJsonObject OnlineComponentManager::installedMetadata() const {
  if (!safeRoot())
    return {};
  auto r = readRecord(m_root);
  QString d = r["directory"].toString();
  if (!QRegularExpression("^v[0-9A-Za-z.-]+-[a-f0-9]{32}$")
           .match(d)
           .hasMatch() ||
      !validateMetadata(r["metadata"].toObject()))
    return {};
  QString path = m_root + "/" + d + "/reader-online";
  QFileInfo f(path);
  if (!noLinks(path) || !f.isFile() || !f.isExecutable())
    return {};
  return r["metadata"].toObject();
}
bool OnlineComponentManager::isInstalled() const {
  return !installedMetadata().isEmpty();
}
bool OnlineComponentManager::isEnabled() const {
  return isInstalled() && readRecord(m_root)["enabled"].toBool();
}
QString OnlineComponentManager::executablePath() const {
  return isEnabled()
             ? m_root + "/" + readRecord(m_root)["directory"].toString() +
                   "/reader-online"
             : QString();
}
bool OnlineComponentManager::downloadAvailable() const {
  return validateMetadata(m_metadata) &&
         official(QUrl(m_metadata["url"].toString()));
}
bool OnlineComponentManager::writeRecord(const QJsonObject &j, QString *e) {
  if (!safeRoot() || QFileInfo(m_root + "/active.json").isSymLink() ||
      !QDir().mkpath(m_root))
    return fail(e, "组件目录不安全或不可写");
  QSaveFile f(m_root + "/active.json");
  auto b = QJsonDocument(j).toJson(QJsonDocument::Compact);
  if (!f.open(QIODevice::WriteOnly) ||
      !f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
      f.write(b) != b.size() || !f.commit())
    return fail(e, "无法保存组件状态");
  return true;
}
bool OnlineComponentManager::setEnabled(bool enabled, QString *e) {
  if (m_busy || !isInstalled())
    return fail(e, "组件未安装或安装任务进行中");
  emit stopping();
  auto j = readRecord(m_root);
  j["enabled"] = enabled;
  if (!writeRecord(j, e))
    return false;
  emit changed();
  return true;
}
bool OnlineComponentManager::uninstall(QString *e) {
  if (m_busy || !safeRoot())
    return fail(e, "组件目录不安全或任务进行中");
  emit stopping();
  if (!QFile::exists(m_root + "/active.json"))
    return true;
  if (!isInstalled())
    return fail(e, "组件激活记录无效，请检查组件目录");
  QString d = readRecord(m_root)["directory"].toString();
  if (!QFile::remove(m_root + "/active.json"))
    return fail(e, "无法移除组件状态");
  for (const auto &entry :
       QDir(m_root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    if (QRegularExpression("^v[0-9A-Za-z.-]+-[a-f0-9]{32}$")
            .match(entry.fileName())
            .hasMatch() &&
        !entry.isSymLink())
      QDir(entry.absoluteFilePath()).removeRecursively();
  }
  emit changed();
  return true;
}
bool OnlineComponentManager::importPackage(const QString &path,
                                           const QJsonObject &j, QString *e) {
  if (m_busy)
    return fail(e, "已有组件任务进行中");
  if (!validateMetadata(j, e) || !safeRoot())
    return fail(e, e && !e->isEmpty() ? *e : "组件目录不安全");
  if (!QFileInfo(path).isFile())
    return fail(e, "请选择组件压缩包");
  m_busy = true;
  m_cancel = std::make_shared<std::atomic_bool>(false);
  extract(path, j);
  return true;
}
void OnlineComponentManager::extract(const QString &path,
                                     const QJsonObject &j) {
  m_candidate = j;
  m_stage = m_root + "/.stage-" + QUuid::createUuid().toString(QUuid::Id128);
  if (!safeRoot() || !QDir().mkpath(m_stage)) {
    complete(false, "无法创建安装暂存目录");
    return;
  }
  auto token = ++m_generation;
  auto stage = m_stage;
  auto flag = m_cancel;
  m_worker = QThread::create([this, path, stage, j, flag, token] {
    auto error = unpack(path, stage, j, flag);
    QMetaObject::invokeMethod(
        this,
        [this, error, j, token] {
          if (token != m_generation)
            return;
          auto worker = m_worker;
          m_worker = nullptr;
          worker->wait();
          delete worker;
          if (!error.isEmpty() || *m_cancel) {
            complete(false, error.isEmpty() ? "已取消安装" : error);
            return;
          }
          verifyHelper(m_stage, j);
        },
        Qt::QueuedConnection);
  });
  m_worker->start();
}
void OnlineComponentManager::verifyHelper(const QString &stage,
                                          const QJsonObject &j) {
  if (!m_busy)
    return;
  if (m_probe.state() != QProcess::NotRunning) {
    QTimer::singleShot(10, this, [this, stage, j] { verifyHelper(stage, j); });
    return;
  }
  m_probing = true;
  m_candidate = j;
  m_hello.clear();
  m_probe.setProgram(stage + "/reader-online");
  m_probe.setArguments({});
  m_timeout.start();
  m_probe.start();
}
bool OnlineComponentManager::download(QString *e) {
  if (m_busy)
    return fail(e, "已有组件任务进行中");
  if (!downloadAvailable())
    return fail(e, "未配置组件下载，请导入本地组件");
  if (!safeRoot())
    return fail(e, "组件目录不安全");
  m_download = std::make_unique<QTemporaryFile>();
  if (!m_download->open())
    return fail(e, "无法创建下载文件");
  m_busy = true;
  m_cancel = std::make_shared<std::atomic_bool>(false);
  startDownload(QUrl(m_metadata["url"].toString()), 0);
  return true;
}
void OnlineComponentManager::startDownload(const QUrl &url, int redirects) {
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                   QNetworkRequest::ManualRedirectPolicy);
  req.setTransferTimeout(20000);
  m_reply = m_network->get(req);
  m_reply->setReadBufferSize(65536);
  connect(m_reply, &QNetworkReply::readyRead, this, [this] {
    if (!m_reply)
      return;
    auto bytes = m_reply->readAll();
    int status =
        m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 300 && status < 400)
      return;
    if (m_download->size() + bytes.size() > m_metadata["size"].toInteger() ||
        m_download->write(bytes) != bytes.size()) {
      m_reply->abort();
    }
  });
  connect(m_reply, &QNetworkReply::downloadProgress, this,
          &OnlineComponentManager::progress);
  connect(m_reply, &QNetworkReply::finished, this, [this, redirects] {
    auto reply = m_reply;
    m_reply = nullptr;
    int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 300 && status < 400 && !*m_cancel) {
      QUrl next = reply->url().resolved(
          reply->attribute(QNetworkRequest::RedirectionTargetAttribute)
              .toUrl());
      bool trusted = next.scheme() == "https" && next.userInfo().isEmpty() &&
                     next.port(-1) == -1 &&
                     (official(next) ||
                      next.host() == "release-assets.githubusercontent.com" ||
                      next.host() == "objects.githubusercontent.com");
      reply->deleteLater();
      if (!trusted || redirects >= 5) {
        complete(false, "组件下载重定向不安全或过多");
        return;
      }
      m_download->resize(0);
      m_download->seek(0);
      startDownload(next, redirects + 1);
      return;
    }
    bool ok =
        reply->error() == QNetworkReply::NoError &&
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() ==
            200;
    reply->deleteLater();
    if (!ok || *m_cancel) {
      complete(false,
               *m_cancel ? "已取消安装" : "组件下载失败，请重试或导入本地组件");
      return;
    }
    m_download->flush();
    extract(m_download->fileName(), m_metadata);
  });
}
void OnlineComponentManager::cancel() {
  if (!m_busy)
    return;
  if (m_cancel)
    *m_cancel = true;
  if (m_reply) {
    m_reply->abort();
    return;
  }
  if (!m_worker)
    complete(false, "已取消安装");
}
void OnlineComponentManager::complete(bool ok, const QString &error) {
  if (!m_busy)
    return;
  m_busy = false;
  m_probing = false;
  m_timeout.stop();
  m_probe.kill();
  if (!m_stage.isEmpty() && safeRoot()) {
    QDir(m_stage).removeRecursively();
    m_stage.clear();
  }
  m_download.reset();
  emit changed();
  emit finished(ok, error);
}
} // namespace reader
