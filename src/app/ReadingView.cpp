#include "app/ReadingView.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
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
    if (m_bgImagePath != settings.bgImagePath) {
        m_bgImagePath = settings.bgImagePath;
        m_bgPixmap = QPixmap(m_bgImagePath);
    }
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
    if (!m_bgPixmap.isNull())
        painter.drawPixmap(rect(), m_bgPixmap);
    else
        painter.fillRect(rect(), m_settings.bgColor);
    if (!m_hasBook || m_page.pageCount() == 0) {
        painter.setPen(QColor(140, 140, 140));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("打开一本 TXT 小说开始阅读（Ctrl+O）"));
        return;
    }
    painter.setPen(m_settings.textColor);
    const PageContent &content = m_page.content(m_page.currentPage());
    const int from = m_page.lineOffset();
    for (int i = from; i < content.paragraphIndex.size(); ++i) {
        const QTextLayout &layout = m_page.paragraph(content.paragraphIndex.at(i));
        layout.lineAt(content.lineIndex.at(i)).draw(&painter, content.positions.at(i));
    }
    if (m_matchStart >= 0 && m_matchEnd > m_matchStart) {
        for (int i = from; i < content.paragraphIndex.size(); ++i) {
            const QPair<int, int> range = content.lineCharRange.at(i);
            if (m_matchStart >= range.second || m_matchEnd <= range.first)
                continue;
            const QTextLayout &layout = m_page.paragraph(content.paragraphIndex.at(i));
            const QTextLine line = layout.lineAt(content.lineIndex.at(i));
            const int len = line.textLength();
            const int p1 = qBound(0, m_matchStart - range.first, len);
            const int p2 = qBound(0, m_matchEnd - range.first, len);
            if (p2 <= p1)
                continue;
            const qreal x1 = line.cursorToX(p1);
            const qreal x2 = line.cursorToX(p2);
            painter.fillRect(QRectF(content.positions.at(i).x() + x1,
                                    content.positions.at(i).y(),
                                    x2 - x1, line.height()),
                             QColor(0xFF, 0xE0, 0x66));
        }
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
    if (m_page.scrollLines(delta < 0 ? 1 : -1)) {
        emit pageChanged(m_page.currentPage());
        update();
    }
    event->accept();
}

void ReadingView::keyPressEvent(QKeyEvent *event)
{
    const QKeySequence seq(event->keyCombination());
    const auto is = [this, &seq](KeyAction a) {
        return m_keyset.shortcut(a).matches(seq) == QKeySequence::ExactMatch;
    };
    bool moved = false;
    if (is(KeyAction::PageDown)) {
        moved = m_page.nextPage();
    } else if (is(KeyAction::PageUp)) {
        moved = m_page.prevPage();
    } else if (is(KeyAction::LineDown)) {
        moved = m_page.nextLine();
    } else if (is(KeyAction::LineUp)) {
        moved = m_page.prevLine();
    } else if (is(KeyAction::ChapterDown)) {
        nextChapter();
    } else if (is(KeyAction::ChapterUp)) {
        prevChapter();
    } else if (is(KeyAction::FontZoomIn)) {
        fontZoom(1);
    } else if (is(KeyAction::FontZoomOut)) {
        fontZoom(-1);
    } else if (is(KeyAction::AutoPage)) {
        emit autoPageRequested();
    } else if (is(KeyAction::Search)) {
        emit searchRequested();
    } else if (is(KeyAction::Jump)) {
        emit jumpRequested();
    } else if (is(KeyAction::AddBookmark)) {
        emit bookmarkRequested();
    } else {
        QWidget::keyPressEvent(event);
        return;
    }
    if (moved) {
        emit pageChanged(m_page.currentPage());
        update();
    }
}

void ReadingView::nextChapter()
{
    if (m_hasBook && m_chapter + 1 < m_book->chapters().size())
        goToChapter(m_chapter + 1);
}

void ReadingView::prevChapter()
{
    if (m_hasBook && m_chapter > 0)
        goToChapter(m_chapter - 1);
}

void ReadingView::fontZoom(int delta)
{
    const int size = qBound(6, m_settings.font.pointSize() + delta, 72);
    if (size == m_settings.font.pointSize())
        return;
    m_settings.font.setPointSize(size);
    refreshLayout();
    emit displaySettingsChanged(m_settings);
}

bool ReadingView::findNext(const QString &keyword, bool forward)
{
    if (!m_hasBook || keyword.isEmpty())
        return false;
    const QString text = m_book->chapterText(m_chapter);
    const QPair<int, int> cur = m_page.charRange(m_page.currentPage());
    int idx = -1;
    if (forward)
        idx = text.indexOf(keyword, cur.second);
    else
        idx = text.lastIndexOf(keyword, qMax(0, cur.first - 1));
    if (idx < 0)
        idx = forward ? text.indexOf(keyword) : text.lastIndexOf(keyword);
    if (idx < 0)
        return false;
    m_matchStart = idx;
    m_matchEnd = idx + keyword.size();
    const int page = m_page.pageForChar(idx);
    m_page.goToPage(page);
    emit pageChanged(page);
    update();
    return true;
}

void ReadingView::jumpToBookProgress(qreal progress)
{
    if (!m_hasBook)
        return;
    const qint64 total = m_book->totalCharCount();
    if (total <= 0)
        return;
    const qint64 target = qBound<qint64>(0, qint64(progress * total), total - 1);
    const QVector<Chapter> &ch = m_book->chapters();
    int ci = 0;
    for (int i = 0; i < ch.size(); ++i) {
        const qint64 cstart = ch.at(i).charOffset;
        const qint64 cend = (i + 1 < ch.size()) ? ch.at(i + 1).charOffset : total;
        if (target >= cstart && target < cend) {
            ci = i;
            break;
        }
    }
    goToChapter(ci);
    const qint64 cstart = ch.at(ci).charOffset;
    const qint64 cend = (ci + 1 < ch.size()) ? ch.at(ci + 1).charOffset : total;
    const qreal frac = (cend > cstart) ? qreal(target - cstart) / qreal(cend - cstart) : 0.0;
    m_page.jumpToProgress(frac);
    emit pageChanged(m_page.currentPage());
    update();
}

}
