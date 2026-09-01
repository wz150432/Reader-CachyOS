#include "app/MainWindow.h"
#include "app/ReadingView.h"
#include "app/SettingsDialog.h"
#include <QCloseEvent>
#include <QDateTime>
#include <QDockWidget>
#include <QFileDialog>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QTreeWidget>

namespace reader {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_cache.load();
    m_settings.load();
    m_view = new ReadingView(this);
    m_view->setObjectName(QStringLiteral("readingView"));
    m_view->setSettings(m_settings.display);
    setCentralWidget(m_view);
    connect(m_view, &ReadingView::chapterChanged, this, &MainWindow::onChapterChanged);
    connect(m_view, &ReadingView::pageChanged, this, &MainWindow::onPageChanged);

    auto *dock = new QDockWidget(QStringLiteral("目录"), this);
    dock->setObjectName(QStringLiteral("tocDock"));
    m_toc = new QTreeWidget(dock);
    m_toc->setHeaderHidden(true);
    dock->setWidget(m_toc);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    connect(m_toc, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        m_view->goToChapter(item->data(0, Qt::UserRole).toInt());
    });

    buildMenus();
    updateTitle();
    resize(960, 720);
}

void MainWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    QAction *open = file->addAction(QStringLiteral("打开(&O)"));
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("打开"), QString(),
            QStringLiteral("书籍文件 (*.txt);;所有文件 (*)"));
        if (!path.isEmpty())
            openBook(path);
    });
    QAction *clearRecent = file->addAction(QStringLiteral("清空(&C)"));
    clearRecent->setEnabled(false);
    clearRecent->setToolTip(QStringLiteral("第三阶段开放"));
    QAction *quit = file->addAction(QStringLiteral("退出(&X)"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    QMenu *tocMenu = menuBar()->addMenu(QStringLiteral("目录(&V)"));
    QAction *tocToggle = tocMenu->addAction(QStringLiteral("显示/隐藏目录"));
    connect(tocToggle, &QAction::triggered, this, [this] {
        if (QDockWidget *dock = findChild<QDockWidget *>(QStringLiteral("tocDock")))
            dock->setVisible(!dock->isVisible());
    });

    QMenu *bookmark = menuBar()->addMenu(QStringLiteral("书签(&M)"));
    QAction *bm = bookmark->addAction(QStringLiteral("添加书签"));
    bm->setEnabled(false);
    bm->setToolTip(QStringLiteral("第二阶段开放"));

    QMenu *settings = menuBar()->addMenu(QStringLiteral("设置(&S)"));
    QAction *display = settings->addAction(QStringLiteral("显示设置"));
    connect(display, &QAction::triggered, this, [this] {
        SettingsDialog dlg(&m_settings, this);
        if (dlg.exec() == QDialog::Accepted) {
            m_view->setSettings(m_settings.display);
            m_view->refreshLayout();
        }
    });
    settings->addAction(QStringLiteral("基本设置"))->setEnabled(false);
    settings->addAction(QStringLiteral("高级设置"))->setEnabled(false);
    settings->addAction(QStringLiteral("按键设置"))->setEnabled(false);
    settings->addAction(QStringLiteral("标签设置"))->setEnabled(false);
    settings->addSeparator();
    settings->addAction(QStringLiteral("还原默认设置"))->setEnabled(false);

    QMenu *help = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    QAction *about = help->addAction(QStringLiteral("关于 ..."));
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(this, QStringLiteral("关于"),
            QStringLiteral("Reader（CachyOS 原生版）\n\n"
                           "本地 TXT/EPUB/MOBI 阅读器，功能参考开源项目 binbyu/Reader。\n"
                           "本版本为第一阶段：TXT 阅读核心。"));
    });
}

void MainWindow::openBook(const QString &path)
{
    QString err;
    auto book = Book::create(path, &err);
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveProgress();
    QMainWindow::closeEvent(event);
}

}
