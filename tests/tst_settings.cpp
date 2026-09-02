#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/Settings.h"

using namespace reader;

class TestSettings : public QObject
{
    Q_OBJECT
private slots:
    void defaultsWhenMissing();
    void saveLoadRoundtrip();
    void corruptJsonFallsBackToDefaults();
};

void TestSettings::defaultsWhenMissing()
{
    QTemporaryDir dir;
    Settings s(dir.filePath(QStringLiteral("config.json")));
    s.load();
    QCOMPARE(s.display.font.family(), QStringLiteral("Noto Sans CJK SC"));
    QCOMPARE(s.display.lineGap, 4);
    QCOMPARE(s.display.bgColor, QColor(Qt::white));
    QVERIFY(s.display.wordWrap);
    QVERIFY(s.display.compressBlankLines);
}

void TestSettings::saveLoadRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    s.display.font.setFamily(QStringLiteral("Serif"));
    s.display.font.setPointSize(20);
    s.display.lineGap = 10;
    s.display.paragraphGap = 16;
    s.display.bgColor = QColor(0x11, 0x22, 0x33);
    s.display.textColor = QColor(0xAA, 0xBB, 0xCC);
    s.display.firstLineIndent = false;
    s.display.compressBlankLines = true;
    s.display.chapterPageBreak = true;
    s.display.wordWrap = false;
    s.save();

    Settings t(path);
    t.load();
    QCOMPARE(t.display.font.family(), QStringLiteral("Serif"));
    QCOMPARE(t.display.font.pointSize(), 20);
    QCOMPARE(t.display.lineGap, 10);
    QCOMPARE(t.display.paragraphGap, 16);
    QCOMPARE(t.display.bgColor, QColor(0x11, 0x22, 0x33));
    QCOMPARE(t.display.textColor, QColor(0xAA, 0xBB, 0xCC));
    QVERIFY(!t.display.firstLineIndent);
    QVERIFY(t.display.compressBlankLines);
    QVERIFY(t.display.chapterPageBreak);
    QVERIFY(!t.display.wordWrap);
}

void TestSettings::corruptJsonFallsBackToDefaults()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write("{ not json !!");
    f.close();
    Settings s(path);
    s.load();
    QCOMPARE(s.display.lineGap, 4);
}

QTEST_APPLESS_MAIN(TestSettings)
#include "tst_settings.moc"
