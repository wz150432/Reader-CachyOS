#pragma once
#include "core/Book.h"

namespace reader {
// Text-oriented import of FB2 and unencrypted Kindle publications.
class StructuredBook final : public Book
{
public:
    bool open(const QString &filePath, QString *error = nullptr,
              const QRegularExpression &chapterRegex = QRegularExpression()) override;
    QString title() const override { return m_title; }
    const QVector<Chapter> &chapters() const override { return m_chapters; }
    QString chapterText(int index) const override;
    qint64 totalCharCount() const override { return m_text.size(); }

private:
    bool readFb2(const QString &path, QString &error);
    bool readKindle(const QString &path, QString &error);
    void addHeading(const QString &heading);
    void addText(const QString &text);
    QString m_title;
    QString m_text;
    QVector<Chapter> m_chapters;
};
}
