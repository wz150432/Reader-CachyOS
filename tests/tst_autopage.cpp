#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include "app/ReadingView.h"
#include "core/Book.h"

using namespace reader;

class TestAutoPage : public QObject
{
    Q_OBJECT
private slots:
    void toggleStartsAndStops();
    void timerAdvancesPageInFlipMode();
    void timerScrollsInScrollMode();
};

static std::shared_ptr<Book> makeBook(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("novel.txt"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return nullptr;
    QString text;
    for (int i = 0; i < 40; ++i)
        text += QStringLiteral("第%1章 章节\n").arg(i + 1) + QStringLiteral("正文内容测试\n").repeated(8);
    f.write(text.toUtf8());
    f.close();
    QString err;
    return Book::create(path, &err);
}

void TestAutoPage::toggleStartsAndStops()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    QVERIFY(book);
    ReadingView view;
    view.setSettings(DisplaySettings());
    view.setBehavior(BehaviorSettings());
    view.setBook(book);
    view.show();
    view.resize(400, 300);
    QVERIFY(!view.isAutoPaging());
    view.toggleAutoPage();
    QVERIFY(view.isAutoPaging());
    view.toggleAutoPage();
    QVERIFY(!view.isAutoPaging());
}

void TestAutoPage::timerAdvancesPageInFlipMode()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    ReadingView view;
    view.setSettings(DisplaySettings());
    BehaviorSettings b;
    b.autoPageScrollMode = false;
    b.autoPageIntervalMs = 10;
    view.setBehavior(b);
    view.setBook(book);
    view.show();
    view.resize(400, 200);
    QVERIFY(view.pageCount() > 1);
    const int first = view.currentPage();
    view.toggleAutoPage();
    QTRY_VERIFY_WITH_TIMEOUT(view.currentPage() > first, 1000);
}

void TestAutoPage::timerScrollsInScrollMode()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    ReadingView view;
    view.setSettings(DisplaySettings());
    BehaviorSettings b;
    b.autoPageScrollMode = true;
    b.autoPageIntervalMs = 10;
    b.scrollStep = 2;
    view.setBehavior(b);
    view.setBook(book);
    view.show();
    view.resize(400, 150);
    const int before = view.currentPage();
    view.toggleAutoPage();
    QTRY_VERIFY_WITH_TIMEOUT(view.currentPage() > before || view.lineOffset() > 0, 1000);
}

QTEST_MAIN(TestAutoPage)
#include "tst_autopage.moc"
