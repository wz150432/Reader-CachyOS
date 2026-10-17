#include "BookSourceStore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
namespace reader {
bool BookSourceStore::load(QString *error) {
  QFile f(m_path);
  if (!f.exists()) {
    m_sources = {BookSource::preset()};
    m_corrupt = false;
    return true;
  }
  auto fail = [&]() {
    m_corrupt = true;
    if (error)
      *error = "Invalid source settings";
    return false;
  };
  if (!f.open(QIODevice::ReadOnly) || f.size() > 8 * 1024 * 1024)
    return fail();
  QJsonParseError e;
  auto d = QJsonDocument::fromJson(f.readAll(), &e);
  if (e.error != QJsonParseError::NoError || !d.isArray())
    return fail();
  QVector<BookSource> result{BookSource::preset()};
  QSet<QString> ids;
  for (auto v : d.array()) {
    if (!v.isObject())
      return fail();
    BookSource s(v.toObject());
    if (!s.validate() || ids.contains(s.id()))
      return fail();
    ids.insert(s.id());
    if (s.id() != BookSource::preset().id())
      result.append(s);
  }
  m_sources = result;
  m_corrupt = false;
  return true;
}
bool BookSourceStore::save(QString *error) {
  auto fail = [&]() {
    if (error)
      *error = "Cannot save source settings";
    return false;
  };
  if (m_corrupt)
    return fail();
  QJsonArray a;
  for (const auto &s : m_sources) {
    if (!s.validate(error))
      return false;
    a.append(s.toJson());
  }
  const auto data = QJsonDocument(a).toJson();
  if (data.size() > 8 * 1024 * 1024)
    return fail();
  QDir().mkpath(QFileInfo(m_path).absolutePath());
  QSaveFile f(m_path);
  if (!f.open(QIODevice::WriteOnly))
    return fail();
  if (!f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner))
    return fail();
  if (f.write(data) != data.size() || !f.commit())
    return fail();
  return true;
}
bool BookSourceStore::upsert(const BookSource &s, QString *error) {
  if (s.id() == BookSource::preset().id()) {
    if (error)
      *error = "Preset is immutable";
    return false;
  }
  if (!s.validate(error))
    return false;
  for (auto &old : m_sources)
    if (old.id() == s.id()) {
      old = s;
      return true;
    }
  m_sources.append(s);
  return true;
}
bool BookSourceStore::remove(const QString &id, QString *error) {
  if (id == BookSource::preset().id()) {
    if (error)
      *error = "Preset is immutable";
    return false;
  }
  for (int i = 0; i < m_sources.size(); ++i)
    if (m_sources[i].id() == id) {
      m_sources.removeAt(i);
      return true;
    }
  return false;
}
} // namespace reader
