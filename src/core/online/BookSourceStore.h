#pragma once
#include "BookSource.h"
#include <QVector>
namespace reader {
class BookSourceStore {
public:
  explicit BookSourceStore(QString path) : m_path(path) {}
  bool load(QString *error = nullptr);
  bool save(QString *error = nullptr);
  const QVector<BookSource> &sources() const { return m_sources; }
  bool upsert(const BookSource &, QString *error = nullptr);
  bool remove(const QString &id, QString *error = nullptr);

private:
  QString m_path;
  QVector<BookSource> m_sources;
  bool m_corrupt = false;
};
} // namespace reader
