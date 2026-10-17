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
          if (path.startsWith("/post-redirect/")) {
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
