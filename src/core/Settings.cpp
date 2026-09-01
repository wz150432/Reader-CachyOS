#include "core/Settings.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace reader {

Settings::Settings(const QString &configFilePath)
    : m_path(configFilePath.isEmpty() ? defaultConfigFilePath() : configFilePath)
{
}

QString Settings::defaultConfigFilePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("Reader/config.json"));
}

void Settings::load()
{
    QFile f(m_path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    readDisplay(doc.object().value(QStringLiteral("display")).toObject());
}

void Settings::save() const
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    QJsonObject root;
    root.insert(QStringLiteral("display"), writeDisplay());
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.commit();
}

void Settings::readDisplay(const QJsonObject &o)
{
    auto readBool = [&o](const char *key, bool fallback) {
        return o.contains(QLatin1String(key)) ? o.value(QLatin1String(key)).toBool() : fallback;
    };
    auto readInt = [&o](const char *key, int fallback) {
        return o.contains(QLatin1String(key)) ? o.value(QLatin1String(key)).toInt() : fallback;
    };
    auto readColor = [&o](const char *key, const QColor &fallback) {
        return o.contains(QLatin1String(key))
            ? QColor(o.value(QLatin1String(key)).toString())
            : fallback;
    };
    const QString fam = o.value(QStringLiteral("font_family")).toString();
    const QString titleFam = o.value(QStringLiteral("title_font_family")).toString();
    if (!fam.isEmpty())
        display.font.setFamily(fam);
    if (!titleFam.isEmpty())
        display.titleFont.setFamily(titleFam);
    if (o.contains(QStringLiteral("font_size")))
        display.font.setPointSize(o.value(QStringLiteral("font_size")).toInt());
    if (o.contains(QStringLiteral("title_font_size")))
        display.titleFont.setPointSize(o.value(QStringLiteral("title_font_size")).toInt());
    display.useSameFont = readBool("use_same_font", display.useSameFont);
    display.textColor = readColor("text_color", display.textColor);
    display.bgColor = readColor("bg_color", display.bgColor);
    display.lineGap = readInt("line_gap", display.lineGap);
    display.paragraphGap = readInt("paragraph_gap", display.paragraphGap);
    display.firstLineIndent = readBool("first_line_indent", display.firstLineIndent);
    display.compressBlankLines = readBool("compress_blank_lines", display.compressBlankLines);
    display.chapterPageBreak = readBool("chapter_page_break", display.chapterPageBreak);
    display.wordWrap = readBool("word_wrap", display.wordWrap);
    display.margin = readInt("margin", display.margin);
}

QJsonObject Settings::writeDisplay() const
{
    QJsonObject o;
    o.insert(QStringLiteral("font_family"), display.font.family());
    o.insert(QStringLiteral("font_size"), display.font.pointSize());
    o.insert(QStringLiteral("title_font_family"), display.titleFont.family());
    o.insert(QStringLiteral("title_font_size"), display.titleFont.pointSize());
    o.insert(QStringLiteral("use_same_font"), display.useSameFont);
    o.insert(QStringLiteral("text_color"), display.textColor.name());
    o.insert(QStringLiteral("bg_color"), display.bgColor.name());
    o.insert(QStringLiteral("line_gap"), display.lineGap);
    o.insert(QStringLiteral("paragraph_gap"), display.paragraphGap);
    o.insert(QStringLiteral("first_line_indent"), display.firstLineIndent);
    o.insert(QStringLiteral("compress_blank_lines"), display.compressBlankLines);
    o.insert(QStringLiteral("chapter_page_break"), display.chapterPageBreak);
    o.insert(QStringLiteral("word_wrap"), display.wordWrap);
    o.insert(QStringLiteral("margin"), display.margin);
    return o;
}

}
