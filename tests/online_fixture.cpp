#include <QCoreApplication>
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
    if (op == "chapter" && j.value("url").toString().contains("/slow/"))
      QThread::msleep(100);
    QJsonObject result;
    if (op == "hello")
      result = {{"protocol", 1}, {"version", "0.1.0"}};
    else
      result = {{"title", "Chapter"}, {"url", j["url"]}, {"text", "Body"}};
    if (j.value("url").toString().contains("/redirect"))
      result["url"] = "https://example.org/final";
    QJsonObject out{{"protocol", 1},
                    {"id", j["id"]},
                    {"session", j["session"]},
                    {"ok", true},
                    {"result", result}};
    auto bytes = QJsonDocument(out).toJson(QJsonDocument::Compact);
    std::cout << bytes.constData() << std::endl;
  }
  return 0;
}
