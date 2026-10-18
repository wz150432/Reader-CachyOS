# 文件格式及窗口性能验证（2026-10-16）

在当前用户的 Niri/Wayland 会话中，使用同一个 20,000 行 TXT 样本，
分别启动修改前和修改后的 Release 构建。每组 12 次隐藏／恢复，
通过 Niri EventStream 的窗口移除／新增事件记录响应时间中位数。
这衡量窗口状态响应，不是屏幕动画或完整绘制耗时，不代表所有设备上的结果。

| 指标 | 修改前 | 修改后 |
| --- | ---: | ---: |
| 隐藏响应中位数 | 32.62 ms | 3.60 ms |
| 恢复响应中位数 | 34.81 ms | 12.65 ms |
| 隐藏空闲 CPU（3 秒采样） | 0.0% | 0.0% |
| 隐藏 PSS | 117803 KiB | 117474 KiB |
| 隐藏 RSS | 141228 KiB | 143468 KiB |

内存基本持平，不能据此声称降低了内存占用。新增 libmobi 共享库约
164 KiB。后台优化重点是停止隐藏时的自动翻页和桌面事件订阅，
移除切换路径上的同步外部命令，并限制进度写盘频率。
测试也验证桌面 IPC 不应答时退出不再等待原先的 700 ms 超时。

重测命令：
```sh
python3 tests/benchmark_visibility.py /path/to/reader
```
需在 Niri 会话中运行。脚本使用临时配置、书籍及控制 socket，
只关闭自身创建的 Reader 进程。性能结果会受系统负载、缓存和桌面动画影响。

格式测试包括真实的 PalmDOC MOBI、HUFF/CDIC、Windows-1252、KF8/AZW3，
以及 FB2 嵌套章节、前言、DRM 拒绝和损坏文件。


## 快捷键延迟复查

前面的可见性测量直接向 Reader socket 发送命令，**没有测量全局快捷键启动
控制进程的开销**，因此不能用 33 → 4 ms 推断按快捷键到隐藏的整体耗时。
鼠标直接调用隐藏路径；原来的 Alt+Z 绑定每次启动 `reader --toggle-hide`，
先构造 QApplication，加载显示插件、主题和字体后才发指令。

在相同桌面会话中，用临时 socket 接收控制命令，10 次进程启动测量得到：

| 控制入口 | 进程启动到命令送达中位数 |
| --- | ---: |
| 原 reader（QApplication） | 226.65 ms |
| reader（仅 QCoreApplication） | 40.11 ms |
| reader-control（仅 libc） | 1.43 ms |

真实 Niri spawn 路径的可见性测试使用同一个新版 Reader 接收进程、20,000 行
TXT 和每组 12 次隐藏／恢复。差异只在控制命令入口：

| 包含控制命令启动的测量 | 旧控制入口 | reader-control |
| --- | ---: | ---: |
| 隐藏状态变化 | 255.06 ms | 41.01 ms |
| 恢复状态变化 | 237.36 ms | 47.24 ms |

起点为测试进程发出 `niri msg action spawn`，终点为 Niri EventStream 中的
窗口移除／新增事件。它额外包含测试用的 niri CLI 进程开销，**不包含实体键盘
事件分发，也不测量动画最后一帧**。该轮测量时桌面的 window-open/window-close
各配置了 150 ms 动画；这会影响视觉体感，不能与事件耗时当作同一个指标。

复测命令（旧控制入口需保留同目录的 lib/reader 依赖）：

```sh
python3 tests/benchmark_shortcut.py /path/to/reader-control
python3 tests/benchmark_shortcut.py /path/to/reader-control --via-niri
python3 tests/benchmark_visibility.py /path/to/reader --control-binary /path/to/control-client --via-niri
```

辅助测量发现，Kitty 正常冷启动到窗口登记的中位数约 450–560 ms，
fish 就绪约 700–840 ms；使用独立测试组复用 Kitty 进程时开窗中位数为
73.11 ms。这些数据仅定位应用启动成本，不据此认定 Niri 存在固定延迟。


## 用户桌面配置应用

随后按用户选择即时重载了以下个人配置（这些文件位于仓库之外）：

- Alt+Z 改为启动已安装的 `~/.local/bin/reader-control --toggle-hide`。
- Win+Enter 使用 `kitty --single-instance --app-id kitty`；Ctrl+Alt+T 的
  scratchpad 脚本使用 `kitty --single-instance --app-id scratchpad`。
- window-open 和 window-close 改为 `off`，其余动画设置保留。

`niri validate` 和脚本语法校验通过。独立 Kitty 测试组验证了两个不同 app-id
的窗口共享同一 PID，普通终端显式指定 kitty app-id，避免继承先启动的浮动终端标识。
没有额外启动常驻 Kitty 服务；已有复用进程时开窗加速，关闭所有 Kitty 窗口后
下次仍需要冷启动。

原始三个配置文件备份在
`~/.config/niri/backups/reader-latency-20261016-093346/`。
