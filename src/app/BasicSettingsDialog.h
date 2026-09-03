#pragma once
#include <QDialog>
#include "core/Settings.h"

class QSpinBox;
class QCheckBox;
class QComboBox;

namespace reader {

class BasicSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BasicSettingsDialog(Settings *settings, QWidget *parent = nullptr);

private slots:
    void accept() override;

private:
    Settings *m_settings;
    QSpinBox *m_intervalSpin;
    QComboBox *m_modeCombo;
    QSpinBox *m_scrollStepSpin;
    QCheckBox *m_minimizeToTray;
    QCheckBox *m_doubleClickHide;
    QCheckBox *m_globalHide;
    QCheckBox *m_globalHidePopup;
    QCheckBox *m_mouseLeaveHide;
};

}
