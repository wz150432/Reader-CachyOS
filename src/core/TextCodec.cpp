#include "core/TextCodec.h"
#include <QStringConverter>
#include <QStringDecoder>
#include <iconv.h>
#include <vector>

namespace reader {

static bool looksLikeUtf8(const QByteArray &bytes)
{
    QStringDecoder dec(QStringConverter::Utf8);
    const QString s = dec(bytes);
    return !s.contains(QChar::ReplacementCharacter);
}

static QString decodeGb18030(const QByteArray &bytes)
{
    iconv_t cd = iconv_open("UTF-8", "GB18030");
    if (cd == reinterpret_cast<iconv_t>(-1))
        return QString();
    std::vector<char> outBuf(static_cast<size_t>(bytes.size()) * 4 + 16);
    char *inPtr = const_cast<char *>(bytes.constData());
    size_t inLeft = static_cast<size_t>(bytes.size());
    char *outPtr = outBuf.data();
    size_t outLeft = outBuf.size();
    const size_t rc = iconv(cd, &inPtr, &inLeft, &outPtr, &outLeft);
    iconv_close(cd);
    if (rc == static_cast<size_t>(-1))
        return QString();
    return QString::fromUtf8(outBuf.data(), static_cast<qsizetype>(outBuf.size() - outLeft));
}

TextEncoding TextCodec::detect(const QByteArray &bytes)
{
    if (bytes.startsWith("\xEF\xBB\xBF"))
        return TextEncoding::Utf8Bom;
    if (bytes.startsWith("\xFF\xFE"))
        return TextEncoding::Utf16LE;
    if (bytes.startsWith("\xFE\xFF"))
        return TextEncoding::Utf16BE;
    if (looksLikeUtf8(bytes))
        return TextEncoding::Utf8;
    return TextEncoding::Gb18030;
}

DecodeResult TextCodec::decode(const QByteArray &bytes)
{
    return decode(bytes, detect(bytes));
}

DecodeResult TextCodec::decode(const QByteArray &bytes, TextEncoding encoding)
{
    DecodeResult r;
    r.encoding = encoding;
    switch (encoding) {
    case TextEncoding::Utf8Bom:
        r.text = QStringDecoder(QStringConverter::Utf8)(bytes.mid(3));
        r.ok = true;
        break;
    case TextEncoding::Utf8:
        r.text = QStringDecoder(QStringConverter::Utf8)(bytes);
        r.ok = true;
        break;
    case TextEncoding::Utf16LE:
        r.text = QStringDecoder(QStringConverter::Utf16LE)(bytes.mid(2));
        r.ok = true;
        break;
    case TextEncoding::Utf16BE:
        r.text = QStringDecoder(QStringConverter::Utf16BE)(bytes.mid(2));
        r.ok = true;
        break;
    case TextEncoding::Gb18030: {
        r.text = decodeGb18030(bytes);
        r.ok = !r.text.isEmpty();
        break;
    }
    default:
        break;
    }
    return r;
}

QString TextCodec::encodingName(TextEncoding encoding)
{
    switch (encoding) {
    case TextEncoding::Utf8: return QStringLiteral("UTF-8");
    case TextEncoding::Utf8Bom: return QStringLiteral("UTF-8 BOM");
    case TextEncoding::Utf16LE: return QStringLiteral("UTF-16 LE");
    case TextEncoding::Utf16BE: return QStringLiteral("UTF-16 BE");
    case TextEncoding::Gb18030: return QStringLiteral("GB18030");
    default: return QStringLiteral("未知");
    }
}

}
