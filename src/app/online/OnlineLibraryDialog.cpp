#include "OnlineLibraryDialog.h"
#include "BookSourceDialog.h"
#include "app/UiStyle.h"
#include "core/Cache.h"
#include "core/online/OnlineBook.h"
#include "core/online/OnlineEngineBridge.h"
#include <QComboBox>
#include <QDirIterator>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
namespace reader {
OnlineLibraryDialog::OnlineLibraryDialog(BookSourceStore *s,
                                         OnlineEngineBridge *b, QString root,
                                         QWidget *p)
    : QDialog(p), m_store(s), m_bridge(b), m_root(root),
      m_session(QUuid::createUuid().toString()) {
  setWindowTitle(tr("在线阅读"));
  setObjectName("onlineLibraryDialog");
  setStyleSheet(readerUiStyle());
  resize(980, 700);
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 18, 20, 18);
  layout->setSpacing(12);
  auto sources = new QHBoxLayout;
  layout->addLayout(sources);
  sources->addWidget(new QLabel(tr("书源"), this));
  m_sources = new QComboBox(this);
  m_sources->setObjectName("librarySources");
  sources->addWidget(m_sources, 1);
  auto manage = new QPushButton(tr("管理书源…"), this);
  sources->addWidget(manage);
  connect(manage, &QPushButton::clicked, this, [this] {
    auto d = new BookSourceDialog(m_store, m_bridge, this);
    d->setAttribute(Qt::WA_DeleteOnClose);
    connect(d, &BookSourceDialog::sourcesChanged, this,
            &OnlineLibraryDialog::reloadSources);
    d->show();
  });
  auto searchRow = new QHBoxLayout;
  layout->addLayout(searchRow);
  m_keyword = new QLineEdit(this);
  m_keyword->setObjectName("onlineKeyword");
  m_keyword->setPlaceholderText(tr("输入书名或作者"));
  searchRow->addWidget(m_keyword, 1);
  auto searchButton = new QPushButton(tr("搜索"), this);
  searchButton->setObjectName("searchOnline");
  searchButton->setDefault(true);
  searchRow->addWidget(searchButton);
  connect(searchButton, &QPushButton::clicked, this, [this] { search(1); });
  connect(m_keyword, &QLineEdit::returnPressed, this, [this] { search(1); });
  auto directRow = new QHBoxLayout;
  layout->addLayout(directRow);
  m_direct = new QLineEdit(this);
  m_direct->setObjectName("directUrl");
  m_direct->setPlaceholderText(tr("粘贴书籍或目录网址 https://…"));
  directRow->addWidget(m_direct, 1);
  auto import = new QPushButton(tr("获取目录"), this);
  import->setObjectName("importUrl");
  directRow->addWidget(import);
  connect(import, &QPushButton::clicked, this, [this] {
    m_cachedBook.reset();
    toc(m_direct->text());
  });
  connect(m_direct, &QLineEdit::returnPressed, import, &QPushButton::click);
  auto split = new QSplitter(this);
  layout->addWidget(split, 1);
  auto column = [&](const QString &title, QListWidget **list) {
    auto box = new QGroupBox(title, this);
    auto l = new QVBoxLayout(box);
    *list = new QListWidget(box);
    (*list)->setAlternatingRowColors(true);
    l->addWidget(*list);
    split->addWidget(box);
    return l;
  };
  auto results = column(tr("搜索结果"), &m_results);
  m_results->setObjectName("onlineResults");
  auto paging = new QHBoxLayout;
  results->addLayout(paging);
  m_previous = new QPushButton(tr("上一页"), this);
  m_next = new QPushButton(tr("下一页"), this);
  m_pageLabel = new QLabel(tr("第 1 页"), this);
  paging->addWidget(m_previous);
  paging->addWidget(m_pageLabel, 1, Qt::AlignCenter);
  paging->addWidget(m_next);
  m_previous->setEnabled(false);
  m_next->setEnabled(false);
  connect(m_previous, &QPushButton::clicked, this,
          [this] { search(m_page - 1); });
  connect(m_next, &QPushButton::clicked, this, [this] { search(m_page + 1); });
  auto chapters = column(tr("版本与章节"), &m_toc);
  m_toc->setObjectName("onlineToc");
  m_open = new QPushButton(tr("开始阅读"), this);
  m_open->setObjectName("openOnlineBook");
  m_open->setEnabled(false);
  chapters->addWidget(m_open);
  connect(m_open, &QPushButton::clicked, this,
          &OnlineLibraryDialog::openSelection);
  auto cached = column(tr("已缓存书籍"), &m_cached);
  m_cached->setObjectName("cachedBooks");
  auto cachedOpen = new QPushButton(tr("打开缓存"), this);
  cached->addWidget(cachedOpen);
  auto refresh = new QPushButton(tr("手动刷新目录"), this);
  refresh->setObjectName("refreshOnlineToc");
  cached->addWidget(refresh);
  connect(cachedOpen, &QPushButton::clicked, this, [this] {
    auto item = m_cached->currentItem();
    if (item)
      emit openRequested(item->data(Qt::UserRole).toString());
  });
  connect(m_cached, &QListWidget::itemActivated, this,
          [cachedOpen](QListWidgetItem *) { cachedOpen->click(); });
  connect(refresh, &QPushButton::clicked, this, [this] {
    auto item = m_cached->currentItem();
    if (!item) {
      m_status->setText(tr("请先选择已缓存书籍。"));
      return;
    }
    auto book = std::dynamic_pointer_cast<OnlineBook>(
        Book::create(item->data(Qt::UserRole).toString()));
    if (!book) {
      m_status->setText(tr("缓存目录无法读取。"));
      return;
    }
    m_cachedBook = book;
    emit refreshRequested(book->filePath());
    toc(book->url());
  });
  m_status = new QLabel(
      tr("选择书源后搜索，或直接输入书籍网址。下载的章节保存在本机。"), this);
  m_status->setWordWrap(true);
  m_status->setObjectName("libraryStatus");
  layout->addWidget(m_status);
  auto bottom = new QHBoxLayout;
  layout->addLayout(bottom);
  m_cancel = new QPushButton(tr("取消请求"), this);
  m_cancel->setEnabled(false);
  bottom->addWidget(m_cancel);
  connect(m_cancel, &QPushButton::clicked, this, [this] {
    stop();
    m_status->setText(tr("已取消，可重新搜索或获取目录。"));
  });
  bottom->addStretch();
  auto component = new QPushButton(tr("在线组件…"), this);
  bottom->addWidget(component);
  connect(component, &QPushButton::clicked, this,
          &OnlineLibraryDialog::componentRequested);
  auto close = new QPushButton(tr("关闭"), this);
  bottom->addWidget(close);
  connect(close, &QPushButton::clicked, this, &QDialog::close);
  connect(m_results, &QListWidget::itemActivated, this,
          [this](QListWidgetItem *) {
            int i = m_results->currentRow();
            if (i >= 0 && i < m_rows.size()) {
              m_cachedBook.reset();
              toc(m_rows[i].toObject()["url"].toString());
            }
          });
  connect(m_results, &QListWidget::currentRowChanged, this, [this](int i) {
    if (i >= 0 && i < m_rows.size()) {
      m_direct->setText(m_rows[i].toObject()["url"].toString());
    }
  });
  connect(m_toc, &QListWidget::itemActivated, this,
          [this](QListWidgetItem *) { openSelection(); });
  connect(m_sources, &QComboBox::currentIndexChanged, this,
          [this, searchButton](int i) {
            stop();
            m_cachedBook.reset();
            m_results->clear();
            m_toc->clear();
            m_rows = {};
            m_chapters = {};
            m_open->setEnabled(false);
            if (i >= 0 && i < m_store->sources().size())
              m_source = m_store->sources()[i];
            auto j = m_source.toJson();
            bool available = j["enabled"].toBool(true);
            bool searchable =
                available && (j["kind"] == "mediawiki" ||
                              !j["searchUrl"].toString().isEmpty());
            m_keyword->setVisible(searchable);
            searchButton->setVisible(searchable);
            m_previous->setEnabled(false);
            m_next->setEnabled(false);
            if (!available)
              m_status->setText(tr("此书源已停用，可在管理书源中启用。"));
            else if (!searchable)
              m_status->setText(tr("此书源仅支持网址导入，请输入书籍网址。"));
          });
  connect(
      b, &OnlineEngineBridge::result, this,
      [this](const QString &id, const QString &session, const QJsonObject &r) {
        if (id != m_request || session != m_session)
          return;
        m_request.clear();
        m_cancel->setEnabled(false);
        receive(r);
      });
  connect(b, &OnlineEngineBridge::error, this,
          [this](const QString &id, const QString &session, const QString &e) {
            if (id != m_request || session != m_session)
              return;
            m_request.clear();
            m_cancel->setEnabled(false);
            m_status->setText(
                tr("请求失败：%1。请检查网址或书源规则后重试。").arg(e));
          });
  reloadSources();
  refreshCached();
}
OnlineLibraryDialog::~OnlineLibraryDialog() {
  stop();
  auto children = findChildren<BookSourceDialog *>();
  qDeleteAll(children);
}
void OnlineLibraryDialog::reloadSources() {
  QString id = m_sources->currentData().toString();
  m_sources->clear();
  for (const auto &s : m_store->sources())
    m_sources->addItem(s.toJson()["name"].toString(), s.id());
  int i = m_sources->findData(id);
  m_sources->setCurrentIndex(i < 0 ? 0 : i);
}
void OnlineLibraryDialog::refreshCached() {
  m_cached->clear();
  QDirIterator it(m_root, {"*.rnovel"}, QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    auto path = it.next();
    auto book = Book::create(path);
    if (!book)
      continue;
    auto item = new QListWidgetItem(book->title(), m_cached);
    item->setData(Qt::UserRole, path);
    item->setToolTip(path);
  }
  if (!m_cached->count())
    m_cached->setToolTip(
        tr("尚无缓存。搜索并打开一本书后，已读章节会出现在本机。"));
}
void OnlineLibraryDialog::stop() {
  if (!m_request.isEmpty()) {
    auto id = m_request;
    m_request.clear();
    m_bridge->cancel(id);
  }
  m_cancel->setEnabled(false);
}
void OnlineLibraryDialog::request(const QString &op,
                                  const QJsonObject &fields) {
  stop();
  if (m_bridge->executablePath().isEmpty()) {
    m_status->setText(
        tr("在线组件未启用。请从“在线组件”启用或安装；已缓存章节仍可打开。"));
    return;
  }
  m_operation = op;
  m_request = m_bridge->request(op, fields, m_session);
  m_cancel->setEnabled(true);
  m_status->setText(op == "search" ? tr("正在搜索…") : tr("正在获取目录…"));
}
void OnlineLibraryDialog::search(int page) {
  m_cachedBook.reset();
  if (page < 1 || m_keyword->text().trimmed().isEmpty()) {
    m_status->setText(tr("请输入书名或作者。"));
    return;
  }
  m_page = page;
  request("search", {{"source", m_source.toJson()},
                     {"keyword", m_keyword->text()},
                     {"page", page}});
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
    m_status->setText(tr("请输入完整的 HTTP 或 HTTPS 书籍网址。"));
    return;
  }
  m_open->setEnabled(false);
  m_url = url;
  request("toc", {{"source", m_cachedBook ? m_cachedBook->source().toJson()
                                          : m_source.toJson()},
                  {"url", url}});
}
void OnlineLibraryDialog::showRows(QListWidget *list, const QJsonArray &rows) {
  list->clear();
  for (auto value : rows) {
    auto j = value.toObject();
    auto item = new QListWidgetItem(j["title"].toString() +
                                        (j["author"].toString().isEmpty()
                                             ? QString()
                                             : " · " + j["author"].toString()),
                                    list);
    item->setToolTip(j["url"].toString());
  }
  if (list->count())
    list->setCurrentRow(0);
}
void OnlineLibraryDialog::receive(const QJsonObject &r) {
  if (m_operation == "search") {
    m_rows = r["books"].toArray();
    showRows(m_results, m_rows);
    m_pageLabel->setText(tr("第 %1 页").arg(m_page));
    m_previous->setEnabled(m_page > 1);
    m_next->setEnabled(r["hasMore"].toBool());
    m_status->setText(m_rows.isEmpty()
                          ? tr("没有搜索结果，可更换关键词或直接导入网址。")
                          : tr("选中作品后点击“获取目录”，或双击结果。"));
    return;
  }
  m_choices = !r["choices"].toArray().isEmpty();
  m_chapters = m_choices ? r["choices"].toArray() : r["chapters"].toArray();
  m_title = r["title"].toString();
  if (!r["url"].toString().isEmpty())
    m_url = r["url"].toString();
  showRows(m_toc, m_chapters);
  m_open->setEnabled(!m_chapters.isEmpty());
  m_open->setText(m_choices ? tr("选择此版本") : tr("开始阅读"));
  m_status->setText(
      m_choices ? tr("作品有多个版本，请选择版本后继续。")
      : m_chapters.isEmpty()
          ? tr("没有有效章节；请选择其他版本或检查书源规则。")
          : tr("共 %1 章。阅读时先获取当前章，再预取后面最多 3 章。")
                .arg(m_chapters.size()));
  if (m_cachedBook && !m_choices && !m_chapters.isEmpty()) {
    QStringList oldUrls, newUrls;
    for (int i = 0; i < m_cachedBook->chapters().size(); ++i)
      oldUrls << m_cachedBook->chapterUrl(i);
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
    for (const auto &bookmark : cache.bookmarks(m_cachedBook->filePath())) {
      if (bookmark.chapterIndex < 0 ||
          bookmark.chapterIndex >= oldUrls.size() ||
          !refreshedUrls.contains(oldUrls[bookmark.chapterIndex])) {
        m_open->setEnabled(false);
        m_status->setText(tr("新目录缺少书签引用的章节，已保留旧目录、进度和书"
                             "签。请检查书源规则。"));
        return;
      }
    }
    QString e;
    auto updated = OnlineBook::createManifest(
        m_root, m_cachedBook->source(), m_cachedBook->url(),
        m_title.isEmpty() ? m_cachedBook->title() : m_title, m_chapters, &e);
    if (!updated) {
      m_open->setEnabled(false);
      m_status->setText(e);
      return;
    }
    for (int i = 0; i < updated->chapters().size(); ++i)
      newUrls << updated->chapterUrl(i);
    emit tocRefreshed(updated->filePath(), oldUrls, newUrls);
    m_cachedBook = updated;
    refreshCached();
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
  refreshCached();
  emit openRequested(book->filePath());
}
} // namespace reader
