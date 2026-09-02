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

void ReadingView::setShowPageIndicator(bool show)
{
    if (m_showPageIndicator == show)
        return;
    m_showPageIndicator = show;
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
    resetPixelScroll();
    if (m_page.goToPage(page)) {
        update();
        emit pageChanged(m_page.currentPage());
    }
}

void ReadingView::pageUp()
{
    if (m_page.prevPage()) {
        resetPixelScroll();
        emit pageChanged(m_page.currentPage());
        update();
    }
}

void ReadingView::pageDown()
{
    if (m_page.nextPage()) {
        resetPixelScroll();
        emit pageChanged(m_page.currentPage());
        update();
    }
}

void ReadingView::lineUp()
{
    if (scrollByPixels(-currentLineStep())) {
        emit pageChanged(m_page.currentPage());
        update();
    }
}

void ReadingView::lineDown()
{
    if (scrollByPixels(currentLineStep())) {
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
    resetPixelScroll();
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
    resetPixelScroll();
    m_page.setParams(toPageParams(m_settings));
    m_page.setViewSize(width(), height());
    m_page.setText(m_book->chapterText(m_chapter));
    m_page.goToPage(0);
    emit pageChanged(0);
    update();
}

void ReadingView::resetPixelScroll()
{
    m_pixelOffset = 0.0;
}

qreal ReadingView::currentPageContentHeight() const
{
    if (m_page.pageCount() == 0)
        return 0.0;
    const PageContent &content = m_page.content(m_page.currentPage());
    if (content.paragraphIndex.isEmpty())
        return 0.0;
    const QTextLayout &layout = m_page.paragraph(content.paragraphIndex.last());
    return content.positions.last().y() + layout.lineAt(content.lineIndex.last()).height()
        + m_settings.lineGap;
}

qreal ReadingView::currentLineStep() const
{
    if (m_page.pageCount() == 0)
        return 20.0;
    const PageContent &content = m_page.content(m_page.currentPage());
    if (content.paragraphIndex.isEmpty())
        return 20.0;
    const QTextLayout &layout = m_page.paragraph(content.paragraphIndex.first());
    return layout.lineAt(content.lineIndex.first()).height() + m_settings.lineGap;
}

bool ReadingView::scrollByPixels(qreal delta)
{
    if (!m_hasBook || m_page.pageCount() == 0)
        return false;
    const int startPage = m_page.currentPage();
    const qreal viewHeight = qMax<qreal>(20.0, height() - 2 * m_settings.margin);
    qreal target = m_pixelOffset + delta;
    while (target < 0.0) {
        if (!m_page.prevPage()) {
            target = 0.0;
            break;
        }
        target += currentPageContentHeight();
    }
    while (m_page.currentPage() + 1 < m_page.pageCount()) {
        const qreal pageHeight = currentPageContentHeight();
        if (pageHeight <= 0.0 || target < pageHeight)
            break;
        target -= pageHeight;
        m_page.nextPage();
    }
    if (m_page.currentPage() + 1 >= m_page.pageCount()) {
        const qreal pageHeight = currentPageContentHeight();
        target = qBound(0.0, target, qMax(0.0, pageHeight - viewHeight));
    }
    target = qMax(0.0, target);
    const qreal step = currentLineStep();
    if (step > 0.0)
        target = qRound(target / step) * step;
    if (m_page.currentPage() + 1 < m_page.pageCount()) {
        const qreal pageHeight = currentPageContentHeight();
        if (pageHeight > 0.0 && target >= pageHeight) {
            m_page.nextPage();
            target = 0.0;
        }
    }
    if (m_page.currentPage() + 1 >= m_page.pageCount()) {
        const qreal pageHeight = currentPageContentHeight();
        target = qBound(0.0, target, qMax(0.0, pageHeight - viewHeight));
    }
    const bool changed = m_page.currentPage() != startPage
        || !qFuzzyCompare(m_pixelOffset, target);
    m_pixelOffset = target;
    return changed;
}

void ReadingView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const qreal bgOpacity = m_settings.windowAlpha / 255.0;
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (!m_bgPixmap.isNull())
        painter.setOpacity(bgOpacity);
    QColor bg = m_settings.bgColor;
    bg.setAlpha(m_settings.windowAlpha);
    if (bg.alpha() > 0)
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
    const int currentPage = m_page.currentPage();
    const PageContent &content = m_page.content(currentPage);
    const qreal offset = m_pixelOffset;
    const qreal currentHeight = currentPageContentHeight();
    QVector<DisplayLine> lines;
    const auto addLines = [this, &lines](const PageContent &page, qreal yShift) {
        for (int i = 0; i < page.paragraphIndex.size(); ++i) {
            const QTextLayout &layout = m_page.paragraph(page.paragraphIndex.at(i));
            const QTextLine tl = layout.lineAt(page.lineIndex.at(i));
            const qreal y = page.positions.at(i).y() + yShift;
            if (y + tl.height() <= 0 || y >= height())
                continue;
            lines.append({&layout, page.lineIndex.at(i),
                          QPointF(page.positions.at(i).x(), y),
                          page.lineCharRange.at(i)});
        }
    };
    addLines(content, -offset);
    if (offset > 0.0 && currentPage + 1 < m_page.pageCount())
        addLines(m_page.content(currentPage + 1), currentHeight - offset);
    for (const DisplayLine &line : lines) {
        const QTextLine tl = line.layout->lineAt(line.lineIndex);
        tl.draw(&painter, QPointF(line.pos.x() - tl.position().x(),
                                  line.pos.y() - tl.position().y()));
    }
    applyTagHighlight(painter, lines);
    if (m_showPageIndicator) {
        painter.setPen(QColor(128, 128, 128));
        painter.drawText(rect().adjusted(0, 0, -12, -8), Qt::AlignRight | Qt::AlignBottom,
                         QStringLiteral("%1 / %2").arg(m_page.currentPage() + 1).arg(m_page.pageCount()));
    }
}

void ReadingView::resizeEvent(QResizeEvent *event)
{
    const qreal keep = m_page.progress();
    resetPixelScroll();
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
        resetPixelScroll();
        emit pageChanged(m_page.currentPage());
        update();
    } else if (event->button() == Qt::RightButton && m_page.prevPage()) {
        resetPixelScroll();
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
    const qreal step = currentLineStep()
        * qMax(1, qRound(qAbs(delta) / 120.0));
    if (scrollByPixels(delta < 0 ? step : -step)) {
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
    resetPixelScroll();
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
    resetPixelScroll();
    emit pageChanged(m_page.currentPage());
    update();
}

qreal ReadingView::currentBookProgress() const
{
    if (!m_hasBook || m_book->chapters().isEmpty())
        return 0.0;
    const qint64 total = m_book->totalCharCount();
    if (total <= 0)
        return 0.0;
    const QVector<Chapter> &ch = m_book->chapters();
    if (m_chapter < 0 || m_chapter >= ch.size())
        return 0.0;
    const QPair<int, int> range = currentPageCharRange();
    const qint64 offset = ch.at(m_chapter).charOffset + qMax(0, range.first);
    return qBound<qreal>(0.0, qreal(offset) / qreal(total), 1.0);
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
        ? scrollByPixels(currentLineStep() * m_behavior.scrollStep)
        : m_page.nextPage();
    if (advanced && !m_behavior.autoPageScrollMode)
        resetPixelScroll();
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

void ReadingView::applyTagHighlight(QPainter &painter, const QVector<DisplayLine> &lines)
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
        for (const DisplayLine &line : lines) {
            const QPair<int, int> range = line.charRange;
            for (const int h : hits) {
                if (h >= range.second || h + tag.keyword.size() <= range.first)
                    continue;
                const QTextLine tl = line.layout->lineAt(line.lineIndex);
                const int len = tl.textLength();
                const int p1 = qBound(0, h - range.first, len);
                const int p2 = qBound(0, h + tag.keyword.size() - range.first, len);
                if (p2 <= p1)
                    continue;
                const qreal x1 = tl.cursorToX(p1);
                const qreal x2 = tl.cursorToX(p2);
                painter.fillRect(QRectF(line.pos.x() + x1, line.pos.y(),
                                        x2 - x1, tl.height()), bg);
            }
        }
    }
}

}
