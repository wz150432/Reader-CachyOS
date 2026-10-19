#include "OnlineBook.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUrl>
namespace reader {
namespace {
QString digest(const QByteArray &s) {
  return QString::fromLatin1(
      QCryptographicHash::hash(s, QCryptographicHash::Sha256).toHex());
}
QString normalized(const QString &s) {
  QUrl u(s);
  u.setFragment({});
  return u.adjusted(QUrl::NormalizePathSegments).toString(QUrl::FullyEncoded);
}
bool validUrl(const QString &s) {
  QUrl u(s);
  return u.isValid() && !u.host().isEmpty() &&
         (u.scheme() == "https" || u.scheme() == "http") &&
         u.userInfo().isEmpty();
}
bool writeJson(const QString &path, const QJsonObject &o, QString *e,
               qint64 limit = 16 * 1024 * 1024) {
  const auto b = QJsonDocument(o).toJson(QJsonDocument::Compact);
  if (b.size() > limit) {
    if (e)
      *e = "Cache size limit exceeded";
    return false;
  }
  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly)) {
    if (e)
      *e = "Cannot write cache";
    return false;
  }
  if (!f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
    if (e)
      *e = "Cannot protect cache permissions";
    return false;
  }
  if (f.write(b) != b.size() || !f.commit()) {
    if (e)
      *e = "Cannot commit cache";
    return false;
  }
  return true;
}
QJsonObject readJson(const QString &path, qint64 limit) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly) || f.size() > limit)
    return {};
  QJsonParseError e;
  auto d = QJsonDocument::fromJson(f.readAll(), &e);
  return e.error == QJsonParseError::NoError && d.isObject() ? d.object()
                                                             : QJsonObject{};
}
} // namespace
std::shared_ptr<OnlineBook>
OnlineBook::createManifest(const QString &root, const BookSource &source,
                           const QString &url, const QString &title,
                           const QJsonArray &chapters, QString *error) {
  if (!source.validate(error) || !validUrl(url) || title.isEmpty() ||
      chapters.isEmpty() || chapters.size() > 20000) {
    if (error)
      *error = "Invalid book manifest";
    return {};
  }
  QString identity =
      digest(source.id().toUtf8() + '\n' + normalized(url).toUtf8());
  QString dir = QDir(root).filePath(identity);
  if (!QDir().mkpath(dir)) {
    if (error)
      *error = "Cannot create cache directory";
    return {};
  }
  QString path = QDir(dir).filePath(identity + ".rnovel");
  QJsonObject j{{"version", 1},
                {"source", source.toJson()},
                {"url", normalized(url)},
                {"title", title},
                {"chapters", chapters}};
  // Validate before replacing an existing manifest.
  for (auto c : chapters)
    if (!c.isObject() || !c.toObject().value("title").isString() ||
        !validUrl(c.toObject().value("url").toString())) {
      if (error)
        *error = "Invalid chapter manifest";
      return {};
    }
  if (!writeJson(path, j, error, 8 * 1024 * 1024))
    return {};
  auto b = std::make_shared<OnlineBook>();
  if (!b->open(path, error))
    return {};
  return b;
}
bool OnlineBook::open(const QString &path, QString *error,
                      const QRegularExpression &) {
  auto j = readJson(path, 8 * 1024 * 1024);
  auto fail = [&]() {
    if (error)
      *error = "Invalid online book cache";
    return false;
  };
  BookSource source(j.value("source").toObject());
  auto chapters = j.value("chapters").toArray();
  if (j.value("version").toInt() != 1 || !source.validate() ||
      !validUrl(j.value("url").toString()) || !j.value("title").isString() ||
      chapters.isEmpty() || chapters.size() > 20000)
    return fail();
  QVector<Chapter> cs;
  QStringList urls;
  for (auto c : chapters) {
    auto o = c.toObject();
    if (!c.isObject() || !o.value("title").isString() ||
        !validUrl(o.value("url").toString()))
      return fail();
    cs.append({o.value("title").toString(), 0});
    urls.append(normalized(o.value("url").toString()));
  }
  m_filePath = QFileInfo(path).absoluteFilePath();
  m_title = j.value("title").toString();
  m_url = j.value("url").toString();
  m_source = source;
  m_chapters = cs;
  m_urls = urls;
  return true;
}
QString OnlineBook::chapterUrl(int i) const {
  return i >= 0 && i < m_urls.size() ? m_urls[i] : QString();
}
bool OnlineBook::refreshManifest(const QString &title, const QJsonArray &chapters,
                                 QString *error) {
  if (m_filePath.isEmpty() || title.isEmpty() || chapters.isEmpty() || chapters.size() > 20000) {
    if (error) *error = "Invalid book manifest";
    return false;
  }
  for (auto chapter : chapters) {
    if (!chapter.isObject() || !chapter.toObject().value("title").isString() ||
        !validUrl(chapter.toObject().value("url").toString())) {
      if (error) *error = "Invalid chapter manifest";
      return false;
    }
  }
  const QJsonObject json{{"version", 1}, {"source", m_source.toJson()},
                         {"url", m_url}, {"title", title}, {"chapters", chapters}};
  return writeJson(m_filePath, json, error, 8 * 1024 * 1024) && open(m_filePath, error);
}
QString OnlineBook::chapterFilePath(int i) const {
  return chapterUrl(i).isEmpty()
             ? QString()
             : QFileInfo(m_filePath)
                   .dir()
                   .filePath(digest(chapterUrl(i).toUtf8()) + ".json");
}
bool OnlineBook::saveChapter(int i, const QString &text, QString *error) {
  if (chapterUrl(i).isEmpty() || text.toUtf8().size() > 8 * 1024 * 1024) {
    if (error)
      *error = "Invalid chapter body";
    return false;
  }
  return writeJson(chapterFilePath(i),
                   {{"version", 1},
                    {"url", chapterUrl(i)},
                    {"text", text},
                    {"sha256", digest(text.toUtf8())}},
                   error);
}
bool OnlineBook::chapterAvailable(int i) const {
  if (chapterUrl(i).isEmpty())
    return false;
  auto j = readJson(chapterFilePath(i), 16 * 1024 * 1024);
  return j.value("version").toInt() == 1 &&
         j.value("url").toString() == chapterUrl(i) &&
         j.value("text").isString() &&
         j.value("sha256").toString() ==
             digest(j.value("text").toString().toUtf8());
}
QString OnlineBook::chapterText(int i) const {
  if (!chapterAvailable(i))
    return {};
  return readJson(chapterFilePath(i), 16 * 1024 * 1024)
      .value("text")
      .toString();
}
} // namespace reader
