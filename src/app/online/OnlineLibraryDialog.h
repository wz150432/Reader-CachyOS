#pragma once
#include "core/online/BookSourceStore.h"
#include "app/MenuPanel.h"
#include <QJsonArray>
#include <QHash>
#include <QSet>
#include <memory>
class QComboBox;
class QLineEdit;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
namespace reader {
class OnlineEngineBridge;
class OnlineBook;
class OnlineLibraryDialog : public MenuPanel {
  Q_OBJECT

public:
  OnlineLibraryDialog(BookSourceStore *, OnlineEngineBridge *,
                      QString cacheRoot, QWidget *parent = nullptr);
  ~OnlineLibraryDialog() override;
  void refreshBook(const QString &path);
  void setRequiredChapterUrl(const QString &url) { m_requiredChapterUrl = url; }
signals:
  void componentRequested();
  void openRequested(const QString &);
  void refreshRequested(const QString &);
  void refreshFailed(const QString &);
  void tocRefreshed(const QString &, const QStringList &, const QStringList &);

protected:
  void hideEvent(QHideEvent *) override;

private:
  void setBusy(bool);
  void selectResult();
  void search();
  void toc(const QString &);
  void request(const QString &, const QJsonObject &);
  void stop();
  void receive(const QJsonObject &);
  void reloadSources();
  void pumpSearch();
  void finishSearch(const QString &, const QJsonObject &, const QString &error = {});
  bool busy() const { return !m_request.isEmpty() || m_searching; }
  void showRows(QListWidget *, const QJsonArray &);
  void openSelection();
  BookSourceStore *m_store;
  OnlineEngineBridge *m_bridge;
  QString m_root, m_request, m_session, m_operation, m_url, m_title;
  QString m_requiredChapterUrl;
  bool m_choices = false;
  QComboBox *m_sources;
  QLineEdit *m_keyword;
  QLabel *m_status, *m_bookTitle;
  QStackedWidget *m_pages;
  QListWidget *m_results, *m_toc;
  QPushButton *m_open, *m_search;
  QJsonArray m_rows, m_chapters;
  BookSource m_source;
  QVector<BookSource> m_searchQueue;
  QHash<QString, BookSource> m_searchRequests;
  QSet<QString> m_searchKeys;
  QStringList m_searchErrors;
  QString m_searchKeyword;
  int m_searchTotal = 0, m_searchDone = 0;
  bool m_searching = false;
  std::shared_ptr<OnlineBook> m_cachedBook;
};
} // namespace reader
