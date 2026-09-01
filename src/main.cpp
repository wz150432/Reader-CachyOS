#include <QApplication>
#include "app/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Reader"));
    app.setOrganizationName(QStringLiteral("Reader"));
    reader::MainWindow w;
    w.resize(960, 720);
    w.show();
    return app.exec();
}
