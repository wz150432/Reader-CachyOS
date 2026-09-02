#pragma once

#include <QString>

namespace reader {

// 在 niri rules.kdl 文本中重建 Reader 的 window-rule 块，
// 写入 opacity 值（0.0 ~ 1.0）。幂等：块存在则更新，不存在则追加。
bool patchReaderOpacity(QString *content, double opacity);

// 在 niri binds.kdl 中启用/停用 Reader 的全局隐藏快捷键。
bool patchReaderGlobalHide(QString *content, bool enabled, const QString &command);

}
