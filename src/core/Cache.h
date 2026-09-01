#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace reader {

struct BookProgress
{
    QString filePath;
    int chapterIndex = 0;
    int pageIndex = 0;
    qint64 lastOpened = 0;
};

class Cache
{
public:
    explicit Cache(const QString &cacheFilePath = QString());
    void load();
    void save() const;
    void upsertProgress(const BookProgress &p);
    std::optional<BookProgress> progress(const QString &filePath) const;
    QStringList recentFiles() const;
    void clearAll();
    static QString defaultCacheFilePath();

private:
    QString m_path;
    QVector<BookProgress> m_progress;
};

}
