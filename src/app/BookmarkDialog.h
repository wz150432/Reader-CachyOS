#pragma once
#include <QDialog>
#include <QVector>
#include "core/Cache.h"

class QListWidget;

namespace reader {

class BookmarkDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BookmarkDialog(QVector<Bookmark> bookmarks, QWidget *parent = nullptr);
    int selectedBookmarkIndex() const { return m_selected; }
    int count() const;
    void setCurrentRow(int row);
    void deleteSelectedBookmark();
    void jumpToSelectedBookmark();

signals:
    void jumpRequested(const reader::Bookmark &bookmark);
    void deleteRequested(const reader::Bookmark &bookmark);

private slots:
    void onDoubleClicked();
    void showContextMenu(const QPoint &pos);

private:
    QListWidget *m_list;
    QVector<Bookmark> m_bookmarks;
    int m_selected = -1;
};

}
