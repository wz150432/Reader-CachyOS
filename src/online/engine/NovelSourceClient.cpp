#include "NovelSourceClient.h"
#include "HtmlRuleParser.h"
#include <QDateTime>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QTimer>
#include <QUrlQuery>
#include <QtConcurrent>
#include <algorithm>
#include <iconv.h>
namespace {
QByteArray escapedKeyword(const QString &keyword, const QString &encoding) {
  auto bytes = keyword.toUtf8();
  if (encoding == "GB18030") {
    // GB18030 represents every Unicode scalar; Linux iconv is part of libc.
    auto converter = iconv_open("GB18030", "UTF-8");
    if (converter == (iconv_t)-1)
      return {};
    QByteArray converted(bytes.size() * 4 + 16, '\0');
    char *input = bytes.data(), *output = converted.data();
    size_t inputLeft = bytes.size(), outputLeft = converted.size();
    auto result = iconv(converter, &input, &inputLeft, &output, &outputLeft);
    iconv_close(converter);
    if (result == size_t(-1))
      return {};
    converted.resize(converted.size() - outputLeft);
    bytes = converted;
  }
  return bytes.toPercentEncoding();
}
} // namespace
struct NovelSourceClient::Job {
  QJsonObject request, source, result;
  QString id, op, key;
  QSet<QString> visited, chapters;
  QPointer<QNetworkReply> reply;
  bool cancelled = false;
  int pages = 0;
};
NovelSourceClient::NovelSourceClient(QObject *p) : QObject(p) {}
void NovelSourceClient::cancel(const QString &id) {
  auto j = jobs.take(id);
  if (j) {
    j->cancelled = true;
    if (j->reply)
      j->reply->abort();
  }
}
void NovelSourceClient::finish(std::shared_ptr<Job> j, QString error) {
  if (j->cancelled)
    return;
  jobs.remove(j->id);
  j->cancelled = true;
  QJsonObject r{{"protocol", 1},
                {"id", j->id},
                {"session", j->request["session"].toString()},
                {"ok", error.isEmpty()}};
  if (error.isEmpty())
    r["result"] = j->result;
  else
    r["error"] = error;
  emit completed(r);
}
bool NovelSourceClient::validEnvelope(const QJsonObject &r) {
  return r["protocol"].isDouble() && r["protocol"].toDouble() == 1 &&
         r["id"].isString() && !r["id"].toString().isEmpty() &&
         r["session"].isString();
}
void NovelSourceClient::submit(QJsonObject r) {
  auto j = std::make_shared<Job>();
  j->request = r;
  j->source = r["source"].toObject();
  j->id = r["id"].toString();
  j->op = r["op"].toString();
  if (!validEnvelope(r)) {
    // An invalid envelope must not remove an existing job with the same ID.
    j->id = "";
    finish(j, "Malformed protocol");
    return;
  }
  if (jobs.contains(j->id)) {
    j->id = "";
    finish(j, "Duplicate request ID");
    return;
  }
  if (j->op == "hello") {
    j->result = {{"protocol", 1}, {"version", READER_ONLINE_VERSION}};
    finish(j);
    return;
  }
  if (j->op == "validate") {
    auto error=HtmlRuleParser::validateRules(j->source);
    j->result={{"valid",error.isEmpty()}};
    finish(j,error);
    return;
  }
  if (j->op == "cancel") {
    cancel(r["target"].toString());
    finish(j);
    return;
  }
  if (!QStringList{"search", "toc", "chapter"}.contains(j->op)) {
    finish(j, "Unknown operation");
    return;
  }
  if (j->source["enabled"].isBool() && !j->source["enabled"].toBool()) {
    finish(j, "Source disabled");
    return;
  }
  if (!QStringList{"html", "mediawiki"}.contains(
          j->source["kind"].toString())) {
    finish(j, "Unknown source kind");
    return;
  }
  jobs[j->id] = j;
  j->key = j->source["id"].toString() + j->source["baseUrl"].toString();
  QUrl u(r["url"].toString());
  if (j->source["kind"] == "mediawiki") {
    u = QUrl(j->source["baseUrl"].toString() + "/w/api.php");
    QUrlQuery q;
    q.addQueryItem("format", "json");
    q.addQueryItem("formatversion", "2");
    if (j->op == "search") {
      q.addQueryItem("action", "query");
      q.addQueryItem("list", "search");
      q.addQueryItem("srsearch", r["keyword"].toString());
      q.addQueryItem("srnamespace", "0");
      q.addQueryItem("sroffset",
                     QString::number(qMax(0, r["page"].toInt(1) - 1) * 10));
    } else {
      q.addQueryItem("action", "parse");
      q.addQueryItem("prop", "text");
      QString page = QUrl(r["url"].toString()).path();
      if (page.startsWith("/wiki/"))
        page = page.mid(6);
      else if (page.startsWith('/'))
        page = page.mid(1);
      q.addQueryItem("page", page);
    }
    u.setQuery(q);
  } else if (j->op == "search") {
    QString s = j->source["searchUrl"].toString();
    s.replace("{keyword}", QString::fromLatin1(escapedKeyword(
                               r["keyword"].toString(), j->source["encoding"].toString())));
    s.replace("{page}", QString::number(r["page"].toInt(1)));
    u = QUrl(s);
  }
  fetch(j, u, 0, 0, j->op == "search" && j->source["kind"] == "html" &&
                          j->source["searchMethod"] == "POST");
}
void NovelSourceClient::fetch(std::shared_ptr<Job> j, QUrl u, int redirects,
                              int retries, bool post) {
  if (j->cancelled)
    return;
  if (!u.isValid() || !QStringList{"https", "http"}.contains(u.scheme()) ||
      !u.userInfo().isEmpty()) {
    finish(j, "Invalid URL");
    return;
  }
  if (!redirects && !retries) {
    if (j->visited.contains(u.toString())) {
      finish(j, "Pagination cycle");
      return;
    }
    if (++j->pages > (j->op == "toc" ? 100 : 20)) {
      finish(j, "Pagination limit");
      return;
    }
    j->visited.insert(u.toString());
  }
  if (redirects > 5) {
    finish(j, "Redirect limit");
    return;
  }
  int interval = qMax(500, j->source["intervalMs"].toInt(1000));
  qint64 now = QDateTime::currentMSecsSinceEpoch(),
         when = qMax(now, last.value(j->key));
  last[j->key] = when + interval;
  QTimer::singleShot(int(when - now), this, [this, j, u, redirects, retries, post] {
    if (j->cancelled)
      return;
    QNetworkRequest req(u);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::ManualRedirectPolicy);
    req.setAttribute(QNetworkRequest::CookieLoadControlAttribute,
                     QNetworkRequest::Manual);
    req.setAttribute(QNetworkRequest::CookieSaveControlAttribute,
                     QNetworkRequest::Manual);
    req.setTransferTimeout(20000);
    req.setRawHeader(
        "User-Agent",
        "ReaderCachyOS/0.1 (https://github.com/wz150432/Reader-CachyOS)");
    QUrl base(j->source["baseUrl"].toString());
    bool same = u.scheme() == base.scheme() && u.host() == base.host() &&
                u.port() == base.port();
    if (same) {
      auto h = j->source["headers"].toObject();
      for (auto i = h.begin(); i != h.end(); ++i)
        if (!QStringList{"host", "content-length", "connection"}.contains(
                i.key().toLower()))
          req.setRawHeader(i.key().toLatin1(), i.value().toString().toUtf8());
      if (!j->source["cookie"].toString().isEmpty())
        req.setRawHeader("Cookie", j->source["cookie"].toString().toUtf8());
    }
    QNetworkReply *r;
    if (post) {
      QString body = j->source["searchBody"].toString();
      body.replace("{keyword}", QString::fromLatin1(escapedKeyword(
                                    j->request["keyword"].toString(), j->source["encoding"].toString())));
      body.replace("{page}", QString::number(j->request["page"].toInt(1)));
      req.setHeader(QNetworkRequest::ContentTypeHeader,
                    "application/x-www-form-urlencoded");
      r = network.post(req, body.toUtf8());
    } else
      r = network.get(req);
    j->reply = r;
    auto data = std::make_shared<QByteArray>();
    auto tooBig = std::make_shared<bool>(false);
    connect(r, &QNetworkReply::readyRead, this, [r, data, tooBig] {
      *data += r->readAll();
      if (data->size() > 8 * 1024 * 1024) {
        *tooBig = true;
        r->abort();
      }
    });
    auto deadline = new QTimer(r);
    deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, r, [r] {
      r->setProperty("deadline", true);
      r->abort();
    });
    deadline->start(20000);
    connect(
        r, &QNetworkReply::finished, this,
        [this, j, r, data, tooBig, u, redirects, retries, post] {
          *data += r->readAll();
          r->deleteLater();
          if (j->cancelled)
            return;
          if (*tooBig || data->size() > 8 * 1024 * 1024) {
            finish(j, "Response limit exceeded");
            return;
          }
          int status =
              r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
          if (status >= 300 && status < 400) {
            auto next = u.resolved(
                r->attribute(QNetworkRequest::RedirectionTargetAttribute)
                    .toUrl());
            if (u.scheme() == "https" && next.scheme() != "https") {
              finish(j, "Unsafe redirect");
              return;
            }
            fetch(j, next, redirects + 1, retries,
                  post && status != 301 && status != 302 && status != 303);
            return;
          }
          if (status == 429 || status == 502 || status == 503 ||
              status == 504 ||
              (!status && r->error() != QNetworkReply::NoError &&
               !r->property("deadline").toBool())) {
            if (retries < 2) {
              constexpr qint64 maxRetryDelayMs = 30000;
              qint64 delay = 1000LL << retries;
              const auto header = QString::fromLatin1(r->rawHeader("Retry-After")).trimmed();
              if (QRegularExpression("^[0-9]+$").match(header).hasMatch()) {
                bool valid = false;
                const auto seconds = header.toLongLong(&valid);
                delay = !valid || seconds > maxRetryDelayMs / 1000
                            ? maxRetryDelayMs + 1 : qMax(delay, seconds * 1000);
              } else {
                // Qt's RFC2822 parser requires a numeric zone; HTTP uses GMT.
                const auto normalized = header.endsWith(" GMT")
                                            ? header.chopped(3) + "+0000" : header;
                const auto date = QDateTime::fromString(normalized, Qt::RFC2822Date);
                if (date.isValid())
                  delay = qMax(delay, QDateTime::currentDateTimeUtc().msecsTo(date));
              }
              // Never retry before the server's requested time. Long waits are
              // surfaced so the user can retry later instead of tying up a job.
              if (delay > maxRetryDelayMs) {
                finish(j, "Rate limit exceeded");
                return;
              }
              QTimer::singleShot(int(delay), Qt::PreciseTimer, this,
                                 [this, j, u, redirects, retries, post] {
                if (!j->cancelled)
                  fetch(j, u, redirects, retries + 1, post);
              });
              return;
            }
          }
          if (status >= 400) {
            finish(j, "HTTP " + QString::number(status));
            return;
          }
          if (r->error() != QNetworkReply::NoError) {
            finish(j, r->property("deadline").toBool() ? "Request timeout"
                                                       : "Network failure");
            return;
          }
          QString charset;
          auto type = QString::fromLatin1(r->rawHeader("Content-Type"));
          auto match =
              QRegularExpression("charset\\s*=\\s*[\"']?([A-Za-z0-9_-]+)",
                                 QRegularExpression::CaseInsensitiveOption)
                  .match(type);
          if (match.hasMatch())
            charset = match.captured(1);
          parse(j, *data, u, charset);
        });
  });
}
void NovelSourceClient::parse(std::shared_ptr<Job> j, QByteArray body, QUrl url,
                              QString charset) {
  QString wikiTitle;
  if (j->source["kind"] == "mediawiki") {
    auto doc = QJsonDocument::fromJson(body);
    auto root = doc.object();
    if (root.contains("error") || !doc.isObject()) {
      finish(j, "MediaWiki API error");
      return;
    }
    if (j->op == "search") {
      QJsonArray books;
      for (auto v : root["query"].toObject()["search"].toArray()) {
        auto o = v.toObject();
        books.append(
            QJsonObject{{"title", o["title"]},
                        {"url", j->source["baseUrl"].toString() + "/wiki/" +
                                    QString::fromLatin1(QUrl::toPercentEncoding(
                                        o["title"].toString(), "/"))},
                        {"author", ""}});
      }
      j->result = {{"books", books}, {"hasMore", root.contains("continue")}};
      finish(j);
      return;
    }
    wikiTitle = root["parse"].toObject()["title"].toString();
    body = root["parse"].toObject()["text"].toString().toUtf8();
    url = QUrl(j->request["url"].toString());
  }
  auto source = j->source;
  if ((source["encoding"].toString().isEmpty() ||
       source["encoding"] == "auto") &&
      !charset.isEmpty())
    source["encoding"] = charset;
  bool wiki = source["kind"] == "mediawiki";
  if (wiki) {
    source["contentXPath"] =
        "//div[contains(concat(' ',normalize-space(@class),' '),' "
        "mw-parser-output ')]";
    source["chapterXPath"] =
        "//a[starts-with(@href,'/wiki/') and not(contains(@href,':'))]";
    source["removeXPath"] =
        "//*[contains(@class,'noprint') or contains(@class,'ws-noexport') or "
        "contains(@class,'navbox') or contains(@class,'mw-editsection')] | "
        "//a[translate(normalize-space(.),' ◀▶←→‹›','')='下一回' or translate(normalize-space(.),' ◀▶←→‹›','')='上一回' or "
        "translate(normalize-space(.),' ◀▶←→‹›','')='下一章' or translate(normalize-space(.),' ◀▶←→‹›','')='上一章' or "
        "translate(normalize-space(.),' ◀▶←→‹›','')='Next' or translate(normalize-space(.),' ◀▶←→‹›','')='Previous']";
    source["encoding"] = "UTF-8";
  }
  auto watcher = new QFutureWatcher<ParsedPage>(this);
  connect(watcher, &QFutureWatcher<ParsedPage>::finished, this,
          [this, j, watcher, url, wiki, wikiTitle] {
            auto page = watcher->result();
            watcher->deleteLater();
            if (j->cancelled)
              return;
            if (!page.error.isEmpty()) {
              finish(j, page.error);
              return;
            }
            if (wiki)
              page.result["title"] = wikiTitle;
            if (j->result.isEmpty())
              j->result = page.result;
            else if (j->op == "chapter")
              j->result["text"] = j->result["text"].toString() + "\n\n" +
                                  page.result["text"].toString();
            if (j->op == "toc") {
              auto a = j->result["chapters"].toArray();
              if (j->pages == 1)
                a = QJsonArray();
              for (auto v : page.result["chapters"].toArray()) {
                QString link = v.toObject()["url"].toString();
                if (wiki) {
                  QString path = QUrl(link).path().replace('_', ' ');
                  QString parent = url.path().replace('_', ' ');
                  if (!path.startsWith(parent + "/") &&
                      !path.startsWith(parent + " (") &&
                      !path.startsWith(parent + "（"))
                    continue;
                }
                if (!j->chapters.contains(link)) {
                  j->chapters.insert(link);
                  a.append(v);
                  if (a.size() > 20000) {
                    finish(j, "Chapter limit exceeded");
                    return;
                  }
                }
              }
              j->result["chapters"] = a;
              if (wiki) {
                QJsonArray chapters, choices;
                for (auto v : a) {
                  auto o = v.toObject();
                  QString label = o["title"].toString();
                  if (label.contains(QRegularExpression(
                          "第.{1,12}[回章节卷]|Chapter|/第",
                          QRegularExpression::CaseInsensitiveOption)))
                    chapters.append(o);
                  else
                    choices.append(o);
                }
                j->result["chapters"] = chapters;
                j->result["choices"] = choices;
              }
            }
            if (!page.next.isEmpty()) {
              fetch(j, QUrl(page.next));
              return;
            }
            if (j->op == "toc" && j->source["reverseToc"].toBool()) {
              auto a = j->result["chapters"].toArray();
              QJsonArray b;
              for (int i = a.size() - 1; i >= 0; i--)
                b.append(a[i]);
              j->result["chapters"] = b;
            }
            finish(j);
          });
  watcher->setFuture(QtConcurrent::run([body, url, source, j] {
    return HtmlRuleParser::parse(body, url, source, j->op);
  }));
}
