#include "OnlineLibraryDialog.h"
#include "BookSourceDialog.h"
#include "app/ComboBox.h"
#include "app/UiStyle.h"
#include "core/Cache.h"
#include "core/online/OnlineBook.h"
#include "core/online/OnlineEngineBridge.h"
#include <QComboBox>
#include <algorithm>
#include <QDirIterator>
#include <QHideEvent>
#include <QMenu>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
namespace reader {
namespace {
constexpr int AuthorRole = Qt::UserRole + 1;

int authorColumn(const QListWidget *view) {
  return qRound(view->viewport()->width() * 0.70);
}

class BookResultDelegate final : public QStyledItemDelegate {
public:
  explicit BookResultDelegate(QListWidget *view)
      : QStyledItemDelegate(view), m_view(view) {}

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem background(option);
    initStyleOption(&background, index);
    background.text.clear();
    background.state &= ~QStyle::State_HasFocus;
    m_view->style()->drawControl(QStyle::CE_ItemViewItem, &background, painter,
                                 m_view);
    const int split = authorColumn(m_view);
    const QRect title(option.rect.left() + 12, option.rect.top(),
                      qMax(0, split - option.rect.left() - 24), option.rect.height());
    const QRect author(split + 12, option.rect.top(),
                       qMax(0, option.rect.right() - split - 23), option.rect.height());
    painter->save();
    painter->setClipRect(option.rect);
    painter->setFont(background.font);
    painter->setPen(background.state & QStyle::State_Selected
                        ? QColor("#243d2e") : QColor("#333333"));
    painter->drawText(title, Qt::AlignLeft | Qt::AlignVCenter,
                     background.fontMetrics.elidedText(index.data().toString(),
                                                        Qt::ElideRight, title.width()));
    painter->drawText(author, Qt::AlignLeft | Qt::AlignVCenter,
                     background.fontMetrics.elidedText(index.data(AuthorRole).toString(),
                                                        Qt::ElideRight, author.width()));
    painter->restore();
  }

private:
  QListWidget *m_view;
};

class BookResultsView final : public QListWidget {
public:
  explicit BookResultsView(QWidget *parent) : QListWidget(parent) {
    setItemDelegate(new BookResultDelegate(this));
    setUniformItemSizes(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    QListWidget::paintEvent(event);
    QPainter painter(viewport());
    painter.setPen(QColor("#d5d9d6"));
    const int split = authorColumn(this);
    painter.drawLine(split, 0, split, viewport()->height() - 1);
  }
};
} // namespace

OnlineLibraryDialog::OnlineLibraryDialog(BookSourceStore *s,
                                         OnlineEngineBridge *b, QString root,
                                         QWidget *p)
    : MenuPanel(p), m_store(s), m_bridge(b), m_root(root),
      m_session(QUuid::createUuid().toString()) {
  setWindowTitle(tr("在线阅读"));
  setObjectName("onlineLibraryDialog");
  setStyleSheet(readerUiStyle());
  resize(560, 380);
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 10);
  layout->setSpacing(8);
  auto sources = new QHBoxLayout;
  layout->addLayout(sources);
  m_sources = new ComboBox(this);
  m_sources->setObjectName("librarySources");
  m_sources->setAccessibleName(tr("书源"));
  sources->addWidget(m_sources, 1);
  auto more = new QPushButton(tr("更多"), this);
  more->setObjectName("onlineMore");
  auto menu = new QMenu(more);
  auto manage = menu->addAction(tr("管理书源…"));
  connect(manage, &QAction::triggered, this, [this] {
    auto d = new BookSourceDialog(m_store, m_bridge, this);
    d->setAttribute(Qt::WA_DeleteOnClose);
    connect(d, &BookSourceDialog::sourcesChanged, this,
            &OnlineLibraryDialog::reloadSources);
    hide();
    d->show();
  });
  auto component = menu->addAction(tr("在线组件…"));
  connect(component, &QAction::triggered, this, [this] {
    hide();
    emit componentRequested();
  });
  more->setMenu(menu);
  sources->addWidget(more);
  auto searchRow = new QHBoxLayout;
  layout->addLayout(searchRow);
  m_keyword = new QLineEdit(this);
  m_keyword->setObjectName("onlineKeyword");
  m_keyword->setPlaceholderText(tr("输入书名或作者"));
  m_keyword->setClearButtonEnabled(true);
  searchRow->addWidget(m_keyword, 1);
  m_search = new QPushButton(tr("搜索"), this);
  m_search->setObjectName("searchOnline");
  m_search->setProperty("primaryAction", true);
  searchRow->addWidget(m_search);
  connect(m_search, &QPushButton::clicked, this, [this] {
    if (busy()) {
      stop();
      m_status->setText(tr("已取消"));
    } else {
      search();
    }
  });
  connect(m_keyword, &QLineEdit::returnPressed, m_search, &QPushButton::click);

  m_pages = new QStackedWidget(this);
  layout->addWidget(m_pages, 1);
  auto resultsPage = new QWidget(this);
  auto results = new QVBoxLayout(resultsPage);
  results->setContentsMargins(0, 0, 0, 0);
  results->setSpacing(6);
  m_results = new BookResultsView(resultsPage);
  m_results->setObjectName("onlineResults");
  m_results->setAccessibleName(tr("搜索结果"));
  results->addWidget(m_results);
  m_pages->addWidget(resultsPage);

  auto chaptersPage = new QWidget(this);
  auto chapters = new QVBoxLayout(chaptersPage);
  chapters->setContentsMargins(0, 0, 0, 0);
  chapters->setSpacing(6);
  auto heading = new QHBoxLayout;
  auto back = new QPushButton(tr("返回结果"), this);
  back->setObjectName("backToOnlineResults");
  heading->addWidget(back);
  m_bookTitle = new QLabel(this);
  m_bookTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  heading->addWidget(m_bookTitle, 1);
  chapters->addLayout(heading);
  connect(back, &QPushButton::clicked, this, [this] {
    stop();
    m_pages->setCurrentIndex(0);
    m_status->clear();
  });
  m_toc = new QListWidget(chaptersPage);
  m_toc->setObjectName("onlineToc");
  m_toc->setAccessibleName(tr("版本与章节"));
  chapters->addWidget(m_toc);
  m_open = new QPushButton(tr("开始阅读"), this);
  m_open->setObjectName("openOnlineBook");
  m_open->setEnabled(false);
  chapters->addWidget(m_open);
  connect(m_open, &QPushButton::clicked, this, &OnlineLibraryDialog::openSelection);
  m_pages->addWidget(chaptersPage);
  m_status = new QLabel(this);
  m_status->setWordWrap(true);
  m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  m_status->setMaximumHeight(40);
  m_status->setObjectName("libraryStatus");
  layout->addWidget(m_status);
  connect(m_results, &QListWidget::itemClicked, this, [this](QListWidgetItem *) {
    selectResult();
  });
  connect(m_results, &QListWidget::itemActivated, this, [this](QListWidgetItem *) {
    selectResult();
  });
  connect(m_toc, &QListWidget::itemActivated, this,
          [this](QListWidgetItem *) { openSelection(); });
  connect(m_sources, &QComboBox::currentIndexChanged, this, [this](int i) {
    stop();
    m_cachedBook.reset();
    m_results->clear();
    m_toc->clear();
    m_rows = {};
    m_chapters = {};
    m_open->setEnabled(false);
    m_pages->setCurrentIndex(0);
    m_source = BookSource();
    for (const auto &source : m_store->sources())
      if (source.id() == m_sources->itemData(i).toString()) m_source = source;
    auto j = m_source.toJson();
    const bool all = m_sources->itemData(i).toString().isEmpty();
    bool available = all || j["enabled"].toBool(true);
    auto canSearch = [](const BookSource &source) {
      auto json = source.toJson();
      return json["enabled"].toBool(true) &&
          (json["kind"] == "mediawiki" || !json["searchUrl"].toString().isEmpty());
    };
    bool searchable = all ? std::any_of(m_store->sources().cbegin(), m_store->sources().cend(), canSearch)
                          : canSearch(m_source);
    m_keyword->setEnabled(searchable);
    m_search->setEnabled(searchable);
    m_status->setText(!available ? tr("此书源已停用，可在“更多”中管理。")
                      : !searchable ? tr("此书源没有搜索规则，请在“更多”中配置。")
                      : QString());
  });
  connect(
      b, &OnlineEngineBridge::result, this,
      [this](const QString &id, const QString &session, const QJsonObject &r) {
        if (session != m_session) return;
        if (m_searchRequests.contains(id)) {
          finishSearch(id, r);
          return;
        }
        if (id != m_request) return;
        m_request.clear();
        setBusy(false);
        receive(r);
      });
  connect(b, &OnlineEngineBridge::error, this,
          [this](const QString &id, const QString &session, const QString &e) {
            if (session != m_session) return;
            if (m_searchRequests.contains(id)) {
              finishSearch(id, {}, e);
              return;
            }
            if (id != m_request) return;
            m_request.clear();
            setBusy(false);
            m_status->setText(
                tr("请求失败：%1。可重试或更换书源。").arg(e));
            if (m_cachedBook) emit refreshFailed(m_status->text());
          });
  for (auto button : findChildren<QPushButton *>())
    button->setAutoDefault(false);
  reloadSources();
}
OnlineLibraryDialog::~OnlineLibraryDialog() {
  stop();
  auto children = findChildren<BookSourceDialog *>();
  qDeleteAll(children);
}
void OnlineLibraryDialog::reloadSources() {
  QString id = m_sources->currentData().toString();
  m_sources->clear();
  m_sources->addItem(tr("全部书源"), QString());
  for (const auto &s : m_store->sources())
    m_sources->addItem(s.toJson()["name"].toString(), s.id());
  int i = m_sources->findData(id);
  m_sources->setCurrentIndex(i < 0 ? 0 : i);
}
void OnlineLibraryDialog::selectResult() {
  if (!m_request.isEmpty()) return;
  int i = m_results->currentRow();
  if (i >= 0 && i < m_rows.size()) {
    const auto row = m_rows[i].toObject();
    m_source = BookSource(row["_source"].toObject());
    m_cachedBook.reset();
    toc(row["url"].toString());
  }
}
void OnlineLibraryDialog::refreshBook(const QString &path) {
  auto book = std::dynamic_pointer_cast<OnlineBook>(Book::create(path));
  if (!book) {
    m_status->setText(tr("无法读取这本在线书籍。"));
    emit refreshFailed(m_status->text());
    return;
  }
  m_cachedBook = book;
  emit refreshRequested(path);
  toc(book->url());
}
void OnlineLibraryDialog::hideEvent(QHideEvent *event) {
  stop();
  MenuPanel::hideEvent(event);
}
void OnlineLibraryDialog::setBusy(bool busy) {
  m_search->setText(busy ? tr("取消") : tr("搜索"));
  m_keyword->setReadOnly(busy);
}
void OnlineLibraryDialog::stop() {
  m_searching = false;
  m_searchQueue.clear();
  const auto searches = m_searchRequests.keys();
  m_searchRequests.clear();
  for (const auto &id : searches) m_bridge->cancel(id);
  if (!m_request.isEmpty()) {
    auto id = m_request;
    m_request.clear();
    m_bridge->cancel(id);
  }
  setBusy(false);
}
void OnlineLibraryDialog::request(const QString &op,
                                  const QJsonObject &fields) {
  stop();
  if (m_bridge->executablePath().isEmpty()) {
    m_status->setText(
        tr("在线组件未启用，请从“更多”安装或启用。"));
    if (m_cachedBook) emit refreshFailed(m_status->text());
    return;
  }
  m_operation = op;
  m_request = m_bridge->request(op, fields, m_session);
  setBusy(true);
  m_status->setText(op == "search" ? tr("正在搜索…") : tr("正在获取目录…"));
}
void OnlineLibraryDialog::search() {
  m_cachedBook.reset();
  if (m_keyword->text().trimmed().isEmpty()) {
    m_status->setText(tr("请输入书名或作者。"));
    return;
  }
  m_pages->setCurrentIndex(0);
  m_open->setEnabled(false);
  m_toc->clear();
  m_chapters = {};
  m_rows = {};
  m_results->clear();
  stop();
  if (m_bridge->executablePath().isEmpty()) {
    m_status->setText(tr("在线组件未启用，请从“更多”安装或启用。"));
    return;
  }
  m_searchKeys.clear();
  m_searchErrors.clear();
  m_searchDone = 0;
  m_searchKeyword = m_keyword->text().trimmed();
  const QString selected = m_sources->currentData().toString();
  for (const auto &source : m_store->sources()) {
    auto json = source.toJson();
    if ((selected.isEmpty() || selected == source.id()) && json["enabled"].toBool(true) &&
        (json["kind"] == "mediawiki" || !json["searchUrl"].toString().isEmpty()))
      m_searchQueue.append(source);
  }
  m_searchTotal = m_searchQueue.size();
  m_searching = true;
  pumpSearch();
}
void OnlineLibraryDialog::pumpSearch() {
  while (m_searching && m_searchRequests.size() < 3 && !m_searchQueue.isEmpty()) {
    const auto source = m_searchQueue.takeFirst();
    const auto id = m_bridge->request("search", {{"source", source.toJson()},
        {"keyword", m_searchKeyword}, {"page", 1}}, m_session);
    m_searchRequests.insert(id, source);
  }
  m_searching = !m_searchRequests.isEmpty() || !m_searchQueue.isEmpty();
  setBusy(m_searching);
  m_status->setText(m_searching ? tr("已搜 %1/%2 个书源 · %3 项结果")
      .arg(m_searchDone).arg(m_searchTotal).arg(m_rows.size())
      : tr("搜索完成 · %1 项结果%2").arg(m_rows.size()).arg(m_searchErrors.isEmpty()
          ? QString() : tr(" · %1 个书源失败").arg(m_searchErrors.size())));
  if (!m_searching && m_searchTotal == 1 && !m_searchErrors.isEmpty())
    m_status->setText(tr("请求失败：%1").arg(m_searchErrors.first()));
  m_status->setToolTip(m_searchErrors.join('\n'));
}
void OnlineLibraryDialog::finishSearch(const QString &id, const QJsonObject &result,
                                       const QString &error) {
  const auto source = m_searchRequests.take(id);
  ++m_searchDone;
  if (!error.isEmpty()) {
    m_searchErrors.append(source.toJson()["name"].toString() + ": " + error);
  } else {
    for (const auto &value : result["books"].toArray()) {
      auto row = value.toObject();
      QUrl url(row["url"].toString());
      url.setFragment({});
      const auto key = source.id() + '\n' + url.adjusted(QUrl::NormalizePathSegments).toString();
      if (m_searchKeys.contains(key)) continue;
      m_searchKeys.insert(key);
      row["_source"] = source.toJson();
      m_rows.append(row);
      // Append without replacing existing rows or changing the user's selection.
      auto item = new QListWidgetItem(row["title"].toString(), m_results);
      item->setData(AuthorRole, row["author"].toString());
      const auto description = row["title"].toString() + tr("，作者：") + row["author"].toString();
      item->setData(Qt::AccessibleTextRole, description);
      item->setToolTip(description + '\n' + source.toJson()["name"].toString() + '\n' + row["url"].toString());
    }
    if (m_results->currentRow() < 0 && m_results->count()) m_results->setCurrentRow(0);
  }
  pumpSearch();
}
void OnlineLibraryDialog::toc(const QString &url) {
  stop();
  m_open->setEnabled(false);
  m_toc->clear();
  m_chapters = {};
  m_choices = false;
  m_title.clear();
  m_url.clear();
  QUrl u(url);
  if (!u.isValid() || u.host().isEmpty() ||
      (u.scheme() != "https" && u.scheme() != "http") ||
      !u.userInfo().isEmpty()) {
    m_status->setText(tr("书源返回的书籍链接无效，请更换书源。"));
    return;
  }
  m_open->setEnabled(false);
  m_pages->setCurrentIndex(1);
  m_bookTitle->setText(tr("正在获取目录…"));
  m_url = url;
  request("toc", {{"source", m_cachedBook ? m_cachedBook->source().toJson()
                                          : m_source.toJson()},
                  {"url", url}});
}
void OnlineLibraryDialog::showRows(QListWidget *list, const QJsonArray &rows) {
  list->clear();
  for (auto value : rows) {
    auto j = value.toObject();
    const QString title = j["title"].toString();
    const QString author = j["author"].toString();
    auto item = new QListWidgetItem(title, list);
    item->setData(AuthorRole, author);
    const QString description = author.isEmpty() ? title : tr("%1，作者：%2").arg(title, author);
    item->setData(Qt::AccessibleTextRole, description);
    item->setToolTip(description + '\n' + j["url"].toString());
  }
  if (list->count())
    list->setCurrentRow(0);
}
void OnlineLibraryDialog::receive(const QJsonObject &r) {
  m_choices = r["chapters"].toArray().isEmpty() && !r["choices"].toArray().isEmpty();
  m_chapters = m_choices ? r["choices"].toArray() : r["chapters"].toArray();
  m_title = r["title"].toString();
  m_bookTitle->setText(m_title);
  m_bookTitle->setToolTip(m_title);
  if (!r["url"].toString().isEmpty())
    m_url = r["url"].toString();
  showRows(m_toc, m_chapters);
  m_open->setEnabled(!m_chapters.isEmpty());
  m_open->setText(m_choices ? tr("选择此版本") : tr("开始阅读"));
  m_status->setText(
      m_choices ? tr("作品有多个版本，请选择版本后继续。")
      : m_chapters.isEmpty()
          ? tr("没有有效章节；请选择其他版本或检查书源规则。")
          : tr("共 %1 章")
                .arg(m_chapters.size()));
  if (m_cachedBook && (m_choices || m_chapters.isEmpty())) {
    emit refreshFailed(m_status->text());
    return;
  }
  if (m_cachedBook && !m_choices && !m_chapters.isEmpty()) {
    QStringList oldUrls, newUrls;
    for (int i = 0; i < m_cachedBook->chapters().size(); ++i)
      oldUrls << m_cachedBook->chapterUrl(i);
    // Background refresh must map the position reached while its request was pending.
    emit refreshRequested(m_cachedBook->filePath());
    Cache cache;
    cache.load();
    auto saved = cache.progress(m_cachedBook->filePath());
    if (saved && saved->chapterIndex >= 0 &&
        saved->chapterIndex < oldUrls.size()) {
      QString wanted = oldUrls[saved->chapterIndex];
      bool found = false;
      for (auto row : m_chapters) {
        QUrl u(row.toObject()["url"].toString());
        u.setFragment({});
        if (u.adjusted(QUrl::NormalizePathSegments)
                .toString(QUrl::FullyEncoded) == wanted)
          found = true;
      }
      if (!found) {
        m_status->setText(
            tr("新目录缺少当前阅读章节，已保留旧目录与进度。请检查书源规则。"));
        m_open->setEnabled(false);
        emit refreshFailed(m_status->text());
        return;
      }
    }
    QSet<QString> refreshedUrls;
    for (auto row : m_chapters) {
      QUrl u(row.toObject()["url"].toString());
      u.setFragment({});
      refreshedUrls.insert(
          u.adjusted(QUrl::NormalizePathSegments).toString(QUrl::FullyEncoded));
    }
    if (!m_requiredChapterUrl.isEmpty() && !refreshedUrls.contains(m_requiredChapterUrl)) {
      m_open->setEnabled(false);
      m_status->setText(tr("新目录缺少正在获取的章节，已保留旧目录与进度。"));
      emit refreshFailed(m_status->text());
      return;
    }
    for (const auto &bookmark : cache.bookmarks(m_cachedBook->filePath())) {
      if (bookmark.chapterIndex < 0 ||
          bookmark.chapterIndex >= oldUrls.size() ||
          !refreshedUrls.contains(oldUrls[bookmark.chapterIndex])) {
        m_open->setEnabled(false);
        m_status->setText(tr("新目录缺少书签引用的章节，已保留旧目录、进度和书"
                             "签。请检查书源规则。"));
        emit refreshFailed(m_status->text());
        return;
      }
    }
    QString e;
    auto updated = m_cachedBook;
    if (!updated->refreshManifest(m_title.isEmpty() ? updated->title() : m_title,
                                  m_chapters, &e)) {
      m_open->setEnabled(false);
      m_status->setText(e);
      emit refreshFailed(e);
      return;
    }
    for (int i = 0; i < updated->chapters().size(); ++i)
      newUrls << updated->chapterUrl(i);
    emit tocRefreshed(updated->filePath(), oldUrls, newUrls);
    m_cachedBook = updated;
    m_status->setText(tr("目录已刷新，阅读进度按原章节网址匹配。"));
  }
}
void OnlineLibraryDialog::openSelection() {
  if (!m_request.isEmpty() || !m_open->isEnabled() || m_chapters.isEmpty())
    return;
  if (m_choices) {
    int i = m_toc->currentRow();
    if (i >= 0 && i < m_chapters.size())
      toc(m_chapters[i].toObject()["url"].toString());
    return;
  }
  QDirIterator existing(m_root, {"*.rnovel"}, QDir::Files,
                        QDirIterator::Subdirectories);
  QUrl wanted(m_cachedBook ? m_cachedBook->url() : m_url);
  wanted.setFragment({});
  while (existing.hasNext()) {
    auto book =
        std::dynamic_pointer_cast<OnlineBook>(Book::create(existing.next()));
    if (book &&
        book->source().id() ==
            (m_cachedBook ? m_cachedBook->source().id() : m_source.id()) &&
        QUrl(book->url()).adjusted(QUrl::NormalizePathSegments) ==
            wanted.adjusted(QUrl::NormalizePathSegments)) {
      hide();
      emit openRequested(book->filePath());
      return;
    }
  }
  QString e;
  auto book = OnlineBook::createManifest(
      m_root, m_cachedBook ? m_cachedBook->source() : m_source,
      m_cachedBook ? m_cachedBook->url() : m_url,
      m_title.isEmpty() ? m_url : m_title, m_chapters, &e);
  if (!book) {
    m_status->setText(e);
    return;
  }
  hide();
  emit openRequested(book->filePath());
}
} // namespace reader
