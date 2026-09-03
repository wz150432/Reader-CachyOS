#include <QtTest>
#include "core/Keyset.h"

using namespace reader;

class TestKeyset : public QObject
{
    Q_OBJECT
private slots:
    void defaults();
    void saveLoadRoundtrip();
    void resetRestores();
};

void TestKeyset::defaults()
{
    Keyset k;
    QCOMPARE(k.shortcut(KeyAction::Search), QKeySequence(QStringLiteral("Ctrl+F")));
    QCOMPARE(k.shortcut(KeyAction::Jump), QKeySequence(QStringLiteral("Ctrl+G")));
    QCOMPARE(k.shortcut(KeyAction::AddBookmark), QKeySequence(QStringLiteral("Ctrl+M")));
    QCOMPARE(k.shortcut(KeyAction::PageDown), QKeySequence(Qt::Key_Right));
    QCOMPARE(k.shortcut(KeyAction::LineDown), QKeySequence(Qt::Key_Down));
    QCOMPARE(k.shortcut(KeyAction::ChapterDown), QKeySequence(Qt::CTRL | Qt::Key_Right));
    QCOMPARE(k.shortcut(KeyAction::Fullscreen), QKeySequence(Qt::Key_F11));
    QCOMPARE(k.shortcut(KeyAction::HideBorder), QKeySequence(Qt::Key_F12));
    QCOMPARE(k.shortcut(KeyAction::AlwaysOnTop), QKeySequence(QStringLiteral("Alt+T")));
    QCOMPARE(k.shortcut(KeyAction::HideWindow), QKeySequence(QStringLiteral("Alt+H")));
    QCOMPARE(k.shortcut(KeyAction::MouseLeaveHide), QKeySequence(QStringLiteral("Ctrl+Shift+Alt+P")));
}

void TestKeyset::saveLoadRoundtrip()
{
    Keyset k;
    k.setShortcut(KeyAction::Search, QKeySequence(QStringLiteral("Alt+F")));
    k.setShortcut(KeyAction::PageDown, QKeySequence(QStringLiteral("PgDown")));
    const QJsonObject saved = k.save();
    Keyset t;
    t.load(saved);
    QCOMPARE(t.shortcut(KeyAction::Search), QKeySequence(QStringLiteral("Alt+F")));
    QCOMPARE(t.shortcut(KeyAction::PageDown), QKeySequence(QStringLiteral("PgDown")));
    QCOMPARE(t.shortcut(KeyAction::Fullscreen), QKeySequence(Qt::Key_F11));
}

void TestKeyset::resetRestores()
{
    Keyset k;
    k.setShortcut(KeyAction::Search, QKeySequence(QStringLiteral("Alt+F")));
    k.reset();
    QCOMPARE(k.shortcut(KeyAction::Search), QKeySequence(QStringLiteral("Ctrl+F")));
}

QTEST_APPLESS_MAIN(TestKeyset)
#include "tst_keyset.moc"
