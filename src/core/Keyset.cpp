#include "core/Keyset.h"

namespace reader {

static QMap<KeyAction, QKeySequence> makeDefaults()
{
    QMap<KeyAction, QKeySequence> d;
    d.insert(KeyAction::PageUp, QKeySequence(Qt::Key_Left));
    d.insert(KeyAction::PageDown, QKeySequence(Qt::Key_Right));
    d.insert(KeyAction::LineUp, QKeySequence(Qt::Key_Up));
    d.insert(KeyAction::LineDown, QKeySequence(Qt::Key_Down));
    d.insert(KeyAction::ChapterUp, QKeySequence(Qt::CTRL | Qt::Key_Left));
    d.insert(KeyAction::ChapterDown, QKeySequence(Qt::CTRL | Qt::Key_Right));
    d.insert(KeyAction::Search, QKeySequence(QStringLiteral("Ctrl+F")));
    d.insert(KeyAction::Jump, QKeySequence(QStringLiteral("Ctrl+G")));
    d.insert(KeyAction::AddBookmark, QKeySequence(QStringLiteral("Ctrl+M")));
    d.insert(KeyAction::EditMode, QKeySequence(QStringLiteral("Ctrl+E")));
    d.insert(KeyAction::AutoPage, QKeySequence(Qt::Key_Space));
    d.insert(KeyAction::FontZoomIn, QKeySequence(QStringLiteral("Ctrl+=")));
    d.insert(KeyAction::FontZoomOut, QKeySequence(QStringLiteral("Ctrl+-")));
    d.insert(KeyAction::Fullscreen, QKeySequence(Qt::Key_F11));
    d.insert(KeyAction::HideBorder, QKeySequence(Qt::Key_F12));
    d.insert(KeyAction::AlwaysOnTop, QKeySequence(QStringLiteral("Alt+T")));
    d.insert(KeyAction::HideWindow, QKeySequence(QStringLiteral("Alt+H")));
    d.insert(KeyAction::OpenFile, QKeySequence(QStringLiteral("Ctrl+O")));
    d.insert(KeyAction::Quit, QKeySequence(QStringLiteral("Ctrl+Q")));
    d.insert(KeyAction::MouseLeaveHide, QKeySequence(QStringLiteral("Ctrl+Shift+Alt+P")));
    return d;
}

const QMap<KeyAction, QKeySequence> &Keyset::defaults()
{
    static const QMap<KeyAction, QKeySequence> d = makeDefaults();
    return d;
}

QKeySequence Keyset::defaultShortcut(KeyAction action)
{
    return defaults().value(action);
}

void Keyset::reset()
{
    m_keys = defaults();
}

void Keyset::load(const QJsonObject &o)
{
    reset();
    const QStringList names = o.keys();
    for (const QString &name : names) {
        bool ok = false;
        const int v = name.toInt(&ok);
        if (!ok)
            continue;
        const KeyAction a = static_cast<KeyAction>(v);
        if (!m_keys.contains(a))
            continue;
        const QString seq = o.value(name).toString();
        if (!seq.isEmpty())
            m_keys.insert(a, QKeySequence(seq));
    }
}

QJsonObject Keyset::save() const
{
    QJsonObject o;
    for (auto it = m_keys.cbegin(); it != m_keys.cend(); ++it)
        o.insert(QString::number(int(it.key())), it.value().toString());
    return o;
}

}
