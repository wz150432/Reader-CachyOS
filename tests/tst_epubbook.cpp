#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <archive.h>
#include <archive_entry.h>
#include "core/Book.h"
using namespace reader;

#include "EpubFixture.h"
class TestEpubBook : public QObject {
    Q_OBJECT
private slots:
    void spineOrderAndText() {
        QTemporaryDir dir;
        QString error;
        auto book = Book::create(fixture(dir), &error);
        QVERIFY2(book, qPrintable(error));
        QCOMPARE(book->title(), QStringLiteral("测试书"));
        QCOMPARE(book->chapters().size(), 2);
        QCOMPARE(book->chapters()[0].title, QStringLiteral("目录乙"));
        QCOMPARE(book->chapters()[1].title, QStringLiteral("目录甲"));
        QCOMPARE(book->chapterText(0), QStringLiteral("乙章\n中文正文"));
        QCOMPARE(book->chapterText(1), QStringLiteral("甲章\nAlpha & beta.\n第二段\n下一行"));
        QCOMPARE(book->chapters()[1].charOffset, 7);
        QCOMPARE(book->totalCharCount(), qint64(31));
        QVERIFY(book->chapterText(-1).isEmpty());
        QVERIFY(book->chapterText(2).isEmpty());
    }
    void epub2Ncx() {
        QTemporaryDir dir;
        auto book = Book::create(fixture(dir, "<itemref idref='b'/><itemref idref='a'/>", true));
        QVERIFY(book);
        QCOMPARE(book->chapters()[0].title, QStringLiteral("目录乙"));
    }
    void headingFallback() {
        QTemporaryDir dir;
        auto book = Book::create(fixture(dir, "<itemref idref='a'/>", false, false));
        QVERIFY(book);
        QCOMPARE(book->chapters().size(), 1);
        QCOMPARE(book->chapters()[0].title, QStringLiteral("甲章"));
    }
    void skipsSvgCover() {
        QTemporaryDir dir;
        QString error;
        auto book = Book::create(fixture(dir, "<itemref idref='cover'/><itemref idref='b'/><itemref idref='a'/>"), &error);
        QVERIFY2(book, qPrintable(error));
        QCOMPARE(book->chapters().size(), 2);
        QCOMPARE(book->chapterText(0), QStringLiteral("乙章\n中文正文"));
    }
    void rejectsBrokenSpine() {
        QTemporaryDir dir;
        QString error;
        auto book = Book::create(fixture(dir, "<itemref idref='missing'/>"), &error);
        QVERIFY(!book);
        QVERIFY(!error.isEmpty());
    }
    void rejectsInvalidArchive() {
        QTemporaryDir dir;
        QString path = dir.filePath("broken.epub");
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("broken"); f.close();
        QString error;
        QVERIFY(!Book::create(path, &error));
        QVERIFY(!error.isEmpty());
    }
};
QTEST_MAIN(TestEpubBook)
#include "tst_epubbook.moc"
