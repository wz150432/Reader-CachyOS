#pragma once
#include <QDialog>
#include "core/Cache.h"

class QListWidget;

namespace reader {

class BookmarkDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BookmarkDialog(const QVector<Bookmark> &bookmarks, QWidget *parent = nullptr);
    int selectedBookmarkIndex() const { return m_selected; }

signals:
    void jumpRequested(int bookmarkIndex);

private slots:
    void onDoubleClicked();

private:
    QListWidget *m_list;
    int m_selected = -1;
};

}
