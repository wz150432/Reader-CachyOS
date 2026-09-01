#include <QtTest>
#include "core/ChapterParser.h"

using namespace reader;

class TestChapterParser : public QObject
{
    Q_OBJECT
private slots:
    void defaultChapters();
    void defaultIgnoresPlainLines();
    void standaloneJianZi();
    void invalidChapterNumber();
    void regexChapters();
};

void TestChapterParser::defaultChapters()
{
    const QString text = QStringLiteral(
        "第一章 相遇\n"
        "正文内容\n"
        "第二章 离别\n"
        "正文内容\n"
        "第100章 终章\n"
        "楔子 前言部分\n"
        "序章 开始");
    const QVector<Chapter> chapters = ChapterParser::parseDefault(text);
    QCOMPARE(chapters.size(), 5);
    QCOMPARE(chapters.at(0).title, QStringLiteral("第一章 相遇"));
    QCOMPARE(chapters.at(1).title, QStringLiteral("第二章 离别"));
    QCOMPARE(chapters.at(2).title, QStringLiteral("第100章 终章"));
    QCOMPARE(chapters.at(3).title, QStringLiteral("楔子 前言部分"));
    QCOMPARE(chapters.at(4).title, QStringLiteral("序章 开始"));
    QVERIFY(chapters.at(0).charOffset < chapters.at(1).charOffset);
}

void TestChapterParser::defaultIgnoresPlainLines()
{
    const QString text = QStringLiteral("这是一段普通文字\n没有章节标题\n希望不会误判");
    QCOMPARE(ChapterParser::parseDefault(text).size(), 0);
}

void TestChapterParser::standaloneJianZi()
{
    const QString text = QStringLiteral("楔子\n正文\n第一章 开始");
    const QVector<Chapter> chapters = ChapterParser::parseDefault(text);
    QCOMPARE(chapters.size(), 2);
    QCOMPARE(chapters.at(0).title, QStringLiteral("楔子"));
}

void TestChapterParser::invalidChapterNumber()
{
    const QString text = QStringLiteral("第X章 不是章节\n正文");
    QCOMPARE(ChapterParser::parseDefault(text).size(), 0);
}

void TestChapterParser::regexChapters()
{
    const QString text = QStringLiteral(
        "第一章 相遇\n"
        "正文\n"
        "第二章 离别\n"
        "正文\n"
        "尾声\n");
    const QRegularExpression re(QStringLiteral("第[0-9一二三四五六七八九十]+章.*"));
    const QVector<Chapter> chapters = ChapterParser::parseRegex(text, re);
    QCOMPARE(chapters.size(), 2);
    QCOMPARE(chapters.at(0).title, QStringLiteral("第一章 相遇"));
    QCOMPARE(chapters.at(1).title, QStringLiteral("第二章 离别"));
}

QTEST_APPLESS_MAIN(TestChapterParser)
#include "tst_chapterparser.moc"
