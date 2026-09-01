#pragma once
#include <QDialog>
#include "core/Settings.h"

class QTableWidget;
class QLineEdit;

namespace reader {

class TagsetDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TagsetDialog(Settings *settings, QWidget *parent = nullptr);

private slots:
    void addTag();
    void removeTag();
    void accept() override;

private:
    void reload();
    Settings *m_settings;
    QTableWidget *m_table;
    QLineEdit *m_keywordEdit;
};

}
