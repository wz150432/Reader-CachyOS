#include "core/online/BookSourceStore.h"
#include "core/online/OnlineBook.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace reader;
class HostTest : public QObject {
  Q_OBJECT
private slots:
  void sourceValidationAndPrivateWrites() {
    QTemporaryDir dir;
    auto preset = BookSource::preset();
    auto json = preset.copy("copy", "Copy").toJson();
    json["enabled"] = "yes";
    QVERIFY(!BookSource(json).validate());
    json["enabled"] = true;
    json["intervalMs"] = 499;
    QVERIFY(!BookSource(json).validate());
    json["intervalMs"] = 500;
    BookSourceStore store(dir.filePath("sources.json"));
    QString error;
    QVERIFY(store.load(&error));
    QVERIFY(!store.upsert(preset, &error));
    QVERIFY(store.upsert(BookSource(json), &error));
    QVERIFY(store.save(&error));
    QCOMPARE(QFile::permissions(dir.filePath("sources.json")) &
                 (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                  QFileDevice::ReadOther | QFileDevice::WriteOther),
             QFileDevice::Permissions());
    BookSourceStore reopened(dir.filePath("sources.json"));
    QVERIFY(reopened.load(&error));
    QCOMPARE(reopened.sources().size(), 2);
  }
  void oversizedSourceSavePreservesExistingStore() {
    QTemporaryDir d;
    BookSourceStore store(d.filePath("sources.json"));
    QVERIFY(store.load());
    QVERIFY(store.save());
    auto j = BookSource::preset().copy("large", "Large").toJson();
    j["cookie"] = QString(9 * 1024 * 1024, 'x');
    QVERIFY(store.upsert(BookSource(j)));
    QVERIFY(!store.save());
    BookSourceStore reopened(d.filePath("sources.json"));
    QVERIFY(reopened.load());
    QCOMPARE(reopened.sources().size(), 1);
  }
  void oversizedWritesPreserveReadableCache() {
    QTemporaryDir d;
    auto source = BookSource::preset();
    QJsonArray chapters{
        QJsonObject{{"title", "One"}, {"url", "https://example.org/1"}}};
    auto b = OnlineBook::createManifest(
        d.path(), source, "https://example.org/book", "Book", chapters);
    QVERIFY(b);
    QVERIFY(b->saveChapter(0, "original"));
    QVERIFY(!b->saveChapter(0, QString(3 * 1024 * 1024, QChar(0))));
    QCOMPARE(b->chapterText(0), QString("original"));
    QVERIFY(!OnlineBook::createManifest(
        d.path(), source, "https://example.org/book",
        QString(9 * 1024 * 1024, 'x'), chapters));
    QVERIFY(Book::create(b->filePath()));
  }
  void cacheIntegrity() {
    QTemporaryDir dir;
    QString error;
    BookSource s(QJsonObject{{"id", "test"},
                             {"kind", "mediawiki"},
                             {"name", "Test"},
                             {"baseUrl", "https://example.org"}});
    auto b = OnlineBook::createManifest(
        dir.path(), s, "https://example.org/book#x", "Book",
        QJsonArray{
            QJsonObject{{"title", "../bad"}, {"url", "https://example.org/1"}}},
        &error);
    QVERIFY2(b, qPrintable(error));
    QVERIFY(!b->chapterAvailable(0));
    QVERIFY(b->saveChapter(0, "body", &error));
    QCOMPARE(b->chapterText(0), QString("body"));
    auto reopened = Book::create(b->filePath(), &error);
    QVERIFY(reopened);
    QCOMPARE(reopened->chapterText(0), QString("body"));
    QFile f(b->chapterFilePath(0));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{\"text\":");
    f.close();
    QVERIFY(!b->chapterAvailable(0));
  }
  void corruptStorePreserved() {
    QTemporaryDir d;
    QString path = d.filePath("sources.json");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("broken");
    f.close();
    BookSourceStore store(path);
    QString e;
    QVERIFY(!store.load(&e));
    QVERIFY(!e.isEmpty());
    QVERIFY(!store.save(&e));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("broken"));
  }
};
QTEST_GUILESS_MAIN(HostTest)
#include "tst_onlinehost.moc"
