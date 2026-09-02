#include "core/NiriConfig.h"
namespace reader {

bool patchReaderOpacity(QString *content, double opacity)
{
    if (!content || opacity < 0.0 || opacity > 1.0)
        return false;

    QString opacityText = QString::number(opacity, 'f', 3);
    if (!opacityText.contains(QLatin1Char('.')))
        opacityText += QLatin1String(".0");
    QString newBlock = QStringLiteral("window-rule {\n");
    newBlock += QStringLiteral("    match app-id=r\"^reader(\\.desktop)?$\"\n");
    newBlock += QStringLiteral("    match title=r\".* - Reader$\"\n");
    newBlock += QStringLiteral("    open-floating true\n");
    newBlock += QStringLiteral("    opacity ") + opacityText + QLatin1Char('\n');
    newBlock += QStringLiteral("    shadow { off; }\n");
    newBlock += QStringLiteral("    draw-border-with-background false\n");
    newBlock += QLatin1Char('}');

    const QString target = QStringLiteral("reader(\\.desktop)");
    const QString blockStart = QStringLiteral("window-rule {");
    int pos = content->indexOf(blockStart);
    while (pos >= 0) {
        const int close = content->indexOf(QLatin1Char('}'), pos);
        if (close < 0)
            break;
        const QString block = content->mid(pos, close - pos + 1);
        if (block.contains(target)) {
            content->replace(pos, close - pos + 1, newBlock);
            return true;
        }
        pos = content->indexOf(blockStart, close);
    }

    if (!content->endsWith(QLatin1Char('\n')))
        content->append(QLatin1Char('\n'));
    content->append(newBlock + QLatin1Char('\n'));
    return true;
}

}
