#pragma once
#include <QMainWindow>
#include <QList>
#include <QPlainTextEdit>
#include <QRect>
#include <QPointer>
#include <memory>
#include "core/Book.h"
#include "core/Cache.h"
#include "core/Settings.h"

class QTreeWidget;
class QLineEdit;
class QMenu;
class QMenuBar;
class QTimer;
class QSystemTrayIcon;
class QWidgetAction;

namespace reader {

class MenuPanel;
class ReadingView;
class RemoteControl;
class NiriClient;
class OnlineComponentManager;
class OnlineEngineBridge;
class OnlineReadingController;
class BookSourceStore;
class OnlineLibraryDialog;
class OnlineComponentDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void openBook(const QString &path);
    void openLastRead();
    QString currentBookTitle() const;
    int tocItemCount() const;
    void addBookmarkForCurrentBook();
    void resetSettings();
    int currentChapter() const;
    void showHideWindow();
    void quitApplication();
    bool mouseLeaveHideEnabled() const { return m_settings.behavior.mouseLeaveHideEnabled; }
    bool mouseLeaveHideActive() const;
    bool hasSavedWindowGeometry() const { return !m_settings.windowGeometry.isEmpty(); }
    bool editModeActive() const { return m_editor && m_editor->isVisible(); }
    void applyWindowState();
    void restoreNiriFloatingPosition();
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
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onChapterChanged(int index);
    void onPageChanged(int index);
    void onSearchRequested();
    void onJumpRequested();
    void onBookmarkRequested();
    void onDisplaySettingsChanged(const DisplaySettings &settings);

private:
    void buildMenus();
    void showOnlineReading();
    void ensureOnlineLibrary();
    void refreshCurrentBook();
    void showOnlineComponent();
    void requestOnlineChapter(int index);
    void remapOnlineProgress(const QString &,const QStringList &,const QStringList &);
    void updateChromeSpacing();
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
    void saveWindowState();
    void rememberNiriFloatingPosition();
    void prepareNiriFloatingPositionRestore();
    void tryRestoreNiriFloatingPosition(int attemptsLeft, bool moveIssued, int serial);
    void finishNiriFloatingPositionRestore();
    void syncNiriWindowRule();
    void createResizeGrips();
    void updateResizeGrips();
    void toggleMouseLeaveHide();
    void applyMouseLeaveHideMode();
    void onMouseWatchTick();
    void toggleEditMode();
    void leaveEditMode();
    bool saveEditMode();
    bool leaveEditModeIfActive();
    OnlineComponentManager *m_onlineComponent = nullptr;
    OnlineEngineBridge *m_onlineBridge = nullptr;
    OnlineReadingController *m_onlineController = nullptr;
    std::unique_ptr<BookSourceStore> m_onlineSources;
    QPointer<OnlineLibraryDialog> m_onlineLibrary;
    QPointer<OnlineComponentDialog> m_onlineComponentDialog;
    QPointer<MenuPanel> m_aboutPanel;
    bool m_openOnlineAfterInstall = false;
    bool m_restoringOnline = false;
    int m_onlineRestoreChapter = -1;
    int m_onlineRestorePage = 0;
    qreal m_onlineRestorePixel = 0;
    QString m_onlineRoot;
    ReadingView *m_view = nullptr;
    QMenuBar *m_dragMenuBar = nullptr;
    QTreeWidget *m_toc = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    bool m_currentRecordRemoved = false;
    QWidgetAction *m_deleteConfirmation = nullptr;
    QPlainTextEdit *m_editor = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    RemoteControl *m_control = nullptr;
    QTimer *m_mouseWatchTimer = nullptr;
    QTimer *m_progressSaveTimer = nullptr;
    QTimer *m_niriSettingsTimer = nullptr;
    NiriClient *m_niriClient = nullptr;
    bool m_restoringNiriPosition = false;
    QList<QWidget *> m_resizeGrips;
    QRect m_hiddenGeometry;
    bool m_topHintShown = false;
    bool m_hiddenWasMaximized = false;
    bool m_hiddenByMouseLeave = false;
    bool m_globalHideBindReady = false;
    bool m_niriRuleReady = false;
    int m_globalHideSyncSerial = 0;
    int m_niriRuleSyncSerial = 0;
    bool m_positioningWindow = false;
    int m_niriRestoreSerial = 0;
    QString m_titleBeforePositioning;
    qint64 m_leaveHideIgnoreUntil = 0;
    Cache m_cache;
    Settings m_settings;
    std::shared_ptr<Book> m_book;
    QString m_currentPath;
};

}
