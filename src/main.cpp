#include <QApplication>
#include <QIcon>
#include <QPalette>
#include <QSurfaceFormat>
#include <QStyleFactory>
#include "app/MainWindow.h"
#include "app/RemoteControl.h"

int main(int argc, char *argv[])
{
    const QByteArray command = argc >= 2 ? QByteArray(argv[1]) : QByteArray();
    if (command == "--toggle-hide" || command == "--hide" || command == "--show") {
        // Global shortcuts only need local IPC: avoid display, theme and font startup.
        QCoreApplication app(argc, argv);
        QString error;
        return reader::RemoteControl::sendCommand(QString::fromLatin1(command.mid(2)), &error)
            ? 0 : 1;
    }
    QSurfaceFormat fmt;
    fmt.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);
    QApplication app(argc, argv);
    const QStringList args = app.arguments();
    app.setApplicationName(QStringLiteral("Reader"));
    app.setOrganizationName(QStringLiteral("Reader"));
    app.setDesktopFileName(QStringLiteral("reader.desktop"));
    app.setWindowIcon(QIcon(QStringLiteral(":/reader/icon.svg")));
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
    QStringList openFiles;
    for (int i = 1; i < args.size(); ++i) {
        if (!args.at(i).startsWith(QLatin1Char('-')))
            openFiles.append(args.at(i));
    }
    w.applyWindowState();
    if (!w.hasSavedWindowGeometry())
        w.resize(960, 720);
    if (openFiles.isEmpty())
        w.openLastRead();
    else
        w.openBook(openFiles.first());
    w.show();
    w.restoreNiriFloatingPosition();
    return app.exec();
}
