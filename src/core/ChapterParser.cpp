#include "core/ChapterParser.h"

namespace reader {

static const QString kValidChapterChars =
    QStringLiteral(" \t0123456789零一二三四五六七八九十百千万亿壹贰叁肆伍陆柒捌玖拾佰仟萬億两\u3000");

static bool isChapterNumber(const QString &s)
{
    if (s.isEmpty())
        return false;
    for (const QChar c : s) {
        if (!kValidChapterChars.contains(c))
            return false;
    }
    return true;
}

static bool isSep(const QChar &c)
{
    return c == QChar(0x20) || c == QChar(0x09) || c == QChar(0x3000) || c == QChar(0xA0)
        || c == QChar(0xFF1A) || c == QChar(0x3A);
}

QVector<Chapter> ChapterParser::parseDefault(const QString &text)
{
    QVector<Chapter> chapters;
    const QStringList lines = text.split(QLatin1Char('\n'));
    int offset = 0;
    for (const QString &line : lines) {
        int idx1 = -1;
        int idx2 = -1;
        bool found = false;
        for (int i = 0; i < line.size(); ++i) {
            if (line.at(i) == QChar(0x7B2C)) // 第
                idx1 = i;
            const QChar next = (i + 1 < line.size()) ? line.at(i + 1) : QChar();
            if (idx1 >= 0 && (next.isNull() || isSep(next))) {
                const QChar c = line.at(i);
                if (c == QChar(0x5377) || c == QChar(0x7AE0) || c == QChar(0x90E8) || c == QChar(0x8282)) {
                    idx2 = i;
                    found = true;
                    break;
                }
            }
            // 楔子 / 序章（行内独立词，后跟分隔符或行尾）
            if (idx1 == -1 && i + 1 < line.size()
                && line.at(i) == QChar(0x6954) && line.at(i + 1) == QChar(0x5B50)) { // 楔子
                const bool rest = (i + 2 == line.size()) || isSep(line.at(i + 2));
                if (rest) {
                    idx1 = i;
                    idx2 = line.size() - 1;
                    found = true;
                    break;
                }
            }
            if (idx1 == -1 && i + 1 < line.size()
                && line.at(i) == QChar(0x5E8F) && line.at(i + 1) == QChar(0x7AE0)) { // 序章
                const bool rest = (i + 2 == line.size()) || isSep(line.at(i + 2));
                if (rest) {
                    idx1 = i;
                    idx2 = line.size() - 1;
                    found = true;
                    break;
                }
            }
        }
        bool isChapter = false;
        if (found) {
            if (line.at(idx1) == QChar(0x6954) || line.at(idx1) == QChar(0x5E8F)) { // 楔 / 序
                isChapter = true;
            } else if (isChapterNumber(line.mid(idx1 + 1, idx2 - idx1 - 1))) {
                isChapter = true;
            }
        }
        if (isChapter) {
            Chapter ch;
            ch.title = line.mid(idx1);
            ch.charOffset = offset + idx1;
            chapters.append(ch);
        }
        offset += line.size() + 1; // +1 对应换行符
    }
    return chapters;
}

QVector<Chapter> ChapterParser::parseRegex(const QString &text, const QRegularExpression &re)
{
    QVector<Chapter> chapters;
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        Chapter ch;
        ch.title = m.captured(0).trimmed();
        ch.charOffset = m.capturedStart(0);
        chapters.append(ch);
    }
    return chapters;
}

}
