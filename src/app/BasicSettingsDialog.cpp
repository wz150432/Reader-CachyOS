#include "app/BasicSettingsDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace reader {

BasicSettingsDialog::BasicSettingsDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("基本设置"));
    m_intervalSpin = new QSpinBox(this);
    m_intervalSpin->setRange(500, 60000);
    m_intervalSpin->setSingleStep(500);
    m_intervalSpin->setSuffix(QStringLiteral(" ms"));
    m_intervalSpin->setValue(m_settings->behavior.autoPageIntervalMs);
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QStringLiteral("翻页"), false);
    m_modeCombo->addItem(QStringLiteral("滚动"), true);
    m_modeCombo->setCurrentIndex(m_settings->behavior.autoPageScrollMode ? 1 : 0);
    m_scrollStepSpin = new QSpinBox(this);
    m_scrollStepSpin->setRange(1, 20);
    m_scrollStepSpin->setValue(m_settings->behavior.scrollStep);
    m_minimizeToTray = new QCheckBox(QStringLiteral("关闭窗口时最小化到托盘"), this);
    m_minimizeToTray->setChecked(m_settings->behavior.minimizeToTray);
    m_doubleClickHide = new QCheckBox(QStringLiteral("左右键同时按下隐藏窗口"), this);
    m_doubleClickHide->setChecked(m_settings->behavior.doubleClickHide);
    m_mouseLeaveHide = new QCheckBox(
        QStringLiteral("鼠标离开自动隐藏 / Ctrl+悬停恢复（Ctrl+Shift+Alt+P）"), this);
    m_mouseLeaveHide->setChecked(m_settings->behavior.mouseLeaveHideEnabled);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("自动翻页间隔"), m_intervalSpin);
    form->addRow(QStringLiteral("自动翻页模式"), m_modeCombo);
    form->addRow(QStringLiteral("滚动速度（像素/步）"), m_scrollStepSpin);
    form->addRow(QString(), m_minimizeToTray);
    form->addRow(QString(), m_doubleClickHide);
    form->addRow(QString(), m_mouseLeaveHide);

    auto *ok = new QPushButton(QStringLiteral("确定"), this);
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    connect(ok, &QPushButton::clicked, this, &BasicSettingsDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttons);
}

void BasicSettingsDialog::accept()
{
    m_settings->behavior.autoPageIntervalMs = m_intervalSpin->value();
    m_settings->behavior.autoPageScrollMode = m_modeCombo->currentData().toBool();
    m_settings->behavior.scrollStep = m_scrollStepSpin->value();
    m_settings->behavior.minimizeToTray = m_minimizeToTray->isChecked();
    m_settings->behavior.doubleClickHide = m_doubleClickHide->isChecked();
    m_settings->behavior.mouseLeaveHideEnabled = m_mouseLeaveHide->isChecked();
    m_settings->save();
    QDialog::accept();
}

}
