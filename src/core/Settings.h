#pragma once
#include <QColor>
#include <QFont>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include "core/Keyset.h"

namespace reader {

struct DisplaySettings
{
    QFont font{QStringLiteral("Noto Sans CJK SC"), 12};
    QFont titleFont{QStringLiteral("Noto Sans CJK SC"), 15};
    bool useSameFont = false;
    QColor textColor{QColor(0x33, 0x33, 0x33)};
    QColor bgColor{Qt::white};
    int lineGap = 4;
    int paragraphGap = 8;
    bool firstLineIndent = true;
    bool compressBlankLines = false;
    bool chapterPageBreak = false;
    bool wordWrap = true;
    int margin = 24;
    QString bgImagePath;
    int windowAlpha = 255;
};

struct BehaviorSettings
{
    int autoPageIntervalMs = 3000;
    bool autoPageScrollMode = false;
    int scrollStep = 1;
    bool minimizeToTray = false;
    bool doubleClickHide = true;
};

struct TagItem
{
    QString keyword;
    QColor fg;
    QColor bg;
    bool enabled = true;
};

class Settings
{
public:
    explicit Settings(const QString &configFilePath = QString());
    DisplaySettings display;
    Keyset keyset;
    BehaviorSettings behavior;
    QVector<TagItem> tags;
    void load();
    void save() const;
    static QString defaultConfigFilePath();

private:
    void readDisplay(const QJsonObject &o);
    QJsonObject writeDisplay() const;
    static BehaviorSettings readBehavior(const QJsonObject &o);
    QJsonObject writeBehavior() const;
    static QVector<TagItem> readTags(const QJsonArray &a);
    QJsonArray writeTags() const;
    QString m_path;
};

}
