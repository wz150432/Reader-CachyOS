#pragma once
#include <QMainWindow>
#include <memory>
#include "core/Book.h"
#include "core/Cache.h"
#include "core/Settings.h"

class QTreeWidget;
class QLineEdit;

namespace reader {

class ReadingView;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void openBook(const QString &path);
    QString currentBookTitle() const;
    int tocItemCount() const;
    void addBookmarkForCurrentBook();
    void resetSettings();
    int currentChapter() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onChapterChanged(int index);
    void onPageChanged(int index);
    void onSearchRequested();
    void onJumpRequested();
    void onBookmarkRequested();
    void onDisplaySettingsChanged(const DisplaySettings &settings);

private:
    void buildMenus();
    void populateToc();
    void updateTitle();
    void saveProgress();
    void openBookmarkList();
    void applyKeyset();
    ReadingView *m_view = nullptr;
    QTreeWidget *m_toc = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    Cache m_cache;
    Settings m_settings;
    std::shared_ptr<Book> m_book;
    QString m_currentPath;
};

}
