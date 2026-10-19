#pragma once
#include "core/online/BookSourceStore.h"
#include <QDialog>
#include <QJsonArray>
#include <QMap>
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QCheckBox;
class QSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
namespace reader {
class OnlineEngineBridge;
class BookSourceDialog : public QDialog {
  Q_OBJECT
public:
  BookSourceDialog(BookSourceStore *, OnlineEngineBridge *,
                   QWidget *parent = nullptr);
  ~BookSourceDialog() override;
signals:
  void sourcesChanged();

private:
  void loadSource();
  void saveSource();
  void commitSource();
  void test(const QString &);
  void stopTest();
  void showResult(const QJsonObject &);
  BookSourceStore *m_store;
  OnlineEngineBridge *m_bridge;
  QComboBox *m_sources;
  QComboBox *m_kind, *m_method, *m_encoding;
  QMap<QString, QLineEdit *> m_fields;
  QPlainTextEdit *m_headers, *m_preview;
  QCheckBox *m_enabled, *m_reverse;
  QSpinBox *m_interval;
  QLabel *m_error;
  QListWidget *m_testItems;
  QLineEdit *m_testInput;
  QPushButton *m_testChapter;
  QJsonArray m_testRows;
  QString m_request, m_operation, m_session;
  bool m_choices = false;
  bool m_readOnly = false;
  QJsonObject m_saveCandidate;
  BookSource formSource(QString *error = nullptr) const;
};
} // namespace reader
