#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include "app/ReadingView.h"
#include "core/Book.h"

using namespace reader;

class TestReadingView2 : public QObject
{
    Q_OBJECT
private slots:
    void customKeysetTriggersSignals();
    void wheelScrollsByLine();
    void findNextJumpsAndHighlights();
    void jumpToBookProgressWholeBook();
    void ctrlWheelAdjustsAlpha();
    void ctrlShiftWheelSnapsAlpha();
    void translucentBackgroundEnabled();
};

static std::shared_ptr<Book> makeBook(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("novel.txt"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return nullptr;
    QString text;
    for (int i = 0; i < 30; ++i) {
        text += QStringLiteral("第%1章 章节\n").arg(i + 1);
        if (i == 5)
            text += QStringLiteral("独一无二的线索词\n");
        text += QStringLiteral("正文内容测试\n").repeated(8);
    }
    f.write(text.toUtf8());
    f.close();
    QString err;
    return Book::create(path, &err);
}

void TestReadingView2::customKeysetTriggersSignals()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    QVERIFY(book);
    ReadingView view;
    view.resize(500, 600);
    view.setSettings(DisplaySettings());
    Keyset k;
    k.setShortcut(KeyAction::Search, QKeySequence(QStringLiteral("F6")));
    view.setKeyset(k);
    view.setBook(book);
    QSignalSpy spy(&view, &ReadingView::searchRequested);
    QTest::keyClick(&view, Qt::Key_F6);
    QCOMPARE(spy.count(), 1);
}

void TestReadingView2::wheelScrollsByLine()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    QVERIFY(book);
    ReadingView view;
    DisplaySettings s;
    s.font = QFont(QStringLiteral("Noto Sans CJK SC"), 16);
    view.setSettings(s);
    view.setBook(book);
    view.show();
    view.resize(300, 100);
    QVERIFY(view.pageCount() > 1);
    QCOMPARE(view.lineOffset(), 0);
    QWheelEvent wheel(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, -120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &wheel);
    QVERIFY(view.lineOffset() > 0 || view.currentPage() > 0);
}

void TestReadingView2::findNextJumpsAndHighlights()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    QVERIFY(book);
    ReadingView view;
    view.setSettings(DisplaySettings());
    view.setBook(book);
    view.resize(500, 600);
    view.goToChapter(5);
    QVERIFY(view.findNext(QStringLiteral("独一无二的线索词")));
    QVERIFY(view.currentMatchStart() >= 0);
    QCOMPARE(view.currentChapter(), 5);
    const QString text = book->chapterText(5);
    const QPair<int, int> range = view.currentPageCharRange();
    QVERIFY(view.currentMatchStart() >= range.first);
    QVERIFY(view.currentMatchStart() < range.second);
    Q_UNUSED(text);
}

void TestReadingView2::jumpToBookProgressWholeBook()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    QVERIFY(book);
    ReadingView view;
    view.setSettings(DisplaySettings());
    view.setBook(book);
    view.resize(500, 600);
    view.jumpToBookProgress(0.0);
    QCOMPARE(view.currentChapter(), 0);
    QCOMPARE(view.currentPage(), 0);
    view.jumpToBookProgress(1.0);
    QCOMPARE(view.currentChapter(), book->chapters().size() - 1);
    view.jumpToBookProgress(0.5);
    QVERIFY(view.currentChapter() > 0);
}

void TestReadingView2::ctrlWheelAdjustsAlpha()
{
    ReadingView view;
    view.setSettings(DisplaySettings());
    QSignalSpy spy(&view, &ReadingView::displaySettingsChanged);
    // Ctrl+滚轮向上：更透明
    QWheelEvent up(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, 120),
                   Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &up);
    QCOMPARE(spy.count(), 1);
    const auto gotUp = spy.last().at(0).value<DisplaySettings>();
    QCOMPARE(gotUp.windowAlpha, 245);
    // Ctrl+滚轮向下：更不透明
    QWheelEvent down(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, -120),
                     Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &down);
    QCOMPARE(spy.count(), 2);
    const auto gotDown = spy.last().at(0).value<DisplaySettings>();
    QCOMPARE(gotDown.windowAlpha, 255);
}

void TestReadingView2::ctrlShiftWheelSnapsAlpha()
{
    ReadingView view;
    view.setSettings(DisplaySettings());
    QSignalSpy spy(&view, &ReadingView::displaySettingsChanged);
    // Ctrl+Shift+滚轮向上：基本全透明
    QWheelEvent up(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, 120),
                   Qt::NoButton, Qt::ControlModifier | Qt::ShiftModifier,
                   Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &up);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).value<DisplaySettings>().windowAlpha, 1);
    // Ctrl+Shift+滚轮向下：完全不透明
    QWheelEvent down(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, -120),
                     Qt::NoButton, Qt::ControlModifier | Qt::ShiftModifier,
                     Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &down);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.last().at(0).value<DisplaySettings>().windowAlpha, 255);
}

void TestReadingView2::translucentBackgroundEnabled()
{
    ReadingView view;
    QVERIFY(view.testAttribute(Qt::WA_TranslucentBackground));
}

QTEST_MAIN(TestReadingView2)
#include "tst_readingview2.moc"
