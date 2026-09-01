#include "app/MainWindow.h"

namespace reader {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Reader"));
}

}
