#pragma once
#include <QMainWindow>

namespace reader {

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
};

}
