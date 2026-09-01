#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include "app/MainWindow.h"
#include "core/Cache.h"

using namespace reader;

class TestMainWindow : public QObject
{
    Q_OBJECT
private slots:
    void openBookPopulatesTocAndTitle();
    void pageChangeSavesProgress();
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
