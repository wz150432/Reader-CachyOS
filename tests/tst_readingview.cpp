#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include "app/ReadingView.h"
#include "core/Book.h"

using namespace reader;

class TestReadingView : public QObject
{
    Q_OBJECT
private slots:
    void setBookAndNavigate();
    void keyAndWheelNavigation();
};

static std::shared_ptr<Book> makeBook(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("novel.txt"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return nullptr;
    QString text;
    for (int i = 0; i < 50; ++i)
        text += QStringLiteral("第%1章 章节\n").arg(i + 1)
            + QStringLiteral("正文内容测试\n").repeated(10);
    const qint64 written = f.write(text.toUtf8());
    f.close();
    if (written <= 0)
        return nullptr;
    QString err;
    return Book::create(path, &err);
}

void TestReadingView::setBookAndNavigate()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    QVERIFY(book);
    ReadingView view;
    view.setSettings(DisplaySettings());
    view.setBook(book);
    view.resize(600, 800);
    QCOMPARE(view.pageCount(), 1);
    QCOMPARE(view.currentChapter(), 0);
    view.goToChapter(3);
    QCOMPARE(view.currentChapter(), 3);
    QCOMPARE(view.currentPage(), 0);
    QCOMPARE(view.pageCount(), 1);
}

void TestReadingView::keyAndWheelNavigation()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    ReadingView view;
    DisplaySettings s;
    s.font = QFont(QStringLiteral("Noto Sans CJK SC"), 18);
    view.setSettings(s);
    view.setBook(book);
    view.show();
    view.resize(300, 100);
    QVERIFY(view.pageCount() > 1);
    const int first = view.currentPage();
    QTest::keyClick(&view, Qt::Key_Right);
    QCOMPARE(view.currentPage(), first + 1);
    QTest::keyClick(&view, Qt::Key_Left);
    QCOMPARE(view.currentPage(), first);
    QWheelEvent wheel(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, -120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &wheel);
    QVERIFY(view.pixelOffset() > 0.0 || view.currentPage() > first);
}

QTEST_MAIN(TestReadingView)
#include "tst_readingview.moc"
