#pragma once
#include "BookSource.h"
#include "core/Book.h"
#include <QJsonArray>
namespace reader {
class OnlineBook : public Book {
public:
  static std::shared_ptr<OnlineBook>
  createManifest(const QString &root, const BookSource &source,
                 const QString &url, const QString &title,
                 const QJsonArray &chapters, QString *error = nullptr);
  bool open(const QString &, QString *error = nullptr,
            const QRegularExpression & = {}) override;
  QString title() const override { return m_title; }
  const QVector<Chapter> &chapters() const override { return m_chapters; }
  QString chapterText(int) const override;
  qint64 totalCharCount() const override { return 0; }
  bool isOnline() const override { return true; }
  bool chapterAvailable(int) const override;
  bool saveChapter(int, const QString &, QString *error = nullptr);
  QString chapterFilePath(int) const;
  QString chapterUrl(int) const;
  BookSource source() const { return m_source; }
  QString url() const { return m_url; }

private:
  QString m_title, m_url;
  BookSource m_source;
  QVector<Chapter> m_chapters;
  QStringList m_urls;
};
} // namespace reader
