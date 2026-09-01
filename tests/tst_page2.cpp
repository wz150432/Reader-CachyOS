#include <QtTest>
#include "core/Page.h"

using namespace reader;

class TestPage2 : public QObject
{
    Q_OBJECT
private slots:
    void lineScrolling();
    void scrollLinesAcrossPages();
    void charRangesPartitionText();
    void pageForCharFindsPage();
};

static QString longText()
{
    QString text;
    for (int i = 0; i < 200; ++i)
        text += QStringLiteral("第%1章 测试\n正文内容测试\n").arg(i + 1);
    return text;
}

void TestPage2::lineScrolling()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 12);
    page.setParams(p);
    page.setViewSize(300, 120);
    page.setText(longText());
    QVERIFY(page.pageCount() > 1);
    QCOMPARE(page.lineOffset(), 0);
    QVERIFY(page.nextLine());
    QCOMPARE(page.lineOffset(), 1);
    QVERIFY(page.prevLine());
    QCOMPARE(page.lineOffset(), 0);
}

void TestPage2::scrollLinesAcrossPages()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 12);
    page.setParams(p);
    page.setViewSize(300, 80);
    page.setText(longText());
    QVERIFY(page.pageCount() > 2);
    const int firstPage = page.currentPage();
    const int firstLines = page.linesOnCurrentPage();
    QVERIFY(page.scrollLines(firstLines));
    QVERIFY(page.currentPage() >= firstPage + 1);
    QVERIFY(page.scrollLines(-firstLines));
    QCOMPARE(page.currentPage(), firstPage);
}

void TestPage2::charRangesPartitionText()
{
    Page page;
    PageLayoutParams p;
    page.setParams(p);
    page.setViewSize(500, 200);
    const QString text = longText();
    page.setText(text);
    const int count = page.pageCount();
    QVERIFY(count > 1);
    for (int i = 0; i + 1 < count; ++i) {
        const QPair<int, int> a = page.charRange(i);
        const QPair<int, int> b = page.charRange(i + 1);
        QVERIFY(a.second <= b.first);
    }
    const QPair<int, int> last = page.charRange(count - 1);
    QVERIFY(last.second > last.first);
}

void TestPage2::pageForCharFindsPage()
{
    Page page;
    PageLayoutParams p;
    page.setParams(p);
    page.setViewSize(500, 200);
    const QString text = longText();
    page.setText(text);
    const int count = page.pageCount();
    QVERIFY(page.pageForChar(0) == 0);
    QVERIFY(page.pageForChar(text.size() - 1) == count - 1);
    const int mid = page.pageForChar(text.size() / 2);
    const QPair<int, int> range = page.charRange(mid);
    QVERIFY(text.size() / 2 >= range.first);
    QVERIFY(text.size() / 2 < range.second);
}

QTEST_MAIN(TestPage2)
#include "tst_page2.moc"
