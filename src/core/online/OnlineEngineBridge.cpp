#include "OnlineEngineBridge.h"
#include <QJsonDocument>
namespace reader {
OnlineEngineBridge::OnlineEngineBridge(QObject *parent) : QObject(parent) {
  connect(&m_process, &QProcess::started, this, [this] {
    send({{"protocol", 1}, {"id", "0"}, {"session", ""}, {"op", "hello"}});
  });
  connect(&m_process, &QProcess::readyReadStandardOutput, this,
          &OnlineEngineBridge::receive);
  connect(&m_process, &QProcess::readyReadStandardError, this,
          [this] { m_process.readAllStandardError(); });
  connect(
      &m_process, &QProcess::errorOccurred, this,
      [this](QProcess::ProcessError) { failAll("Online engine unavailable"); });
  connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this](int, QProcess::ExitStatus) {
            failAll("Online engine stopped");
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
    emit error(id, p.message["session"].toString(), "Online request timed out");
    if (m_pending.isEmpty() && !m_ready) {
      m_process.kill();
    }
  });
  timer->start(m_timeoutMs);
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
  if (m_process.state() != QProcess::NotRunning)
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
    send(message);
  }
}
void OnlineEngineBridge::receive() {
  m_buffer += m_process.readAllStandardOutput();
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
      emit error(id, j["session"].toString(), "Online request failed");
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
  if (m_process.state() != QProcess::NotRunning)
    m_process.kill();
  for (auto it = pending.begin(); it != pending.end(); ++it) {
    it->timer->stop();
    it->timer->deleteLater();
    emit error(it.key(), it->message["session"].toString(), message);
  }
}
void OnlineEngineBridge::stop() { failAll("Online request cancelled"); }
} // namespace reader
