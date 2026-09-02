#include <QApplication>
#include <QPalette>
#include <QSurfaceFormat>
#include <QStyleFactory>
#include "app/MainWindow.h"

int main(int argc, char *argv[])
{
    QSurfaceFormat fmt;
    fmt.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Reader"));
    app.setOrganizationName(QStringLiteral("Reader"));
    app.setDesktopFileName(QStringLiteral("reader.desktop"));
    if (QStyle *style = QStyleFactory::create(QStringLiteral("Fusion")))
        app.setStyle(style);
    QPalette pal;
    pal.setColor(QPalette::Window, Qt::white);
    pal.setColor(QPalette::Base, Qt::white);
    pal.setColor(QPalette::AlternateBase, QColor(0xF5, 0xF5, 0xF5));
    pal.setColor(QPalette::Text, QColor(0x33, 0x33, 0x33));
    pal.setColor(QPalette::WindowText, QColor(0x33, 0x33, 0x33));
    pal.setColor(QPalette::Button, QColor(0xF7, 0xF7, 0xF7));
    pal.setColor(QPalette::ButtonText, QColor(0x33, 0x33, 0x33));
    pal.setColor(QPalette::Highlight, QColor(0xCF, 0xE2, 0xF9));
    pal.setColor(QPalette::HighlightedText, QColor(0x11, 0x11, 0x11));
    pal.setColor(QPalette::PlaceholderText, QColor(0x99, 0x99, 0x99));
    pal.setColor(QPalette::ToolTipBase, Qt::white);
    pal.setColor(QPalette::ToolTipText, QColor(0x33, 0x33, 0x33));
    app.setPalette(pal);
    reader::MainWindow w;
    w.resize(960, 720);
    w.show();
    return app.exec();
}
