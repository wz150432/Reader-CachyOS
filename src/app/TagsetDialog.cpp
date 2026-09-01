#include "app/TagsetDialog.h"
#include <QColorDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace reader {

TagsetDialog::TagsetDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("标签设置"));
    m_keywordEdit = new QLineEdit(this);
    m_keywordEdit->setPlaceholderText(QStringLiteral("输入关键字"));
    auto *addBtn = new QPushButton(QStringLiteral("添加"), this);
    auto *rmBtn = new QPushButton(QStringLiteral("删除选中"), this);
    connect(addBtn, &QPushButton::clicked, this, &TagsetDialog::addTag);
    connect(rmBtn, &QPushButton::clicked, this, &TagsetDialog::removeTag);
    auto *top = new QHBoxLayout;
    top->addWidget(m_keywordEdit);
    top->addWidget(addBtn);
    top->addWidget(rmBtn);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("关键字"), QStringLiteral("前景"), QStringLiteral("背景"), QStringLiteral("启用")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    reload();

    auto *ok = new QPushButton(QStringLiteral("确定"), this);
    connect(ok, &QPushButton::clicked, this, &TagsetDialog::accept);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_table);
    layout->addWidget(ok, 0, Qt::AlignRight);
    resize(500, 420);
}

void TagsetDialog::reload()
{
    m_table->setRowCount(m_settings->tags.size());
    for (int i = 0; i < m_settings->tags.size(); ++i) {
        const TagItem &t = m_settings->tags.at(i);
        auto *kw = new QTableWidgetItem(t.keyword);
        auto *fg = new QTableWidgetItem(t.fg.isValid() ? t.fg.name() : QString());
        auto *bg = new QTableWidgetItem(t.bg.isValid() ? t.bg.name() : QString());
        auto *en = new QTableWidgetItem();
        en->setCheckState(t.enabled ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(i, 0, kw);
        m_table->setItem(i, 1, fg);
        m_table->setItem(i, 2, bg);
        m_table->setItem(i, 3, en);
    }
}

void TagsetDialog::addTag()
{
    const QString kw = m_keywordEdit->text().trimmed();
    if (kw.isEmpty())
        return;
    const QColor bg = QColorDialog::getColor(QColor(0xFF, 0xE0, 0x66), this, QStringLiteral("选择高亮背景色"));
    TagItem t;
    t.keyword = kw;
    t.bg = bg.isValid() ? bg : QColor(0xFF, 0xE0, 0x66);
    t.fg = QColor();
    t.enabled = true;
    m_settings->tags.append(t);
    m_keywordEdit->clear();
    reload();
}

void TagsetDialog::removeTag()
{
    const int row = m_table->currentRow();
    if (row >= 0 && row < m_settings->tags.size())
        m_settings->tags.removeAt(row);
    reload();
}

void TagsetDialog::accept()
{
    for (int i = 0; i < m_settings->tags.size() && i < m_table->rowCount(); ++i) {
        m_settings->tags[i].keyword = m_table->item(i, 0)->text().trimmed();
        m_settings->tags[i].fg = QColor(m_table->item(i, 1)->text());
        m_settings->tags[i].bg = QColor(m_table->item(i, 2)->text());
        m_settings->tags[i].enabled = m_table->item(i, 3)->checkState() == Qt::Checked;
    }
    m_settings->save();
    QDialog::accept();
}

}
