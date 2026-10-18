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
    void readingBackgroundHasSmoothCorners_data();
    void readingBackgroundHasSmoothCorners();
    void setBookAndNavigate();
    void keyAndWheelNavigation();
    void leftClickPagesOnRelease();
};

void TestReadingView::readingBackgroundHasSmoothCorners_data()
{
    QTest::addColumn<bool>("backgroundImage");
    QTest::addColumn<int>("alpha");
    QTest::newRow("solid") << false << 255;
    QTest::newRow("image") << true << 255;
    QTest::newRow("transparent-solid") << false << 128;
    QTest::newRow("transparent-image") << true << 128;
}

void TestReadingView::readingBackgroundHasSmoothCorners()
{
    QFETCH(bool, backgroundImage);
    QFETCH(int, alpha);
    QTemporaryDir directory;
    DisplaySettings settings;
    settings.bgColor = Qt::black;
    settings.windowAlpha = alpha;
    if (backgroundImage) {
        QImage texture(30, 30, QImage::Format_ARGB32);
        texture.fill(Qt::blue);
        settings.bgImagePath = directory.filePath(QStringLiteral("background.png"));
        QVERIFY(texture.save(settings.bgImagePath));
    }
    ReadingView view;
    view.resize(320, 120);
    view.setSettings(settings);
    view.show();
    const QImage image = view.grab().toImage();
    for (const QPoint corner : {QPoint(0, 0), QPoint(319, 0), QPoint(0, 119), QPoint(319, 119)})
        QCOMPARE(image.pixelColor(corner).alpha(), 0);
    // The surface remains filled away from its curved edge.
    const QColor edge = image.pixelColor(160, 2);
    if (alpha == 255) {
        QCOMPARE(edge.alpha(), 255);
        QCOMPARE(edge.rgb(), QColor(backgroundImage ? Qt::blue : Qt::black).rgb());
    } else {
        QVERIFY(edge.alpha() > 0 && edge.alpha() < 255);
    }
    bool smoothEdge = false;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const int coverage = image.pixelColor(x, y).alpha();
            smoothEdge |= coverage > 0 && coverage < alpha;
        }
    QVERIFY2(smoothEdge, "Rounded edges must include antialiased coverage");
}

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

void TestReadingView::leftClickPagesOnRelease()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    QVERIFY(book);
    ReadingView view;
    view.setBook(book);
    view.resize(300, 100);
    view.show();
    QVERIFY(view.pageCount() > 1);
    QTest::mousePress(&view, Qt::LeftButton);
    QCOMPARE(view.currentPage(), 0);
    QTest::mouseRelease(&view, Qt::LeftButton);
    QCOMPARE(view.currentPage(), 1);
}

QTEST_MAIN(TestReadingView)
#include "tst_readingview.moc"
