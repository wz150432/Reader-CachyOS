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
  for (auto key : {"enabled", "reverseToc", "builtin"}) {
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
  return BookSource({{"id", "wikisource-zh"}, {"builtin", true},
                     {"kind", "mediawiki"},
                     {"name", QString::fromUtf8("中文维基文库")},
                     {"baseUrl", "https://zh.wikisource.org"},
                     {"enabled", true}});
}
QVector<BookSource> BookSource::presets() {
  auto mirror = [](const QString &id) {
    const QString base = "https://www." + id + ".com";
    return BookSource({
        {"id", id}, {"builtin", true}, {"kind", "html"},
        {"name", QString::fromUtf8("笔趣阁（%1.com，备用）").arg(id)},
        {"baseUrl", base}, {"enabled", true},
        {"searchUrl", base + "/search.php?q={keyword}&p={page}"},
        {"searchMethod", "GET"},
        {"resultXPath", "//dl[dd/h3/a]"},
        {"titleXPath", "substring-after(.//h3/a, ']')"},
        {"linkXPath", ".//h3/a/@href"},
        {"authorXPath", ".//dd[@class='book_other']/span"},
        {"chapterXPath", "//div[contains(concat(' ',normalize-space(@class),' '),' book_list2 ')]//li/a"},
        {"tocNextXPath", "//div[@class='pages']//li[contains(concat(' ',normalize-space(@class),' '),' active ')]/following-sibling::li[a[@href]][1]/a/@href"},
        {"contentXPath", "//article[contains(@class,'font_')]"},
        {"contentNextXPath", "//a[@id='next' and contains(@href,'_')]/@href"},
        {"removeXPath", "//article/text()[contains(.,'第(') and contains(.,')页') and string-length(.)<40]"},
        {"encoding", "UTF-8"}, {"intervalMs", 2500}});
  };
  return {preset(), BookSource({
      {"id", QStringLiteral("bbbiquge")},
      {"builtin", true},
      {"kind", QStringLiteral("html")},
      {"name", QStringLiteral("新笔趣阁")},
      {"baseUrl", QStringLiteral("http://www.bbbiquge.com")},
      {"enabled", true},
      {"searchUrl", QStringLiteral("http://www.bbbiquge.com/search34.html?searchkey={keyword}")},
      {"searchMethod", QStringLiteral("GET")},
      {"resultXPath", QStringLiteral("//article[@id='main']//dl[dt/a[@href]]")},
      {"titleXPath", QStringLiteral(".//dt/a")},
      {"linkXPath", QStringLiteral(".//dt/a/@href")},
      {"authorXPath", QStringLiteral(".//div[@class='btm']/a")},
      {"chapterXPath", QStringLiteral("//div[@id='list']//a[@rel='chapter' and preceding-sibling::dt[contains(.,'全部章节')]]")},
      {"contentXPath", QStringLiteral("//*[@id='chaptercontent']")},
      {"contentNextXPath", QStringLiteral("//a[@id='next_url' and contains(normalize-space(.),'下一页')]/@href")},
      {"removeXPath", QStringLiteral("//div[@id='booktxt']/p[contains(.,'一秒记住') or contains(.,'请收藏')]")},
      {"encoding", QStringLiteral("UTF-8")},
      {"intervalMs", 1000}}),
      BookSource({
          {"id", "biquge5200"}, {"builtin", true}, {"kind", "html"},
          {"name", QString::fromUtf8("笔趣阁5200")},
          {"baseUrl", "http://www.biquge5200.cc"}, {"enabled", true},
          {"searchUrl", "http://www.biquge5200.cc/modules/article/search.php?searchkey={keyword}"},
          {"searchMethod", "GET"}, {"resultXPath", "//tr[td[1]/a]"},
          {"titleXPath", "./td[1]/a"}, {"linkXPath", "./td[1]/a/@href"},
          {"authorXPath", "./td[3]"},
          {"chapterXPath", "//div[@id='list']//dd[preceding-sibling::dt[1][contains(.,'正文')]]/a"},
          {"contentXPath", "//*[@id='content']"},
          {"encoding", "auto"}, {"intervalMs", 1500}}),
      BookSource({
          {"kind", "html"},
          {"enabled", true},
          {"encoding", "GB18030"},
          {"intervalMs", 1000},
          {"headers", QJsonObject{{"User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36"}, {"Referer", "https://www.qishuxia.com/"}}},
          {"id", "qishuxia"},
          {"name", "奇书侠"},
          {"baseUrl", "https://www.qishuxia.com"},
          {"searchUrl", "https://www.qishuxia.com/modules/article/search.php"},
          {"searchMethod", "POST"},
          {"searchBody", "searchkey={keyword}"},
          {"resultXPath", "//li[span[@class='s2']/a]"},
          {"titleXPath", ".//span[@class='s2']/a"},
          {"linkXPath", ".//span[@class='s2']/a/@href"},
          {"authorXPath", ".//span[@class='s4']"},
          {"chapterXPath", "//ul[@id='section-list']//li/a"},
          {"contentXPath", "//*[@id='content']"},
          {"builtin", true}}),
      BookSource({
          {"kind", "html"},
          {"enabled", true},
          {"encoding", "UTF-8"},
          {"intervalMs", 1000},
          {"headers", QJsonObject{{"User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36"}, {"Referer", "http://www.100txt.net/"}}},
          {"id", "shucw"},
          {"name", "书橱网"},
          {"baseUrl", "http://www.100txt.net"},
          {"searchUrl", "http://www.100txt.net/search34.html?searchkey={keyword}"},
          {"searchMethod", "GET"},
          {"resultXPath", "//article[@id='main']//dl[dt/a]"},
          {"titleXPath", ".//dt/a"},
          {"linkXPath", ".//dt/a/@href"},
          {"authorXPath", ".//div[@class='btm']/a"},
          {"tocUrlXPath", "//a[contains(@href,'/indexlist/')]/@href"},
          {"chapterXPath", "//div[@id='content_1']//a[@rel='chapter']"},
          {"tocNextXPath", "//a[normalize-space(.)='下一页' and starts-with(@href,'/indexlist/')]/@href"},
          {"contentXPath", "//*[@id='booktxt']"},
          {"contentNextXPath", "//a[@rel='next' and normalize-space(.)='下一页']/@href"},
          {"removeXPath", "//*[@id='booktxt']/p[contains(.,'一秒记住') or contains(.,'请收藏')] | //*[@id='booktxt']/p[contains(.,'本章未完，点击下一页')]"},
          {"builtin", true}}),
      BookSource({
          {"kind", "html"},
          {"enabled", true},
          {"encoding", "UTF-8"},
          {"intervalMs", 1000},
          {"headers", QJsonObject{{"User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36"}, {"Referer", "http://www.bookben5.org/"}}},
          {"id", "bookben"},
          {"name", "书本网"},
          {"baseUrl", "http://www.bookben5.org"},
          {"searchUrl", "http://www.bookben5.org/search34.html"},
          {"searchMethod", "POST"},
          {"searchBody", "searchkey={keyword}"},
          {"resultXPath", "//li[@class='searchresult']"},
          {"titleXPath", ".//h3"},
          {"linkXPath", ".//h3/parent::a/@href"},
          {"authorXPath", "normalize-space(./div[2]/p[1]/text())"},
          {"chapterXPath", "//ul[@id='ul_all_chapters']//a"},
          {"contentXPath", "//*[@id='article']"},
          {"contentNextXPath", "//a[contains(normalize-space(.),'下一页')]/@href"},
          {"removeXPath", "//*[@id='article']/p[contains(.,'一秒记住')]"},
          {"builtin", true}}),
      BookSource({
          {"kind", "html"},
          {"enabled", true},
          {"encoding", "UTF-8"},
          {"intervalMs", 1000},
          {"headers", QJsonObject{{"User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36"}, {"Referer", "http://www.jjbook.cc/"}}},
          {"id", "jjbook"},
          {"name", "久久小说"},
          {"baseUrl", "http://www.jjbook.cc"},
          {"searchUrl", "http://www.jjbook.cc/search/"},
          {"searchMethod", "POST"},
          {"searchBody", "searchkey={keyword}"},
          {"resultXPath", "//tr[td[2]//a]"},
          {"titleXPath", "./td[2]//a"},
          {"linkXPath", "./td[2]//a/@href"},
          {"authorXPath", "./td[3]//a"},
          {"chapterXPath", "//div[@class='panel-heading' and contains(.,'小说全部章节')]/following-sibling::div[@class='panel-body'][1]//li/a"},
          {"contentXPath", "//div[contains(concat(' ',normalize-space(@class),' '),' content-body ')]"},
          {"contentNextXPath", "//a[@id='next_url' and contains(.,'下一页')]/@href"},
          {"builtin", true}}),
      mirror("biquhua"), mirror("biquche"),
      BookSource({
          {"id", "biquge365"}, {"builtin", true}, {"kind", "html"}, {"name", QString::fromUtf8("笔趣阁365")},
          {"baseUrl", "https://www.biquge365.net"}, {"enabled", true},
          {"searchUrl", "https://www.biquge365.net/s.php"},
          {"searchMethod", "POST"}, {"searchBody", "type=articlename&s={keyword}"},
          {"resultXPath", "//li[span[@class='name']/a]"},
          {"titleXPath", ".//span[@class='name']/a"},
          {"linkXPath", ".//span[@class='name']/a/@href"},
          {"authorXPath", ".//span[@class='zuo']/a"},
          {"tocUrlXPath", "//a[contains(@href,'/newbook/')]/@href"},
          {"chapterXPath", "//ul[@class='info']//li/a[contains(@href,'/chapter/')]"},
          {"contentXPath", "//*[@id='txt']"},
          {"removeXPath", "//*[@id='txt']/p[contains(.,'一秒记住')]"},
          {"encoding", "UTF-8"}, {"intervalMs", 1500}})};
}
bool BookSource::isPreset(const QString &id) {
  for (const auto &source : presets())
    if (source.id() == id)
      return true;
  return false;
}
BookSource BookSource::copy(const QString &id, const QString &name) const {
  auto j = m_json;
  j.remove("builtin");
  j["id"] = id;
  j["name"] = name;
  return BookSource(j);
}
} // namespace reader
