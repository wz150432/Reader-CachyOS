#include "core/Page.h"
#include <QTextOption>
#include <algorithm>
#include <memory>

namespace reader {

void Page::setParams(const PageLayoutParams &params)
{
    m_params = params;
    repaginate();
}

void Page::setViewSize(int width, int height)
{
    if (m_viewWidth == width && m_viewHeight == height)
        return;
    m_viewWidth = width;
    m_viewHeight = height;
    repaginate();
}

void Page::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    repaginate();
}

void Page::repaginate()
{
    m_pages.clear();
    m_current = 0;
    m_lineOffset = 0;
    m_paragraphInfo.clear();
    m_pageCharRange.clear();
    if (m_text.isEmpty() || m_viewWidth <= 0 || m_viewHeight <= 0)
        return;

    const int usableWidth = qMax(20, m_viewWidth - 2 * m_params.margin);
    const qreal usableHeight = qMax(20, m_viewHeight - 2 * m_params.margin);
    const QFont bodyFont = m_params.font;
    const QFont titleFont = m_params.useSameFont ? m_params.font : m_params.titleFont;
    const QTextOption::WrapMode wrap = m_params.wordWrap
        ? QTextOption::WrapAtWordBoundaryOrAnywhere
        : QTextOption::NoWrap;

    m_paragraphs.clear();
    const QStringList paragraphs = m_text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    int running = 0;
    for (int p = 0; p < paragraphs.size(); ++p) {
        const QString part = paragraphs.at(p);
        if (p == paragraphs.size() - 1 && part.isEmpty())
            continue; // 文本末尾换行产生的伪空段落
        const int partStart = running;
        running += part.size() + 1;
        if (m_params.compressBlankLines && part.trimmed().isEmpty())
            continue;
        QString paraText = part;
        const int indent = (m_params.firstLineIndent && !part.isEmpty()) ? 2 : 0;
        if (indent)
            paraText = QStringLiteral("\u3000\u3000") + paraText;
        const QFont &f = (p == 0) ? titleFont : bodyFont;
        auto layout = std::make_unique<QTextLayout>(paraText, f);
        QTextOption opt;
        opt.setWrapMode(wrap);
        layout->setTextOption(opt);
        layout->beginLayout();
        qreal y = 0;
        while (true) {
            QTextLine line = layout->createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(usableWidth);
            line.setPosition(QPointF(0, y));
            y += line.height() + m_params.lineGap;
        }
        layout->endLayout();
        m_paragraphs.push_back(std::move(layout));
        m_paragraphInfo.push_back({partStart, indent});
    }
    if (m_paragraphs.empty())
        return;

    struct LineRef { int para; int line; };
    QVector<LineRef> allLines;
    for (int i = 0; i < m_paragraphs.size(); ++i) {
        for (int j = 0; j < m_paragraphs.at(i)->lineCount(); ++j)
            allLines.append({i, j});
    }

    int i = 0;
    while (i < allLines.size()) {
        PageContent page;
        qreal y = 0;
        int lastPara = -1;
        bool any = false;
        while (i < allLines.size()) {
            const LineRef &ref = allLines.at(i);
            const QTextLayout &layout = *m_paragraphs.at(ref.para);
            const qreal lineH = layout.lineAt(ref.line).height() + m_params.lineGap;
            if (any && y + lineH > usableHeight)
                break;
            if (ref.line == 0 && any && ref.para != lastPara) {
                if (y + m_params.paragraphGap + lineH > usableHeight)
                    break;
                y += m_params.paragraphGap;
            }
            page.paragraphIndex.append(ref.para);
            page.lineIndex.append(ref.line);
            page.positions.append(QPointF(0, y));
            const QTextLine tl = layout.lineAt(ref.line);
            const int indent = m_paragraphInfo.at(ref.para).second;
            const int srcStart = qMax(0, tl.textStart() - indent);
            const int srcEnd = qMax(0, tl.textStart() + tl.textLength() - indent);
            page.lineCharRange.append({m_paragraphInfo.at(ref.para).first + srcStart,
                                       m_paragraphInfo.at(ref.para).first + srcEnd});
            page.lineHeight = qMax(page.lineHeight, lineH);
            y += lineH;
            lastPara = ref.para;
            any = true;
            ++i;
        }
        if (!any)
            break;
        m_pages.append(page);
        m_pageCharRange.append({page.lineCharRange.constFirst().first,
                                page.lineCharRange.constLast().second});
    }
    if (m_pages.isEmpty()) {
        PageContent page;
        page.paragraphIndex.append(0);
        page.lineIndex.append(0);
        page.positions.append(QPointF(0, 0));
        m_pages.append(page);
    }
}

bool Page::goToPage(int page)
{
    if (page < 0 || page >= m_pages.size())
        return false;
    m_current = page;
    m_lineOffset = 0;
    return true;
}

bool Page::nextPage()
{
    return goToPage(m_current + 1);
}

bool Page::prevPage()
{
    return goToPage(m_current - 1);
}

qreal Page::progress() const
{
    if (m_pages.isEmpty())
        return 0.0;
    return qreal(m_current) / qreal(m_pages.size());
}

void Page::jumpToProgress(qreal p)
{
    if (m_pages.isEmpty())
        return;
    m_current = qBound(0, static_cast<int>(p * m_pages.size()), m_pages.size() - 1);
    m_lineOffset = 0;
}

bool Page::nextLine()
{
    if (m_pages.isEmpty())
        return false;
    if (m_lineOffset + 1 < linesOnCurrentPage()) {
        ++m_lineOffset;
        return true;
    }
    if (m_current + 1 < m_pages.size()) {
        ++m_current;
        m_lineOffset = 0;
        return true;
    }
    return false;
}

bool Page::prevLine()
{
    if (m_pages.isEmpty())
        return false;
    if (m_lineOffset > 0) {
        --m_lineOffset;
        return true;
    }
    if (m_current > 0) {
        --m_current;
        m_lineOffset = linesOnCurrentPage() - 1;
        return true;
    }
    return false;
}

bool Page::scrollLines(int delta)
{
    bool any = false;
    while (delta > 0) {
        if (!nextLine())
            break;
        --delta;
        any = true;
    }
    while (delta < 0) {
        if (!prevLine())
            break;
        ++delta;
        any = true;
    }
    return any;
}

int Page::pageForChar(int charPos) const
{
    for (int i = 0; i < m_pageCharRange.size(); ++i) {
        const QPair<int, int> r = m_pageCharRange.at(i);
        if (charPos >= r.first && charPos < r.second)
            return i;
    }
    if (!m_pageCharRange.isEmpty() && charPos < m_pageCharRange.constFirst().first)
        return 0;
    return m_pageCharRange.size() - 1;
}

}
