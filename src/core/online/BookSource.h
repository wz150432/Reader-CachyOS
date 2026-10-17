#pragma once
#include <QJsonObject>
namespace reader {
class BookSource {
public:
  explicit BookSource(QJsonObject json = {}) : m_json(json) {}
  QJsonObject toJson() const { return m_json; }
  QString id() const { return m_json.value("id").toString(); }
  bool validate(QString *error = nullptr) const;
  static BookSource preset();
  BookSource copy(const QString &id, const QString &name) const;

private:
  QJsonObject m_json;
};
} // namespace reader
