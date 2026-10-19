#include "app/KeysetDialog.h"
#include <QAbstractItemView>
#include <QLabel>
#include <QLineEdit>
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
    , m_keyset(settings->keyset)
{
    setWindowTitle(QStringLiteral("按键设置"));
    m_table = new QTableWidget(0, 2, this);
    m_table->setObjectName(QStringLiteral("shortcutTable"));
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(40);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setHorizontalHeaderLabels({QStringLiteral("功能"), QStringLiteral("快捷键")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    reload();

    auto *reset = new QPushButton(QStringLiteral("恢复默认"), this);
    auto *ok = new QPushButton(QStringLiteral("确定"), this);
    ok->setDefault(true);
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    connect(reset, &QPushButton::clicked, this, &KeysetDialog::restoreDefaults);
    connect(ok, &QPushButton::clicked, this, &KeysetDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(reset);
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    auto *hint = new QLabel(QStringLiteral("点击快捷键一栏，直接按下新的按键组合。确定后保存。"), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addWidget(m_table);
    layout->addLayout(buttons);
    resize(560, 520);
}

void KeysetDialog::reload()
{
    const QList<KeyAction> actions = m_keyset.actions();
    m_table->setRowCount(actions.size());
    int row = 0;
    for (const KeyAction a : actions) {
        auto *name = new QTableWidgetItem(actionName(a));
        name->setData(Qt::UserRole, int(a));
        m_table->setItem(row, 0, name);
        auto *edit = new QKeySequenceEdit(m_keyset.shortcut(a), m_table);
        edit->setMaximumSequenceLength(1);
        edit->setFinishingKeyCombinations({});
        edit->setAccessibleName(QStringLiteral("%1的快捷键").arg(actionName(a)));
        if (auto *input = edit->findChild<QLineEdit *>())
            input->setPlaceholderText(QStringLiteral("按下快捷键"));
        connect(edit, &QKeySequenceEdit::keySequenceChanged, this, [this, a](const QKeySequence &sequence) {
            if (!sequence.isEmpty())
                m_keyset.setShortcut(a, sequence);
        });
        m_table->setCellWidget(row, 1, edit);
        ++row;
    }
}

void KeysetDialog::restoreDefaults()
{
    if (QMessageBox::question(this, QStringLiteral("恢复默认"),
            QStringLiteral("确定将快捷键恢复为默认设置？")) != QMessageBox::Yes)
        return;
    m_keyset.reset();
    reload();
}

void KeysetDialog::accept()
{
    m_settings->keyset = m_keyset;
    m_settings->save();
    QDialog::accept();
}

}
