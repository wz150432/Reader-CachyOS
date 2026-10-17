#include "BookSource.h"
#include <QRegularExpression>
#include <QUrl>
namespace reader {
bool BookSource::validate(QString *error) const {
  auto fail = [&](const QString &s) {
    if (error)
      *error = s;
    return false;
  };
  for (auto key :
       {"id", "kind", "name", "baseUrl", "searchUrl", "searchMethod",
        "searchBody", "resultXPath", "titleXPath", "linkXPath", "authorXPath",
        "tocUrlXPath", "chapterXPath", "contentXPath", "removeXPath",
        "tocNextXPath", "contentNextXPath", "encoding", "cookie"}) {
    if (m_json.contains(key) && !m_json.value(key).isString())
      return fail("Invalid source field type");
  }
  for (auto key : {"enabled", "reverseToc"}) {
    if (m_json.contains(key) && !m_json.value(key).isBool())
      return fail("Invalid source flag");
  }
  if (!QRegularExpression("^[A-Za-z0-9_-]{1,128}$").match(id()).hasMatch())
    return fail("Invalid source ID");
  if (m_json.value("name").toString().trimmed().isEmpty())
    return fail("Missing source name");
  auto validUrl = [](const QString &s) {
    QUrl u(s);
    return u.isValid() && !u.host().isEmpty() &&
           (u.scheme() == "https" || u.scheme() == "http") &&
           u.userInfo().isEmpty();
  };
  if (!validUrl(m_json.value("baseUrl").toString()))
    return fail("Invalid source URL");
  QString kind = m_json.value("kind").toString();
  if (kind != "html" && kind != "mediawiki")
    return fail("Invalid source kind");
  if (kind == "html") {
    for (auto key : {"chapterXPath", "contentXPath"})
      if (m_json.value(key).toString().trimmed().isEmpty())
        return fail("Missing content rules");
    QString method = m_json.value("searchMethod").toString("GET");
    if (method != "GET" && method != "POST")
      return fail("Invalid search method");
    QString enc = m_json.value("encoding").toString("auto");
    if (enc != "auto" && enc != "UTF-8" && enc != "GB18030")
      return fail("Invalid encoding");
    QString search = m_json.value("searchUrl").toString();
    if (!search.isEmpty()) {
      search.replace("{keyword}", "test").replace("{page}", "1");
      if (!validUrl(search))
        return fail("Invalid search URL");
      for (auto key : {"resultXPath", "titleXPath", "linkXPath"})
        if (m_json.value(key).toString().isEmpty())
          return fail("Missing search rules");
    }
  }
  if (m_json.contains("intervalMs") &&
      (!m_json.value("intervalMs").isDouble() ||
       m_json.value("intervalMs").toInt() < 500 ||
       m_json.value("intervalMs").toDouble() !=
           m_json.value("intervalMs").toInt()))
    return fail("Invalid request interval");
  if (m_json.contains("headers") && !m_json.value("headers").isObject())
    return fail("Invalid headers");
  auto headers = m_json.value("headers").toObject();
  for (auto it = headers.begin(); it != headers.end(); ++it)
    if (!QRegularExpression("^[!#$%&'*+.^_`|~0-9A-Za-z-]+$")
             .match(it.key())
             .hasMatch() ||
        !it.value().isString() || it.value().toString().contains('\r') ||
        it.value().toString().contains('\n'))
      return fail("Invalid header");
  return true;
}
BookSource BookSource::preset() {
  return BookSource({{"id", "wikisource-zh"},
                     {"kind", "mediawiki"},
                     {"name", QString::fromUtf8("中文维基文库")},
                     {"baseUrl", "https://zh.wikisource.org"},
                     {"enabled", true}});
}
BookSource BookSource::copy(const QString &id, const QString &name) const {
  auto j = m_json;
  j["id"] = id;
  j["name"] = name;
  return BookSource(j);
}
} // namespace reader
