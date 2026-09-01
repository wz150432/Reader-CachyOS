#pragma once
#include <QWidget>
#include <memory>
#include "core/Book.h"
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
    int currentChapter() const { return m_chapter; }
    int currentPage() const { return m_page.currentPage(); }
    int pageCount() const { return m_page.pageCount(); }
    void goToChapter(int index);
    void goToPage(int page);
    void refreshLayout();

signals:
    void chapterChanged(int index);
    void pageChanged(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void loadChapter();
    std::shared_ptr<Book> m_book;
    DisplaySettings m_settings;
    Page m_page;
    int m_chapter = 0;
    bool m_hasBook = false;
};

}
