#include "core/TextBook.h"
#include <QFile>
#include <QFileInfo>

namespace reader {

bool TextBook::open(const QString &filePath, QString *error,
                    const QRegularExpression &chapterRegex)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    const QByteArray bytes = f.readAll();
    const DecodeResult r = TextCodec::decode(bytes);
    if (!r.ok) {
        if (error)
            *error = QStringLiteral("无法识别文件编码");
        return false;
    }
    m_filePath = filePath;
    m_encoding = r.encoding;
    m_text = r.text;
    m_chapters = (chapterRegex.isValid() && !chapterRegex.pattern().isEmpty())
        ? ChapterParser::parseRegex(m_text, chapterRegex)
        : ChapterParser::parseDefault(m_text);
    if (m_chapters.isEmpty()) {
        Chapter ch;
        ch.title = QFileInfo(filePath).completeBaseName();
        ch.charOffset = 0;
        m_chapters.append(ch);
    }
    return true;
}

QString TextBook::title() const
{
    return QFileInfo(m_filePath).completeBaseName();
}

QString TextBook::chapterText(int chapterIndex) const
{
    if (m_chapters.isEmpty())
        return m_text;
    if (chapterIndex < 0 || chapterIndex >= m_chapters.size())
        return QString();
    const int start = m_chapters.at(chapterIndex).charOffset;
    const int end = (chapterIndex + 1 < m_chapters.size())
        ? m_chapters.at(chapterIndex + 1).charOffset
        : m_text.size();
    return m_text.mid(start, end - start);
}

}
