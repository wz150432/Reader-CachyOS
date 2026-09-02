#include <QtTest>
#include <QTemporaryDir>
#include "core/Settings.h"

using namespace reader;

class TestSettings3 : public QObject
{
    Q_OBJECT
private slots:
    void behaviorRoundtrip();
    void tagsRoundtrip();
    void defaultsFilled();
    void advancedRegexRoundtrip();
};

void TestSettings3::behaviorRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    s.behavior.autoPageIntervalMs = 2000;
    s.behavior.autoPageScrollMode = true;
    s.behavior.scrollStep = 3;
    s.behavior.minimizeToTray = true;
    s.save();
    Settings t(path);
    t.load();
    QCOMPARE(t.behavior.autoPageIntervalMs, 2000);
    QVERIFY(t.behavior.autoPageScrollMode);
    QCOMPARE(t.behavior.scrollStep, 3);
    QVERIFY(t.behavior.minimizeToTray);
}

void TestSettings3::tagsRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    s.tags = { TagItem{QStringLiteral("剑"), QColor("red"), QColor("yellow"), true},
               TagItem{QStringLiteral("客栈"), QColor("blue"), QColor(), false} };
    s.save();
    Settings t(path);
    t.load();
    QCOMPARE(t.tags.size(), 2);
    QCOMPARE(t.tags.at(0).keyword, QStringLiteral("剑"));
    QCOMPARE(t.tags.at(1).keyword, QStringLiteral("客栈"));
    QVERIFY(!t.tags.at(1).enabled);
}

void TestSettings3::defaultsFilled()
{
    QTemporaryDir dir;
    Settings s(dir.filePath(QStringLiteral("config.json")));
    s.load();
    QCOMPARE(s.behavior.autoPageIntervalMs, 3000);
    QVERIFY(!s.behavior.autoPageScrollMode);
    QCOMPARE(s.behavior.scrollStep, 1);
    QVERIFY(!s.behavior.minimizeToTray);
    QVERIFY(s.tags.isEmpty());
    QVERIFY(s.chapterRegex.isEmpty());
}

void TestSettings3::advancedRegexRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    s.chapterRegex = QStringLiteral("^第[0-9]+章 .*$");
    s.save();
    Settings t(path);
    t.load();
    QCOMPARE(t.chapterRegex, QStringLiteral("^第[0-9]+章 .*$"));
}

QTEST_APPLESS_MAIN(TestSettings3)
#include "tst_settings3.moc"
