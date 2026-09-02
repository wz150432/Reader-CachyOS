#pragma once
#include <QMetaType>
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

struct Bookmark
{
    QString filePath;
    int chapterIndex = 0;
    int pageIndex = 0;
    QString title;
    qint64 created = 0;
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
    void clearRecent();
    QVector<Bookmark> bookmarks(const QString &filePath) const;
    void addBookmark(const Bookmark &b);
    void removeBookmark(const QString &filePath, qint64 created);
    static QString defaultCacheFilePath();

private:
    QString m_path;
    QVector<BookProgress> m_progress;
    QVector<Bookmark> m_bookmarks;
};

}

Q_DECLARE_METATYPE(reader::Bookmark)
