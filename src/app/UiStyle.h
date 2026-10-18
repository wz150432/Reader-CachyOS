#pragma once

#include <QString>

namespace reader {

// Scope the chrome to Qt controls: ReadingView paints the user's own colors.
inline QString readerUiStyle()
{
    return QStringLiteral(R"(
        QMenuBar { background: #fafaf9; color: #333333; padding: 1px 6px;
                   border-bottom: 1px solid #e3e3df; spacing: 4px; }
        QMenuBar::item { padding: 2px 10px; border-radius: 7px; }
        QMenuBar::item:selected, QMenuBar::item:pressed { background: #e9eeeb; }
        QMenu { background: #ffffff; color: #333333; padding: 6px;
                border: 1px solid #d5d9d6; border-radius: 12px; }
        QMenu::item { padding: 7px 28px 7px 12px; border-radius: 7px; }
        QMenu::item:selected { background: #e9eeeb; color: #263e32; }
        QMenu::item:disabled { color: #767676; }
        QMenu::separator { height: 1px; background: #e3e3df; margin: 5px 8px; }
        QToolBar#searchBar { background: #fafaf9; border: none;
                            border-bottom: 1px solid #e3e3df; padding: 4px 8px; spacing: 8px; }
        QDialog { background: #fafaf9; color: #333333; }
        QDialog QLabel, QDialog QCheckBox { color: #333333; }
        QLabel[sectionHeading="true"] { color: #385746; font-weight: 600;
                                        padding-top: 6px; padding-bottom: 2px; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QKeySequenceEdit {
            min-height: 24px; padding: 3px 8px; background: #ffffff;
            color: #333333; border: 1px solid #c8cec9; border-radius: 8px;
            placeholder-text-color: #6b706b;
            selection-background-color: #dce9e0; selection-color: #243d2e;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QKeySequenceEdit:focus {
            border-color: #567c64;
        }
        QSpinBox { padding-right: 22px; }
        QSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right;
                              width: 20px; height: 15px; }
        QSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right;
                                width: 20px; height: 15px; }
        QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled {
            background: #efefec; color: #767676;
        }
        QPushButton { min-height: 24px; padding: 4px 14px; background: #ffffff;
                      color: #333333; border: 1px solid #c8cec9; border-radius: 11px; }
        QPushButton:hover { background: #edf2ee; border-color: #9dab9f; }
        QPushButton:pressed { background: #dde7df; }
        QPushButton:focus { border-color: #567c64; }
        QPushButton:default { background: #385c46; color: #ffffff; border-color: #385c46; }
        QPushButton:default:hover { background: #2e4c3a; }
        QPushButton:disabled { color: #767676; background: #efefec; border-color: #d9ddd9; }
        QKeySequenceEdit QLineEdit { border: none; background: transparent;
                                    padding: 0; min-height: 0; }
        QTableWidget#shortcutTable QKeySequenceEdit {
            border: 1px solid transparent; background: transparent;
            border-radius: 8px; margin: 2px 4px; padding: 2px 8px;
        }
        QTableWidget#shortcutTable QKeySequenceEdit:focus {
            border-color: #567c64; background: #edf2ee;
        }
        QTableWidget#shortcutTable QKeySequenceEdit QLineEdit:focus {
            border: 1px solid #567c64; border-radius: 7px;
            background: #edf2ee; padding: 2px 6px;
        }
        QToolBar QToolButton { border: none; border-radius: 8px; padding: 4px 8px; }
        QToolBar QToolButton:hover { background: #e9eeeb; }
        QCheckBox { spacing: 8px; padding: 3px 0; }
        QCheckBox:disabled { color: #767676; }
        QTreeWidget, QListWidget, QTableWidget { background: #ffffff; color: #333333;
            border: 1px solid #d5d9d6; alternate-background-color: #f6f7f4;
            selection-background-color: #e1ebe3; selection-color: #243d2e; }
        QTreeWidget::item, QListWidget::item { padding: 8px 12px; }
        QTreeWidget::item:hover, QListWidget::item:hover { background: #f0f3ef; }
        QTreeWidget::item:selected, QListWidget::item:selected { background: #e1ebe3; color: #243d2e; }
        QTreeWidget#tocView { border: none; padding: 12px; }
        QHeaderView::section { background: #f0f2ee; color: #333333;
                               border: none; padding: 8px; }
        QToolTip { background: #ffffff; color: #333333; border: 1px solid #c8cec9; padding: 5px; }
    )");
}

} // namespace reader
