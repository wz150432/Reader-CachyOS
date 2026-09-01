#pragma once
#include <QColor>
#include <QFont>
#include <QJsonObject>
#include <QString>
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

class Settings
{
public:
    explicit Settings(const QString &configFilePath = QString());
    DisplaySettings display;
    Keyset keyset;
    void load();
    void save() const;
    static QString defaultConfigFilePath();

private:
    void readDisplay(const QJsonObject &o);
    QJsonObject writeDisplay() const;
    QString m_path;
};

}
