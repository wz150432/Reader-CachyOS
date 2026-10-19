#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <cstdio>
#include <iostream>
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  std::string line;
  while (std::getline(std::cin, line)) {
    auto j = QJsonDocument::fromJson(QByteArray::fromStdString(line)).object();
    QString op = j["op"].toString();
    if (op == "quit")
      return 0;
    if (op == "cancel")
      continue;
    const QString mode = j.value("mode").toString();
    if (mode == "crash")
      return 7;
    if (mode == "timeout")
      continue;
    if (mode == "malformed") {
      std::cout << "{broken}" << std::endl;
      continue;
    }
    if (mode == "oversize") {
      std::cout << std::string(16 * 1024 * 1024 + 1, 'x') << std::endl;
      continue;
    }
    if ((op == "chapter" || op == "toc") &&
        j.value("url").toString().contains("/slow/"))
      QThread::msleep(100);
    if (op == "search" && j["keyword"].toString() == "slow")
      QThread::msleep(250);
    QJsonObject result;
    if (op == "hello") {
      if (qEnvironmentVariableIsSet("READER_FIXTURE_HANG_HELLO"))
        continue;
      const auto marker = qEnvironmentVariable("READER_FIXTURE_HELLO_MARKER");
      if (!marker.isEmpty()) {
        QFile f(marker);
        if (f.open(QIODevice::WriteOnly))
          f.write("started");
        f.close();
        QThread::msleep(150);
      }
      result = {{"protocol", 1}, {"version", "0.1.0"}};
    } else if (op == "search")
      result = {
          {"books", QJsonArray{QJsonObject{{"title", "Fixture Book"},
                                           {"url", "https://example.org/book"},
                                           {"author", "Writer"}}}},
          {"hasMore", j["page"].toInt() == 1}};
    else if (op == "toc")
      result = {
          {"title", "Fixture Book"},
          {"url", j["url"]},
          {"chapters",
           QJsonArray{QJsonObject{{"title", "One"},
                                  {"url", "https://example.org/chapter-1"}},
                      QJsonObject{{"title", "Two"},
                                  {"url", "https://example.org/chapter-2"}}}}};
    else
      result = {{"title", "Chapter"}, {"url", j["url"]}, {"text", "Body"}};
    if (op == "search") {
      auto keyword = j["keyword"].toString();
      QString path = keyword == "mixed" ? "mixed"
                   : keyword == "versions" ? "versions"
                   : keyword == "lost" ? "lost"
                   : keyword == "slow-book" ? "slow/book-b" : "book";
      result["books"] = QJsonArray{QJsonObject{{"title", "Fixture Book"},
          {"url", "https://example.org/" + path}, {"author", "Writer"}}};
      if (keyword == "layout")
        result["books"] = QJsonArray{
            QJsonObject{{"title", "斗破苍穹之雷霆震碎"}, {"author", "一贫如洗羊羊"}, {"url", "https://example.org/book"}},
            QJsonObject{{"title", "很长的书名用于检查缩小窗口后省略号是否正常显示"}, {"author", "名字也很长的小说作者"}, {"url", "https://example.org/long"}},
            QJsonObject{{"title", "没有作者信息的作品"}, {"url", "https://example.org/anonymous"}}};
    }
    if (op == "search" && j["keyword"] == "federated") {
      result["books"] = QJsonArray{QJsonObject{{"title", "Same Book"},
          {"url", "https://example.org/federated"}, {"author", "Same Writer"}}};
    }
    if (op == "toc" && j["url"].toString().contains("/federated"))
      result["title"] = j["source"].toObject()["id"].toString();
    if (op == "toc" && j["url"].toString().contains("/mixed"))
      result["choices"] = QJsonArray{QJsonObject{{"title", "Supplement"}, {"url", "https://example.org/supplement"}}};
    if (op == "toc" && j["url"].toString().contains("/pending-refresh")) {
      QJsonArray chapters;
      for (int index : {0, 5, 1, 2, 3, 4})
        chapters.append(QJsonObject{{"title", QString::number(index)},
            {"url", index == 5 ? "https://example.org/slow/chapter-5"
                                : QString("https://example.org/chapter-%1").arg(index)}});
      result["chapters"] = chapters;
    }
    if (op == "toc" && j["url"].toString().contains("/versions")) {
      result.remove("chapters");
      result["choices"] = QJsonArray{QJsonObject{
          {"title", "Selected edition"}, {"url", "https://example.org/book"}}};
    }
    if (op == "toc" && j["url"].toString().contains("/lost"))
      result["chapters"] = QJsonArray{QJsonObject{
          {"title", "Two"}, {"url", "https://example.org/chapter-2"}}};
    if (j.value("url").toString().contains("/redirect"))
      result["url"] = "https://example.org/final";
    if (op == "chapter" && j["url"].toString().contains("/long/"))
      result["text"] =
          QString(
              "Paragraph with enough text to paginate through the reader.\n\n")
              .repeated(1500);
    if (j.value("url").toString().contains("/invalid-body"))
      result["text"] = 42;
    QJsonObject out{{"protocol", 1},
                    {"id", j["id"]},
                    {"session", j["session"]},
                    {"ok", true},
                    {"result", result}};
    if (op == "validate" &&
        j["source"].toObject()["id"].toString().startsWith("validate-")) {
      auto mode = j["source"].toObject()["id"].toString();
      out["ok"] = false;
      out.remove("result");
      out["error"] = mode == "validate-safe" ? "Invalid XPath: contentXPath"
                     : mode == "validate-secret"
                         ? "Invalid XPath: contentXPath Cookie=private-secret"
                         : "Invalid XPath: unknownXPath";
    }
    if (j.contains("fixtureError") || j["source"].toObject().contains("fixtureError")) {
      out["ok"] = false;
      out.remove("result");
      out["error"] = j.contains("fixtureError") ? j["fixtureError"] : j["source"].toObject()["fixtureError"];
    }
    auto bytes = QJsonDocument(out).toJson(QJsonDocument::Compact);
    std::cout << bytes.constData() << std::endl;
  }
  return 0;
}
