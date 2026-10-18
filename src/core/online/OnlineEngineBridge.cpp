#include "OnlineEngineBridge.h"
#include <QJsonDocument>
#include <QRegularExpression>
namespace {
int operationBudgetMs(const QJsonObject &message) {
  if (message["op"] == "validate")
    return 5000;
  // Per page: up to three attempts with six redirect hops each, plus
  // two server-requested waits. Keep this distinct from each HTTP's 20s.
  constexpr qint64 hardLimitMs = 24LL * 60 * 60 * 1000;
  const qint64 interval = qBound(qint64(500),
      qint64(message["source"].toObject()["intervalMs"].toInt(1000)), hardLimitMs);
  const qint64 pages = message["op"] == "toc" ? 100 : message["op"] == "chapter" ? 20 : 1;
  const qint64 budget = pages * (18 * (20000 + interval) + 2 * 30000);
  return int(qMin(hardLimitMs, budget));
}
QString safeEngineError(const QString &raw, bool validation) {
  const QStringList fixed{
      "Malformed protocol", "Duplicate request ID", "Unknown operation",
      "Source disabled", "Unknown source kind", "Invalid URL", "Pagination cycle",
      "Pagination limit", "Redirect limit", "Response limit exceeded", "Unsafe redirect",
      "Rate limit exceeded", "Request timeout", "Network failure", "MediaWiki API error",
      "Chapter limit exceeded", "Invalid HTML", "Invalid XPath", "Browser challenge", "Empty content"};
  // Absolute anchors reject trailing newlines as well as secret-bearing suffixes.
  if (fixed.contains(raw) || QRegularExpression("\\AHTTP [45][0-9]{2}\\z").match(raw).hasMatch())
    return raw;
  if (validation) {
    for (const auto key : {"resultXPath", "titleXPath", "linkXPath", "authorXPath",
                           "tocUrlXPath", "chapterXPath", "contentXPath", "removeXPath",
                           "tocNextXPath", "contentNextXPath"})
      if (raw == "Invalid XPath: " + QString::fromLatin1(key))
        return raw;
  }
  return "Online request failed";
}
} // namespace
namespace reader {
OnlineEngineBridge::OnlineEngineBridge(QObject *parent) : QObject(parent) {
  connect(&m_process, &QProcess::started, this, [this] {
    if (m_stopping)
      return;
    send({{"protocol", 1}, {"id", "0"}, {"session", ""}, {"op", "hello"}});
  });
  connect(&m_process, &QProcess::readyReadStandardOutput, this,
          &OnlineEngineBridge::receive);
  connect(&m_process, &QProcess::readyReadStandardError, this,
          [this] { m_process.readAllStandardError(); });
  connect(
      &m_process, &QProcess::errorOccurred, this,
      [this](QProcess::ProcessError error) {
        if (m_stopping) {
          // FailedToStart has no subsequent finished signal.
          if (error == QProcess::FailedToStart)
            finishStopping();
          return;
        }
        failAll("Online engine unavailable");
      });
  connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this](int, QProcess::ExitStatus) {
            if (m_stopping) {
              finishStopping();
            } else {
              failAll("Online engine stopped");
            }
          });
}
OnlineEngineBridge::~OnlineEngineBridge() {
  disconnect(&m_process, nullptr, this, nullptr);
  m_process.kill();
}
void OnlineEngineBridge::setExecutablePath(const QString &path) {
  if (m_path == path)
    return;
  stop();
  m_path = path;
}
QString OnlineEngineBridge::request(const QString &op,
                                    const QJsonObject &fields,
                                    const QString &session) {
  QString id = QString::number(++m_id);
  QJsonObject j = fields;
  j["protocol"] = 1;
  j["id"] = id;
  j["session"] = session;
  j["op"] = op;
  auto timer = new QTimer(this);
  timer->setSingleShot(true);
  m_pending.insert(id, {j, timer, false});
  connect(timer, &QTimer::timeout, this, [this, id] {
    if (!m_pending.contains(id))
      return;
    auto p = m_pending.take(id);
    p.timer->deleteLater();
    if (p.sent)
      send({{"protocol", 1},
            {"id", QString::number(++m_id)},
            {"session", p.message["session"]},
            {"op", "cancel"},
            {"target", id}});
    emit error(id, p.message["session"].toString(),
               p.sent || m_timeoutMs > 0 ? "Online request timed out"
                                        : "Online engine handshake timed out");
    if (m_pending.isEmpty() && !m_ready) {
      m_process.kill();
    }
  });
  timer->start(m_timeoutMs > 0 ? m_timeoutMs : 5000);
  QTimer::singleShot(0, this, [this, id, op] {
    if (!m_pending.contains(id))
      return;
    if (op != "search" && op != "toc" && op != "chapter" && op != "validate") {
      auto p = m_pending.take(id);
      p.timer->deleteLater();
      emit error(id, p.message["session"].toString(),
                 "Invalid online operation");
      return;
    }
    start();
    flush();
  });
  return id;
}
void OnlineEngineBridge::start() {
  if (m_stopping || m_process.state() != QProcess::NotRunning)
    return;
  m_ready = false;
  m_buffer.clear();
  if (m_path.isEmpty()) {
    failAll("Online engine unavailable");
    return;
  }
  m_process.setProgram(m_path);
  m_process.setArguments({});
  m_process.start();
}
void OnlineEngineBridge::send(const QJsonObject &j) {
  auto b = QJsonDocument(j).toJson(QJsonDocument::Compact);
  if (b.size() > 16 * 1024 * 1024) {
    failAll("Online protocol limit exceeded");
    return;
  }
  m_process.write(b + '\n');
}
void OnlineEngineBridge::flush() {
  if (!m_ready)
    return;
  const auto ids = m_pending.keys();
  for (const auto &id : ids) {
    auto it = m_pending.find(id);
    if (it == m_pending.end() || it->sent)
      continue;
    it->sent = true;
    const QJsonObject message = it->message;
    if (m_timeoutMs == 0)
      it->timer->start(operationBudgetMs(message));
    send(message);
  }
}
void OnlineEngineBridge::receive() {
  auto bytes = m_process.readAllStandardOutput();
  if (m_stopping)
    return;
  m_buffer += bytes;
  constexpr int limit = 16 * 1024 * 1024;
  while (true) {
    int nl = m_buffer.indexOf('\n');
    if (nl < 0) {
      if (m_buffer.size() > limit)
        failAll("Online protocol limit exceeded");
      return;
    }
    if (nl > limit) {
      failAll("Online protocol limit exceeded");
      return;
    }
    auto line = m_buffer.left(nl);
    m_buffer.remove(0, nl + 1);
    QJsonParseError parse;
    auto doc = QJsonDocument::fromJson(line, &parse);
    auto j = doc.object();
    if (parse.error != QJsonParseError::NoError || !doc.isObject() ||
        j["protocol"].toInt() != 1 || !j["id"].isString() ||
        !j["session"].isString() || !j["ok"].isBool() ||
        (j["ok"].toBool() ? !j["result"].isObject() : !j["error"].isString())) {
      failAll("Invalid online protocol reply");
      return;
    }
    QString id = j["id"].toString();
    if (id == "0") {
      auto r = j["result"].toObject();
      if (m_ready || !j["ok"].toBool() || r["protocol"].toInt() != 1) {
        failAll("Incompatible online engine");
        return;
      }
      m_ready = true;
      emit ready();
      flush();
      continue;
    }
    if (!m_ready) {
      failAll("Online engine handshake missing");
      return;
    }
    if (!m_pending.contains(id))
      continue;
    auto p = m_pending.value(id);
    if (j["session"] != p.message["session"])
      continue;
    m_pending.remove(id);
    p.timer->stop();
    p.timer->deleteLater();
    if (j["ok"].toBool())
      emit result(id, j["session"].toString(), j["result"].toObject());
    else
      emit error(id, j["session"].toString(),
                 safeEngineError(j["error"].toString(), p.message["op"] == "validate"));
  }
}
void OnlineEngineBridge::cancel(const QString &id) {
  if (!m_pending.contains(id))
    return;
  auto p = m_pending.take(id);
  p.timer->stop();
  p.timer->deleteLater();
  if (p.sent)
    send({{"protocol", 1},
          {"id", QString::number(++m_id)},
          {"session", p.message["session"]},
          {"op", "cancel"},
          {"target", id}});
}
void OnlineEngineBridge::failAll(const QString &message) {
  m_ready = false;
  m_buffer.clear();
  auto pending = m_pending;
  m_pending.clear();
  if (m_process.state() != QProcess::NotRunning) {
    m_stopping = true;
    m_process.kill();
  }
  for (auto it = pending.begin(); it != pending.end(); ++it) {
    it->timer->stop();
    it->timer->deleteLater();
    emit error(it.key(), it->message["session"].toString(), message);
  }
}
void OnlineEngineBridge::finishStopping() {
  m_stopping = false;
  if (!m_pending.isEmpty())
    QTimer::singleShot(0, this, [this] {
      if (!m_pending.isEmpty())
        start();
    });
}
void OnlineEngineBridge::stop() { failAll("Online request cancelled"); }
} // namespace reader
