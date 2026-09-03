#pragma once
#include <QJsonObject>
#include <QKeySequence>
#include <QMap>

namespace reader {

enum class KeyAction {
    PageUp, PageDown, LineUp, LineDown, ChapterUp, ChapterDown,
    Search, Jump, AddBookmark, EditMode, AutoPage,
    FontZoomIn, FontZoomOut, Fullscreen, HideBorder, AlwaysOnTop,
    HideWindow, OpenFile, Quit, MouseLeaveHide
};

class Keyset
{
public:
    Keyset() { reset(); }
    QKeySequence shortcut(KeyAction action) const { return m_keys.value(action); }
    void setShortcut(KeyAction action, const QKeySequence &seq) { m_keys.insert(action, seq); }
    static QKeySequence defaultShortcut(KeyAction action);
    void reset();
    void load(const QJsonObject &o);
    QJsonObject save() const;
    QList<KeyAction> actions() const { return m_keys.keys(); }

private:
    QMap<KeyAction, QKeySequence> m_keys;
    static const QMap<KeyAction, QKeySequence> &defaults();
};

}
