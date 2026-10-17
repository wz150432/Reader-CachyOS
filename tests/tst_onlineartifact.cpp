// Opt-in artifact and live integration. Network never runs in ordinary CTest.
#include "core/online/OnlineComponentManager.h"
#include "core/online/OnlineReadingController.h"
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace reader;
class ArtifactTest : public QObject {
  Q_OBJECT
private slots:
  void realArtifact() {
    const auto archive = qEnvironmentVariable("READER_ONLINE_ARCHIVE");
    const auto metadataPath = qEnvironmentVariable("READER_ONLINE_MANIFEST");
    if (archive.isEmpty() || metadataPath.isEmpty())
      QSKIP("Set READER_ONLINE_ARCHIVE and READER_ONLINE_MANIFEST for local artifact import");
    QFile file(metadataPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    auto metadata = QJsonDocument::fromJson(file.readAll()).object();
    if (qEnvironmentVariableIsSet("READER_EXPECT_BUNDLED_METADATA"))
      QCOMPARE(OnlineComponentManager::bundledMetadata(), metadata);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    OnlineComponentManager manager(dir.path() + "/component", READER_APP_VERSION, metadata);
    QSignalSpy finished(&manager, &OnlineComponentManager::finished);
    QString error;
    QVERIFY2(manager.importPackage(archive, metadata, &error), qPrintable(error));
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 30000);
    QVERIFY2(finished[0][0].toBool(), qPrintable(finished[0][1].toString()));
    QVERIFY(manager.isEnabled());
    QCOMPARE(manager.downloadAvailable(), !metadata["url"].toString().isEmpty());
    QVERIFY(QFileInfo::exists(QFileInfo(manager.executablePath()).path() + "/lib/libxml2.so.16"));
    QProcess loader;
    loader.start("ldd", {manager.executablePath()});
    QVERIFY(loader.waitForFinished(10000));
    auto resolved = loader.readAllStandardOutput();
    QVERIFY(!resolved.contains("not found"));
    QVERIFY(resolved.contains((QFileInfo(manager.executablePath()).path() + "/lib/libxml2.so.16").toUtf8()));
    if (!qEnvironmentVariableIsSet("READER_ONLINE_LIVE"))
      return;
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(manager.executablePath());
    bridge.setTimeoutMs(90000);
    QSignalSpy replies(&bridge, &OnlineEngineBridge::result);
    QSignalSpy errors(&bridge, &OnlineEngineBridge::error);
    const auto source = BookSource::preset();
    auto call = [&](const QString &op, QJsonObject fields) -> QJsonObject {
      fields["source"] = source.toJson();
      auto id = bridge.request(op, fields, "artifact-live");
      QElapsedTimer deadline;
      deadline.start();
      while (deadline.elapsed() < 90000) {
        QTest::qWait(10);
        for (auto reply : replies)
          if (reply[0].toString() == id)
            return reply[2].toJsonObject();
        if (!errors.isEmpty())
          return {};
      }
      return {};
    };
    for (const QString keyword : {QStringLiteral("红楼梦"), QStringLiteral("三国演义")}) {
      auto books = call("search", {{"keyword", keyword}, {"page", 1}})["books"].toArray();
      QVERIFY2(!books.isEmpty(), qPrintable(keyword + ": search failed"));
      auto chosen = books.first().toObject();
      auto toc = call("toc", {{"url", chosen["url"]}});
      for (int i = 0; i < 3 && toc["chapters"].toArray().isEmpty() && !toc["choices"].toArray().isEmpty(); ++i) {
        chosen = toc["choices"].toArray().first().toObject();
        toc = call("toc", {{"url", chosen["url"]}});
      }
      QVERIFY(toc["chapters"].toArray().size() > 4);
      auto book = OnlineBook::createManifest(dir.path() + "/cache", source,
          chosen["url"].toString(), keyword, toc["chapters"].toArray(), &error);
      QVERIFY2(book, qPrintable(error));
      OnlineReadingController controller(&bridge);
      QSignalSpy started(&controller, &OnlineReadingController::chapterRequestStarted);
      QSignalSpy ready(&controller, &OnlineReadingController::currentReady);
      QSignalSpy failed(&controller, &OnlineReadingController::currentError);
      QSignalSpy background(&controller, &OnlineReadingController::backgroundStatus);
      controller.setBook(book);
      controller.requestChapter(0);
      QTRY_VERIFY_WITH_TIMEOUT(!background.isEmpty() || !failed.isEmpty(), 180000);
      QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.first()[1].toString()));
      QCOMPARE(ready.size(), 1);
      QCOMPARE(started.size(), 4);
      for (int index = 0; index < 4; ++index) {
        QCOMPARE(started[index][0].toInt(), index);
        QVERIFY(book->chapterAvailable(index));
        QVERIFY(book->chapterText(index).size() > 100);
      }
      QVERIFY(!book->chapterAvailable(4));
      QCOMPARE(background.last()[0].toInt(), 3);
      QCOMPARE(background.last()[1].toInt(), 3);
      controller.close();
      bridge.stop();
      QVERIFY(manager.setEnabled(false));
      bridge.setExecutablePath({});
      auto offline = std::make_shared<OnlineBook>();
      QVERIFY2(offline->open(book->filePath(), &error), qPrintable(error));
      OnlineReadingController offlineController(&bridge);
      QSignalSpy offlineReady(&offlineController, &OnlineReadingController::currentReady);
      offlineController.setBook(offline);
      offlineController.requestChapter(0);
      QCOMPARE(offlineReady.size(), 1);
      for (int index = 0; index < 4; ++index)
        QCOMPARE(offline->chapterText(index), book->chapterText(index));
      QVERIFY(!offline->chapterAvailable(4));
      qInfo().noquote() << keyword << toc["chapters"].toArray().size()
          << "chapters; requested 0,1,2,3; chapter4 absent; disabled-helper offline cache verified";
      offlineController.close();
      QVERIFY(manager.setEnabled(true));
      bridge.setExecutablePath(manager.executablePath());
    }
    bridge.stop();
    QVERIFY(manager.uninstall());
  }
};
QTEST_GUILESS_MAIN(ArtifactTest)
#include "tst_onlineartifact.moc"
