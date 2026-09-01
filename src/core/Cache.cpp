#include "core/Cache.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

namespace reader {

Cache::Cache(const QString &cacheFilePath)
    : m_path(cacheFilePath.isEmpty() ? defaultCacheFilePath() : cacheFilePath)
{
}

QString Cache::defaultCacheFilePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("cache.json"));
}

void Cache::load()
{
    m_progress.clear();
    QFile f(m_path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    const QJsonArray arr = doc.object().value(QStringLiteral("progress")).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        BookProgress p;
        p.filePath = o.value(QStringLiteral("file")).toString();
        p.chapterIndex = o.value(QStringLiteral("chapter")).toInt();
        p.pageIndex = o.value(QStringLiteral("page")).toInt();
        p.lastOpened = static_cast<qint64>(o.value(QStringLiteral("last_opened")).toDouble());
        if (!p.filePath.isEmpty())
            m_progress.append(p);
    }
}

void Cache::save() const
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    QJsonArray arr;
    for (const BookProgress &p : m_progress) {
        QJsonObject o;
        o.insert(QStringLiteral("file"), p.filePath);
        o.insert(QStringLiteral("chapter"), p.chapterIndex);
        o.insert(QStringLiteral("page"), p.pageIndex);
        o.insert(QStringLiteral("last_opened"), static_cast<double>(p.lastOpened));
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("progress"), arr);
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.commit();
}

void Cache::upsertProgress(const BookProgress &p)
{
    for (BookProgress &item : m_progress) {
        if (item.filePath == p.filePath) {
            item = p;
            return;
        }
    }
    m_progress.append(p);
}

std::optional<BookProgress> Cache::progress(const QString &filePath) const
{
    for (const BookProgress &p : m_progress) {
        if (p.filePath == filePath)
            return p;
    }
    return std::nullopt;
}

QStringList Cache::recentFiles() const
{
    QStringList files;
    for (const BookProgress &p : m_progress)
        files.append(p.filePath);
    std::stable_sort(files.begin(), files.end(),
                     [this](const QString &a, const QString &b) {
                         return progress(a)->lastOpened > progress(b)->lastOpened;
                     });
    return files;
}

void Cache::clearAll()
{
    m_progress.clear();
}

}
