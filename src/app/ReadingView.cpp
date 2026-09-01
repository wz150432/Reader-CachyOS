#include "app/ReadingView.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace {
reader::PageLayoutParams toPageParams(const reader::DisplaySettings &s)
{
    reader::PageLayoutParams p;
    p.font = s.font;
    p.titleFont = s.titleFont;
    p.useSameFont = s.useSameFont;
    p.textColor = s.textColor;
    p.lineGap = s.lineGap;
    p.paragraphGap = s.paragraphGap;
    p.firstLineIndent = s.firstLineIndent;
    p.compressBlankLines = s.compressBlankLines;
    p.wordWrap = s.wordWrap;
    p.bgColor = s.bgColor;
    p.margin = s.margin;
    return p;
}
}

namespace reader {

ReadingView::ReadingView(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
}

void ReadingView::setBook(std::shared_ptr<Book> book)
{
    m_book = std::move(book);
    m_hasBook = bool(m_book);
    m_chapter = 0;
    if (m_hasBook)
        loadChapter();
    else
        m_page.setText(QString());
    update();
}

void ReadingView::setSettings(const DisplaySettings &settings)
{
    m_settings = settings;
    refreshLayout();
}

void ReadingView::goToChapter(int index)
{
    if (!m_hasBook)
        return;
    const QVector<Chapter> &chapters = m_book->chapters();
    if (index < 0 || index >= chapters.size())
        return;
    m_chapter = index;
    loadChapter();
    emit chapterChanged(index);
}

void ReadingView::goToPage(int page)
{
    if (m_page.goToPage(page)) {
        update();
        emit pageChanged(m_page.currentPage());
    }
}

void ReadingView::refreshLayout()
{
    m_page.setParams(toPageParams(m_settings));
    m_page.setViewSize(width(), height());
    if (m_hasBook)
        m_page.setText(m_book->chapterText(m_chapter));
    update();
}

void ReadingView::loadChapter()
{
    if (!m_hasBook)
        return;
    m_page.setParams(toPageParams(m_settings));
    m_page.setViewSize(width(), height());
    m_page.setText(m_book->chapterText(m_chapter));
    m_page.goToPage(0);
    emit pageChanged(0);
    update();
}

void ReadingView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), m_settings.bgColor);
    if (!m_hasBook || m_page.pageCount() == 0) {
        painter.setPen(QColor(140, 140, 140));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("打开一本 TXT 小说开始阅读（Ctrl+O）"));
        return;
    }
    painter.setPen(m_settings.textColor);
    const PageContent &content = m_page.content(m_page.currentPage());
    for (int i = 0; i < content.paragraphIndex.size(); ++i) {
        const QTextLayout &layout = m_page.paragraph(content.paragraphIndex.at(i));
        layout.lineAt(content.lineIndex.at(i)).draw(&painter, content.positions.at(i));
    }
    painter.setPen(QColor(128, 128, 128));
    painter.drawText(rect().adjusted(0, 0, -12, -8), Qt::AlignRight | Qt::AlignBottom,
                     QStringLiteral("%1 / %2").arg(m_page.currentPage() + 1).arg(m_page.pageCount()));
}

void ReadingView::resizeEvent(QResizeEvent *event)
{
    const qreal keep = m_page.progress();
    m_page.setViewSize(width(), height());
    m_page.jumpToProgress(keep);
    QWidget::resizeEvent(event);
    update();
    emit pageChanged(m_page.currentPage());
}

void ReadingView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_page.nextPage()) {
        emit pageChanged(m_page.currentPage());
        update();
    } else if (event->button() == Qt::RightButton && m_page.prevPage()) {
        emit pageChanged(m_page.currentPage());
        update();
    }
    QWidget::mousePressEvent(event);
}

void ReadingView::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    bool moved = delta < 0 ? m_page.nextPage() : m_page.prevPage();
    if (moved) {
        emit pageChanged(m_page.currentPage());
        update();
    }
    event->accept();
}

void ReadingView::keyPressEvent(QKeyEvent *event)
{
    bool moved = false;
    switch (event->key()) {
    case Qt::Key_Right:
    case Qt::Key_PageDown:
        moved = m_page.nextPage();
        break;
    case Qt::Key_Left:
    case Qt::Key_PageUp:
        moved = m_page.prevPage();
        break;
    case Qt::Key_Space:
        moved = m_page.nextPage();
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    if (moved) {
        emit pageChanged(m_page.currentPage());
        update();
    }
}

}
