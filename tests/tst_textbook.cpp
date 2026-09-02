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
    void factoryAppliesCustomRegex();
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

void TestTextBook::factoryAppliesCustomRegex()
{
    QTemporaryDir dir;
    const QString path = writeTemp(dir, "parts.txt",
        QStringLiteral("PART 1 开端\n正文一\nPART 2 发展\n正文二\n尾声\n").toUtf8());
    QString err;
    auto book = Book::create(path, &err,
        QRegularExpression(QStringLiteral("^PART [0-9]+ .*$")));
    QVERIFY(book);
    QCOMPARE(book->chapters().size(), 2);
    QCOMPARE(book->chapters().at(0).title, QStringLiteral("PART 1 开端"));
    QCOMPARE(book->chapters().at(1).title, QStringLiteral("PART 2 发展"));
    QVERIFY(book->chapterText(0).contains(QStringLiteral("正文一")));
}

QTEST_APPLESS_MAIN(TestTextBook)
#include "tst_textbook.moc"
