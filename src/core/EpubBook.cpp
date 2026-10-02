#include "core/EpubBook.h"
#include <archive.h>
#include <archive_entry.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QTextDocument>
#include <QUrl>
#include <QXmlStreamReader>
#include <limits>

namespace reader {
namespace {
// Resolve publication URLs inside the archive; never extract or fetch resources.
QString resolve(const QString &base, const QString &href)
{
    const QUrl url(href);
    if (!url.isRelative() || href.startsWith('/') || href.contains('\\'))
        return {};
    const QString path = QDir::cleanPath(QFileInfo(base).path() + '/' + url.path(QUrl::FullyDecoded));
    if (path == ".." || path.startsWith("../"))
        return {};
    return path;
}

bool readArchive(const QString &path, QHash<QString, QByteArray> &files, QString &error)
{
    std::unique_ptr<archive, decltype(&archive_read_free)> input(archive_read_new(), archive_read_free);
    archive_read_support_format_zip(input.get());
    if (archive_read_open_filename(input.get(), QFile::encodeName(path).constData(), 10240) != ARCHIVE_OK) {
        error = QStringLiteral("无法读取 EPUB：%1").arg(QString::fromUtf8(archive_error_string(input.get())));
        return false;
    }
    constexpr qint64 maxEntry = 32 * 1024 * 1024;
    constexpr qint64 maxTotal = 256 * 1024 * 1024;
    qint64 total = 0;
    archive_entry *entry = nullptr;
    int status;
    while ((status = archive_read_next_header(input.get(), &entry)) == ARCHIVE_OK) {
        if (archive_entry_filetype(entry) != AE_IFREG)
            continue;
        const char *name = archive_entry_pathname_utf8(entry);
        if (!name) name = archive_entry_pathname(entry);
        const QString key = QDir::cleanPath(QString::fromUtf8(name));
        if (key.startsWith('/') || key == ".." || key.startsWith("../") || files.contains(key)) {
            error = QStringLiteral("EPUB 包含无效或重复的文件路径");
            return false;
        }
        QByteArray bytes;
        char buffer[16384];
        la_ssize_t count;
        while ((count = archive_read_data(input.get(), buffer, sizeof(buffer))) > 0) {
            total += count;
            if (bytes.size() + count > maxEntry || total > maxTotal) {
                error = QStringLiteral("EPUB 解压后内容过大");
                return false;
            }
            bytes.append(buffer, count);
        }
        if (count < 0) {
            error = QStringLiteral("EPUB 内容损坏或已加密");
            return false;
        }
        files.insert(key, bytes);
    }
    if (status != ARCHIVE_EOF) {
        error = QStringLiteral("EPUB 压缩包损坏");
        return false;
    }
    return true;
}

class HtmlEntities : public QXmlStreamEntityResolver {
    QString resolveUndeclaredEntity(const QString &name) override {
        QTextDocument document;
        document.setHtml('&' + name + ';');
        return document.toPlainText();
    }
};

bool bodyText(const QByteArray &bytes, QString &text, QString &heading)
{
    QXmlStreamReader xml(bytes);
    HtmlEntities entities;
    xml.setEntityResolver(&entities);
    bool body = false;
    bool inHeading = false;
    const QSet<QString> blocks = {"p", "div", "h1", "h2", "h3", "h4", "h5", "h6", "li", "blockquote", "pre", "section", "tr"};
    while (!xml.atEnd()) {
        xml.readNext();
        const QString name = xml.name().toString().toLower();
        if (xml.isStartElement()) {
            if (name == "body") body = true;
            if (!body) continue;
            if (name == "script" || name == "style") {
                xml.skipCurrentElement();
                continue;
            }
            if (blocks.contains(name) || name == "br") text += '\n';
            if (heading.isEmpty() && (name == "h1" || name == "h2")) inHeading = true;
        } else if (xml.isCharacters() && body) {
            QString chunk = xml.text().toString();
            chunk.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
            text += chunk;
            if (inHeading) heading += chunk;
        } else if (xml.isEndElement() && body) {
            if (blocks.contains(name)) text += '\n';
            if (name == "h1" || name == "h2") inHeading = false;
            if (name == "body") body = false;
        }
    }
    QStringList lines;
    for (const QString &line : text.split('\n'))
        if (!line.trimmed().isEmpty()) lines.append(line.trimmed());
    text = lines.join('\n');
    heading = heading.simplified();
    return !xml.hasError();
}

QHash<QString, QString> navigation(const QByteArray &bytes, const QString &path, bool ncx)
{
    QHash<QString, QString> labels;
    QXmlStreamReader xml(bytes);
    if (ncx) {
        // A stack preserves parent labels when NCX navPoints are nested.
        QVector<QString> stack;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                if (xml.name() == u"navPoint") stack.append(QString());
                else if (xml.name() == u"text" && !stack.isEmpty())
                    stack.last() = xml.readElementText().simplified();
                else if (xml.name() == u"content" && !stack.isEmpty()) {
                    const QString target = resolve(path, xml.attributes().value("src").toString());
                    if (!target.isEmpty() && !stack.last().isEmpty() && !labels.contains(target))
                        labels.insert(target, stack.last());
                }
            } else if (xml.isEndElement() && xml.name() == u"navPoint" && !stack.isEmpty())
                stack.removeLast();
        }
    } else {
        bool toc = false;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == u"nav") {
                const QString type = xml.attributes().value("http://www.idpf.org/2007/ops", "type").toString();
                toc = type.split(' ').contains("toc");
            } else if (toc && xml.isStartElement() && xml.name() == u"a") {
                const QString target = resolve(path, xml.attributes().value("href").toString());
                const QString label = xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
                if (!target.isEmpty() && !label.isEmpty() && !labels.contains(target)) labels.insert(target, label);
            } else if (xml.isEndElement() && xml.name() == u"nav") toc = false;
        }
    }
    return xml.hasError() ? QHash<QString, QString>() : labels;
}
}

bool EpubBook::open(const QString &filePath, QString *error, const QRegularExpression &)
{
    auto fail = [&](const QString &message) { if (error) *error = message; return false; };
    QHash<QString, QByteArray> files;
    QString archiveError;
    if (!readArchive(filePath, files, archiveError)) return fail(archiveError);
    QXmlStreamReader container(files.value("META-INF/container.xml"));
    QString packagePath;
    while (!container.atEnd()) {
        container.readNext();
        if (container.isStartElement() && container.name() == u"rootfile" && packagePath.isEmpty())
            packagePath = QDir::cleanPath(container.attributes().value("full-path").toString());
    }
    if (container.hasError() || !files.contains(packagePath))
        return fail(QStringLiteral("EPUB 缺少有效的 container.xml 或书籍描述文件"));

    struct Item { QString path; QString type; };
    QHash<QString, Item> manifest;
    QStringList spine;
    QString title, navPath, ncxId;
    QXmlStreamReader package(files.value(packagePath));
    while (!package.atEnd()) {
        package.readNext();
        if (!package.isStartElement()) continue;
        const auto attrs = package.attributes();
        if (package.name() == u"title" && title.isEmpty()) title = package.readElementText().simplified();
        else if (package.name() == u"item") {
            const QString path = resolve(packagePath, attrs.value("href").toString());
            manifest.insert(attrs.value("id").toString(), {path, attrs.value("media-type").toString()});
            if (attrs.value("properties").toString().split(' ').contains("nav")) navPath = path;
        } else if (package.name() == u"spine") ncxId = attrs.value("toc").toString();
        else if (package.name() == u"itemref" && attrs.value("linear") != u"no")
            spine.append(attrs.value("idref").toString());
    }
    if (package.hasError() || spine.isEmpty()) return fail(QStringLiteral("EPUB 缺少有效的阅读顺序"));
    auto labels = navigation(files.value(navPath), navPath, false);
    if (labels.isEmpty()) {
        const QString ncxPath = manifest.value(ncxId).path;
        labels = navigation(files.value(ncxPath), ncxPath, true);
    }
    QVector<Chapter> chapters;
    QVector<QString> texts;
    qint64 total = 0;
    for (const QString &id : spine) {
        const Item item = manifest.value(id);
        if (item.path.isEmpty() || !files.contains(item.path)) return fail(QStringLiteral("EPUB 章节文件缺失：%1").arg(id));
        if (item.type.startsWith("image/")) continue;
        if (item.type != "application/xhtml+xml" && item.type != "text/html")
            return fail(QStringLiteral("EPUB 包含不支持的章节格式：%1").arg(item.type));
        QString text, heading;
        if (!bodyText(files.value(item.path), text, heading)) return fail(QStringLiteral("EPUB 章节内容损坏：%1").arg(item.path));
        if (text.isEmpty()) continue; // Image-only covers do not create empty pages.
        if (total + text.size() > std::numeric_limits<int>::max()) return fail(QStringLiteral("EPUB 正文过大"));
        QString label = labels.value(item.path);
        if (label.isEmpty()) label = heading;
        if (label.isEmpty()) label = QFileInfo(item.path).completeBaseName();
        chapters.append({label, int(total)});
        texts.append(text);
        total += text.size();
    }
    if (texts.isEmpty()) return fail(QStringLiteral("EPUB 没有可阅读的文字内容（可能是图片书或加密书籍）"));
    m_filePath = filePath;
    m_title = title.isEmpty() ? QFileInfo(filePath).completeBaseName() : title;
    m_chapters = std::move(chapters);
    m_texts = std::move(texts);
    m_charCount = total;
    if (error) error->clear();
    return true;
}

QString EpubBook::chapterText(int index) const
{
    return index >= 0 && index < m_texts.size() ? m_texts.at(index) : QString();
}
}
