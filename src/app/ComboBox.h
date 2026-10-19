#pragma once

#include <QComboBox>
#include <QPainter>
#include <QPainterPath>

namespace reader {
class ComboBox final : public QComboBox {
public:
  explicit ComboBox(QWidget *parent = nullptr) : QComboBox(parent) {
    setProperty("readerCombo", true);
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    QComboBox::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(isEnabled() ? QColor("#626d65") : QColor("#a2aaa4"),
                        1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const qreal x = width() - 17, y = height() / 2.0;
    QPainterPath arrow;
    arrow.moveTo(x - 3, y - 1.5);
    arrow.lineTo(x, y + 1.5);
    arrow.lineTo(x + 3, y - 1.5);
    painter.drawPath(arrow);
  }
};
} // namespace reader
