#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include "app/ReadingView.h"
#include "core/Book.h"

using namespace reader;

class TestTags : public QObject
{
    Q_OBJECT
private slots:
    void disabledTagIgnored();
    void enabledTagStored();
};

void TestTags::disabledTagIgnored()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(QStringLiteral("第一章\n剑光如雪\n").toUtf8());
    f.close();
    QString err;
    auto book = Book::create(path, &err);
    ReadingView view;
    view.setSettings(DisplaySettings());
    view.setBook(book);
    view.setTags({TagItem{QStringLiteral("剑"), QColor("red"), QColor("yellow"), false}});
    view.resize(500, 400);
    QVERIFY(view.tagCount() == 0);
}

void TestTags::enabledTagStored()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("b.txt"));
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(QStringLiteral("第一章\n剑光如雪\n").toUtf8());
    f.close();
    QString err;
    auto book = Book::create(path, &err);
    ReadingView view;
    view.setSettings(DisplaySettings());
    view.setBook(book);
    view.setTags({TagItem{QStringLiteral("剑"), QColor("red"), QColor("yellow"), true}});
    view.resize(500, 400);
    QVERIFY(view.tagCount() > 0);
}

QTEST_MAIN(TestTags)
#include "tst_tags.moc"
