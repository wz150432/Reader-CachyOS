#include "app/SettingsDialog.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QCursor>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace reader {

SettingsDialog::SettingsDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("显示设置"));
    m_fontCombo = new QFontComboBox(this);
    m_fontCombo->setCurrentFont(m_settings->display.font);
    m_sizeSpin = new QSpinBox(this);
    m_sizeSpin->setRange(6, 72);
    m_sizeSpin->setValue(m_settings->display.font.pointSize());
    m_lineGapSpin = new QSpinBox(this);
    m_lineGapSpin->setRange(0, 40);
    m_lineGapSpin->setValue(m_settings->display.lineGap);
    m_firstLineIndent = new QCheckBox(QStringLiteral("首行缩进"), this);
    m_firstLineIndent->setChecked(m_settings->display.firstLineIndent);
    m_paragraphGapSpin = new QSpinBox(this);
    m_paragraphGapSpin->setRange(0, 80);
    m_paragraphGapSpin->setValue(m_settings->display.paragraphGap);
    m_compressBlankLines = new QCheckBox(QStringLiteral("压缩空行"), this);
    m_compressBlankLines->setChecked(m_settings->display.compressBlankLines);
    m_wordWrap = new QCheckBox(QStringLiteral("英文单词自动换行"), this);
    m_wordWrap->setChecked(m_settings->display.wordWrap);
    m_chapterPageBreak = new QCheckBox(QStringLiteral("章前分页"), this);
    m_chapterPageBreak->setChecked(m_settings->display.chapterPageBreak);
    m_titleFontCombo = new QFontComboBox(this);
    m_titleFontCombo->setCurrentFont(m_settings->display.titleFont);
    m_titleSizeSpin = new QSpinBox(this);
    m_titleSizeSpin->setRange(6, 72);
    m_titleSizeSpin->setValue(m_settings->display.titleFont.pointSize());
    m_useSameFont = new QCheckBox(QStringLiteral("正文与标题使用同一字体"), this);
    m_useSameFont->setChecked(m_settings->display.useSameFont);
    m_bgImageButton = new QPushButton(this);
    m_bgImageButton->setText(m_settings->display.bgImagePath.isEmpty()
        ? QStringLiteral("无") : QFileInfo(m_settings->display.bgImagePath).fileName());
    connect(m_bgImageButton, &QPushButton::clicked, this, &SettingsDialog::pickBackgroundImage);
    auto *clearBg = new QPushButton(QStringLiteral("清除背景图"), this);
    connect(clearBg, &QPushButton::clicked, this, &SettingsDialog::clearBackgroundImage);
    m_alphaSlider = new QSlider(Qt::Horizontal, this);
    m_alphaSlider->setRange(0, 255);
    m_alphaSlider->setValue(m_settings->display.windowAlpha);
    m_alphaLabel = new QLabel(this);
    connect(m_alphaSlider, &QSlider::valueChanged, this, &SettingsDialog::updateAlphaLabel);
    updateAlphaLabel(m_settings->display.windowAlpha);
    m_bgButton = new QPushButton(this);
    m_textButton = new QPushButton(this);
    const auto setColor = [](QPushButton *b, const QColor &c) {
        QPixmap pm(24, 24);
        pm.fill(c);
        b->setIcon(QIcon(pm));
        b->setText(c.name());
    };
    setColor(m_bgButton, m_settings->display.bgColor);
    setColor(m_textButton, m_settings->display.textColor);
    connect(m_bgButton, &QPushButton::clicked, this, &SettingsDialog::pickBackgroundColor);
    connect(m_textButton, &QPushButton::clicked, this, &SettingsDialog::pickTextColor);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("字体"), m_fontCombo);
    form->addRow(QStringLiteral("字号"), m_sizeSpin);
    form->addRow(QStringLiteral("行距"), m_lineGapSpin);
    form->addRow(QStringLiteral("段距"), m_paragraphGapSpin);
    form->addRow(QString(), m_compressBlankLines);
    form->addRow(QString(), m_wordWrap);
    form->addRow(QString(), m_chapterPageBreak);
    form->addRow(QStringLiteral("章节字体"), m_titleFontCombo);
    form->addRow(QStringLiteral("章节字号"), m_titleSizeSpin);
    form->addRow(QString(), m_useSameFont);
    auto *bgRow = new QHBoxLayout;
    bgRow->addWidget(m_bgImageButton);
    bgRow->addWidget(clearBg);
    form->addRow(QStringLiteral("背景图片"), bgRow);
    auto *alphaRow = new QHBoxLayout;
    alphaRow->addWidget(m_alphaSlider);
    alphaRow->addWidget(m_alphaLabel);
    form->addRow(QStringLiteral("窗口透明度"), alphaRow);
    form->addRow(QStringLiteral("文字颜色"), m_textButton);
    form->addRow(QString(), m_firstLineIndent);
    auto *pickScreen = new QPushButton(QStringLiteral("屏幕取色"), this);
    connect(pickScreen, &QPushButton::clicked, this, &SettingsDialog::pickScreenColor);
    form->addRow(QStringLiteral("背景色"), m_bgButton);
    form->addRow(QStringLiteral("取色"), pickScreen);

    auto *ok = new QPushButton(QStringLiteral("确定"), this);
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    connect(ok, &QPushButton::clicked, this, &SettingsDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttons);
}

void SettingsDialog::pickBackgroundColor()
{
    const QColor c = QColorDialog::getColor(m_settings->display.bgColor, this, QStringLiteral("选择背景色"));
    if (c.isValid()) {
        m_settings->display.bgColor = c;
        QPixmap pm(24, 24);
        pm.fill(c);
        m_bgButton->setIcon(QIcon(pm));
        m_bgButton->setText(c.name());
    }
}

void SettingsDialog::pickTextColor()
{
    const QColor c = QColorDialog::getColor(m_settings->display.textColor, this, QStringLiteral("选择文字颜色"));
    if (c.isValid()) {
        m_settings->display.textColor = c;
        QPixmap pm(24, 24);
        pm.fill(c);
        m_textButton->setIcon(QIcon(pm));
        m_textButton->setText(c.name());
    }
}

void SettingsDialog::pickScreenColor()
{
    hide();
    QCoreApplication::processEvents();
    QScreen *screen = QGuiApplication::primaryScreen();
    const QPixmap pm = screen ? screen->grabWindow(0) : QPixmap();
    const QColor c = pm.isNull() ? QColor() : pm.toImage().pixelColor(QCursor::pos());
    show();
    if (c.isValid()) {
        m_settings->display.bgColor = c;
        QPixmap sw(24, 24);
        sw.fill(c);
        m_bgButton->setIcon(QIcon(sw));
        m_bgButton->setText(c.name());
    }
}

void SettingsDialog::pickBackgroundImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择背景图片"), QString(),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp)"));
    if (!path.isEmpty()) {
        m_settings->display.bgImagePath = path;
        m_bgImageButton->setText(QFileInfo(path).fileName());
    }
}

void SettingsDialog::clearBackgroundImage()
{
    m_settings->display.bgImagePath.clear();
    m_bgImageButton->setText(QStringLiteral("无"));
}

void SettingsDialog::updateAlphaLabel(int value)
{
    m_alphaLabel->setText(QStringLiteral("%1 / 255").arg(value));
}

void SettingsDialog::accept()
{
    m_settings->display.font.setFamily(m_fontCombo->currentFont().family());
    m_settings->display.font.setPointSize(m_sizeSpin->value());
    m_settings->display.lineGap = m_lineGapSpin->value();
    m_settings->display.paragraphGap = m_paragraphGapSpin->value();
    m_settings->display.compressBlankLines = m_compressBlankLines->isChecked();
    m_settings->display.wordWrap = m_wordWrap->isChecked();
    m_settings->display.chapterPageBreak = m_chapterPageBreak->isChecked();
    m_settings->display.titleFont.setFamily(m_titleFontCombo->currentFont().family());
    m_settings->display.titleFont.setPointSize(m_titleSizeSpin->value());
    m_settings->display.useSameFont = m_useSameFont->isChecked();
    m_settings->display.windowAlpha = m_alphaSlider->value();
    m_settings->display.firstLineIndent = m_firstLineIndent->isChecked();
    m_settings->save();
    QDialog::accept();
}

}
