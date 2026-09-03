#include "app/KeysetDialog.h"
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace reader {

static QString actionName(KeyAction a)
{
    switch (a) {
    case KeyAction::PageUp: return QStringLiteral("上一页");
    case KeyAction::PageDown: return QStringLiteral("下一页");
    case KeyAction::LineUp: return QStringLiteral("上一行");
    case KeyAction::LineDown: return QStringLiteral("下一行");
    case KeyAction::ChapterUp: return QStringLiteral("上一章");
    case KeyAction::ChapterDown: return QStringLiteral("下一章");
    case KeyAction::Search: return QStringLiteral("搜索");
    case KeyAction::Jump: return QStringLiteral("进度跳转");
    case KeyAction::AddBookmark: return QStringLiteral("添加书签");
    case KeyAction::EditMode: return QStringLiteral("编辑模式");
    case KeyAction::AutoPage: return QStringLiteral("自动翻页");
    case KeyAction::FontZoomIn: return QStringLiteral("字号放大");
    case KeyAction::FontZoomOut: return QStringLiteral("字号缩小");
    case KeyAction::Fullscreen: return QStringLiteral("全屏");
    case KeyAction::HideBorder: return QStringLiteral("隐藏边框");
    case KeyAction::AlwaysOnTop: return QStringLiteral("窗口置顶");
    case KeyAction::HideWindow: return QStringLiteral("隐藏窗口");
    case KeyAction::OpenFile: return QStringLiteral("打开文件");
    case KeyAction::Quit: return QStringLiteral("退出");
    case KeyAction::MouseLeaveHide: return QStringLiteral("鼠标离开隐藏");
    }
    return QString();
}

KeysetDialog::KeysetDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("按键设置"));
    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("功能"), QStringLiteral("快捷键")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    reload();

    auto *edit = new QPushButton(QStringLiteral("修改选中项"), this);
    auto *reset = new QPushButton(QStringLiteral("恢复默认"), this);
    auto *ok = new QPushButton(QStringLiteral("确定"), this);
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    connect(edit, &QPushButton::clicked, this, &KeysetDialog::editShortcut);
    connect(reset, &QPushButton::clicked, this, &KeysetDialog::restoreDefaults);
    connect(ok, &QPushButton::clicked, this, &KeysetDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(edit);
    buttons->addWidget(reset);
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_table);
    layout->addLayout(buttons);
    resize(420, 480);
}

void KeysetDialog::reload()
{
    const QList<KeyAction> actions = m_settings->keyset.actions();
    m_table->setRowCount(actions.size());
    int row = 0;
    for (const KeyAction a : actions) {
        auto *name = new QTableWidgetItem(actionName(a));
        auto *seq = new QTableWidgetItem(m_settings->keyset.shortcut(a).toString());
        name->setData(Qt::UserRole, int(a));
        m_table->setItem(row, 0, name);
        m_table->setItem(row, 1, seq);
        ++row;
    }
}

void KeysetDialog::editShortcut()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;
    const KeyAction a = static_cast<KeyAction>(m_table->item(row, 0)->data(Qt::UserRole).toInt());
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("设置快捷键"));
    auto *edit = new QKeySequenceEdit(m_settings->keyset.shortcut(a), &dlg);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto *layout = new QVBoxLayout(&dlg);
    layout->addWidget(edit);
    layout->addWidget(box);
    if (dlg.exec() == QDialog::Accepted && !edit->keySequence().isEmpty()) {
        m_settings->keyset.setShortcut(a, edit->keySequence());
        reload();
    }
}

void KeysetDialog::restoreDefaults()
{
    if (QMessageBox::question(this, QStringLiteral("恢复默认"),
            QStringLiteral("确定将快捷键恢复为默认设置？")) != QMessageBox::Yes)
        return;
    m_settings->keyset.reset();
    reload();
}

void KeysetDialog::accept()
{
    m_settings->save();
    QDialog::accept();
}

}
