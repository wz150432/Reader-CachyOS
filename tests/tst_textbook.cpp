#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/Book.h"
#include "core/TextBook.h"

using namespace reader;

class TestTextBook : public QObject
{
    Q_OBJECT
private slots:
    void opensUtf8();
    void opensGb18030();
    void chapterTextSlices();
    void noChaptersYieldsWholeText();
    void factoryTxtOnly();
};

static QString writeTemp(const QTemporaryDir &dir, const QString &name, const QByteArray &bytes)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(bytes);
    return path;
}

void TestTextBook::opensUtf8()
{
    QTemporaryDir dir;
    const QString path = writeTemp(dir, "a.txt",
        QStringLiteral("第一章 相遇\n正文\n第二章 离别\n正文").toUtf8());
    TextBook book;
    QString err;
    QVERIFY(book.open(path, &err));
    QCOMPARE(book.chapters().size(), 2);
    QCOMPARE(book.chapterText(0).trimmed(), QStringLiteral("第一章 相遇\n正文"));
}

void TestTextBook::opensGb18030()
{
    QTemporaryDir dir;
    const QString path = writeTemp(dir, "b.txt", QByteArray::fromHex("b5dad2bbd5c220b2e2cad4"));
    TextBook book;
    QVERIFY(book.open(path));
    QCOMPARE(book.encoding(), TextEncoding::Gb18030);
    QCOMPARE(book.chapterText(0).trimmed(), QStringLiteral("第一章 测试"));
}

void TestTextBook::chapterTextSlices()
{
    QTemporaryDir dir;
    const QString path = writeTemp(dir, "c.txt",
        QStringLiteral("第一章 甲\nAAAA\n第二章 乙\nBBBB").toUtf8());
    TextBook book;
    QVERIFY(book.open(path));
    QCOMPARE(book.chapters().size(), 2);
    QVERIFY(book.chapterText(0).contains(QStringLiteral("AAAA")));
    QVERIFY(!book.chapterText(0).contains(QStringLiteral("BBBB")));
    QVERIFY(book.chapterText(1).contains(QStringLiteral("BBBB")));
}

void TestTextBook::noChaptersYieldsWholeText()
{
    QTemporaryDir dir;
    const QString path = writeTemp(dir, "d.txt", QStringLiteral("无章节的正文内容").toUtf8());
    TextBook book;
    QVERIFY(book.open(path));
    QCOMPARE(book.chapters().size(), 1);
    QCOMPARE(book.chapterText(0).trimmed(), QStringLiteral("无章节的正文内容"));
}

void TestTextBook::factoryTxtOnly()
{
    QTemporaryDir dir;
    const QString path = writeTemp(dir, "e.txt", QByteArray("hi"));
    QString err;
    auto book = Book::create(path, &err);
    QVERIFY(book);
    QCOMPARE(book->title(), QStringLiteral("e"));
    const QString bad = writeTemp(dir, "f.epub", QByteArray("fake"));
    auto badBook = Book::create(bad, &err);
    QVERIFY(!badBook);
    QVERIFY(!err.isEmpty());
}

QTEST_APPLESS_MAIN(TestTextBook)
#include "tst_textbook.moc"
