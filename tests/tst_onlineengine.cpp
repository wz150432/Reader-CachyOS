#include "core/online/OnlineEngineBridge.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>
class EngineTest : public QObject {
  Q_OBJECT
  QProcess p;
  QTcpServer server;
  QString base;
  QByteArray lastRequest;
  QList<qint64> retryTimes;
  QByteArray retryAfter;
  QJsonObject call(QJsonObject o) {
    o["protocol"] = 1;
    o["id"] = "x";
    o["session"] = "s";
    p.write(QJsonDocument(o).toJson(QJsonDocument::Compact) + "\n");
    QByteArray b;
    QElapsedTimer t;
    t.start();
    while (!b.contains('\n') && t.elapsed() < 15000) {
      QCoreApplication::processEvents();
      b += p.readAllStandardOutput();
      QTest::qWait(2);
    }
    return QJsonDocument::fromJson(b.trimmed()).object();
  }
  static QByteArray fixture(const QString &name) {
    QFile f(QString(ONLINE_FIXTURES) + "/" + name);
    if (!f.open(QIODevice::ReadOnly))
      qFatal("Missing online fixture");
    return f.readAll();
  }
  QJsonObject source() {
    return {{"kind", "html"},
            {"baseUrl", base},
            {"searchUrl", base + "/search"},
            {"resultXPath", "//li"},
            {"titleXPath", ".//a"},
            {"linkXPath", ".//a/@href"},
            {"chapterXPath", "//a[@class='chapter']"},
            {"contentXPath", "//article"},
            {"tocNextXPath", "//a[@rel='next']/@href"},
            {"contentNextXPath", "//a[@rel='next']/@href"},
            {"intervalMs", 500}};
  }
private slots:
  void validateRulesWithoutNetwork() {
    auto valid = source();
    auto good = call({{"op", "validate"}, {"source",valid}});
    QVERIFY(good["ok"].toBool());
    valid["contentNextXPath"]="//*[";
    auto bad=call({{"op","validate"},{"source",valid}});
    QVERIFY(!bad["ok"].toBool());
    QVERIFY(bad["error"].toString().contains("contentNextXPath"));
  }
  void initTestCase() {
    QVERIFY(server.listen(QHostAddress::LocalHost));
    base = "http://127.0.0.1:" + QString::number(server.serverPort());
    connect(&server, &QTcpServer::newConnection, this, [this] {
      while (server.hasPendingConnections()) {
        auto s = server.nextPendingConnection();
        connect(s, &QTcpSocket::readyRead, this, [this, s] {
          auto req = s->readAll();
          lastRequest = req;
          auto path = req.split(' ').value(1);
          QByteArray charset = "utf-8";
          QByteArray body;
          int status = 200;
          if (path.startsWith("/pages/")) {
            int page = path.mid(7).toInt();
            body = "<a class='chapter' href='/chapter/" + QByteArray::number(page) + "'>Chapter</a>";
            if (page < 24)
              body += "<a rel='next' href='/pages/" + QByteArray::number(page + 1) + "'>Next</a>";
          } else if (path == "/retry") {
            retryTimes.append(QDateTime::currentMSecsSinceEpoch());
            s->write("HTTP/1.1 429 Limited\r\nRetry-After: " + retryAfter +
                     "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            s->disconnectFromHost();
            return;
          } else if (path.startsWith("/post-redirect/")) {
            auto code = path.mid(QByteArray("/post-redirect/").size());
            s->write("HTTP/1.1 " + code + " Redirect\r\nLocation: /search\r\n"
                     "Content-Length: 0\r\nConnection: close\r\n\r\n");
            s->disconnectFromHost();
            return;
          } else if (path == "/redirect") {
            s->write("HTTP/1.1 302 Found\r\nLocation: http://localhost:" +
                     QByteArray::number(server.serverPort()) +
                     "/c1\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            s->disconnectFromHost();
            return;
          } else if (path == "/gb") {
            charset = "gb18030";
            body = QByteArray("<article>") + QByteArray::fromHex("bcd7d2d2") +
                   "</article>";
          } else if (path == "/landing") {
            body = "<a id=contents href=/toc>Contents</a>";
          } else if (path.startsWith("/w/api.php")) {
            QJsonObject parsed{{"title", "Work"},
                               {"text",
                                "<div class='mw-parser-output'><p><a "
                                "href='/wiki/Work/Next'>下一回▶</a></p><a "
                                "href='/wiki/Work_(Edition)'>Edition</a><a "
                                "href='/wiki/Work/First'>Chapter 1</a><a "
                                "href='/wiki/Other'>Other</a></div>"}};
            body = QJsonDocument(QJsonObject{{"parse", parsed}}).toJson();
          } else if (path == "/search")
            body = fixture("search.html");
          else if (path == "/toc")
            body = fixture("toc-1.html");
          else if (path == "/toc2")
            body = fixture("toc-2.html");
          else if (path == "/cycle")
            body =
                "<article>text</article><a rel='next' href='/cycle'>Next</a>";
          else if (path == "/challenge")
            body = "<title>Just a moment...</title><div>Enable JavaScript and "
                   "cookies</div>";
          else if (path == "/large")
            body = QByteArray(8 * 1024 * 1024 + 1, 'x');
          else if (path == "/slow")
            return;
          else if (path == "/404") {
            status = 404;
            body = "missing";
          } else
            body = fixture("chapter.html");
          s->write("HTTP/1.1 " + QByteArray::number(status) +
                   " OK\r\nContent-Type: text/html; "
                   "charset=" +
                   charset +
                   "\r\nContent-Length: " + QByteArray::number(body.size()) +
                   "\r\nConnection: close\r\n\r\n" + body);
          s->disconnectFromHost();
        });
      }
    });
    p.start(ENGINE_PATH);
    QVERIFY(p.waitForStarted());
  }
  void handshake() {
    auto r = call({{"op", "hello"}});
    QVERIFY(r["ok"].toBool());
    QCOMPARE(r["result"].toObject()["version"].toString(), READER_ONLINE_VERSION);
  }
  void malformed() { QVERIFY(!call({{"op", "explode"}})["ok"].toBool()); }
  void strictProtocol() {
    auto r = call({{"op", "hello"}});
    QVERIFY(r["ok"].toBool());
    p.write("{\"protocol\":1.5,\"id\":\"fraction\",\"op\":\"hello\"}\n");
    QTRY_VERIFY_WITH_TIMEOUT(p.bytesAvailable() > 0, 3000);
    auto invalid = QJsonDocument::fromJson(p.readAllStandardOutput()).object();
    QVERIFY(!invalid["ok"].toBool());
  }
  void malformedQuit() {
    QProcess engine;
    engine.start(ENGINE_PATH);
    QVERIFY(engine.waitForStarted());
    engine.write("{\"protocol\":1,\"op\":\"quit\"}\n");
    QVERIFY(engine.waitForReadyRead(3000));
    auto r = QJsonDocument::fromJson(engine.readAllStandardOutput()).object();
    QVERIFY(!r["ok"].toBool());
    engine.closeWriteChannel();
    QVERIFY(engine.waitForFinished(3000));
  }
  void redirectCredentials() {
    auto s = source();
    s["cookie"] = "private=value";
    s["headers"] = QJsonObject{{"Authorization", "Bearer private"}};
    auto r = call({{"op", "chapter"}, {"source", s}, {"url", base + "/redirect"}});
    QVERIFY(r["ok"].toBool());
    QVERIFY(r["result"].toObject()["url"].toString().contains("localhost"));
    QVERIFY(!lastRequest.contains("private"));
  }
  void postRedirects_data() {
    QTest::addColumn<int>("status");
    for (int code : {301, 302, 303, 307, 308})
      QTest::newRow(qPrintable(QString::number(code))) << code;
  }
  void postRedirects() {
    QFETCH(int, status);
    auto s = source();
    s["searchUrl"] = base + "/post-redirect/" + QString::number(status);
    s["searchMethod"] = "POST";
    s["searchBody"] = "keyword={keyword}&page={page}";
    auto r = call({{"op", "search"}, {"source", s}, {"keyword", "test"}, {"page", 2}});
    QVERIFY(r["ok"].toBool());
    if (status == 307 || status == 308) {
      QVERIFY2(lastRequest.startsWith("POST /search "), lastRequest.constData());
      QVERIFY(lastRequest.endsWith("keyword=test&page=2"));
    } else {
      QVERIFY(lastRequest.startsWith("GET /search "));
      QVERIFY(!lastRequest.contains("keyword=test"));
    }
  }
  void envelope_data() {
    QTest::addColumn<QString>("op");
    QTest::addColumn<QJsonValue>("session");
    for (const auto &op : {"hello", "chapter", "cancel", "quit"}) {
      QTest::newRow(qPrintable(QString(op) + "-missing")) << QString(op) << QJsonValue(QJsonValue::Undefined);
      QTest::newRow(qPrintable(QString(op) + "-number")) << QString(op) << QJsonValue(7);
      QTest::newRow(qPrintable(QString(op) + "-object")) << QString(op) << QJsonValue(QJsonObject());
    }
  }
  void envelope() {
    QFETCH(QString, op);
    QFETCH(QJsonValue, session);
    QProcess engine;
    engine.start(ENGINE_PATH);
    QVERIFY(engine.waitForStarted());
    QJsonObject request{{"protocol", 1}, {"id", "bad"}, {"op", op},
                        {"source", source()}, {"url", base + "/c1"}, {"target", "pending"}};
    if (!session.isUndefined())
      request["session"] = session;
    lastRequest.clear();
    engine.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n");
    QByteArray reply;
    QElapsedTimer timer;
    timer.start();
    while (!reply.contains('\n') && timer.elapsed() < 3000) {
      QCoreApplication::processEvents();
      reply += engine.readAllStandardOutput();
      QTest::qWait(2);
    }
    auto response = QJsonDocument::fromJson(reply).object();
    bool rejected = response.contains("ok") && !response["ok"].toBool();
    engine.closeWriteChannel();
    engine.waitForFinished(3000);
    QVERIFY2(rejected, reply.constData());
    QVERIFY(lastRequest.isEmpty());
  }
  void bridgeAllowsPacedPaginationBeyondTwentySeconds() {
    reader::OnlineEngineBridge bridge;
    bridge.setExecutablePath(ENGINE_PATH);
    QSignalSpy replies(&bridge, &reader::OnlineEngineBridge::result);
    QSignalSpy errors(&bridge, &reader::OnlineEngineBridge::error);
    auto s = source();
    s["intervalMs"] = 1000;
    QElapsedTimer elapsed;
    elapsed.start();
    bridge.request("toc", {{"source", s}, {"url", base + "/pages/1"}}, "long-toc");
    QTRY_VERIFY_WITH_TIMEOUT(!replies.isEmpty() || !errors.isEmpty(), 28000);
    QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors.first()[2].toString()));
    QCOMPARE(replies.size(), 1);
    QCOMPARE(replies.first()[2].toJsonObject()["chapters"].toArray().size(), 24);
    QVERIFY(elapsed.elapsed() > 20000);
    bridge.setExecutablePath({});
    bridge.request("validate", {}, "teardown");
    QTRY_COMPARE(errors.size(), 1);
  }
  void retryBackoff_data() {
    QTest::addColumn<QByteArray>("header");
    QTest::newRow("increasing") << QByteArray("invalid");
    QTest::newRow("delta-seconds") << QByteArray("2");
    QTest::newRow("http-date") << QByteArray("date");
    QTest::newRow("beyond-bound") << QByteArray("3600");
  }
  void retryBackoff() {
    QFETCH(QByteArray, header);
    retryTimes.clear();
    retryAfter = header;
    qint64 dateDeadline = 0;
    if (header == "date") {
      auto date = QDateTime::currentDateTimeUtc().addSecs(3);
      retryAfter = QLocale::c().toString(date, "ddd, dd MMM yyyy HH:mm:ss 'GMT'").toLatin1();
      dateDeadline = (date.toMSecsSinceEpoch() / 1000) * 1000;
    }
    auto r = call({{"op", "chapter"}, {"source", source()}, {"url", base + "/retry"}});
    QVERIFY(!r["ok"].toBool());
    if (header == "3600") {
      QCOMPARE(r["error"].toString(), QString("Rate limit exceeded"));
      QCOMPARE(retryTimes.size(), 1);
      return;
    }
    QCOMPARE(r["error"].toString(), QString("HTTP 429"));
    QCOMPARE(retryTimes.size(), 3);
    QVERIFY(retryTimes[1] - retryTimes[0] >= (header == "2" ? 2000 : 1000));
    QVERIFY(retryTimes[2] - retryTimes[1] >= 2000);
    if (dateDeadline)
      QVERIFY(retryTimes[1] >= dateDeadline);
  }
  void cancelDuringRetryDelay() {
    retryTimes.clear();
    retryAfter = "2";
    QJsonObject request{{"protocol", 1}, {"id", "retry-cancel"}, {"session", "s"},
                        {"op", "chapter"}, {"source", source()}, {"url", base + "/retry"}};
    p.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n");
    QTRY_COMPARE_WITH_TIMEOUT(retryTimes.size(), 1, 1500);
    auto r = call({{"op", "cancel"}, {"target", "retry-cancel"}});
    QVERIFY(r["ok"].toBool());
    QTest::qWait(2200);
    QCOMPARE(retryTimes.size(), 1);
    QVERIFY(p.readAllStandardOutput().isEmpty());
  }
  void keywordEncoding_data() {
    QTest::addColumn<QString>("method");
    QTest::addColumn<QString>("encoding");
    QTest::addColumn<QByteArray>("escaped");
    for (const auto method : {"GET", "POST"}) {
      QTest::newRow(qPrintable(QString(method) + "-GB18030"))
          << QString(method) << QString("GB18030") << QByteArray("%BA%EC%C2%A5%C3%CE");
      QTest::newRow(qPrintable(QString(method) + "-UTF8"))
          << QString(method) << QString("UTF-8") << QByteArray("%E7%BA%A2%E6%A5%BC%E6%A2%A6");
    }
  }
  void keywordEncoding() {
    QFETCH(QString, method);
    QFETCH(QString, encoding);
    QFETCH(QByteArray, escaped);
    auto s = source();
    s["encoding"] = encoding;
    s["searchMethod"] = method;
    s["searchUrl"] = base + (method == "GET" ? "/search?q={keyword}" : "/search");
    s["searchBody"] = "q={keyword}";
    auto r = call({{"op", "search"}, {"source", s}, {"keyword", "红楼梦"}});
    QVERIFY(r["ok"].toBool());
    QVERIFY2(lastRequest.contains("q=" + escaped), lastRequest.constData());
  }
  void linkForms_data() {
    QTest::addColumn<QString>("xpath");
    QTest::addColumn<bool>("missing");
    QTest::newRow("attribute") << QString(".//a/@href") << false;
    QTest::newRow("element") << QString(".//a") << false;
    QTest::newRow("scalar") << QString("string(.//a/@href)") << false;
    QTest::newRow("missing") << QString(".//missing/@href") << true;
    QTest::newRow("empty-scalar") << QString("string(.//missing/@href)") << true;
  }
  void linkForms() {
    QFETCH(QString, xpath);
    QFETCH(bool, missing);
    auto s = source();
    s["linkXPath"] = xpath;
    if (xpath.startsWith("string(") && !missing)
      s["titleXPath"] = "string(.//a)";
    auto r = call({{"op", "search"}, {"source", s}, {"keyword", "x"}});
    QVERIFY(r["ok"].toBool());
    auto books = r["result"].toObject()["books"].toArray();
    QCOMPARE(books.size(), missing ? 0 : 1);
    if (!missing)
      QCOMPARE(books.first().toObject()["url"].toString(), base + "/book/1");
  }
  void paginationLinkForms_data() {
    QTest::addColumn<QString>("next");
    QTest::addColumn<QString>("landing");
    QTest::newRow("element") << QString("//a[@rel='next']") << QString("//a[@id='contents']");
    QTest::newRow("scalar") << QString("string(//a[@rel='next']/@href)") << QString("string(//a[@id='contents']/@href)");
  }
  void paginationLinkForms() {
    QFETCH(QString, next);
    QFETCH(QString, landing);
    auto s = source();
    s["tocNextXPath"] = next;
    s["tocUrlXPath"] = landing;
    auto r = call({{"op", "toc"}, {"source", s}, {"url", base + "/landing"}});
    QVERIFY(r["ok"].toBool());
    QCOMPARE(r["result"].toObject()["chapters"].toArray().size(), 2);
  }
  void search() {
    auto r = call({{"op", "search"}, {"source", source()}, {"keyword", "x"}});
    QVERIFY2(r["ok"].toBool(), qPrintable(r["error"].toString()));
    QCOMPARE(r["result"]
                 .toObject()["books"]
                 .toArray()[0]
                 .toObject()["url"]
                 .toString(),
             base + "/book/1");
  }
  void toc() {
    auto r =
        call({{"op", "toc"}, {"source", source()}, {"url", base + "/toc"}});
    QVERIFY(r["ok"].toBool());
    QCOMPARE(r["result"].toObject()["chapters"].toArray().size(), 2);
  }
  void chapter() {
    auto r =
        call({{"op", "chapter"}, {"source", source()}, {"url", base + "/c1"}});
    QVERIFY(r["ok"].toBool());
    auto text = r["result"].toObject()["text"].toString();
    QVERIFY(!text.contains("bad()"));
    QVERIFY(text.contains(QString::fromUtf8("甲&乙\n\n丙\n丁")));
  }
  void failures() {
    for (auto path : {"/cycle", "/challenge", "/large", "/404"}) {
      auto r =
          call({{"op", "chapter"}, {"source", source()}, {"url", base + path}});
      QVERIFY2(!r["ok"].toBool(), path);
      if (QString(path) == "/large")
        QVERIFY(r["error"].toString().contains("limit"));
      if (QString(path) == "/404")
        QVERIFY(r["error"].toString().contains("404"));
    }
    auto s = source();
    s["contentXPath"] = "//*[";
    QVERIFY(
        !call({{"op", "chapter"}, {"source", s}, {"url", base + "/c1"}})["ok"]
             .toBool());
  }
  void httpCharset() {
    auto r =
        call({{"op", "chapter"}, {"source", source()}, {"url", base + "/gb"}});
    QVERIFY(r["ok"].toBool());
    QCOMPARE(r["result"].toObject()["text"].toString(),
             QString::fromUtf8("甲乙"));
  }
  void tocIndirection() {
    auto s = source();
    s["tocUrlXPath"] = "//a[@id='contents']/@href";
    auto r = call({{"op", "toc"}, {"source", s}, {"url", base + "/landing"}});
    QVERIFY(r["ok"].toBool());
    QCOMPARE(r["result"].toObject()["chapters"].toArray().size(), 2);
  }
  void wikiEditions() {
    QJsonObject s{
        {"kind", "mediawiki"}, {"baseUrl", base}, {"intervalMs", 500}};
    auto r = call({{"op", "toc"}, {"source", s}, {"url", base + "/wiki/Work"}});
    QVERIFY(r["ok"].toBool());
    QCOMPARE(r["result"].toObject()["choices"].toArray().size(), 1);
    QCOMPARE(r["result"].toObject()["chapters"].toArray().size(), 1);
    QVERIFY(lastRequest.contains("action=parse"));
    QVERIFY(lastRequest.contains("page=Work"));
  }
  void wikiNavigation() {
    QJsonObject s{
        {"kind", "mediawiki"}, {"baseUrl", base}, {"intervalMs", 500}};
    auto r =
        call({{"op", "chapter"}, {"source", s}, {"url", base + "/wiki/Work"}});
    QVERIFY(r["ok"].toBool());
    QVERIFY(!r["result"].toObject()["text"].toString().contains(
        QString::fromUtf8("下一回")));
  }
  void cancellation() {
    QJsonObject o{{"protocol", 1},
                  {"id", "pending"},
                  {"session", "s"},
                  {"op", "chapter"},
                  {"source", source()},
                  {"url", base + "/slow"}};
    p.write(QJsonDocument(o).toJson(QJsonDocument::Compact) + "\n");
    auto r = call({{"op", "cancel"}, {"target", "pending"}});
    QVERIFY(r["ok"].toBool());
    QTest::qWait(100);
    QVERIFY(!p.readAllStandardOutput().contains("\"ok\":true"));
  }
  void cleanupTestCase() {
    p.closeWriteChannel();
    QVERIFY(p.waitForFinished(3000));
  }
};
QTEST_GUILESS_MAIN(EngineTest)
#include "tst_onlineengine.moc"
