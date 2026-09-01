#pragma once
#include <QDialog>
#include "core/Settings.h"

class QFontComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QSlider;
class QLabel;

namespace reader {

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(Settings *settings, QWidget *parent = nullptr);

private slots:
    void pickBackgroundColor();
    void pickTextColor();
    void pickScreenColor();
    void pickBackgroundImage();
    void clearBackgroundImage();
    void updateAlphaLabel(int value);
    void accept() override;

private:
    Settings *m_settings;
    QFontComboBox *m_fontCombo;
    QSpinBox *m_sizeSpin;
    QSpinBox *m_lineGapSpin;
    QSpinBox *m_paragraphGapSpin;
    QCheckBox *m_compressBlankLines;
    QCheckBox *m_wordWrap;
    QCheckBox *m_chapterPageBreak;
    QFontComboBox *m_titleFontCombo;
    QSpinBox *m_titleSizeSpin;
    QCheckBox *m_useSameFont;
    QCheckBox *m_firstLineIndent;
    QPushButton *m_bgButton;
    QPushButton *m_textButton;
    QPushButton *m_bgImageButton;
    QSlider *m_alphaSlider;
    QLabel *m_alphaLabel;
};

}
