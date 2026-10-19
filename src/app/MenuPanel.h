#pragma once
#include <QDialog>
#include <QEvent>
#include <QPointer>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QScreen>

namespace reader {
// A native popup owned by the reader, positioned at its menu bar.
class MenuPanel : public QDialog {
public:
  explicit MenuPanel(QWidget *parent = nullptr)
      : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint) {
    setProperty("menuPanel", true);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground);
  }
  void showAnchored(QWidget *anchor, const QRect &item = {}) {
    if (!anchor || !anchor->window()->isVisible())
      return;
    if (m_anchor != anchor) {
      if (m_anchor)
        m_anchor->window()->removeEventFilter(this);
      m_anchor = anchor;
      if (m_anchor)
        m_anchor->window()->installEventFilter(this);
    }
    m_item = item;
    ensurePolished();
    reposition();
    show();
    raise();
    activateWindow();
  }
protected:
  void paintEvent(QPaintEvent *) override {
    QStyleOption option;
    option.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
  }
  bool eventFilter(QObject *object, QEvent *event) override {
    if (m_anchor && object == m_anchor->window() && isVisible()) {
      if (event->type() == QEvent::Move || event->type() == QEvent::Resize)
        reposition();
      else if (event->type() == QEvent::Hide || event->type() == QEvent::Close)
        hide();
    }
    return QDialog::eventFilter(object, event);
  }
private:
  void reposition() {
    if (!m_anchor)
      return;
    const QRect available = m_anchor->screen()->availableGeometry().adjusted(8, 8, -8, -8);
    setMaximumSize(available.size());
    resize(size().boundedTo(available.size()));
    const int x = m_item.isNull() ? 0 : m_item.left();
    QPoint position = m_anchor->mapToGlobal(QPoint(x, m_anchor->height()));
    if (position.y() + height() > available.bottom() + 1)
      position.setY(m_anchor->mapToGlobal(QPoint(0, 0)).y() - height());
    position.setX(qBound(available.left(), position.x(), qMax(available.left(), available.right() - width() + 1)));
    position.setY(qBound(available.top(), position.y(), qMax(available.top(), available.bottom() - height() + 1)));
    move(position);
  }
  QPointer<QWidget> m_anchor;
  QRect m_item;
};
} // namespace reader
