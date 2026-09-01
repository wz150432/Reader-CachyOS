#include <QtTest>
#include "core/TextCodec.h"

using namespace reader;

class TestTextCodec : public QObject
{
    Q_OBJECT
private slots:
    void utf8Bom();
    void utf8Plain();
    void utf16le();
    void utf16be();
    void gb18030();
    void asciiIsUtf8();
};

void TestTextCodec::utf8Bom()
{
    const QByteArray bytes = QByteArray::fromHex("efbbbf") + QStringLiteral("你好").toUtf8();
    const DecodeResult r = TextCodec::decode(bytes);
    QVERIFY(r.ok);
    QCOMPARE(r.encoding, TextEncoding::Utf8Bom);
    QCOMPARE(r.text, QStringLiteral("你好"));
}

void TestTextCodec::utf8Plain()
{
    const QByteArray bytes = QStringLiteral("第一章 测试").toUtf8();
    const DecodeResult r = TextCodec::decode(bytes);
    QVERIFY(r.ok);
    QCOMPARE(r.encoding, TextEncoding::Utf8);
    QCOMPARE(r.text, QStringLiteral("第一章 测试"));
}

void TestTextCodec::utf16le()
{
    QByteArray data = QByteArray::fromHex("fffe");
    const QString text = QStringLiteral("你好");
    for (const QChar c : text)
        data.append(char(c.unicode() & 0xff)).append(char(c.unicode() >> 8));
    const DecodeResult r = TextCodec::decode(data);
    QVERIFY(r.ok);
    QCOMPARE(r.encoding, TextEncoding::Utf16LE);
    QCOMPARE(r.text, text);
}

void TestTextCodec::utf16be()
{
    QByteArray data = QByteArray::fromHex("feff");
    const QString text = QStringLiteral("你好");
    for (const QChar c : text)
        data.append(char(c.unicode() >> 8)).append(char(c.unicode() & 0xff));
    const DecodeResult r = TextCodec::decode(data);
    QVERIFY(r.ok);
    QCOMPARE(r.encoding, TextEncoding::Utf16BE);
    QCOMPARE(r.text, text);
}

void TestTextCodec::gb18030()
{
    // "第一章 测试" 的 GBK 字节
    const QByteArray bytes = QByteArray::fromHex("b5dad2bbd5c220b2e2cad4");
    const DecodeResult r = TextCodec::decode(bytes);
    QVERIFY(r.ok);
    QCOMPARE(r.encoding, TextEncoding::Gb18030);
    QCOMPARE(r.text, QStringLiteral("第一章 测试"));
}

void TestTextCodec::asciiIsUtf8()
{
    const QByteArray bytes = "hello world\n";
    const DecodeResult r = TextCodec::decode(bytes);
    QVERIFY(r.ok);
    QCOMPARE(r.encoding, TextEncoding::Utf8);
    QCOMPARE(r.text, QStringLiteral("hello world\n"));
}

QTEST_APPLESS_MAIN(TestTextCodec)
#include "tst_textcodec.moc"
