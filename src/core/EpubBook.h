#pragma once
#include "core/Book.h"

namespace reader {
class EpubBook final : public Book
{
public:
    bool open(const QString &filePath, QString *error = nullptr,
              const QRegularExpression &chapterRegex = QRegularExpression()) override;
    QString title() const override { return m_title; }
    const QVector<Chapter> &chapters() const override { return m_chapters; }
    QString chapterText(int chapterIndex) const override;
    qint64 totalCharCount() const override { return m_charCount; }
private:
    QString m_title;
    QVector<Chapter> m_chapters;
    QVector<QString> m_texts;
    qint64 m_charCount = 0;
};
}
