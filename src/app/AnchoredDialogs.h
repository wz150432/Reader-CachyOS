#pragma once
#include "MenuPanel.h"
#include "UiStyle.h"
#include <QApplication>
#include <QDialog>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QScreen>
#include <QTimer>

namespace reader {
// Apply the same reader-owned popup policy to standard Qt dialogs, including
// message boxes and file/color pickers created inside other panels.
class AnchoredDialogs final : public QObject {
public:
  explicit AnchoredDialogs(QMainWindow *owner) : QObject(owner), m_owner(owner) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    qApp->installEventFilter(this);
  }

protected:
  bool eventFilter(QObject *object, QEvent *event) override {
    if (object == m_owner) {
      if (event->type() == QEvent::Move) {
        const auto delta = m_owner->pos() - m_origin;
        m_origin = m_owner->pos();
        for (const auto &panel : m_panels)
          if (panel && panel->isVisible()) panel->move(panel->pos() + delta);
      } else if (event->type() == QEvent::Show) {
        m_origin = m_owner->pos();
      } else if (event->type() == QEvent::Hide || event->type() == QEvent::Close) {
        const auto panels = m_panels;
        for (const auto &panel : panels) {
          if (!panel || !panel->isVisible()) continue;
          if (auto dialog = qobject_cast<QDialog *>(panel)) dialog->reject();
          else panel->hide();
        }
      }
    }
    auto widget = qobject_cast<QWidget *>(object);
    if (!widget || (!qobject_cast<QDialog *>(widget) && !qobject_cast<QMenu *>(widget)) ||
        dynamic_cast<MenuPanel *>(widget) || !owned(widget))
      return false;
    if (event->type() == QEvent::Polish && qobject_cast<QDialog *>(widget)) {
      widget->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
      widget->setProperty("menuPanel", true);
      widget->setAttribute(Qt::WA_StyledBackground);
      widget->setStyleSheet(readerUiStyle());
    }
    if (event->type() == QEvent::Show) {
      if (!m_owner->isVisible()) {
        // exec() installs its event loop after show(): close on the next turn.
        QPointer<QWidget> panel(widget);
        QTimer::singleShot(0, this, [panel] {
          if (!panel) return;
          if (auto dialog = qobject_cast<QDialog *>(panel.data())) dialog->reject();
          else panel->hide();
        });
        return false;
      }
      bool tracked = false;
      for (const auto &panel : m_panels) tracked |= panel == widget;
      if (!tracked) m_panels.append(widget);
      if (qobject_cast<QDialog *>(widget)) {
        position(widget);
        // Some standard dialogs choose a default position during showEvent.
        QPointer<QWidget> panel(widget);
        QTimer::singleShot(0, this, [this, panel] {
          if (panel && panel->isVisible()) position(panel);
        });
      }
    }
    return false;
  }

private:
  bool owned(QObject *object) const {
    for (auto parent = object->parent(); parent; parent = parent->parent())
      if (parent == m_owner) return true;
    return false;
  }
  void position(QWidget *panel) {
    if (!m_owner || !m_owner->isVisible()) return;
    const QRect available = m_owner->screen()->availableGeometry().adjusted(8, 8, -8, -8);
    QPoint point = m_owner->menuBar()->mapToGlobal(QPoint(0, m_owner->menuBar()->height()));
    if (point.y() + panel->height() > available.bottom() + 1)
      point.setY(m_owner->mapToGlobal(QPoint(0, 0)).y() - panel->height());
    point.setX(qBound(available.left(), point.x(), qMax(available.left(), available.right() - panel->width() + 1)));
    point.setY(qBound(available.top(), point.y(), qMax(available.top(), available.bottom() - panel->height() + 1)));
    panel->move(point);
  }
  QPointer<QMainWindow> m_owner;
  QList<QPointer<QWidget>> m_panels;
  QPoint m_origin;
};
} // namespace reader
