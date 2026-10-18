#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/Book.h"

using namespace reader;
class TestEbookFormats : public QObject {
    Q_OBJECT
private slots:
    void fb2SectionsAndMetadata() {
        QTemporaryDir dir;
        const QString path = dir.filePath("novel.FB2");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QFile source(QFINDTESTDATA("fixtures/sections.fb2"));
        QVERIFY(source.open(QIODevice::ReadOnly));
        file.write(source.readAll());
        file.close();
        QString error;
        auto book = Book::create(path, &error);
        QVERIFY2(book, qPrintable(error));
        QCOMPARE(book->title(), QStringLiteral("测试书"));
        QCOMPARE(book->chapters().size(), 2);
        QCOMPARE(book->chapters()[0].title, QStringLiteral("第一章"));
        QCOMPARE(book->chapters()[1].title, QStringLiteral("第二节"));
        QVERIFY(book->chapterText(0).contains(QStringLiteral("甲乙 & 丙")));
        QVERIFY(book->chapterText(1).contains(QStringLiteral("后续正文")));
        QVERIFY(!book->chapterText(0).contains(QStringLiteral("不属于正文")));
        QCOMPARE(book->chapters()[1].charOffset, book->chapterText(0).size());
        QCOMPARE(book->totalCharCount(), qint64(book->chapterText(0).size() + book->chapterText(1).size()));
        QVERIFY(book->chapterText(-1).isEmpty());
    }
    void preservesTextBeforeDetectedChapters() {
        QTemporaryDir dir;
        const QString path = dir.filePath("preface.fb2");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QStringLiteral("<FictionBook><body><section><p>前言正文</p><p>第一章 开始</p><p>后续正文</p></section></body></FictionBook>").toUtf8());
        file.close();
        QString error;
        auto book = Book::create(path, &error);
        QVERIFY2(book, qPrintable(error));
        QVERIFY(book->chapterText(0).startsWith(QStringLiteral("前言正文")));
        QCOMPARE(book->chapters().size(), 2);
    }
    void kindleFormats_data() {
        QTest::addColumn<QString>("fixture");
        QTest::addColumn<QString>("extension");
        QTest::addColumn<QString>("title");
        QTest::newRow("mobi-palmdoc") << "sample-ncx.mobi" << "mobi" << "libmobi ncx test";
        QTest::newRow("azw3-kf8") << "sample-obfuscated-fonts.mobi" << "azw3" << "font";
        QTest::newRow("mobi-huffdic") << "sample-unicode-huffdic.mobi" << "mobi" << "";
        QTest::newRow("mobi-cp1252") << "sample-cp1252.mobi" << "MOBI" << "";
    }
    void kindleFormats() {
        QFETCH(QString, fixture); QFETCH(QString, extension); QFETCH(QString, title);
        QTemporaryDir dir;
        const QString path = dir.filePath("book." + extension);
        QVERIFY(QFile::copy(QFINDTESTDATA("fixtures/kindle/" + fixture), path));
        QString error;
        auto book = Book::create(path, &error);
        QVERIFY2(book, qPrintable(error));
        if (!title.isEmpty()) QCOMPARE(book->title(), title);
        QVERIFY(!book->chapters().isEmpty());
        QVERIFY(book->totalCharCount() > 20);
        QVERIFY(!book->chapterText(0).isEmpty());
        QVERIFY(!book->chapterText(0).contains("<html"));
        qint64 total = 0;
        for (int i = 0; i < book->chapters().size(); ++i) {
            QCOMPARE(qint64(book->chapters()[i].charOffset), total);
            total += book->chapterText(i).size();
        }
        QCOMPARE(book->totalCharCount(), total);
    }
    void rejectsDrm() {
        QString error;
        auto book = Book::create(QFINDTESTDATA("fixtures/kindle/sample-drm-v1.mobi"), &error);
        QVERIFY(!book);
        QVERIFY2(error.contains("DRM"), qPrintable(error));
    }
    void rejectsBroken_data() {
        QTest::addColumn<QString>("ext");
        QTest::newRow("fb2") << "fb2";
        QTest::newRow("mobi") << "mobi";
        QTest::newRow("azw3") << "azw3";
    }
    void rejectsBroken() {
        QFETCH(QString, ext);
        QTemporaryDir dir;
        const QString path = dir.filePath("bad." + ext);
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("<FictionBook><body>broken"); file.close();
        QString error;
        QVERIFY(!Book::create(path, &error));
        QVERIFY(!error.isEmpty());
    }
};
QTEST_MAIN(TestEbookFormats)
#include "tst_ebookformats.moc"
