#include "NovelSourceClient.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QSocketNotifier>
#include <fcntl.h>
#include <unistd.h>
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  NovelSourceClient client;
  QFile output;
  if (!output.open(stdout, QIODevice::WriteOnly))
    return 1;
  auto send = [&](QJsonObject r) {
    auto b = QJsonDocument(r).toJson(QJsonDocument::Compact);
    if (b.size() > 16 * 1024 * 1024) {
      r.remove("result");
      r["ok"] = false;
      r["error"] = "Message limit exceeded";
      b = QJsonDocument(r).toJson(QJsonDocument::Compact);
    }
    output.write(b + '\n');
    output.flush();
  };
  QObject::connect(&client, &NovelSourceClient::completed, &app, send);
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
  QSocketNotifier notifier(STDIN_FILENO, QSocketNotifier::Read);
  QByteArray buffer;
  QObject::connect(&notifier, &QSocketNotifier::activated, &app, [&] {
    char bytes[65536];
    ssize_t n;
    while ((n = read(STDIN_FILENO, bytes, sizeof bytes)) > 0) {
      buffer.append(bytes, n);
      if (buffer.size() > 16 * 1024 * 1024 && !buffer.contains('\n')) {
        app.exit(2);
        return;
      }
      int pos;
      while ((pos = buffer.indexOf('\n')) >= 0) {
        auto line = buffer.left(pos);
        buffer.remove(0, pos + 1);
        QJsonParseError error;
        auto d = QJsonDocument::fromJson(line, &error);
        if (line.size() > 16 * 1024 * 1024 ||
            error.error != QJsonParseError::NoError || !d.isObject()) {
          send({{"protocol", 1},
                {"id", ""},
                {"session", ""},
                {"ok", false},
                {"error", "Malformed protocol"}});
          continue;
        }
        auto r = d.object();
        if (r["op"] == "quit" && NovelSourceClient::validEnvelope(r)) {
          send({{"protocol", 1},
                {"id", r["id"]},
                {"session", r["session"]},
                {"ok", true},
                {"result", QJsonObject()}});
          app.quit();
          return;
        }
        client.submit(r);
      }
    }
    if (n == 0)
      app.quit();
  });
  return app.exec();
}
