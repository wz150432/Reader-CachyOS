#pragma once
#include "core/online/BookSourceStore.h"
#include <QDialog>
#include <QJsonArray>
#include <memory>
class QComboBox;
class QLineEdit;
class QLabel;
class QListWidget;
class QPushButton;
namespace reader {
class OnlineEngineBridge;
class OnlineBook;
class OnlineLibraryDialog : public QDialog {
  Q_OBJECT

public:
  OnlineLibraryDialog(BookSourceStore *, OnlineEngineBridge *,
                      QString cacheRoot, QWidget *parent = nullptr);
  ~OnlineLibraryDialog() override;
  void refreshCached();
signals:
  void componentRequested();
  void openRequested(const QString &);
  void refreshRequested(const QString &);
  void tocRefreshed(const QString &, const QStringList &, const QStringList &);

private:
  void search(int);
  void toc(const QString &);
  void request(const QString &, const QJsonObject &);
  void stop();
  void receive(const QJsonObject &);
  void reloadSources();
  void showRows(QListWidget *, const QJsonArray &);
  void openSelection();
  BookSourceStore *m_store;
  OnlineEngineBridge *m_bridge;
  QString m_root, m_request, m_session, m_operation, m_url, m_title;
  int m_page = 1;
  bool m_choices = false;
  QComboBox *m_sources;
  QLineEdit *m_keyword, *m_direct;
  QLabel *m_status, *m_pageLabel;
  QListWidget *m_results, *m_toc, *m_cached;
  QPushButton *m_previous, *m_next, *m_open, *m_cancel;
  QJsonArray m_rows, m_chapters;
  BookSource m_source;
  std::shared_ptr<OnlineBook> m_cachedBook;
};
} // namespace reader
