#include "app/AdvancedSettingsDialog.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace reader {

AdvancedSettingsDialog::AdvancedSettingsDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("高级设置"));
    m_regexEdit = new QLineEdit(this);
    m_regexEdit->setClearButtonEnabled(true);
    m_regexEdit->setPlaceholderText(QStringLiteral("留空使用默认解析"));
    m_regexEdit->setText(m_settings->chapterRegex);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("章节正则"), m_regexEdit);

    auto *clear = new QPushButton(QStringLiteral("清除正则"), this);
    connect(clear, &QPushButton::clicked, m_regexEdit, &QLineEdit::clear);
    auto *ok = new QPushButton(QStringLiteral("确定"), this);
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    connect(ok, &QPushButton::clicked, this, &AdvancedSettingsDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(clear);
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttons);
    resize(420, 120);
}

void AdvancedSettingsDialog::accept()
{
    const QString pattern = m_regexEdit->text().trimmed();
    if (!pattern.isEmpty()) {
        const QRegularExpression re(pattern);
        if (!re.isValid()) {
            QMessageBox::warning(this, QStringLiteral("正则表达式无效"),
                QStringLiteral("无法编译章节正则：%1").arg(re.errorString()));
            return;
        }
    }
    m_settings->chapterRegex = pattern;
    m_settings->save();
    QDialog::accept();
}

}
