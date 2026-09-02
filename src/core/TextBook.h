#pragma once
#include "core/Book.h"
#include "core/TextCodec.h"
#include <QRegularExpression>

namespace reader {

class TextBook final : public Book
{
public:
    bool open(const QString &filePath, QString *error = nullptr,
              const QRegularExpression &chapterRegex = QRegularExpression()) override;
    QString title() const override;
    const QVector<Chapter> &chapters() const override { return m_chapters; }
    QString chapterText(int chapterIndex) const override;
    qint64 totalCharCount() const override { return m_text.size(); }
    TextEncoding encoding() const { return m_encoding; }

private:
    QString m_text;
    QVector<Chapter> m_chapters;
    TextEncoding m_encoding = TextEncoding::Unknown;
};

}
