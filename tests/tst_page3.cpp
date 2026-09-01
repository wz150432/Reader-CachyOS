#include <QtTest>
#include "core/Page.h"

using namespace reader;

class TestPage3 : public QObject
{
    Q_OBJECT
private slots:
    void scrollStepScrollsMultipleLines();
};

void TestPage3::scrollStepScrollsMultipleLines()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 12);
    page.setParams(p);
    page.setViewSize(300, 120);
    QString text;
    for (int i = 0; i < 40; ++i)
        text += QStringLiteral("第%1章 测试\n正文内容测试正文内容测试\n").arg(i + 1);
    page.setText(text);
    page.setScrollStep(3);
    QVERIFY(page.scrollDown());
    QVERIFY(page.lineOffset() == 3 || page.currentPage() > 0);
    page.scrollUp();
}

QTEST_MAIN(TestPage3)
#include "tst_page3.moc"
