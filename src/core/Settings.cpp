#include "core/Settings.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
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
    keyset.load(doc.object().value(QStringLiteral("keys")).toObject());
    behavior = readBehavior(doc.object().value(QStringLiteral("behavior")).toObject());
    tags = readTags(doc.object().value(QStringLiteral("tags")).toArray());
    chapterRegex = doc.object().value(QStringLiteral("advanced"))
                       .toObject()
                       .value(QStringLiteral("chapter_regex"))
                       .toString();
}

void Settings::save() const
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    QJsonObject root;
    root.insert(QStringLiteral("display"), writeDisplay());
    root.insert(QStringLiteral("keys"), keyset.save());
    root.insert(QStringLiteral("behavior"), writeBehavior());
    root.insert(QStringLiteral("tags"), writeTags());
    QJsonObject advanced;
    advanced.insert(QStringLiteral("chapter_regex"), chapterRegex);
    root.insert(QStringLiteral("advanced"), advanced);
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.commit();
}

static int intOr(const QJsonObject &o, const char *key, int fallback)
{
    const QString k = QLatin1String(key);
    return o.contains(k) ? o.value(k).toInt(fallback) : fallback;
}

static bool boolOr(const QJsonObject &o, const char *key, bool fallback)
{
    const QString k = QLatin1String(key);
    return o.contains(k) ? o.value(k).toBool() : fallback;
}

BehaviorSettings Settings::readBehavior(const QJsonObject &o)
{
    BehaviorSettings b;
    b.autoPageIntervalMs = intOr(o, "auto_page_interval_ms", b.autoPageIntervalMs);
    b.autoPageScrollMode = boolOr(o, "auto_page_scroll_mode", b.autoPageScrollMode);
    b.scrollStep = intOr(o, "scroll_step", b.scrollStep);
    b.minimizeToTray = boolOr(o, "minimize_to_tray", b.minimizeToTray);
    b.doubleClickHide = boolOr(o, "double_click_hide", b.doubleClickHide);
    b.globalHideEnabled = boolOr(o, "global_hide_enabled", b.globalHideEnabled);
    b.globalHidePopup = boolOr(o, "global_hide_popup", b.globalHidePopup);
    return b;
}

QJsonObject Settings::writeBehavior() const
{
    QJsonObject o;
    o.insert(QStringLiteral("auto_page_interval_ms"), behavior.autoPageIntervalMs);
    o.insert(QStringLiteral("auto_page_scroll_mode"), behavior.autoPageScrollMode);
    o.insert(QStringLiteral("scroll_step"), behavior.scrollStep);
    o.insert(QStringLiteral("minimize_to_tray"), behavior.minimizeToTray);
    o.insert(QStringLiteral("double_click_hide"), behavior.doubleClickHide);
    o.insert(QStringLiteral("global_hide_enabled"), behavior.globalHideEnabled);
    o.insert(QStringLiteral("global_hide_popup"), behavior.globalHidePopup);
    return o;
}

QVector<TagItem> Settings::readTags(const QJsonArray &a)
{
    QVector<TagItem> out;
    for (const QJsonValue &v : a) {
        const QJsonObject o = v.toObject();
        TagItem t;
        t.keyword = o.value(QStringLiteral("keyword")).toString();
        t.fg = QColor(o.value(QStringLiteral("fg")).toString());
        t.bg = QColor(o.value(QStringLiteral("bg")).toString());
        if (o.contains(QStringLiteral("enabled")))
            t.enabled = o.value(QStringLiteral("enabled")).toBool();
        if (!t.keyword.isEmpty())
            out.append(t);
    }
    return out;
}

QJsonArray Settings::writeTags() const
{
    QJsonArray a;
    for (const TagItem &t : tags) {
        QJsonObject o;
        o.insert(QStringLiteral("keyword"), t.keyword);
        o.insert(QStringLiteral("fg"), t.fg.isValid() ? t.fg.name() : QString());
        o.insert(QStringLiteral("bg"), t.bg.isValid() ? t.bg.name() : QString());
        o.insert(QStringLiteral("enabled"), t.enabled);
        a.append(o);
    }
    return a;
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
    display.bgImagePath = o.value(QStringLiteral("bg_image")).toString();
    display.windowAlpha = readInt("window_alpha", display.windowAlpha);
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
    o.insert(QStringLiteral("bg_image"), display.bgImagePath);
    o.insert(QStringLiteral("window_alpha"), display.windowAlpha);
    return o;
}

}
