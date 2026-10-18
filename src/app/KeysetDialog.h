#pragma once
#include <QDialog>
#include "core/Settings.h"

class QTableWidget;

namespace reader {

class KeysetDialog : public QDialog
{
    Q_OBJECT
public:
    explicit KeysetDialog(Settings *settings, QWidget *parent = nullptr);

private slots:
    void restoreDefaults();
    void accept() override;

private:
    void reload();
    Settings *m_settings;
    Keyset m_keyset;
    QTableWidget *m_table;
};

}
