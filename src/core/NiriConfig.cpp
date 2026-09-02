#include "core/NiriConfig.h"
#include <QStringList>
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

bool patchReaderGlobalHide(QString *content, bool enabled, const QString &command)
{
    if (!content || command.isEmpty())
        return false;
    const QString marker = QStringLiteral("// reader-global-hide");
    const QStringList lines = content->split(QLatin1Char('\n'));
    int bindsStart = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines.at(i).trimmed().startsWith(QStringLiteral("binds {"))) {
            bindsStart = i;
            break;
        }
    }
    if (bindsStart < 0)
        return false;
    int insertAt = -1;
    for (int i = bindsStart + 1; i < lines.size(); ++i) {
        if (lines.at(i).trimmed() == QLatin1String("}")) {
            insertAt = i;
            break;
        }
    }
    if (insertAt < 0)
        return false;
    int existingStart = -1;
    for (int i = bindsStart + 1; i < insertAt; ++i) {
        if (lines.at(i).contains(marker)) {
            existingStart = i;
            break;
        }
    }

    if (!enabled) {
        if (existingStart < 0)
            return false;
        QStringList updated;
        for (int i = 0; i < lines.size(); ++i) {
            if (i == existingStart || i == existingStart + 1)
                continue;
            updated.append(lines.at(i));
        }
        *content = updated.join(QLatin1Char('\n'));
        return true;
    }

    const QString newLine = QStringLiteral("    Ctrl+Shift+H { spawn \"%1\" \"--toggle-hide\"; }")
                                .arg(command);
    if (existingStart >= 0) {
        if (existingStart + 1 >= lines.size())
            return false;
        if (lines.at(existingStart + 1).trimmed() == newLine.trimmed())
            return false;
        QStringList updated = lines;
        updated[existingStart + 1] = newLine;
        *content = updated.join(QLatin1Char('\n'));
        return true;
    }

    QStringList updated = lines;
    updated.insert(insertAt, QStringLiteral("    ") + marker);
    updated.insert(insertAt + 1, newLine);
    *content = updated.join(QLatin1Char('\n'));
    return true;
}

}
