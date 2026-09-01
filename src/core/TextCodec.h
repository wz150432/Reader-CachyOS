#pragma once
#include <QByteArray>
#include <QString>

namespace reader {

enum class TextEncoding { Utf8, Utf8Bom, Utf16LE, Utf16BE, Gb18030, Unknown };

struct DecodeResult
{
    TextEncoding encoding = TextEncoding::Unknown;
    QString text;
    bool ok = false;
};

class TextCodec
{
public:
    static TextEncoding detect(const QByteArray &bytes);
    static DecodeResult decode(const QByteArray &bytes);
    static DecodeResult decode(const QByteArray &bytes, TextEncoding encoding);
    static QString encodingName(TextEncoding encoding);
};

}
