#include <QtTest>
#include <QTemporaryDir>
#include "core/Settings.h"

using namespace reader;

class TestSettings2 : public QObject
{
    Q_OBJECT
private slots:
    void newFieldsRoundtrip();
    void keysetPersistsWithSettings();
    void missingFieldsFallBackToDefaults();
};

void TestSettings2::newFieldsRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    s.display.bgImagePath = QStringLiteral("/tmp/bg.png");
    s.display.windowAlpha = 128;
    s.save();
    Settings t(path);
    t.load();
    QCOMPARE(t.display.bgImagePath, QStringLiteral("/tmp/bg.png"));
    QCOMPARE(t.display.windowAlpha, 128);
}

void TestSettings2::keysetPersistsWithSettings()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    s.keyset.setShortcut(KeyAction::Search, QKeySequence(QStringLiteral("Alt+F")));
    s.save();
    Settings t(path);
    t.load();
    QCOMPARE(t.keyset.shortcut(KeyAction::Search), QKeySequence(QStringLiteral("Alt+F")));
}

void TestSettings2::missingFieldsFallBackToDefaults()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    QCOMPARE(s.display.windowAlpha, 255);
    QVERIFY(s.display.bgImagePath.isEmpty());
    QCOMPARE(s.keyset.shortcut(KeyAction::Search), QKeySequence(QStringLiteral("Ctrl+F")));
}

QTEST_APPLESS_MAIN(TestSettings2)
#include "tst_settings2.moc"
