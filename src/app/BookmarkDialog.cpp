#include "app/BookmarkDialog.h"
#include <QAction>
#include <QListWidget>
#include <QMenu>
#include <QShortcut>
#include <QVBoxLayout>
#include <utility>

namespace reader {

BookmarkDialog::BookmarkDialog(QVector<Bookmark> bookmarks, QWidget *parent)
    : QDialog(parent)
    , m_bookmarks(std::move(bookmarks))
{
    setWindowTitle(QStringLiteral("书签"));
    m_list = new QListWidget(this);
    for (const Bookmark &b : m_bookmarks) {
        const QString text = QStringLiteral("%1（第 %2 页）")
            .arg(b.title.isEmpty() ? QStringLiteral("未命名") : b.title)
            .arg(b.pageIndex + 1);
        m_list->addItem(text);
    }
    connect(m_list, &QListWidget::itemDoubleClicked, this, &BookmarkDialog::onDoubleClicked);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &BookmarkDialog::showContextMenu);
    auto *del = new QShortcut(QKeySequence::Delete, m_list);
    connect(del, &QShortcut::activated, this, &BookmarkDialog::deleteSelectedBookmark);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_list);
    resize(360, 420);
}

int BookmarkDialog::count() const
{
    return m_list->count();
}

void BookmarkDialog::setCurrentRow(int row)
{
    m_list->setCurrentRow(row);
}

void BookmarkDialog::onDoubleClicked()
{
    m_selected = m_list->currentRow();
    if (m_selected >= 0)
        jumpToSelectedBookmark();
}

void BookmarkDialog::jumpToSelectedBookmark()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_bookmarks.size())
        return;
    m_selected = row;
    emit jumpRequested(m_bookmarks.at(row));
}

void BookmarkDialog::deleteSelectedBookmark()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_bookmarks.size())
        return;
    const Bookmark b = m_bookmarks.at(row);
    m_bookmarks.remove(row);
    delete m_list->takeItem(row);
    m_selected = -1;
    emit deleteRequested(b);
}

void BookmarkDialog::showContextMenu(const QPoint &pos)
{
    const int row = m_list->indexAt(pos).row();
    if (row < 0)
        return;
    m_list->setCurrentRow(row);
    QMenu menu(this);
    QAction *jump = menu.addAction(QStringLiteral("跳转到书签"));
    QAction *remove = menu.addAction(QStringLiteral("删除书签"));
    QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (chosen == jump)
        jumpToSelectedBookmark();
    else if (chosen == remove)
        deleteSelectedBookmark();
}

}
