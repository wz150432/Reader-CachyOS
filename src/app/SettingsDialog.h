#pragma once
#include <QDialog>
#include "core/Settings.h"

class QFontComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;

namespace reader {

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(Settings *settings, QWidget *parent = nullptr);

private slots:
    void pickBackgroundColor();
    void pickTextColor();
    void accept() override;

private:
    Settings *m_settings;
    QFontComboBox *m_fontCombo;
    QSpinBox *m_sizeSpin;
    QSpinBox *m_lineGapSpin;
    QCheckBox *m_firstLineIndent;
    QPushButton *m_bgButton;
    QPushButton *m_textButton;
};

}
