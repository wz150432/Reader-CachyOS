#pragma once
#include <QAbstractItemView>
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QMenu>
#include <QPointer>
#include <QWindow>

namespace reader {
// Keep dialog controls interactive while allowing their unused surfaces to move.
class WindowDragFilter final : public QObject {
public:
  explicit WindowDragFilter(QWidget *owner) : QObject(owner), m_owner(owner) {
    qApp->installEventFilter(this);
  }

protected:
  bool eventFilter(QObject *object, QEvent *event) override {
    auto widget = qobject_cast<QWidget *>(object);
    if (!widget)
      return false;
    if (event->type() == QEvent::MouseButtonPress) {
      m_window.clear();
      auto mouse = static_cast<QMouseEvent *>(event);
      if (mouse->button() != Qt::LeftButton || !canDrag(widget, mouse->position().toPoint()))
        return false;
      m_window = widget->window();
      m_press = mouse->globalPosition().toPoint();
      m_origin = m_window->pos();
      m_started = false;
      m_systemMove = false;
      return true;
    }
    if (!m_window || widget->window() != m_window)
      return false;
    if (event->type() == QEvent::MouseMove) {
      auto mouse = static_cast<QMouseEvent *>(event);
      if (!(mouse->buttons() & Qt::LeftButton)) {
        m_window.clear();
        return false;
      }
      const QPoint delta = mouse->globalPosition().toPoint() - m_press;
      if (!m_started && delta.manhattanLength() >= QApplication::startDragDistance()) {
        m_started = true;
        // Wayland popups have no system move operation; move their anchored surface.
        if (m_window->windowType() != Qt::Popup && m_window->windowHandle())
          m_systemMove = m_window->windowHandle()->startSystemMove();
      }
      if (m_started && !m_systemMove)
        m_window->move(m_origin + delta);
      return true;
    }
    if (event->type() == QEvent::MouseButtonRelease &&
        static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
      m_window.clear();
      return true;
    }
    return false;
  }

private:
  bool canDrag(QWidget *widget, const QPoint &position) const {
    QWidget *window = widget->window();
    if (!window->rect().contains(widget->mapTo(window, position)))
      return false;
    auto menu = qobject_cast<QMenu *>(window);
    if (!qobject_cast<QDialog *>(window) && !menu)
      return false;
    QObject *owner = window;
    while (owner && owner != m_owner)
      owner = owner->parent();
    if (!owner)
      return false;
    if (auto label = qobject_cast<QLabel *>(widget))
      return !label->buddy() && !label->openExternalLinks() &&
             !(label->textInteractionFlags() & (Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard)) &&
             !label->text().contains("<a", Qt::CaseInsensitive);
    if (widget == window)
      return !menu || !menu->actionAt(position);
    if (widget->metaObject() != &QWidget::staticMetaObject)
      return false;
    // A list's blank area may move the panel; rows, text editors and scrollbars keep their behavior.
    for (QWidget *parent = widget->parentWidget(); parent && parent != window;
         parent = parent->parentWidget()) {
      if (auto view = qobject_cast<QAbstractItemView *>(parent))
        return widget == view->viewport() && !view->indexAt(position).isValid();
      if (qobject_cast<QAbstractScrollArea *>(parent))
        return false;
    }
    return true;
  }
  QPointer<QWidget> m_owner, m_window;
  QPoint m_press, m_origin;
  bool m_started = false, m_systemMove = false;
};
} // namespace reader
