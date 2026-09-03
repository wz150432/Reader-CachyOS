#pragma once
#include <QMainWindow>
#include <QList>
#include <memory>
#include "core/Book.h"
#include "core/Cache.h"
#include "core/Settings.h"

class QTreeWidget;
class QLineEdit;
class QMenu;
class QSystemTrayIcon;

namespace reader {

class ReadingView;
class RemoteControl;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void openBook(const QString &path);
    void openLastRead();
    QString currentBookTitle() const;
    int tocItemCount() const;
    void addBookmarkForCurrentBook();
    void resetSettings();
    int currentChapter() const;
    void showHideWindow();
    void quitApplication();
    void toggleFullscreen();
    void toggleAlwaysOnTop();
    void toggleHideBorder();
    void toggleAutoPage();
    bool autoPageActive() const;
    int currentProgressPercent() const;
    bool windowHiddenForTest() const { return !isVisible(); }
    void clearRecentList();
    void removeRecentFile(const QString &path);
    void handleRemoteCommand(const QString &command);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
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
    void populateOpenMenu(QMenu *menu);
    void refreshOpenMenu();
    void showOpenMenu();
    void chooseNewBook();
    void closeSearchBar();
    void updateTitle();
    void saveProgress();
    void openBookmarkList();
    void applyKeyset();
    void createTrayIcon();
    void applyWindowOpacity();
    void syncGlobalHide();
    void handleKeyAction(KeyAction action);
    ReadingView *m_view = nullptr;
    QTreeWidget *m_toc = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QMenu *m_deleteRecentMenu = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    RemoteControl *m_control = nullptr;
    bool m_topHintShown = false;
    Cache m_cache;
    Settings m_settings;
    std::shared_ptr<Book> m_book;
    QString m_currentPath;
};

}
