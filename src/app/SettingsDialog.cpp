#include "app/SettingsDialog.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace reader {

SettingsDialog::SettingsDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("显示设置"));
    m_fontCombo = new QFontComboBox(this);
    m_fontCombo->setCurrentFont(m_settings->display.font);
    m_sizeSpin = new QSpinBox(this);
    m_sizeSpin->setRange(6, 72);
    m_sizeSpin->setValue(m_settings->display.font.pointSize());
    m_lineGapSpin = new QSpinBox(this);
    m_lineGapSpin->setRange(0, 40);
    m_lineGapSpin->setValue(m_settings->display.lineGap);
    m_firstLineIndent = new QCheckBox(QStringLiteral("首行缩进"), this);
    m_firstLineIndent->setChecked(m_settings->display.firstLineIndent);
    m_bgButton = new QPushButton(this);
    m_textButton = new QPushButton(this);
    const auto setColor = [](QPushButton *b, const QColor &c) {
        QPixmap pm(24, 24);
        pm.fill(c);
        b->setIcon(QIcon(pm));
        b->setText(c.name());
    };
    setColor(m_bgButton, m_settings->display.bgColor);
    setColor(m_textButton, m_settings->display.textColor);
    connect(m_bgButton, &QPushButton::clicked, this, &SettingsDialog::pickBackgroundColor);
    connect(m_textButton, &QPushButton::clicked, this, &SettingsDialog::pickTextColor);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("字体"), m_fontCombo);
    form->addRow(QStringLiteral("字号"), m_sizeSpin);
    form->addRow(QStringLiteral("行距"), m_lineGapSpin);
    form->addRow(QStringLiteral("背景色"), m_bgButton);
    form->addRow(QStringLiteral("文字颜色"), m_textButton);
    form->addRow(QString(), m_firstLineIndent);

    auto *ok = new QPushButton(QStringLiteral("确定"), this);
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    connect(ok, &QPushButton::clicked, this, &SettingsDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttons);
}

void SettingsDialog::pickBackgroundColor()
{
    const QColor c = QColorDialog::getColor(m_settings->display.bgColor, this, QStringLiteral("选择背景色"));
    if (c.isValid()) {
        m_settings->display.bgColor = c;
        QPixmap pm(24, 24);
        pm.fill(c);
        m_bgButton->setIcon(QIcon(pm));
        m_bgButton->setText(c.name());
    }
}

void SettingsDialog::pickTextColor()
{
    const QColor c = QColorDialog::getColor(m_settings->display.textColor, this, QStringLiteral("选择文字颜色"));
    if (c.isValid()) {
        m_settings->display.textColor = c;
        QPixmap pm(24, 24);
        pm.fill(c);
        m_textButton->setIcon(QIcon(pm));
        m_textButton->setText(c.name());
    }
}

void SettingsDialog::accept()
{
    m_settings->display.font.setFamily(m_fontCombo->currentFont().family());
    m_settings->display.font.setPointSize(m_sizeSpin->value());
    m_settings->display.lineGap = m_lineGapSpin->value();
    m_settings->display.firstLineIndent = m_firstLineIndent->isChecked();
    m_settings->save();
    QDialog::accept();
}

}
