#include "app/MainWindow.h"
#include "app/UiStyle.h"
#include "app/MenuPanel.h"
#include "app/WindowDragFilter.h"
#include "app/AnchoredDialogs.h"
#include "core/NiriClient.h"
#include "core/online/OnlineComponentManager.h"
#include "core/online/OnlineEngineBridge.h"
#include "core/online/OnlineReadingController.h"
#include "core/online/OnlineBook.h"
#include "core/online/BookSourceStore.h"
#include "app/online/OnlineLibraryDialog.h"
#include "app/online/OnlineComponentDialog.h"
#include <QShowEvent>
#include <QHideEvent>
#include <QLayout>
#include <QSignalBlocker>
#include "app/AdvancedSettingsDialog.h"
#include "app/BasicSettingsDialog.h"
#include "app/BookmarkDialog.h"
#include "app/KeysetDialog.h"
#include "app/ReadingView.h"
#include "app/RemoteControl.h"
#include "app/SettingsDialog.h"
#include "app/TagsetDialog.h"
#include "core/NiriConfig.h"
#include "core/NiriWindow.h"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QWidgetAction>
#include <QWindow>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolTip>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <utility>

namespace reader {

class ResizeGrip final : public QWidget
{
public:
    ResizeGrip(QWidget *parent, Qt::Edges edges, Qt::CursorShape cursor,
               const QString &name)
        : QWidget(parent), m_edges(edges)
    {
        setObjectName(name);
        setCursor(cursor);
        setAutoFillBackground(false);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            if (QWindow *handle = window()->windowHandle())
                handle->startSystemResize(m_edges);
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

private:
    Qt::Edges m_edges;
};

static QString niriKeyFromSequence(const QKeySequence &seq)
{
    QString key = seq.toString(QKeySequence::PortableText);
    key.replace(QStringLiteral("Meta"), QStringLiteral("Mod"));
    key.replace(QStringLiteral("Super"), QStringLiteral("Mod"));
    return key;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setStyleSheet(readerUiStyle());
    setAttribute(Qt::WA_TranslucentBackground, true);
    QApplication::instance()->installEventFilter(this);
    new WindowDragFilter(this);
    new AnchoredDialogs(this);
    m_niriClient = new NiriClient(this);
    m_progressSaveTimer = new QTimer(this);
    m_progressSaveTimer->setSingleShot(true);
    m_progressSaveTimer->setInterval(300);
    connect(m_progressSaveTimer, &QTimer::timeout, this, &MainWindow::saveProgress);
    m_niriSettingsTimer = new QTimer(this);
    m_niriSettingsTimer->setSingleShot(true);
    m_niriSettingsTimer->setInterval(150);
    connect(m_niriSettingsTimer, &QTimer::timeout, this, [this] {
        m_settings.save();
        syncNiriWindowRule();
    });
    m_cache.load();
    m_settings.load();
    m_view = new ReadingView(this);
    m_view->setObjectName(QStringLiteral("readingView"));
    m_view->setSettings(m_settings.display);
    m_view->setKeyset(m_settings.keyset);
    m_view->setBehavior(m_settings.behavior);
    m_view->setTags(m_settings.tags);
    setCentralWidget(m_view);
    connect(m_view, &ReadingView::chapterChanged, this, &MainWindow::onChapterChanged);
    connect(m_view, &ReadingView::pageChanged, this, &MainWindow::onPageChanged);
    connect(m_view, &ReadingView::searchRequested, this, &MainWindow::onSearchRequested);
    connect(m_view, &ReadingView::jumpRequested, this, &MainWindow::onJumpRequested);
    connect(m_view, &ReadingView::bookmarkRequested, this, &MainWindow::onBookmarkRequested);
    connect(m_view, &ReadingView::fileDropRequested, this, &MainWindow::openBook);
    connect(m_view, &ReadingView::hideWindowRequested, this, &MainWindow::showHideWindow);
    connect(m_view, &ReadingView::displaySettingsChanged, this, &MainWindow::onDisplaySettingsChanged);

    m_toc = new QTreeWidget(m_view);
    m_toc->setObjectName(QStringLiteral("tocView"));
    m_toc->setHeaderHidden(true);
    m_toc->setRootIsDecorated(false);
    m_toc->setUniformRowHeights(true);
    m_toc->setAlternatingRowColors(true);
    m_toc->setTextElideMode(Qt::ElideRight);
    m_toc->setAccessibleName(QStringLiteral("章节目录"));
    m_toc->hide();
    const auto openChapter = [this](QTreeWidgetItem *item) {
        m_view->goToChapter(item->data(0, Qt::UserRole).toInt());
        m_toc->hide();
        m_view->setFocus();
    };
    connect(m_toc, &QTreeWidget::itemClicked, this, openChapter);
    connect(m_toc, &QTreeWidget::itemActivated, this, openChapter);

    auto *searchBar = addToolBar(QStringLiteral("搜索"));
    searchBar->setObjectName(QStringLiteral("searchBar"));
    searchBar->setMovable(false);
    m_searchEdit = new QLineEdit(searchBar);
    m_searchEdit->setObjectName(QStringLiteral("searchEdit"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setAccessibleName(QStringLiteral("搜索内容"));
    m_searchEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_searchEdit->installEventFilter(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索（Enter 下一个，Esc 关闭）"));
    searchBar->addWidget(m_searchEdit);
    auto *wholeBook = new QCheckBox(QStringLiteral("全书"), searchBar);
    wholeBook->setObjectName("searchWholeBook");
    searchBar->addWidget(wholeBook);
    auto *closeSearch = searchBar->addAction(QStringLiteral("关闭"));
    closeSearch->setToolTip(QStringLiteral("关闭搜索（Esc）"));
    connect(closeSearch, &QAction::triggered, this, &MainWindow::closeSearchBar);
    connect(wholeBook, &QCheckBox::toggled, m_view, &ReadingView::setSearchWholeBook);
    searchBar->setVisible(false);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this] {
        m_view->findNext(m_searchEdit->text());
    });

    m_onlineRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/online";
    m_onlineSources = std::make_unique<BookSourceStore>(m_onlineRoot + "/sources.json");
    QString sourceError;
    if (!m_onlineSources->load(&sourceError))
        setProperty("onlineSourceError", sourceError);
    m_onlineComponent = new OnlineComponentManager(m_onlineRoot + "/component", READER_APP_VERSION,
                                                  OnlineComponentManager::bundledMetadata(), this);
    m_onlineBridge = new OnlineEngineBridge(this);
    m_onlineBridge->setExecutablePath(m_onlineComponent->executablePath());
    m_onlineController = new OnlineReadingController(m_onlineBridge, this);
    connect(m_view, &ReadingView::chapterRequested, this, &MainWindow::requestOnlineChapter);
    connect(m_view, &ReadingView::chapterRetryRequested, this, &MainWindow::requestOnlineChapter);
    connect(m_view, &ReadingView::chapterCancelRequested, m_onlineController, &OnlineReadingController::cancel);
    connect(m_onlineController, &OnlineReadingController::currentReady, this, [this](int index) {
        bool restore = index == m_onlineRestoreChapter;
        if (restore) m_restoringOnline = true;
        m_view->setChapterReady(index);
        if (restore) {
            m_view->goToPage(m_onlineRestorePage);
            m_view->scrollByPixels(m_onlineRestorePixel);
            m_onlineRestoreChapter = -1;
            m_restoringOnline = false;
        }
        saveProgress();
    });
    connect(m_onlineController, &OnlineReadingController::currentError, m_view, &ReadingView::setChapterError);
    connect(m_onlineComponent, &OnlineComponentManager::stopping, this, [this] {
        m_onlineController->cancel();
        m_onlineBridge->stop();
        if (m_view->pendingChapter() >= 0)
            m_view->setChapterError(m_view->pendingChapter(), tr("在线组件已停用，请启用后重试。"));
    });
    connect(m_onlineComponent, &OnlineComponentManager::changed, this, [this] {
        m_onlineBridge->setExecutablePath(m_onlineComponent->executablePath());
    });
    auto refresh = new QAction(tr("刷新"), this);
    refresh->setObjectName("actRefresh");
    refresh->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    addAction(refresh);
    connect(refresh, &QAction::triggered, this, &MainWindow::refreshCurrentBook);
    buildMenus();
    menuBar()->setVisible(!m_settings.menuBarHidden);
    updateChromeSpacing();
    setWindowFlag(Qt::FramelessWindowHint, m_settings.menuBarHidden);
    m_dragMenuBar = menuBar();
    m_dragMenuBar->installEventFilter(this);
    updateTitle();
    resize(960, 720);
    createResizeGrips();
    applyWindowOpacity();
    createTrayIcon();
    m_mouseWatchTimer = new QTimer(this);
    m_mouseWatchTimer->setInterval(100);
    connect(m_mouseWatchTimer, &QTimer::timeout, this, &MainWindow::onMouseWatchTick);
    m_control = new RemoteControl(this);
    connect(m_control, &RemoteControl::commandReceived,
            this, &MainWindow::handleRemoteCommand);
    syncGlobalHide();
    applyMouseLeaveHideMode();
}

MainWindow::~MainWindow()
{
    saveProgress();
    delete m_onlineLibrary;
    delete m_onlineComponentDialog;
    delete m_onlineController;
    m_onlineController = nullptr;
    delete m_onlineBridge;
    m_onlineBridge = nullptr;
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    m_niriClient->start();
}

void MainWindow::hideEvent(QHideEvent *event)
{
    rememberNiriFloatingPosition();
    ++m_niriRestoreSerial;
    finishNiriFloatingPositionRestore();
    m_niriClient->stop();
    QMainWindow::hideEvent(event);
}

void MainWindow::buildMenus()
{
    QMenu *openMenu = menuBar()->addMenu(QStringLiteral("文件"));
    openMenu->setObjectName(QStringLiteral("openMenu"));
    openMenu->setStyleSheet(QStringLiteral("QMenu { menu-scrollable: 1; }"));
    openMenu->menuAction()->setObjectName(QStringLiteral("actOpen"));
    openMenu->menuAction()->setShortcut(QKeySequence::Open);
    connect(openMenu, &QMenu::aboutToShow, this, [this, openMenu] {
        populateOpenMenu(openMenu);
    });
    populateOpenMenu(openMenu);

    QAction *tocToggle = menuBar()->addAction(QStringLiteral("目录"));
    tocToggle->setObjectName(QStringLiteral("actToggleToc"));
    connect(tocToggle, &QAction::triggered, this, [this] {
        if (m_toc->isVisible()) {
            m_toc->hide();
            m_view->setFocus();
        } else {
            if (!leaveEditModeIfActive())
                return;
            closeSearchBar();
            m_toc->setGeometry(m_view->rect());
            m_toc->show();
            m_toc->raise();
            m_toc->setFocus();
        }
    });

    QMenu *bookmark = menuBar()->addMenu(QStringLiteral("书签"));
    QAction *bm = bookmark->addAction(QStringLiteral("添加书签"));
    connect(bm, &QAction::triggered, this, &MainWindow::onBookmarkRequested);
    QAction *bmList = bookmark->addAction(QStringLiteral("书签列表"));
    connect(bmList, &QAction::triggered, this, &MainWindow::openBookmarkList);

    QMenu *settings = menuBar()->addMenu(QStringLiteral("设置"));
    QAction *display = settings->addAction(QStringLiteral("显示设置"));
    connect(display, &QAction::triggered, this, [this] {
        SettingsDialog dlg(&m_settings, this, menuBar()->isHidden() ? 0 : 1);
        if (dlg.exec() == QDialog::Accepted) {
            m_view->setSettings(m_settings.display);
            m_view->refreshLayout();
            applyWindowOpacity();
        }
    });
    QAction *basicAction = settings->addAction(QStringLiteral("基本设置"));
    connect(basicAction, &QAction::triggered, this, [this] {
        BasicSettingsDialog dlg(&m_settings, this);
        if (dlg.exec() == QDialog::Accepted) {
            m_view->setBehavior(m_settings.behavior);
            applyMouseLeaveHideMode();
            applyWindowOpacity();
            // m_leaveHideIgnoreUntil = QDateTime::currentMSecsSinceEpoch() + 1000;
        }
    });
    QAction *advancedAction = settings->addAction(QStringLiteral("高级设置"));
    connect(advancedAction, &QAction::triggered, this, [this] {
        AdvancedSettingsDialog dlg(&m_settings, this);
        dlg.exec();
    });
    QAction *keysetAction = settings->addAction(QStringLiteral("按键设置"));
    connect(keysetAction, &QAction::triggered, this, [this] {
        KeysetDialog dlg(&m_settings, this);
        if (dlg.exec() == QDialog::Accepted) {
            applyKeyset();
            syncGlobalHide();
            // m_leaveHideIgnoreUntil = QDateTime::currentMSecsSinceEpoch() + 1000;
        }
    });
    QAction *tagsetAction = settings->addAction(QStringLiteral("标签设置"));
    connect(tagsetAction, &QAction::triggered, this, [this] {
        TagsetDialog dlg(&m_settings, this);
        if (dlg.exec() == QDialog::Accepted)
            m_view->setTags(m_settings.tags);
    });
    settings->addSeparator();
    QMenu *windowMenu = menuBar()->addMenu(QStringLiteral("窗口"));
    QAction *fullAction = windowMenu->addAction(QStringLiteral("全屏"));
    connect(fullAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);
    QAction *topAction = windowMenu->addAction(QStringLiteral("窗口置顶"));
    connect(topAction, &QAction::triggered, this, &MainWindow::toggleAlwaysOnTop);
    QAction *borderAction = windowMenu->addAction(QStringLiteral("隐藏菜单栏"));
    connect(borderAction, &QAction::triggered, this, &MainWindow::toggleHideBorder);
    QAction *hideAction = windowMenu->addAction(QStringLiteral("隐藏窗口"));
    connect(hideAction, &QAction::triggered, this, &MainWindow::showHideWindow);
    QAction *autoPageAction = windowMenu->addAction(QStringLiteral("自动翻页"));
    connect(autoPageAction, &QAction::triggered, this, &MainWindow::toggleAutoPage);
    QAction *resetAction = settings->addAction(QStringLiteral("还原默认设置"));
    connect(resetAction, &QAction::triggered, this, [this] {
        if (QMessageBox::question(this, QStringLiteral("还原默认设置"),
                QStringLiteral("确定恢复所有默认设置？")) != QMessageBox::Yes)
            return;
        resetSettings();
    });

    QAction *about = menuBar()->addAction(QStringLiteral("关于"));
    about->setObjectName(QStringLiteral("actAbout"));
    connect(about, &QAction::triggered, this, [this, about] {
        if (!m_aboutPanel) {
            auto *panel = new MenuPanel(this);
            m_aboutPanel = panel;
            panel->setObjectName(QStringLiteral("aboutDialog"));
            panel->setWindowTitle(QStringLiteral("关于 Reader"));
            panel->resize(320, 160);
            auto *layout = new QVBoxLayout(panel);
            layout->setContentsMargins(16, 14, 16, 14);
            layout->setSpacing(10);
            auto *intro = new QHBoxLayout;
            intro->setSpacing(12);
            auto *icon = new QLabel(panel);
            icon->setObjectName(QStringLiteral("aboutIcon"));
            icon->setPixmap(QIcon(QStringLiteral(":/reader/icon.svg")).pixmap(40, 40));
            intro->addWidget(icon);
            auto *titles = new QVBoxLayout;
            auto *title = new QLabel(QStringLiteral("Reader"), panel);
            title->setObjectName(QStringLiteral("aboutTitle"));
            title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600;"));
            titles->addWidget(title);
            titles->addWidget(new QLabel(QStringLiteral("安静地读完一本书"), panel));
            intro->addLayout(titles, 1);
            layout->addLayout(intro);
            auto *description = new QLabel(QStringLiteral("TXT / EPUB / MOBI / AZW3 / FB2 · 在线阅读"), panel);
            description->setWordWrap(true);
            layout->addWidget(description);
            auto *link = new QLabel(panel);
            link->setObjectName(QStringLiteral("projectLink"));
            link->setText(QStringLiteral("<a href=\"https://github.com/wz150432/Reader-CachyOS\" style=\"color:#385c46\">在 GitHub 查看项目</a>"));
            link->setOpenExternalLinks(true);
            layout->addWidget(link);
        }
        if (m_aboutPanel->isVisible())
            m_aboutPanel->hide();
        else
            m_aboutPanel->showAnchored(menuBar(), menuBar()->actionGeometry(about));
    });
    applyKeyset();
}

void MainWindow::openBook(const QString &path)
{
    if (!leaveEditModeIfActive())
        return;
    QString err;
    auto book = Book::create(path, &err, QRegularExpression(m_settings.chapterRegex));
    if (!book) {
        QMessageBox::warning(this, QStringLiteral("无法打开"), err);
        return;
    }
    const auto saved = m_cache.progress(path);
    saveProgress();
    m_onlineController->close();
    if (m_progressSaveTimer) m_progressSaveTimer->stop();
    m_onlineRestoreChapter = -1;
    m_book = std::move(book);
    m_currentPath = path;
    m_currentRecordRemoved = false;
    auto online = std::dynamic_pointer_cast<OnlineBook>(m_book);
    m_onlineController->setBook(online);
    if (!online && !m_onlineLibrary) m_onlineBridge->stop();
    if (auto wholeBook = findChild<QCheckBox *>("searchWholeBook"))
        wholeBook->setText(online ? tr("已缓存章节") : tr("全书"));
    m_toc->hide();
    // Resolve the central widget geometry before restoring a saved page.
    ensurePolished();
    layout()->activate();
    {
        const QSignalBlocker blocker(m_view);
        m_view->setBook(m_book);
        populateToc();
        if (saved) {
            m_view->goToChapter(saved->chapterIndex);
            m_view->goToPage(saved->pageIndex);
            if (online && m_view->pendingChapter() >= 0) {
                m_onlineRestoreChapter = m_view->pendingChapter();
                m_onlineRestorePage = saved->pageIndex;
                m_onlineRestorePixel = saved->pixelOffset;
            } else {
                m_view->scrollByPixels(saved->pixelOffset);
            }
        }
    }
    onChapterChanged(m_view->currentChapter());
    if (online) requestOnlineChapter(m_view->pendingChapter() >= 0 ? m_view->pendingChapter() : m_view->currentChapter());
    updateTitle();
    m_view->setFocus();
    m_view->setKeyset(m_settings.keyset);
    applyWindowOpacity();
    refreshOpenMenu();
    // m_leaveHideIgnoreUntil = QDateTime::currentMSecsSinceEpoch() + 1000;
}

void MainWindow::showOnlineComponent()
{
    if (m_onlineLibrary) m_onlineLibrary->hide();
    if (!m_onlineComponentDialog) {
        m_onlineComponentDialog = new OnlineComponentDialog(m_onlineComponent, this);
        connect(m_onlineComponentDialog, &OnlineComponentDialog::ready, this, [this] {
            if (m_openOnlineAfterInstall) {
                m_openOnlineAfterInstall = false;
                QTimer::singleShot(0, this, [this] {
                    if (isVisible()) showOnlineReading();
                });
            }
        });
    }
    m_onlineComponentDialog->showAnchored(menuBar());
}

void MainWindow::showOnlineReading()
{
    if (!m_onlineComponent->isEnabled()) {
        m_openOnlineAfterInstall = true;
        showOnlineComponent();
        return;
    }
    if (m_onlineComponentDialog) m_onlineComponentDialog->hide();
    if (!property("onlineSourceError").toString().isEmpty()) {
        QMessageBox::warning(this, tr("书源配置损坏"), tr("原配置已保留，无法保存。请检查：%1").arg(m_onlineRoot + "/sources.json"));
    }
    ensureOnlineLibrary();
    m_onlineLibrary->showAnchored(menuBar());
}

void MainWindow::ensureOnlineLibrary()
{
    if (!m_onlineLibrary) {
        m_onlineLibrary = new OnlineLibraryDialog(m_onlineSources.get(), m_onlineBridge, m_onlineRoot + "/books", this);
        connect(m_onlineLibrary, &OnlineLibraryDialog::openRequested, this, &MainWindow::openBook);
        connect(m_onlineLibrary, &OnlineLibraryDialog::componentRequested, this, &MainWindow::showOnlineComponent);
        connect(m_onlineLibrary, &OnlineLibraryDialog::refreshRequested, this, [this](const QString &path) {
            saveProgress();
            auto online = std::dynamic_pointer_cast<OnlineBook>(m_book);
            m_onlineLibrary->setRequiredChapterUrl(online && m_currentPath == path && m_view->pendingChapter() >= 0
                ? online->chapterUrl(m_view->pendingChapter()) : QString());
        });
        connect(m_onlineLibrary, &OnlineLibraryDialog::tocRefreshed, this, &MainWindow::remapOnlineProgress);
        connect(m_onlineLibrary, &OnlineLibraryDialog::refreshFailed, m_view, &QWidget::setToolTip);
        connect(m_onlineLibrary, &OnlineLibraryDialog::tocRefreshed, this, [this] { m_view->setToolTip({}); });
    }
}

void MainWindow::refreshCurrentBook()
{
    if (!m_book || !leaveEditModeIfActive()) return;
    saveProgress();
    if (!m_book->isOnline()) {
        openBook(m_currentPath);
        return;
    }
    if (!m_onlineComponent->isEnabled()) {
        m_view->setToolTip(tr("在线组件已停用，缓存章节仍可阅读。"));
        return;
    }
    ensureOnlineLibrary();
    m_onlineLibrary->refreshBook(m_currentPath);
}

void MainWindow::requestOnlineChapter(int index)
{
    if (!m_book || !m_book->isOnline()) return;
    if (m_onlineRestoreChapter >= 0 && index != m_onlineRestoreChapter) m_onlineRestoreChapter = -1;
    if (!m_onlineComponent->isEnabled()) {
        if (!m_book->chapterAvailable(index)) {
            const auto error = tr("在线组件未启用。请打开“在线组件”安装或启用后重试；缓存章节仍可阅读。");
            m_view->setChapterError(index, error);
        }
        return;
    }
    m_onlineBridge->setExecutablePath(m_onlineComponent->executablePath());
    m_onlineController->requestChapter(index);
}

void MainWindow::remapOnlineProgress(const QString &path,const QStringList &oldUrls,const QStringList &newUrls)
{
    auto map = [&](int index) { return index >= 0 && index < oldUrls.size() ? newUrls.indexOf(oldUrls[index]) : -1; };
    if (auto progress = m_cache.progress(path)) {
        int index = map(progress->chapterIndex);
        if (index >= 0) { progress->chapterIndex = index; m_cache.upsertProgress(*progress); }
    }
    for (auto bookmark : m_cache.bookmarks(path)) {
        int index = map(bookmark.chapterIndex);
        if (index < 0) continue;
        m_cache.removeBookmark(path, bookmark.created);
        bookmark.chapterIndex = index;
        m_cache.addBookmark(bookmark);
    }
    m_cache.save();
    if (m_currentPath == path) {
        // Persisted indices now address the new TOC. Do not save the old live view again.
        const int pending = map(m_view->pendingChapter());
        const int pendingPage = m_view->pendingPage();
        const bool restoring = m_onlineRestoreChapter >= 0;
        const int restorePage = m_onlineRestorePage;
        const qreal restorePixel = m_onlineRestorePixel;
        m_onlineController->close();
        m_currentRecordRemoved = true;
        openBook(path);
        if (pending >= 0 && m_view->pendingChapter() != pending &&
            (m_view->pendingChapter() >= 0 || m_view->currentChapter() != pending))
            m_view->goToChapter(pending);
        if (pending >= 0 && m_view->pendingChapter() >= 0) {
            m_view->goToPage(pendingPage);
            if (restoring) {
                m_onlineRestoreChapter = pending;
                m_onlineRestorePage = restorePage;
                m_onlineRestorePixel = restorePixel;
            }
        }
    }
}

void MainWindow::openLastRead()
{
    const QStringList recent = m_cache.recentFiles();
    if (!recent.isEmpty())
        openBook(recent.first());
}

void MainWindow::populateToc()
{
    m_toc->clear();
    if (!m_book)
        return;
    const QVector<Chapter> &chapters = m_book->chapters();
    for (int i = 0; i < chapters.size(); ++i) {
        auto *item = new QTreeWidgetItem(m_toc);
        item->setText(0, chapters.at(i).title);
        item->setToolTip(0, chapters.at(i).title);
        item->setData(0, Qt::UserRole, i);
    }
}

void MainWindow::populateOpenMenu(QMenu *menu)
{
    m_deleteConfirmation = nullptr;
    menu->clear();
    const QStringList recent = m_cache.recentFiles();
    if (recent.isEmpty()) {
        QAction *none = menu->addAction(QStringLiteral("暂无书籍"));
        none->setEnabled(false);
    } else {
        for (const QString &path : recent) {
            QString label = QFileInfo(path).completeBaseName();
            if (QFileInfo(path).suffix() == "rnovel") {
                auto onlineBook = Book::create(path);
                if (onlineBook) label = onlineBook->title();
            }
            QAction *item = menu->addAction(label);
            item->setData(path);
            item->setToolTip(path);
            item->setStatusTip(path);
            connect(item, &QAction::triggered, this, [this, path] {
                openBook(path);
            });
        }
    }
    menu->addSeparator();
    QAction *newBook = menu->addAction(QStringLiteral("打开新书..."));
    newBook->setObjectName(QStringLiteral("actOpenNew"));
    connect(newBook, &QAction::triggered, this, &MainWindow::chooseNewBook);
    QAction *online = menu->addAction(tr("在线阅读…"));
    online->setObjectName("actOnlineReading");
    connect(online, &QAction::triggered, this, &MainWindow::showOnlineReading);
    QAction *clearRecent = menu->addAction(QStringLiteral("清空(&C)"));
    clearRecent->setObjectName(QStringLiteral("actClearRecent"));
    connect(clearRecent, &QAction::triggered, this, &MainWindow::clearRecentList);
}

void MainWindow::refreshOpenMenu()
{
    QTimer::singleShot(0, this, [this] {
        if (QMenu *menu = findChild<QMenu *>(QStringLiteral("openMenu")))
            populateOpenMenu(menu);
    });
}

void MainWindow::showOpenMenu()
{
    if (QMenu *menu = findChild<QMenu *>(QStringLiteral("openMenu"))) {
        populateOpenMenu(menu);
        menu->popup(QCursor::pos());
    }
}

void MainWindow::chooseNewBook()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开新书"), QString(),
        QStringLiteral("书籍文件 (*.txt *.epub *.mobi *.azw *.azw3 *.fb2 *.TXT *.EPUB *.MOBI *.AZW *.AZW3 *.FB2);;所有文件 (*)"));
    if (!path.isEmpty())
        openBook(path);
}

void MainWindow::onChapterChanged(int index)
{
    if (!m_book || index < 0 || index >= m_book->chapters().size())
        return;
    if (QTreeWidgetItem *item = m_toc->topLevelItem(index)) {
        m_toc->setCurrentItem(item);
        m_toc->scrollToItem(item);
    }
    updateTitle();
    if (m_book->isOnline() && m_view->pendingChapter() < 0 && m_onlineController->targetChapter() != index)
        requestOnlineChapter(index);
    saveProgress();
}

void MainWindow::onPageChanged(int)
{
    // Cap disk writes during continuous scrolling while bounding the loss window.
    if (!m_progressSaveTimer->isActive())
        m_progressSaveTimer->start();
}

void MainWindow::onSearchRequested()
{
    if (QToolBar *bar = findChild<QToolBar *>(QStringLiteral("searchBar"))) {
        bar->setVisible(true);
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    }
}

void MainWindow::closeSearchBar()
{
    if (QToolBar *bar = findChild<QToolBar *>(QStringLiteral("searchBar")))
        bar->setVisible(false);
    m_searchEdit->clear();
    m_view->clearMatch();
    m_view->setFocus();
}

void MainWindow::onJumpRequested()
{
    if (!m_book)
        return;
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("跳转到进度"));
    auto *spin = new QSpinBox(&dlg);
    spin->setRange(0, 100);
    spin->setSuffix(QStringLiteral(" %"));
    spin->setValue(currentProgressPercent());
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto *layout = new QVBoxLayout(&dlg);
    layout->addWidget(spin);
    layout->addWidget(box);
    if (dlg.exec() == QDialog::Accepted)
        m_view->jumpToBookProgress(spin->value() / 100.0);
}

void MainWindow::onBookmarkRequested()
{
    addBookmarkForCurrentBook();
}

void MainWindow::addBookmarkForCurrentBook()
{
    // A pending chapter may leave an older committed chapter visible; bookmark
    // that displayed chapter, and never the default indices of an empty view.
    if (m_currentPath.isEmpty() || !m_book || !m_view->committedChapterReady())
        return;
    const int ci = m_view->currentChapter();
    QString title;
    if (ci >= 0 && ci < m_book->chapters().size())
        title = m_book->chapters().at(ci).title;
    m_cache.addBookmark({m_currentPath, ci, m_view->currentPage(), title,
                         QDateTime::currentSecsSinceEpoch()});
    m_cache.save();
}

void MainWindow::openBookmarkList()
{
    if (m_currentPath.isEmpty())
        return;
    const QVector<Bookmark> marks = m_cache.bookmarks(m_currentPath);
    BookmarkDialog dlg(marks, this);
    connect(&dlg, &BookmarkDialog::jumpRequested, this, [this](const Bookmark &b) {
        m_view->goToChapter(b.chapterIndex);
        m_view->goToPage(b.pageIndex);
    });
    connect(&dlg, &BookmarkDialog::deleteRequested, this, [this](const Bookmark &b) {
        m_cache.removeBookmark(m_currentPath, b.created);
        m_cache.save();
    });
    dlg.exec();
}

void MainWindow::onDisplaySettingsChanged(const DisplaySettings &settings)
{
    m_settings.display = settings;
    applyWindowOpacity();
    m_settings.save();
}

void MainWindow::applyKeyset()
{
    m_view->setKeyset(m_settings.keyset);
    if (QAction *open = findChild<QAction *>(QStringLiteral("actOpen")))
        open->setShortcut(m_settings.keyset.shortcut(KeyAction::OpenFile));
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (auto *menu = qobject_cast<QMenu *>(obj);
        menu && event->type() == QEvent::Polish) {
        // Rounded QSS alone leaves an opaque native popup behind its corners.
        // QWidget::isAncestorOf stops at popup windows, so follow ownership.
        QObject *owner = menu->parent();
        while (owner && owner != this)
            owner = owner->parent();
        if (owner == this) {
            menu->setAttribute(Qt::WA_TranslucentBackground);
            menu->setWindowFlag(Qt::FramelessWindowHint);
        }
    }
    if (obj == m_dragMenuBar && event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton
            && !m_dragMenuBar->actionAt(mouse->position().toPoint())) {
            if (QWindow *handle = windowHandle())
                return handle->startSystemMove();
        }
    }
    if (obj == m_view && event->type() == QEvent::Resize && m_toc)
        m_toc->setGeometry(m_view->rect());
    if (obj == m_toc && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        m_toc->hide();
        m_view->setFocus();
        return true;
    }
    if (auto *menu = qobject_cast<QMenu *>(obj);
        menu && menu->objectName() == QStringLiteral("openMenu")) {
        // QMenu can activate the highlighted book on release even though the
        // matching right-button press was consumed to show confirmation.
        if (event->type() == QEvent::MouseButtonRelease
            && static_cast<QMouseEvent *>(event)->button() == Qt::RightButton)
            return true;
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::RightButton) {
                QAction *action = menu->actionAt(mouse->position().toPoint());
                const QString path = action ? action->data().toString() : QString();
                if (!path.isEmpty()) {
                    if (m_deleteConfirmation) {
                        menu->removeAction(m_deleteConfirmation);
                        m_deleteConfirmation->deleteLater();
                    }
                    auto *confirmation = new QWidgetAction(menu);
                    confirmation->setObjectName(QStringLiteral("deleteConfirmation"));
                    auto *panel = new QWidget(menu);
                    auto *layout = new QHBoxLayout(panel);
                    layout->setContentsMargins(8, 4, 8, 4);
                    layout->addWidget(new QLabel(QStringLiteral("删除记录？"), panel));
                    auto *remove = new QPushButton(QStringLiteral("删除"), panel);
                    remove->setObjectName(QStringLiteral("confirmDelete"));
                    auto *cancel = new QPushButton(QStringLiteral("取消"), panel);
                    cancel->setObjectName(QStringLiteral("cancelDelete"));
                    layout->addWidget(remove);
                    layout->addWidget(cancel);
                    confirmation->setDefaultWidget(panel);
                    const auto actions = menu->actions();
                    const int index = actions.indexOf(action);
                    menu->insertAction(index + 1 < actions.size() ? actions.at(index + 1) : nullptr,
                                       confirmation);
                    m_deleteConfirmation = confirmation;
                    const auto dismiss = [this, menu, confirmation] {
                        menu->removeAction(confirmation);
                        if (m_deleteConfirmation == confirmation)
                            m_deleteConfirmation = nullptr;
                        confirmation->deleteLater();
                    };
                    connect(cancel, &QPushButton::clicked, this, dismiss);
                    connect(remove, &QPushButton::clicked, this, [this, path, dismiss] {
                        dismiss();
                        removeRecentFile(path);
                    });
                    cancel->setFocus();
                }
                return true;
            }
        }
    }
    if (obj == m_searchEdit && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        closeSearchBar();
        return true;
    }
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        auto *w = qobject_cast<QWidget *>(obj);
        if (!w || w->window() != this)
            return false;
        auto *focus = QApplication::focusWidget();
        if (qApp->activeModalWidget() || qobject_cast<QLineEdit *>(focus))
            return false;
        auto *ke = static_cast<QKeyEvent *>(event);
        if (m_toc->isVisible() && (focus == m_toc || m_toc->isAncestorOf(focus))
            && ke->modifiers() == Qt::NoModifier) {
            switch (ke->key()) {
            case Qt::Key_Up: case Qt::Key_Down: case Qt::Key_Left: case Qt::Key_Right:
            case Qt::Key_PageUp: case Qt::Key_PageDown: case Qt::Key_Home: case Qt::Key_End:
            case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_Space:
                if (event->type() == QEvent::ShortcutOverride)
                    event->accept();
                return false;
            default:
                break;
            }
        }
        const QKeySequence seq(ke->keyCombination());
        KeyAction matched = static_cast<KeyAction>(-1);
        const QList<KeyAction> actions = m_settings.keyset.actions();
        for (const KeyAction a : actions) {
            if (m_settings.keyset.shortcut(a).matches(seq) == QKeySequence::ExactMatch) {
                matched = a;
                break;
            }
        }
        if (matched != static_cast<KeyAction>(-1)) {
            if (event->type() == QEvent::ShortcutOverride) {
                event->accept();
                return false;
            }
            handleKeyAction(matched);
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void MainWindow::handleKeyAction(KeyAction a)
{
    switch (a) {
    case KeyAction::PageDown:
        m_view->pageDown();
        break;
    case KeyAction::PageUp:
        m_view->pageUp();
        break;
    case KeyAction::LineDown:
        m_view->lineDown();
        break;
    case KeyAction::LineUp:
        m_view->lineUp();
        break;
    case KeyAction::ChapterDown:
        if (m_book)
            m_view->goToChapter(m_view->currentChapter() + 1);
        break;
    case KeyAction::ChapterUp:
        if (m_book)
            m_view->goToChapter(m_view->currentChapter() - 1);
        break;
    case KeyAction::FontZoomIn:
        m_view->fontZoomIn();
        break;
    case KeyAction::FontZoomOut:
        m_view->fontZoomOut();
        break;
    case KeyAction::AutoPage:
        m_view->toggleAutoPage();
        break;
    case KeyAction::Search:
        onSearchRequested();
        break;
    case KeyAction::Jump:
        onJumpRequested();
        break;
    case KeyAction::AddBookmark:
        onBookmarkRequested();
        break;
    case KeyAction::Fullscreen:
        toggleFullscreen();
        break;
    case KeyAction::HideBorder:
        toggleHideBorder();
        break;
    case KeyAction::AlwaysOnTop:
        toggleAlwaysOnTop();
        break;
    case KeyAction::HideWindow:
        showHideWindow();
        break;
    case KeyAction::OpenFile:
        showOpenMenu();
        break;
    case KeyAction::Quit:
        quitApplication();
        break;
    case KeyAction::MouseLeaveHide:
        // 鼠标离开隐藏 / Ctrl 恢复功能暂时暂停。
        break;
    case KeyAction::EditMode:
        toggleEditMode();
        break;
    }
}

void MainWindow::toggleEditMode()
{
    if (editModeActive()) {
        leaveEditModeIfActive();
        return;
    }
    if (m_currentPath.isEmpty() || !m_book) {
        QMessageBox::information(this, QStringLiteral("编辑模式"),
                                 QStringLiteral("请先打开一本书"));
        return;
    }
    if (QFileInfo(m_currentPath).suffix().compare(QStringLiteral("txt"), Qt::CaseInsensitive) != 0) {
        QMessageBox::information(this, QStringLiteral("编辑模式"),
                                 QStringLiteral("编辑模式仅支持 TXT 文件，其他格式可正常阅读。"));
        return;
    }
    QFile f(m_currentPath);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("无法打开"),
                             QStringLiteral("无法读取当前文件"));
        return;
    }
    if (!m_editor) {
        m_editor = new QPlainTextEdit(m_view);
        m_editor->setObjectName(QStringLiteral("bookEditor"));
        m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    }
    m_editor->setPlainText(QString::fromUtf8(f.readAll()));
    m_editor->document()->setModified(false);
    m_editor->setGeometry(m_view->rect());
    m_editor->raise();
    m_editor->show();
    m_editor->setFocus();
}

bool MainWindow::leaveEditModeIfActive()
{
    if (!editModeActive())
        return true;
    if (m_editor->document()->isModified()) {
        const auto choice = QMessageBox::question(
            this, QStringLiteral("保存修改"),
            QStringLiteral("编辑内容还没有保存，是否保存？"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if (choice == QMessageBox::Cancel)
            return false;
        if (choice == QMessageBox::Save)
            return saveEditMode();
    }
    leaveEditMode();
    return true;
}

void MainWindow::leaveEditMode()
{
    if (!m_editor)
        return;
    m_editor->hide();
    m_view->refreshLayout();
    m_view->setFocus();
}

bool MainWindow::saveEditMode()
{
    if (!m_editor || m_currentPath.isEmpty())
        return false;
    QSaveFile sf(m_currentPath);
    if (!sf.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("无法写入当前文件"));
        return false;
    }
    sf.write(m_editor->toPlainText().toUtf8());
    if (!sf.commit()) {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("无法写入当前文件"));
        return false;
    }
    const int chapter = m_view->currentChapter();
    leaveEditMode();
    openBook(m_currentPath);
    if (m_book && chapter >= 0 && chapter < m_book->chapters().size())
        m_view->goToChapter(chapter);
    return true;
}

void MainWindow::resetSettings()
{
    Settings fresh;
    m_settings.display = fresh.display;
    m_settings.keyset.reset();
    m_settings.behavior = fresh.behavior;
    m_settings.tags = fresh.tags;
    m_settings.chapterRegex = fresh.chapterRegex;
    m_settings.save();
    m_view->setSettings(m_settings.display);
    m_view->setKeyset(m_settings.keyset);
    m_view->setBehavior(m_settings.behavior);
    m_view->setTags(m_settings.tags);
    applyKeyset();
    applyWindowOpacity();
    syncGlobalHide();
}

void MainWindow::updateTitle()
{
    QString title;
    if (m_book && !m_book->chapters().isEmpty())
        title = m_book->chapters().at(m_view->currentChapter()).title;
    if (title.isEmpty())
        title = m_book ? m_book->title() : QStringLiteral("Reader");
    setWindowTitle(QStringLiteral("%1 - Reader").arg(title));
}

void MainWindow::saveProgress()
{
    if (m_progressSaveTimer) m_progressSaveTimer->stop();
    if (m_currentPath.isEmpty() || m_currentRecordRemoved || m_restoringOnline || m_onlineRestoreChapter >= 0
        || !m_view->committedChapterReady() || m_view->pendingChapter() >= 0)
        return;
    m_cache.upsertProgress({m_currentPath, m_view->currentChapter(), m_view->currentPage(),
                            QDateTime::currentSecsSinceEpoch(), m_view->pixelOffset()});
    m_cache.save();
}

void MainWindow::clearRecentList()
{
    m_currentRecordRemoved = !m_currentPath.isEmpty();
    m_cache.clearRecent();
    m_cache.save();
    refreshOpenMenu();
}

void MainWindow::removeRecentFile(const QString &path)
{
    if (path == m_currentPath)
        m_currentRecordRemoved = true;
    m_cache.removeRecentFile(path);
    m_cache.save();
    refreshOpenMenu();
}

QString MainWindow::currentBookTitle() const
{
    if (!m_book || m_book->chapters().isEmpty())
        return QString();
    return m_book->chapters().at(m_view->currentChapter()).title;
}

int MainWindow::tocItemCount() const
{
    return m_toc->topLevelItemCount();
}

int MainWindow::currentChapter() const
{
    return m_view->currentChapter();
}

int MainWindow::currentProgressPercent() const
{
    if (!m_view)
        return 0;
    return qBound(0, qRound(m_view->currentBookProgress() * 100.0), 100);
}

void MainWindow::showHideWindow()
{
    if (isVisible()) {
        m_hiddenWasMaximized = isMaximized();
        m_hiddenGeometry = geometry();
        m_hiddenByMouseLeave = false;
        if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
            const bool canRestore = (m_tray && m_tray->isVisible())
                || m_globalHideBindReady;
            if (!canRestore)
                return;
            rememberNiriFloatingPosition();
            hide();
            return;
        }
        hide();
    } else {
        m_hiddenByMouseLeave = false;
        if (m_hiddenWasMaximized)
            showMaximized();
        else {
            prepareNiriFloatingPositionRestore();
            if (!m_hiddenGeometry.isEmpty())
                setGeometry(m_hiddenGeometry);
            show();
        }
        raise();
        activateWindow();
        m_view->setFocus();
        restoreNiriFloatingPosition();
        // m_leaveHideIgnoreUntil = QDateTime::currentMSecsSinceEpoch() + 1000;
    }
}

void MainWindow::quitApplication()
{
    saveWindowState();
    saveProgress();
    if (m_tray)
        m_tray->hide();
    QCoreApplication::quit();
}

void MainWindow::applyWindowState()
{
    if (!m_settings.windowGeometry.isEmpty())
        restoreGeometry(m_settings.windowGeometry);
    if (!m_settings.windowState.isEmpty())
        restoreState(m_settings.windowState);
    prepareNiriFloatingPositionRestore();
}

void MainWindow::rememberNiriFloatingPosition()
{
    if (m_restoringNiriPosition) return;
    const auto window = m_niriClient->window();
    if (!window) return;
    if (m_settings.hasNiriFloatingPosition && m_settings.niriFloatingPosition == window->position)
        return;
    m_settings.niriFloatingPosition = window->position;
    m_settings.hasNiriFloatingPosition = true;
    m_niriSettingsTimer->start();
}

void MainWindow::prepareNiriFloatingPositionRestore()
{
    if (!m_settings.hasNiriFloatingPosition || !m_niriRuleReady
        || qEnvironmentVariableIsEmpty("NIRI_SOCKET"))
        return;
    if (!m_positioningWindow)
        m_titleBeforePositioning = windowTitle();
    m_positioningWindow = true;
    setWindowTitle(QStringLiteral("Reader positioning"));
}

void MainWindow::restoreNiriFloatingPosition()
{
    if (!m_settings.hasNiriFloatingPosition || qEnvironmentVariableIsEmpty("NIRI_SOCKET"))
        return;
    m_restoringNiriPosition = true;
    const int serial = ++m_niriRestoreSerial;
    QTimer::singleShot(0, this, [this, serial] {
        tryRestoreNiriFloatingPosition(16, false, serial);
    });
}

void MainWindow::finishNiriFloatingPositionRestore()
{
    m_restoringNiriPosition = false;
    if (!m_positioningWindow)
        return;
    m_positioningWindow = false;
    setWindowTitle(m_titleBeforePositioning);
}

void MainWindow::tryRestoreNiriFloatingPosition(int attemptsLeft, bool moveIssued, int serial)
{
    if (serial != m_niriRestoreSerial || !isVisible())
        return;
    const auto window = m_niriClient->window();
    if (window && (window->position - m_settings.niriFloatingPosition).manhattanLength() <= 2) {
        finishNiriFloatingPositionRestore();
        return;
    }
    if (window && !moveIssued)
        moveIssued = m_niriClient->moveWindow(window->id, m_settings.niriFloatingPosition);
    if (attemptsLeft > 1) {
        QTimer::singleShot(80, this, [this, attemptsLeft, moveIssued, serial] {
            tryRestoreNiriFloatingPosition(attemptsLeft - 1, moveIssued, serial);
        });
    } else {
        finishNiriFloatingPositionRestore();
    }
}

void MainWindow::saveWindowState()
{
    rememberNiriFloatingPosition();
    m_settings.windowGeometry = saveGeometry();
    m_settings.windowState = saveState();
    m_settings.menuBarHidden = menuBar()->isHidden();
    m_settings.save();
}

void MainWindow::toggleMouseLeaveHide()
{
    // 功能暂时暂停：保留方法入口，但不再切换运行状态。
    /*
    m_settings.behavior.mouseLeaveHideEnabled = !m_settings.behavior.mouseLeaveHideEnabled;
    m_settings.save();
    m_view->setBehavior(m_settings.behavior);
    applyMouseLeaveHideMode();
    m_leaveHideIgnoreUntil = QDateTime::currentMSecsSinceEpoch() + 1000;
    */
}

bool MainWindow::mouseLeaveHideActive() const
{
    return m_mouseWatchTimer && m_mouseWatchTimer->isActive();
}

void MainWindow::applyMouseLeaveHideMode()
{
    if (!m_mouseWatchTimer)
        return;
    // 功能暂时暂停：统一停止鼠标监听，下面原逻辑保留注释。
    m_mouseWatchTimer->stop();
    /*
    if (m_settings.behavior.mouseLeaveHideEnabled)
        m_mouseWatchTimer->start();
    else
        m_mouseWatchTimer->stop();
    */
}

void MainWindow::onMouseWatchTick()
{
    // 功能暂时暂停：定时器不会再启动，这里直接返回。
    return;
    /*
    if (!m_settings.behavior.mouseLeaveHideEnabled)
        return;

    const QPoint cursor = QCursor::pos();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (!isVisible()) {
        const bool ctrlPressed =
            QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier)
            || QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
        if (ctrlPressed && !m_hiddenGeometry.isEmpty()
            && m_hiddenGeometry.contains(cursor)) {
            m_leaveHideIgnoreUntil = now + 800;
            showHideWindow();
        }
        return;
    }

    if (isVisible()
        && now >= m_leaveHideIgnoreUntil
        && !geometry().contains(cursor)
        && !QApplication::activePopupWidget()
        && !QApplication::activeModalWidget()) {
        m_hiddenWasMaximized = isMaximized();
        m_hiddenGeometry = geometry();
        m_hiddenByMouseLeave = true;
        hide();
    }
    */
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    if (m_editor && m_editor->isVisible())
        m_editor->setGeometry(m_view->rect());
    QMainWindow::resizeEvent(event);
    updateResizeGrips();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        updateResizeGrips();
    if (m_dragMenuBar && (event->type() == QEvent::FontChange
                         || event->type() == QEvent::StyleChange))
        updateChromeSpacing();
}

void MainWindow::createResizeGrips()
{
    const auto add = [this](Qt::Edges edges, Qt::CursorShape cursor, const char *name) {
        m_resizeGrips.append(new ResizeGrip(this, edges, cursor, QString::fromLatin1(name)));
    };
    add(Qt::TopEdge | Qt::LeftEdge, Qt::SizeFDiagCursor, "resize-top-left");
    add(Qt::TopEdge, Qt::SizeVerCursor, "resize-top");
    add(Qt::TopEdge | Qt::RightEdge, Qt::SizeBDiagCursor, "resize-top-right");
    add(Qt::RightEdge, Qt::SizeHorCursor, "resize-right");
    add(Qt::BottomEdge | Qt::RightEdge, Qt::SizeFDiagCursor, "resize-bottom-right");
    add(Qt::BottomEdge, Qt::SizeVerCursor, "resize-bottom");
    add(Qt::BottomEdge | Qt::LeftEdge, Qt::SizeBDiagCursor, "resize-bottom-left");
    add(Qt::LeftEdge, Qt::SizeHorCursor, "resize-left");
    updateResizeGrips();
}

void MainWindow::updateResizeGrips()
{
    if (m_resizeGrips.size() != 8)
        return;
    if (isFullScreen() || isMaximized()) {
        for (QWidget *grip : m_resizeGrips)
            grip->hide();
        return;
    }
    const int w = width();
    const int h = height();
    const int corner = qMin(16, qMin(w, h) / 2);
    const int edge = qMin(8, corner);
    const QList<QRect> areas = {
        QRect(0, 0, corner, corner),
        QRect(corner, 0, w - 2 * corner, edge),
        QRect(w - corner, 0, corner, corner),
        QRect(w - edge, corner, edge, h - 2 * corner),
        QRect(w - corner, h - corner, corner, corner),
        QRect(corner, h - edge, w - 2 * corner, edge),
        QRect(0, h - corner, corner, corner),
        QRect(0, corner, edge, h - 2 * corner)
    };
    for (int i = 0; i < m_resizeGrips.size(); ++i) {
        QWidget *grip = m_resizeGrips.at(i);
        grip->setGeometry(areas.at(i));
        grip->setVisible(!areas.at(i).isEmpty());
        grip->raise();
    }
}

void MainWindow::handleRemoteCommand(const QString &command)
{
    const bool hide = command == QStringLiteral("hide")
        || (command == QStringLiteral("toggle-hide") && isVisible());
    if (hide) {
        if (!isVisible())
            return;
        showHideWindow();
        return;
    }
    if (command == QStringLiteral("show")
        || (command == QStringLiteral("toggle-hide") && !isVisible())) {
        if (!isVisible())
            showHideWindow();
    }
}

void MainWindow::syncGlobalHide()
{
    const int serial = ++m_globalHideSyncSerial;
    m_globalHideBindReady = false;
    const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
                            .filePath(QStringLiteral("niri"));
    const QString path = dir + QStringLiteral("/binds.kdl");
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;
    const QString content = QString::fromUtf8(f.readAll());
    f.close();
    QString patched = content;
    const QString key = niriKeyFromSequence(
        m_settings.keyset.shortcut(KeyAction::HideWindow));
    const QString helper = QDir(QCoreApplication::applicationDirPath())
                               .filePath(QStringLiteral("reader-control"));
    const QString command = QFileInfo(helper).isExecutable()
        ? helper : QCoreApplication::applicationFilePath();
    if (!reader::patchReaderGlobalHide(&patched, key, command))
        return;
    if (patched == content) {
        m_globalHideBindReady = true;
        return;
    }
    QSaveFile sf(path);
    if (!sf.open(QIODevice::WriteOnly))
        return;
    sf.write(patched.toUtf8());
    if (!sf.commit())
        return;
    m_niriClient->reloadConfig([this, serial](bool success) {
        if (serial == m_globalHideSyncSerial) m_globalHideBindReady = success;
    });
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();
}

void MainWindow::toggleAlwaysOnTop()
{
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        if (!m_topHintShown) {
            m_topHintShown = true;
            QToolTip::showText(QCursor::pos(), QStringLiteral("Wayland 桌面不支持窗口置顶"));
        }
        return;
    }
    setWindowFlag(Qt::WindowStaysOnTopHint, !(windowFlags() & Qt::WindowStaysOnTopHint));
    show();
}

void MainWindow::updateChromeSpacing()
{
    // Keep the reading viewport unchanged when the menu is hidden, including
    // startup with a saved hidden menu. The reserved strip stays transparent.
    menuBar()->ensurePolished();
    const int reservedHeight = menuBar()->isHidden() ? menuBar()->sizeHint().height() : 0;
    setContentsMargins(0, reservedHeight, 0, 0);
    m_view->setTopCornersRounded(menuBar()->isHidden());
}

void MainWindow::toggleHideBorder()
{
    if (menuBar()) {
        const bool hideFrame = !menuBar()->isHidden();
        const bool wasVisible = isVisible();
        const bool wasMaximized = isMaximized();
        if (wasVisible)
            rememberNiriFloatingPosition();
        menuBar()->setVisible(!hideFrame);
        updateChromeSpacing();
        setWindowFlag(Qt::FramelessWindowHint, hideFrame);
        m_settings.menuBarHidden = hideFrame;
        m_settings.save();
        if (wasVisible) {
            prepareNiriFloatingPositionRestore();
            if (wasMaximized)
                showMaximized();
            else
                show();
            restoreNiriFloatingPosition();
        }
        applyWindowOpacity();
    }
}

void MainWindow::toggleAutoPage()
{
    m_view->toggleAutoPage();
}

bool MainWindow::autoPageActive() const
{
    return m_view->isAutoPaging();
}

void MainWindow::createTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    m_tray = new QSystemTrayIcon(this);
    m_tray->setToolTip(QStringLiteral("Reader"));
    auto *menu = new QMenu(this);
    menu->addAction(QStringLiteral("显示/隐藏"), this, &MainWindow::showHideWindow);
    menu->addAction(QStringLiteral("退出"), this, &MainWindow::quitApplication);
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason r) {
                if (r == QSystemTrayIcon::Trigger)
                    showHideWindow();
            });
    m_tray->show();
}

void MainWindow::applyWindowOpacity()
{
    // 窗口本身保持实体，透明只作用于阅读区背景（由 ReadingView 用带 alpha 的背景绘制）
    setWindowOpacity(1.0);
    if (!m_view)
        return;
    // 菜单栏显示时禁止完全透明，包括恢复菜单栏及加载已保存设置时。
    if (menuBar() && !menuBar()->isHidden() && m_settings.display.windowAlpha == 0) {
        m_settings.display.windowAlpha = 1;
        m_view->setSettings(m_settings.display);
        m_settings.save();
    }
    const bool fullyTransparent = m_settings.display.windowAlpha == 0;
    if (menuBar()) {
        menuBar()->setStyleSheet(fullyTransparent
            ? QStringLiteral("QMenuBar { background: transparent; }")
            : QString());
    }
    const QString toolbarStyle = fullyTransparent
        ? QStringLiteral("QToolBar { background: transparent; }")
        : QString();
    const QList<QToolBar *> toolbars = findChildren<QToolBar *>();
    for (QToolBar *bar : toolbars)
        bar->setStyleSheet(toolbarStyle);
    m_toc->setStyleSheet(QStringLiteral("QTreeWidget { background: palette(base); }"));
    m_view->update();
    if (!QGuiApplication::platformName().startsWith(QLatin1String("wayland")))
        return;
    syncNiriWindowRule();
}

void MainWindow::syncNiriWindowRule()
{
    const int serial = ++m_niriRuleSyncSerial;
    m_niriRuleReady = false;
    if (qEnvironmentVariableIsEmpty("NIRI_SOCKET"))
        return;
    const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
                            .filePath(QStringLiteral("niri"));
    const QString path = dir + QStringLiteral("/rules.kdl");
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;
    const QString content = QString::fromUtf8(f.readAll());
    f.close();
    QString patched = content;
    // niri 端固定 1.0，保证整窗不被淡出，文字始终不透明
    const std::optional<QPoint> position = m_settings.hasNiriFloatingPosition
        ? std::optional<QPoint>(m_settings.niriFloatingPosition) : std::nullopt;
    if (!reader::patchReaderOpacity(&patched, 1.0, position))
        return;
    if (patched == content) {
        m_niriRuleReady = true;
        return;
    }
    QSaveFile sf(path);
    if (!sf.open(QIODevice::WriteOnly))
        return;
    sf.write(patched.toUtf8());
    if (!sf.commit())
        return;
    m_niriClient->reloadConfig([this, serial](bool success) {
        if (serial == m_niriRuleSyncSerial) m_niriRuleReady = success;
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!leaveEditModeIfActive()) {
        event->ignore();
        return;
    }
    saveWindowState();
    saveProgress();
    if (m_settings.behavior.minimizeToTray && m_tray && m_tray->isVisible()) {
        event->ignore();
        m_hiddenWasMaximized = isMaximized();
        m_hiddenGeometry = geometry();
        m_hiddenByMouseLeave = false;
        hide();
        return;
    }
    QMainWindow::closeEvent(event);
}

}
