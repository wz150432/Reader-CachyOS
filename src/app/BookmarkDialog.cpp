#include "app/BookmarkDialog.h"
#include <QListWidget>
#include <QVBoxLayout>

namespace reader {

BookmarkDialog::BookmarkDialog(const QVector<Bookmark> &bookmarks, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("书签"));
    m_list = new QListWidget(this);
    for (const Bookmark &b : bookmarks) {
        const QString text = QStringLiteral("%1（第 %2 页）")
            .arg(b.title.isEmpty() ? QStringLiteral("未命名") : b.title)
            .arg(b.pageIndex + 1);
        m_list->addItem(text);
    }
    connect(m_list, &QListWidget::itemDoubleClicked, this, &BookmarkDialog::onDoubleClicked);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_list);
    resize(360, 420);
}

void BookmarkDialog::onDoubleClicked()
{
    m_selected = m_list->currentRow();
    if (m_selected >= 0)
        emit jumpRequested(m_selected);
}

}
