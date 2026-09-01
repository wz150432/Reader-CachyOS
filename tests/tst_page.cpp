#include <QtTest>
#include <QImage>
#include "core/Page.h"

using namespace reader;

class TestPage : public QObject
{
    Q_OBJECT
private slots:
    void singlePage();
    void multiplePages();
    void navigationBounds();
    void progressMonotonic();
    void compressBlankLines();
};

void TestPage::singlePage()
{
    Page page;
    PageLayoutParams p;
    page.setParams(p);
    page.setViewSize(800, 1000);
    page.setText(QStringLiteral("第一章 测试\n内容一\n内容二"));
    QCOMPARE(page.pageCount(), 1);
    QVERIFY(page.lineCount(0) >= 3);
}

void TestPage::multiplePages()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 18);
    page.setParams(p);
    page.setViewSize(400, 120);
    QString text;
    for (int i = 0; i < 200; ++i)
        text += QStringLiteral("第%1行 测试文字测试文字\n").arg(i);
    page.setText(text);
    QVERIFY(page.pageCount() > 1);
}

void TestPage::navigationBounds()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 18);
    page.setParams(p);
    page.setViewSize(400, 120);
    QString text;
    for (int i = 0; i < 100; ++i)
        text += QStringLiteral("第%1行 测试\n").arg(i);
    page.setText(text);
    const int count = page.pageCount();
    QVERIFY(count > 1);
    QVERIFY(!page.goToPage(-1));
    QVERIFY(!page.goToPage(count));
    QVERIFY(page.goToPage(0));
    QCOMPARE(page.currentPage(), 0);
    QVERIFY(page.nextPage());
    QCOMPARE(page.currentPage(), 1);
    QVERIFY(page.prevPage());
    QCOMPARE(page.currentPage(), 0);
    QVERIFY(page.goToPage(count - 1));
    QVERIFY(!page.nextPage());
}

void TestPage::progressMonotonic()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 18);
    page.setParams(p);
    page.setViewSize(400, 120);
    QString text;
    for (int i = 0; i < 100; ++i)
        text += QStringLiteral("第%1行 测试\n").arg(i);
    page.setText(text);
    const int count = page.pageCount();
    QVERIFY(count > 1);
    const qreal first = page.progress();
    QVERIFY(page.nextPage());
    QVERIFY(page.progress() > first);
    page.jumpToProgress(1.0);
    QCOMPARE(page.currentPage(), count - 1);
    page.jumpToProgress(0.0);
    QCOMPARE(page.currentPage(), 0);
}

void TestPage::compressBlankLines()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 18);
    page.setParams(p);
    page.setViewSize(400, 200);
    QString text = QStringLiteral("第一段\n");
    for (int i = 0; i < 30; ++i)
        text += QStringLiteral("\n");
    text += QStringLiteral("第二段\n");

    p.compressBlankLines = false;
    page.setParams(p);
    page.setText(text);
    const int without = page.pageCount();

    p.compressBlankLines = true;
    page.setParams(p);
    page.setText(text);
    const int with = page.pageCount();

    QVERIFY(with < without);
}

QTEST_MAIN(TestPage)
#include "tst_page.moc"
