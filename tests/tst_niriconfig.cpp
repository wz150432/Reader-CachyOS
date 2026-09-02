#include <QtTest>
#include "core/NiriConfig.h"

using namespace reader;

class TestNiriConfig : public QObject
{
    Q_OBJECT
private slots:
    void updatesExistingReaderBlock();
    void appendsWhenMissing();
    void invalidOpacityRejected();
    void globalHideBindAddAndRemove();
};

void TestNiriConfig::updatesExistingReaderBlock()
{
    QString content = QStringLiteral(
        "window-rule {\n"
        "    match app-id=\"zoom\"\n"
        "    open-floating true\n"
        "}\n"
        "// Reader\n"
        "window-rule {\n"
        "    match app-id=r\"^reader(\\.desktop)?$\"\n"
        "    open-floating true\n"
        "}\n");
    QVERIFY(patchReaderOpacity(&content, 0.5));
    QVERIFY(content.contains(QStringLiteral("opacity 0.5")));
    QVERIFY(!content.contains(QStringLiteral("opacity 0.6")));
    QVERIFY(content.contains(QStringLiteral("reader(\\.desktop)")));
    QVERIFY(content.contains(QStringLiteral("match title=r\".* - Reader$\"")));
    QVERIFY(content.contains(QStringLiteral("shadow { off; }")));
    QVERIFY(content.contains(QStringLiteral("draw-border-with-background false")));
    // 幂等：再次调用仍只有一行 opacity
    QVERIFY(patchReaderOpacity(&content, 0.25));
    QCOMPARE(content.count(QStringLiteral("opacity ")), 1);
    QVERIFY(content.contains(QStringLiteral("opacity 0.25")));
}

void TestNiriConfig::appendsWhenMissing()
{
    QString content = QStringLiteral("window-rule {\n    match app-id=\"zoom\"\n}\n");
    QVERIFY(patchReaderOpacity(&content, 1.0));
    QVERIFY(content.contains(QStringLiteral("reader(\\.desktop)")));
    QVERIFY(content.contains(QStringLiteral("opacity 1")));
}

void TestNiriConfig::invalidOpacityRejected()
{
    QString content = QStringLiteral("window-rule { }\n");
    QVERIFY(!patchReaderOpacity(&content, -0.1));
    QVERIFY(!patchReaderOpacity(&content, 1.5));
    QVERIFY(!patchReaderOpacity(nullptr, 0.5));
}

void TestNiriConfig::globalHideBindAddAndRemove()
{
    QString content = QStringLiteral(
        "binds {\n"
        "    Mod+Q { close-window; }\n"
        "}\n");
    QVERIFY(patchReaderGlobalHide(&content, true, QStringLiteral("/tmp/reader")));
    QVERIFY(content.contains(QStringLiteral("Ctrl+Shift+H")));
    QVERIFY(content.contains(QStringLiteral("\"/tmp/reader\"")));
    QVERIFY(content.contains(QStringLiteral("--toggle-hide")));
    QVERIFY(patchReaderGlobalHide(&content, false, QStringLiteral("/tmp/reader")));
    QVERIFY(!content.contains(QStringLiteral("--toggle-hide")));
    QVERIFY(!content.contains(QStringLiteral("reader-global-hide")));
}

QTEST_APPLESS_MAIN(TestNiriConfig)
#include "tst_niriconfig.moc"
