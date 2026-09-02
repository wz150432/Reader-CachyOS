#include "app/ReadingView.h"
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QUrl>
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
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    m_autoPageTimer = new QTimer(this);
    m_autoPageTimer->setInterval(m_behavior.autoPageIntervalMs);
    connect(m_autoPageTimer, &QTimer::timeout, this, &ReadingView::onAutoPageTick);
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

void ReadingView::pageUp()
{
    if (m_page.prevPage()) {
        emit pageChanged(m_page.currentPage());
        update();
    }
}

void ReadingView::pageDown()
{
    if (m_page.nextPage()) {
        emit pageChanged(m_page.currentPage());
        update();
    }
}

void ReadingView::lineUp()
{
    if (m_page.prevLine()) {
        emit pageChanged(m_page.currentPage());
        update();
    }
}

void ReadingView::lineDown()
{
    if (m_page.nextLine()) {
        emit pageChanged(m_page.currentPage());
        update();
    }
}

void ReadingView::fontZoomIn()
{
    fontZoom(1);
}

void ReadingView::fontZoomOut()
{
    fontZoom(-1);
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
    const qreal bgOpacity = m_settings.windowAlpha / 255.0;
    if (!m_bgPixmap.isNull())
        painter.setOpacity(bgOpacity);
    QColor bg = m_settings.bgColor;
    bg.setAlpha(m_settings.windowAlpha);
    painter.fillRect(rect(), bg);
    if (!m_bgPixmap.isNull()) {
        painter.drawPixmap(rect(), m_bgPixmap);
        painter.setOpacity(1.0);
    }
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
    applyTagHighlight(painter, content, from);
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
    if (event->button() == Qt::LeftButton)
        m_leftPressed = true;
    if (event->button() == Qt::RightButton)
        m_rightPressed = true;
    if (m_leftPressed && m_rightPressed && m_behavior.doubleClickHide) {
        m_leftPressed = false;
        m_rightPressed = false;
        event->accept();
        emit hideWindowRequested();
        return;
    }
    if (event->button() == Qt::LeftButton && m_page.nextPage()) {
        emit pageChanged(m_page.currentPage());
        update();
    } else if (event->button() == Qt::RightButton && m_page.prevPage()) {
        emit pageChanged(m_page.currentPage());
        update();
    }
    QWidget::mousePressEvent(event);
}

void ReadingView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_leftPressed = false;
    if (event->button() == Qt::RightButton)
        m_rightPressed = false;
    QWidget::mouseReleaseEvent(event);
}

void ReadingView::dragEnterEvent(QDragEnterEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() == 1 && urls.first().isLocalFile())
        event->acceptProposedAction();
}

void ReadingView::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() == 1 && urls.first().isLocalFile()) {
        const QString path = urls.first().toLocalFile();
        if (QFileInfo(path).isFile()) {
            emit fileDropRequested(path);
            event->acceptProposedAction();
        }
    }
}

void ReadingView::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (event->modifiers() & Qt::ControlModifier) {
        int alpha = m_settings.windowAlpha;
        if (event->modifiers() & Qt::ShiftModifier) {
            // Ctrl+Shift+滚轮：向上基本全透明，向下完全不透明
            alpha = delta > 0 ? 0 : 255;
        } else {
            // Ctrl+滚轮：向上更透明，向下更不透明
            alpha += delta > 0 ? -10 : 10;
            alpha = qBound(0, alpha, 255);
        }
        if (alpha != m_settings.windowAlpha) {
            m_settings.windowAlpha = alpha;
            emit displaySettingsChanged(m_settings);
            update();
        }
        event->accept();
        return;
    }
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
    const QVector<Chapter> &chapters = m_book->chapters();
    const QPair<int, int> cur = m_page.charRange(m_page.currentPage());
    int foundChapter = -1;
    int foundIndex = -1;
    if (!m_searchWholeBook) {
        const QString text = m_book->chapterText(m_chapter);
        int idx = forward ? text.indexOf(keyword, cur.second)
                          : text.lastIndexOf(keyword, qMax(0, cur.first - 1));
        if (idx < 0)
            idx = forward ? text.indexOf(keyword) : text.lastIndexOf(keyword);
        if (idx >= 0) {
            foundChapter = m_chapter;
            foundIndex = idx;
        }
    } else {
        // 全书搜索：先看当前位置之后/之前的章节，再从全书开头/结尾绕回
        QVector<QPair<int, bool>> tasks;
        tasks.append({m_chapter, true});
        if (forward) {
            for (int i = m_chapter + 1; i < chapters.size(); ++i)
                tasks.append({i, false});
            for (int i = 0; i <= m_chapter; ++i)
                tasks.append({i, false});
        } else {
            for (int i = m_chapter - 1; i >= 0; --i)
                tasks.append({i, false});
            for (int i = chapters.size() - 1; i >= m_chapter; --i)
                tasks.append({i, false});
        }
        for (const QPair<int, bool> &task : tasks) {
            const int ci = task.first;
            const QString text = m_book->chapterText(ci);
            const int idx = task.second
                ? (forward ? text.indexOf(keyword, cur.second)
                           : text.lastIndexOf(keyword, qMax(0, cur.first - 1)))
                : (forward ? text.indexOf(keyword) : text.lastIndexOf(keyword));
            if (idx >= 0) {
                foundChapter = ci;
                foundIndex = idx;
                break;
            }
        }
    }
    if (foundChapter < 0 || foundIndex < 0)
        return false;
    m_matchStart = foundIndex;
    m_matchEnd = foundIndex + keyword.size();
    if (foundChapter != m_chapter)
        goToChapter(foundChapter);
    const int page = m_page.pageForChar(foundIndex);
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

int ReadingView::tagCount() const
{
    int n = 0;
    for (const TagItem &t : m_tags) {
        if (t.enabled)
            ++n;
    }
    return n;
}

void ReadingView::onAutoPageTick()
{
    const bool advanced = m_behavior.autoPageScrollMode
        ? m_page.scrollDown()
        : m_page.nextPage();
    if (advanced) {
        emit pageChanged(m_page.currentPage());
        update();
    }
}

void ReadingView::startAutoPage()
{
    if (!m_autoPageTimer)
        return;
    m_autoPageTimer->setInterval(m_behavior.autoPageIntervalMs);
    m_autoPageTimer->start();
}

void ReadingView::stopAutoPage()
{
    if (m_autoPageTimer)
        m_autoPageTimer->stop();
}

void ReadingView::toggleAutoPage()
{
    if (isAutoPaging())
        stopAutoPage();
    else
        startAutoPage();
}

void ReadingView::applyTagHighlight(QPainter &painter, const PageContent &content, int fromIndex)
{
    if (m_tags.isEmpty() || !m_hasBook)
        return;
    const QString text = m_book->chapterText(m_chapter);
    for (const TagItem &tag : m_tags) {
        if (!tag.enabled || tag.keyword.isEmpty())
            continue;
        const QColor bg = tag.bg.isValid() ? tag.bg : QColor(0xFF, 0xE0, 0x66);
        QList<int> hits;
        int idx = 0;
        while ((idx = text.indexOf(tag.keyword, idx)) >= 0) {
            hits.append(idx);
            idx += tag.keyword.size();
        }
        if (hits.isEmpty())
            continue;
        for (int i = fromIndex; i < content.paragraphIndex.size(); ++i) {
            const QPair<int, int> range = content.lineCharRange.at(i);
            for (const int h : hits) {
                if (h >= range.second || h + tag.keyword.size() <= range.first)
                    continue;
                const QTextLayout &layout = m_page.paragraph(content.paragraphIndex.at(i));
                const QTextLine line = layout.lineAt(content.lineIndex.at(i));
                const int len = line.textLength();
                const int p1 = qBound(0, h - range.first, len);
                const int p2 = qBound(0, h + tag.keyword.size() - range.first, len);
                if (p2 <= p1)
                    continue;
                const qreal x1 = line.cursorToX(p1);
                const qreal x2 = line.cursorToX(p2);
                painter.fillRect(QRectF(content.positions.at(i).x() + x1,
                                        content.positions.at(i).y(),
                                        x2 - x1, line.height()), bg);
            }
        }
    }
}

}
