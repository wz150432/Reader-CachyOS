#pragma once

#include <QByteArray>
#include <QPoint>
#include <optional>

namespace reader {

struct NiriReaderWindow
{
    quint64 id;
    QPoint position;
};

std::optional<NiriReaderWindow> findNiriReaderWindow(const QByteArray &windowsJson,
                                                     qint64 pid);
std::optional<NiriReaderWindow> currentNiriReaderWindow();
bool moveNiriReaderWindow(quint64 id, const QPoint &position);
bool reloadNiriConfig();

}
