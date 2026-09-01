#pragma once
#include <QColor>
#include <QFont>
#include <QPointF>
#include <QString>
#include <QTextLayout>
#include <QVector>
#include <memory>
#include <vector>

namespace reader {

struct PageLayoutParams
{
    QFont font{QStringLiteral("Noto Sans CJK SC"), 12};
    QFont titleFont{QStringLiteral("Noto Sans CJK SC"), 15};
    bool useSameFont = false;
    QColor textColor{QColor(0x33, 0x33, 0x33)};
    int lineGap = 4;
    int paragraphGap = 8;
    bool firstLineIndent = true;
    bool compressBlankLines = false;
    bool wordWrap = true;
    QColor bgColor{Qt::white};
    int margin = 24;
};

struct PageContent
{
    QVector<int> paragraphIndex; // 段落布局在 Page 中的下标
    QVector<int> lineIndex;      // 该段落内第几行
    QVector<QPointF> positions;
    qreal lineHeight = 0;
};

class Page
{
public:
    void setParams(const PageLayoutParams &params);
    void setViewSize(int width, int height);
    void setText(const QString &text);

    int pageCount() const { return m_pages.size(); }
    int currentPage() const { return m_current; }
    bool goToPage(int page);
    bool nextPage();
    bool prevPage();
    qreal progress() const;
    void jumpToProgress(qreal p);
    const PageContent &content(int page) const { return m_pages.at(page); }
    int lineCount(int page) const { return m_pages.at(page).paragraphIndex.size(); }
    const QTextLayout &paragraph(int index) const { return *m_paragraphs.at(index); }

private:
    void repaginate();
    int m_viewWidth = 0;
    int m_viewHeight = 0;
    PageLayoutParams m_params;
    QString m_text;
    QVector<PageContent> m_pages;
    std::vector<std::unique_ptr<QTextLayout>> m_paragraphs;
    int m_current = 0;
};

}
