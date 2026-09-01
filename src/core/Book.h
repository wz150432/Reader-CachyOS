#pragma once
#include <QString>
#include <QVector>
#include <memory>
#include "core/ChapterParser.h"

namespace reader {

class Book
{
public:
    virtual ~Book() = default;
    virtual bool open(const QString &filePath, QString *error = nullptr) = 0;
    virtual QString title() const = 0;
    virtual const QVector<Chapter> &chapters() const = 0;
    virtual QString chapterText(int chapterIndex) const = 0;
    static std::shared_ptr<Book> create(const QString &filePath, QString *error = nullptr);
    const QString &filePath() const { return m_filePath; }

protected:
    QString m_filePath;
};

}
