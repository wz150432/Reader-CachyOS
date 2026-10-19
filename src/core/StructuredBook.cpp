#include "core/StructuredBook.h"
#include <QFile>
#include <QFileInfo>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QXmlStreamReader>
#include <mobi.h>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace reader {
namespace {
constexpr qsizetype maxBookBytes = 64 * 1024 * 1024;

}

void StructuredBook::addText(const QString &text)
{
    const QString clean = text.trimmed();
    if (clean.isEmpty())
        return;
    if (m_chapters.isEmpty())
        addHeading(m_title);
    m_text += clean + QLatin1Char('\n');
}

void StructuredBook::addHeading(const QString &heading)
{
    const QString title = heading.simplified();
    if (!m_chapters.isEmpty() && m_chapters.last().charOffset == m_text.size())
        m_chapters.last().title = title;
    else
        m_chapters.append({title.isEmpty() ? m_title : title, int(m_text.size())});
}

QString StructuredBook::chapterText(int index) const
{
    if (index < 0 || index >= m_chapters.size())
        return {};
    const int start = m_chapters[index].charOffset;
    const int end = index + 1 < m_chapters.size() ? m_chapters[index + 1].charOffset : m_text.size();
    return m_text.mid(start, end - start);
}

bool StructuredBook::readFb2(const QString &path, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }
    QXmlStreamReader xml(&file);
    QStringList elements;
    bool body = false;
    int bodyDepth = 0;
    bool foundRoot = false;
    QString paragraph;
    QString heading;
    bool inTitle = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            if (elements.isEmpty()) {
                if (name != QStringLiteral("FictionBook")) {
                    error = QStringLiteral("不是有效的 FB2 文件");
                    return false;
                }
                foundRoot = true;
            }
            elements.append(name);
            if (name == "binary" || (name == "body" && xml.attributes().value("name") == "notes")) {
                xml.skipCurrentElement();
                elements.removeLast();
                continue;
            }
            if (!body && name == "book-title") {
                const QString title = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
                if (!title.isEmpty()) m_title = title;
                elements.removeLast();
                continue;
            }
            if (name == "body") {
                body = true;
                bodyDepth = elements.size();
            }
            if (body && name == "title") {
                inTitle = true;
                heading.clear();
            }
            if (body && (name == "p" || name == "v" || name == "subtitle"))
                paragraph.clear();
        } else if (xml.isCharacters() && body) {
            if (elements.contains(QStringLiteral("p")) || elements.contains(QStringLiteral("v"))
                || elements.contains(QStringLiteral("subtitle")))
                paragraph += xml.text();
        } else if (xml.isEndElement()) {
            const QString name = xml.name().toString();
            if (body && (name == "p" || name == "v" || name == "subtitle")) {
                if (inTitle) heading += paragraph + QLatin1Char(' ');
                else addText(paragraph);
                paragraph.clear();
            }
            if (body && name == "title") {
                inTitle = false;
                addHeading(heading);
                addText(heading);
            }
            if (body && elements.size() == bodyDepth && name == "body")
                body = false;
            if (!elements.isEmpty()) elements.removeLast();
        }
        if (m_text.size() > maxBookBytes) {
            error = QStringLiteral("FB2 正文过大");
            return false;
        }
    }
    if (xml.hasError() || !foundRoot) {
        error = QStringLiteral("FB2 文件损坏：%1").arg(xml.errorString());
        return false;
    }
    return true;
}

bool StructuredBook::readKindle(const QString &path, QString &error)
{
    std::unique_ptr<MOBIData, decltype(&mobi_free)> data(mobi_init(), mobi_free);
    const auto closeFile = [](FILE *file) { std::fclose(file); };
    std::unique_ptr<FILE, decltype(closeFile)> file(
        std::fopen(QFile::encodeName(path).constData(), "rb"), closeFile);
    if (!data || !file) {
        error = QStringLiteral("无法读取 Kindle 文件");
        return false;
    }
    const MOBI_RET loaded = mobi_load_file(data.get(), file.get());
    if (loaded != MOBI_SUCCESS) {
        error = QStringLiteral("Kindle 文件损坏或格式无效（%1）").arg(int(loaded));
        return false;
    }
    if (mobi_is_encrypted(data.get())) {
        error = QStringLiteral("该电子书受 DRM 保护，请使用未加密的 MOBI/AZW3 文件");
        return false;
    }
    const size_t size = mobi_get_text_maxsize(data.get());
    if (size == MOBI_NOTSET || size > size_t(maxBookBytes)) {
        error = QStringLiteral("Kindle 正文过大或长度无效");
        return false;
    }
    std::unique_ptr<char, decltype(&std::free)> title(mobi_meta_get_title(data.get()), std::free);
    if (title && *title) m_title = QString::fromUtf8(title.get());
    std::unique_ptr<MOBIRawml, decltype(&mobi_free_rawml)> raw(mobi_init_rawml(data.get()), mobi_free_rawml);
    if (!raw || mobi_parse_rawml_opt(raw.get(), data.get(), false, false, false) != MOBI_SUCCESS) {
        error = QStringLiteral("无法解析 Kindle 正文");
        return false;
    }
    for (MOBIPart *part = raw->markup; part; part = part->next) {
        if (part->size > size_t(maxBookBytes) || m_text.size() > maxBookBytes) {
            error = QStringLiteral("Kindle 正文过大");
            return false;
        }
        const QByteArray bytes(reinterpret_cast<const char *>(part->data), qsizetype(part->size));
        // libmobi normalizes legacy encodings to UTF-8 during rawml parsing.
        const QString html = QString::fromUtf8(bytes);
        QTextDocument document;
        document.setHtml(html);
        for (auto block = document.begin(); block.isValid(); block = block.next()) {
            QString text = block.text();
            text.remove(QChar::ObjectReplacementCharacter);
            if (text.trimmed().isEmpty()) continue;
            if (block.blockFormat().headingLevel() > 0)
                addHeading(text);
            addText(text);
        }
    }
    return true;
}

bool StructuredBook::open(const QString &path, QString *error, const QRegularExpression &chapterRegex)
{
    m_text.clear();
    m_chapters.clear();
    m_title = QFileInfo(path).completeBaseName();
    QString failure;
    if (QFileInfo(path).size() > maxBookBytes)
        failure = QStringLiteral("电子书文件过大（上限 64 MiB）");
    else if (!(QFileInfo(path).suffix().compare("fb2", Qt::CaseInsensitive) == 0
                   ? readFb2(path, failure) : readKindle(path, failure))) {
        // The importer supplies a specific parse error.
    } else if (m_text.trimmed().isEmpty())
        failure = QStringLiteral("电子书不包含可阅读的文字");
    if (!failure.isEmpty()) {
        m_text.clear();
        m_chapters.clear();
        if (error) *error = failure;
        return false;
    }
    // Prefer embedded FB2/HTML headings. Fall back to the existing text chapter parser.
    if (m_chapters.size() <= 1 || !chapterRegex.pattern().isEmpty()) {
        const auto chapters = chapterRegex.isValid() && !chapterRegex.pattern().isEmpty()
            ? ChapterParser::parseRegex(m_text, chapterRegex) : ChapterParser::parseDefault(m_text);
        if (!chapters.isEmpty()) {
            m_chapters = chapters;
            if (m_chapters.first().charOffset > 0)
                m_chapters.prepend({m_title, 0});
        }
    }
    m_filePath = path;
    return true;
}
}
