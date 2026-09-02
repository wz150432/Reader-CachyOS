#include "app/MainWindow.h"
#include "app/AdvancedSettingsDialog.h"
#include "app/BasicSettingsDialog.h"
#include "app/BookmarkDialog.h"
#include "app/KeysetDialog.h"
#include "app/ReadingView.h"
#include "app/SettingsDialog.h"
#include "app/TagsetDialog.h"
#include "core/NiriConfig.h"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCursor>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolTip>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <utility>

namespace reader {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    QApplication::instance()->installEventFilter(this);
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

    auto *dock = new QDockWidget(QStringLiteral("目录"), this);
    dock->setObjectName(QStringLiteral("tocDock"));
    m_toc = new QTreeWidget(dock);
    m_toc->setHeaderHidden(true);
    dock->setWidget(m_toc);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    connect(m_toc, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        m_view->goToChapter(item->data(0, Qt::UserRole).toInt());
    });

    auto *searchBar = addToolBar(QStringLiteral("搜索"));
    searchBar->setObjectName(QStringLiteral("searchBar"));
    searchBar->setMovable(false);
    m_searchEdit = new QLineEdit(searchBar);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索（Enter 下一个，Esc 关闭）"));
    searchBar->addWidget(m_searchEdit);
    auto *wholeBook = new QCheckBox(QStringLiteral("全书"), searchBar);
    searchBar->addWidget(wholeBook);
    connect(wholeBook, &QCheckBox::toggled, m_view, &ReadingView::setSearchWholeBook);
    searchBar->setVisible(false);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this] {
        m_view->findNext(m_searchEdit->text());
    });

    buildMenus();
    updateTitle();
    resize(960, 720);
    applyWindowOpacity();
    createTrayIcon();
}

void MainWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(QStringLiteral("文件"));
    QMenu *openMenu = file->addMenu(QStringLiteral("打开(&O)"));
    openMenu->setObjectName(QStringLiteral("openMenu"));
    openMenu->menuAction()->setObjectName(QStringLiteral("actOpen"));
    openMenu->menuAction()->setShortcut(QKeySequence::Open);
    connect(openMenu, &QMenu::aboutToShow, this, [this, openMenu] {
        populateOpenMenu(openMenu);
    });
    populateOpenMenu(openMenu);
    QAction *clearRecent = file->addAction(QStringLiteral("清空(&C)"));
    clearRecent->setObjectName(QStringLiteral("actClearRecent"));
    connect(clearRecent, &QAction::triggered, this, &MainWindow::clearRecentList);
    QAction *quit = file->addAction(QStringLiteral("退出(&X)"));
    quit->setObjectName(QStringLiteral("actQuit"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    QMenu *tocMenu = menuBar()->addMenu(QStringLiteral("目录"));
    QAction *tocToggle = tocMenu->addAction(QStringLiteral("显示/隐藏目录"));
    connect(tocToggle, &QAction::triggered, this, [this] {
        if (QDockWidget *dock = findChild<QDockWidget *>(QStringLiteral("tocDock")))
            dock->setVisible(!dock->isVisible());
    });

    QMenu *bookmark = menuBar()->addMenu(QStringLiteral("书签"));
    QAction *bm = bookmark->addAction(QStringLiteral("添加书签"));
    connect(bm, &QAction::triggered, this, &MainWindow::onBookmarkRequested);
    QAction *bmList = bookmark->addAction(QStringLiteral("书签列表"));
    connect(bmList, &QAction::triggered, this, &MainWindow::openBookmarkList);

    QMenu *settings = menuBar()->addMenu(QStringLiteral("设置"));
    QAction *display = settings->addAction(QStringLiteral("显示设置"));
    connect(display, &QAction::triggered, this, [this] {
        SettingsDialog dlg(&m_settings, this);
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
            applyWindowOpacity();
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
        if (dlg.exec() == QDialog::Accepted)
            applyKeyset();
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

    QMenu *help = menuBar()->addMenu(QStringLiteral("帮助"));
    QAction *about = help->addAction(QStringLiteral("关于 ..."));
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(this, QStringLiteral("关于"),
            QStringLiteral("Reader（CachyOS 原生版）\n\n"
                           "本地 TXT/EPUB/MOBI 阅读器，功能参考开源项目 binbyu/Reader。\n"
                           "本版本：TXT 阅读核心（第二阶段进行中）。"));
    });
    applyKeyset();
}

void MainWindow::openBook(const QString &path)
{
    QString err;
    auto book = Book::create(path, &err, QRegularExpression(m_settings.chapterRegex));
    if (!book) {
        QMessageBox::warning(this, QStringLiteral("无法打开"), err);
        return;
    }
    saveProgress();
    m_book = std::move(book);
    m_currentPath = path;
    m_view->setBook(m_book);
    populateToc();
    if (const auto p = m_cache.progress(path)) {
        m_view->goToChapter(p->chapterIndex);
        m_view->goToPage(p->pageIndex);
    }
    updateTitle();
    m_view->setFocus();
    m_view->setKeyset(m_settings.keyset);
    applyWindowOpacity();
    refreshOpenMenu();
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
        item->setData(0, Qt::UserRole, i);
    }
}

void MainWindow::populateOpenMenu(QMenu *menu)
{
    menu->clear();
    QAction *header = menu->addAction(QStringLiteral("最近阅读"));
    header->setEnabled(false);
    const QStringList recent = m_cache.recentFiles();
    if (recent.isEmpty()) {
        QAction *none = menu->addAction(QStringLiteral("暂无最近阅读"));
        none->setEnabled(false);
    } else {
        const int count = qMin(recent.size(), 10);
        for (int i = 0; i < count; ++i) {
            const QString path = recent.at(i);
            QAction *item = menu->addAction(QFileInfo(path).completeBaseName());
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
}

void MainWindow::refreshOpenMenu()
{
    QTimer::singleShot(0, this, [this] {
        if (QMenu *menu = findChild<QMenu *>(QStringLiteral("openMenu")))
            populateOpenMenu(menu);
    });
}

void MainWindow::chooseNewBook()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开新书"), QString(),
        QStringLiteral("书籍文件 (*.txt);;所有文件 (*)"));
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
    saveProgress();
}

void MainWindow::onPageChanged(int)
{
    saveProgress();
}

void MainWindow::onSearchRequested()
{
    if (QToolBar *bar = findChild<QToolBar *>(QStringLiteral("searchBar"))) {
        bar->setVisible(true);
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    }
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
    if (m_currentPath.isEmpty() || !m_book)
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
    m_settings.save();
    applyWindowOpacity();
}

void MainWindow::applyKeyset()
{
    m_view->setKeyset(m_settings.keyset);
    if (QAction *open = findChild<QAction *>(QStringLiteral("actOpen")))
        open->setShortcut(m_settings.keyset.shortcut(KeyAction::OpenFile));
    if (QAction *quit = findChild<QAction *>(QStringLiteral("actQuit")))
        quit->setShortcut(m_settings.keyset.shortcut(KeyAction::Quit));
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        auto *w = qobject_cast<QWidget *>(obj);
        if (!w || w->window() != this)
            return false;
        auto *focus = QApplication::focusWidget();
        if (qApp->activeModalWidget() || qobject_cast<QLineEdit *>(focus))
            return false;
        auto *ke = static_cast<QKeyEvent *>(event);
        const QKeySequence seq(ke->keyCombination());
        KeyAction matched = static_cast<KeyAction>(-1);
        const QList<KeyAction> actions = m_settings.keyset.actions();
        for (const KeyAction a : actions) {
            if (a == KeyAction::OpenFile || a == KeyAction::Quit)
                continue;
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
    case KeyAction::EditMode:
    case KeyAction::OpenFile:
    case KeyAction::Quit:
        break;
    }
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
    if (m_currentPath.isEmpty())
        return;
    m_cache.upsertProgress({m_currentPath, m_view->currentChapter(), m_view->currentPage(),
                            QDateTime::currentSecsSinceEpoch()});
    m_cache.save();
}

void MainWindow::clearRecentList()
{
    m_cache.clearRecent();
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

void MainWindow::showHideWindow()
{
    if (isVisible()) {
        if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
            if (!QSystemTrayIcon::isSystemTrayAvailable()) {
                QMessageBox::information(this, QStringLiteral("无法隐藏"),
                    QStringLiteral("Wayland 下隐藏窗口后只能通过托盘或 niri 快捷键恢复，"
                                   "但当前都没有可用条件，已取消隐藏。"));
                return;
            }
            hide();
            if (m_tray && m_tray->isVisible())
                m_tray->showMessage(QStringLiteral("Reader 已隐藏"),
                                    QStringLiteral("点击托盘图标可恢复显示"));
            return;
        }
        hide();
    } else {
        show();
    }
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

void MainWindow::toggleHideBorder()
{
    // 隐藏/显示窗口顶部的菜单栏（文件/目录/书签/设置/窗口/帮助）
    if (menuBar())
        menuBar()->setVisible(!menuBar()->isVisible());
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
    menu->addAction(QStringLiteral("退出"), this, &QWidget::close);
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
    const QString dockStyle = fullyTransparent
        ? QStringLiteral("QDockWidget { background: transparent; }"
                         "QDockWidget::title { background: transparent; }"
                         "QTreeWidget { background: transparent; }")
        : QString();
    const QList<QDockWidget *> docks = findChildren<QDockWidget *>();
    for (QDockWidget *dock : docks)
        dock->setStyleSheet(dockStyle);
    m_view->update();
    if (!QGuiApplication::platformName().startsWith(QLatin1String("wayland")))
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
    if (!reader::patchReaderOpacity(&patched, 1.0))
        return;
    if (patched == content)
        return;
    QSaveFile sf(path);
    if (!sf.open(QIODevice::WriteOnly))
        return;
    sf.write(patched.toUtf8());
    if (!sf.commit())
        return;
    QProcess::startDetached(QStringLiteral("niri"),
                            {QStringLiteral("msg"), QStringLiteral("action"),
                             QStringLiteral("load-config-file")});
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveProgress();
    if (m_settings.behavior.minimizeToTray && m_tray && m_tray->isVisible()) {
        event->ignore();
        hide();
        return;
    }
    QMainWindow::closeEvent(event);
}

}
