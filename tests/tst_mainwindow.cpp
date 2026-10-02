#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include "app/MainWindow.h"
#include "core/Cache.h"
#include "app/ReadingView.h"
#include "EpubFixture.h"
#include <QTimer>
#include <QMessageBox>

using namespace reader;

class TestMainWindow : public QObject
{
    Q_OBJECT
private slots:
    void openBookPopulatesTocAndTitle();
    void pageChangeSavesProgress();
    void epubReadingAndSafeEditMode();
};

static QString makeTxt(const QTemporaryDir &dir, const QString &name)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    QString text;
    for (int i = 0; i < 10; ++i)
        text += QStringLiteral("第%1章 章节\n正文内容\n").arg(i + 1);
    f.write(text.toUtf8());
    f.close();
    return path;
}

void TestMainWindow::openBookPopulatesTocAndTitle()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString path = makeTxt(dir, QStringLiteral("test_book.txt"));
    w.openBook(path);
    QCOMPARE(w.tocItemCount(), 10);
    QVERIFY(w.currentBookTitle().contains(QStringLiteral("第1章 章节")));
}

void TestMainWindow::pageChangeSavesProgress()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString path = makeTxt(dir, QStringLiteral("save_book.txt"));
    w.openBook(path);
    QTest::keyClick(w.findChild<QWidget *>(QStringLiteral("readingView")), Qt::Key_Right);
    QTest::qWait(50);
    const QString cachePath = Cache::defaultCacheFilePath();
    Cache c(cachePath);
    c.load();
    const auto p = c.progress(path);
    QVERIFY(p.has_value());
    QVERIFY(p->pageIndex >= 0);
}

void TestMainWindow::epubReadingAndSafeEditMode()
{
    QTemporaryDir dir;
    const QString path = fixture(dir);
    QFile original(path);
    QVERIFY(original.open(QIODevice::ReadOnly));
    const QByteArray bytes = original.readAll();
    original.close();
    {
        MainWindow w;
        w.openBook(path);
        w.show();
        QCOMPARE(w.tocItemCount(), 2);
        QVERIFY(w.currentBookTitle().contains(QStringLiteral("目录乙")));
        auto *view = w.findChild<ReadingView *>();
        QVERIFY(view);
        view->pageDown();
        QCOMPARE(w.currentChapter(), 1);
        view->goToChapter(0);
        view->setSearchWholeBook(true);
        QVERIFY(view->findNext(QStringLiteral("beta")));
        QCOMPARE(view->currentMatchStart(), 11);
        QCOMPARE(w.currentChapter(), 1);
        QVERIFY(w.currentProgressPercent() > 0);
        w.addBookmarkForCurrentBook();
        QTimer::singleShot(0, [] {
            if (auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                dialog->accept();
        });
        QTest::keyClick(view, Qt::Key_E, Qt::ControlModifier);
        QVERIFY(!w.editModeActive());
        w.close();
    }
    Cache cache(Cache::defaultCacheFilePath());
    cache.load();
    auto progress = cache.progress(path);
    QVERIFY(progress);
    QCOMPARE(progress->chapterIndex, 1);
    const auto marks = cache.bookmarks(path);
    QCOMPARE(marks.size(), 1);
    QCOMPARE(marks.first().chapterIndex, 1);
    MainWindow restored;
    restored.openBook(path);
    QCOMPARE(restored.currentChapter(), 1);
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), bytes);
}

int main(int argc, char *argv[])
{
    QTemporaryDir tmp;
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
    QApplication app(argc, argv);
    TestMainWindow tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_mainwindow.moc"
