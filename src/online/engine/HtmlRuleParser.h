#pragma once
#include <QJsonObject>
#include <QUrl>
struct ParsedPage {
  QJsonObject result;
  QString next;
  QString error;
};
class HtmlRuleParser {
public:
  static QString validateRules(const QJsonObject &);
  static ParsedPage parse(const QByteArray &, const QUrl &, const QJsonObject &,
                          const QString &);
};
