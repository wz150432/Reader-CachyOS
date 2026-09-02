#pragma once
#include <QDialog>
#include "core/Settings.h"

class QLineEdit;

namespace reader {

class AdvancedSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AdvancedSettingsDialog(Settings *settings, QWidget *parent = nullptr);

private slots:
    void accept() override;

private:
    Settings *m_settings;
    QLineEdit *m_regexEdit;
};

}
