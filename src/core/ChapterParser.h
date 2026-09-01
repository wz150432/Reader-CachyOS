#pragma once
#include <QRegularExpression>
#include <QString>
#include <QVector>

namespace reader {

struct Chapter
{
    QString title;
    int charOffset = 0; // 章节在整本书解码文本中的字符偏移
};

class ChapterParser
{
public:
    static QVector<Chapter> parseDefault(const QString &text);
    static QVector<Chapter> parseRegex(const QString &text, const QRegularExpression &re);
};

}
