#include <QtTest>
#include <QTemporaryDir>
#include "core/Cache.h"

using namespace reader;

class TestCache : public QObject
{
    Q_OBJECT
private slots:
    void missingFileIsEmpty();
    void upsertAndRoundtrip();
    void recentFilesOrdering();
    void clearAll();
    void clearRecentKeepsBookmarks();
};

void TestCache::missingFileIsEmpty()
{
    QTemporaryDir dir;
    Cache c(dir.filePath(QStringLiteral("cache.json")));
    c.load();
    QVERIFY(!c.progress(QStringLiteral("/x/a.txt")).has_value());
    QVERIFY(c.recentFiles().isEmpty());
}

void TestCache::upsertAndRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cache.json"));
    {
        Cache c(path);
        c.load();
        c.upsertProgress({QStringLiteral("/books/a.txt"), 2, 5, 1000});
        c.upsertProgress({QStringLiteral("/books/a.txt"), 3, 7, 2000});
        c.save();
    }
    {
        Cache c(path);
        c.load();
        const auto p = c.progress(QStringLiteral("/books/a.txt"));
        QVERIFY(p.has_value());
        QCOMPARE(p->chapterIndex, 3);
        QCOMPARE(p->pageIndex, 7);
        QCOMPARE(p->lastOpened, 2000);
    }
}

void TestCache::recentFilesOrdering()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cache.json"));
    Cache c(path);
    c.load();
    c.upsertProgress({QStringLiteral("/books/a.txt"), 0, 0, 100});
    c.upsertProgress({QStringLiteral("/books/b.txt"), 0, 0, 300});
    c.upsertProgress({QStringLiteral("/books/c.txt"), 0, 0, 200});
    c.save();
    Cache d(path);
    d.load();
    const QStringList recent = d.recentFiles();
    QCOMPARE(recent, QStringList({QStringLiteral("/books/b.txt"),
                                  QStringLiteral("/books/c.txt"),
                                  QStringLiteral("/books/a.txt")}));
}

void TestCache::clearAll()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cache.json"));
    Cache c(path);
    c.load();
    c.upsertProgress({QStringLiteral("/books/a.txt"), 1, 1, 100});
    c.save();
    c.clearAll();
    c.save();
    Cache d(path);
    d.load();
    QVERIFY(!d.progress(QStringLiteral("/books/a.txt")).has_value());
}

void TestCache::clearRecentKeepsBookmarks()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cache.json"));
    Cache c(path);
    c.load();
    c.upsertProgress({QStringLiteral("/books/a.txt"), 1, 1, 100});
    c.addBookmark({QStringLiteral("/books/a.txt"), 0, 0, QStringLiteral("标记"), 50});
    c.clearRecent();
    c.save();
    Cache d(path);
    d.load();
    QVERIFY(!d.progress(QStringLiteral("/books/a.txt")).has_value());
    QCOMPARE(d.bookmarks(QStringLiteral("/books/a.txt")).size(), 1);
}

QTEST_APPLESS_MAIN(TestCache)
#include "tst_cache.moc"
