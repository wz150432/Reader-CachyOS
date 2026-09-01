#pragma once
#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <memory>
#include "core/Book.h"
#include "core/Keyset.h"
#include "core/Page.h"
#include "core/Settings.h"

namespace reader {

class ReadingView : public QWidget
{
    Q_OBJECT
public:
    explicit ReadingView(QWidget *parent = nullptr);
    void setBook(std::shared_ptr<Book> book);
    void setSettings(const DisplaySettings &settings);
    void setKeyset(const Keyset &keyset) { m_keyset = keyset; }
    void setBehavior(const BehaviorSettings &behavior) { m_behavior = behavior; }
    void setTags(const QVector<TagItem> &tags) { m_tags = tags; update(); }
    int tagCount() const;
    void startAutoPage();
    void stopAutoPage();
    bool isAutoPaging() const { return m_autoPageTimer && m_autoPageTimer->isActive(); }
    void toggleAutoPage();
    int currentChapter() const { return m_chapter; }
    int currentPage() const { return m_page.currentPage(); }
    int pageCount() const { return m_page.pageCount(); }
    int lineOffset() const { return m_page.lineOffset(); }
    QPair<int, int> currentPageCharRange() const { return m_page.charRange(m_page.currentPage()); }
    void pageUp();
    void pageDown();
    void lineUp();
    void lineDown();
    void fontZoomIn();
    void fontZoomOut();
    void goToChapter(int index);
    void goToPage(int page);
    void refreshLayout();
    bool findNext(const QString &keyword, bool forward = true);
    void jumpToBookProgress(qreal progress);
    void clearMatch() { m_matchStart = -1; m_matchEnd = -1; update(); }
    int currentMatchStart() const { return m_matchStart; }
    int currentMatchEnd() const { return m_matchEnd; }

signals:
    void chapterChanged(int index);
    void pageChanged(int index);
    void searchRequested();
    void jumpRequested();
    void bookmarkRequested();
    void autoPageRequested();
    void displaySettingsChanged(const reader::DisplaySettings &settings);
    void behaviorChanged(const reader::BehaviorSettings &behavior);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void loadChapter();
    void nextChapter();
    void prevChapter();
    void fontZoom(int delta);
    void onAutoPageTick();
    void applyTagHighlight(QPainter &painter, const PageContent &content, int fromIndex);
    std::shared_ptr<Book> m_book;
    DisplaySettings m_settings;
    Keyset m_keyset;
    BehaviorSettings m_behavior;
    QVector<TagItem> m_tags;
    Page m_page;
    QTimer *m_autoPageTimer = nullptr;
    QPixmap m_bgPixmap;
    QString m_bgImagePath;
    int m_chapter = 0;
    bool m_hasBook = false;
    int m_matchStart = -1;
    int m_matchEnd = -1;
};

}
