#include <QtTest>
#include <QTemporaryDir>
#include "core/Cache.h"

using namespace reader;

class TestBookmarks : public QObject
{
    Q_OBJECT
private slots:
    void addAndRoundtrip();
    void removeBookmark();
    void emptyForUnknownFile();
};

void TestBookmarks::addAndRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cache.json"));
    {
        Cache c(path);
        c.load();
        c.addBookmark({QStringLiteral("/b/a.txt"), 2, 5, QStringLiteral("第二章 标题"), 1000});
        c.addBookmark({QStringLiteral("/b/a.txt"), 1, 0, QStringLiteral("第一章 标题"), 900});
        c.addBookmark({QStringLiteral("/b/other.txt"), 0, 0, QStringLiteral("别书"), 800});
        c.save();
    }
    {
        Cache c(path);
        c.load();
        const QVector<Bookmark> marks = c.bookmarks(QStringLiteral("/b/a.txt"));
        QCOMPARE(marks.size(), 2);
        QCOMPARE(marks.at(0).title, QStringLiteral("第一章 标题"));
        QCOMPARE(marks.at(1).pageIndex, 5);
        QCOMPARE(c.bookmarks(QStringLiteral("/b/other.txt")).size(), 1);
    }
}

void TestBookmarks::removeBookmark()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cache.json"));
    Cache c(path);
    c.load();
    c.addBookmark({QStringLiteral("/b/a.txt"), 0, 1, QStringLiteral("标记"), 100});
    c.removeBookmark(QStringLiteral("/b/a.txt"), 100);
    QVERIFY(c.bookmarks(QStringLiteral("/b/a.txt")).isEmpty());
}

void TestBookmarks::emptyForUnknownFile()
{
    QTemporaryDir dir;
    Cache c(dir.filePath(QStringLiteral("cache.json")));
    c.load();
    QVERIFY(c.bookmarks(QStringLiteral("/nope.txt")).isEmpty());
}

QTEST_APPLESS_MAIN(TestBookmarks)
#include "tst_bookmarks.moc"
