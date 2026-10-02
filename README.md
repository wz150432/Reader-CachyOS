# Reader CachyOS (Niri, with Xwayland)

> 致敬 [binbyu/Reader](https://github.com/binbyu/Reader) -- 一款简洁好用的 Windows 桌面阅读器。
> 本项目将其核心阅读体验移植到 Linux 原生环境，献给每一位在 CachyOS / Arch 上安静读书的人。

## 这是什么

Reader CachyOS 是一款**完全离线**的桌面阅读器，功能设计参考 [binbyu/Reader](https://github.com/binbyu/Reader) 的本地阅读部分，使用 C++17 + Qt 6 从零重写，原生运行于 CachyOS / Arch Linux（Wayland / niri）。

不做在线书源、不做联网爬虫，只专注于一件事：**打开一本书，安静地读完它。**

## 功能

**阅读格式**
- TXT（自动识别 UTF-8 / UTF-16 / GB2312 / GBK 等编码）
- EPUB（EPUB 2 / 3 文字阅读，按书内顺序加载章节，支持目录标题；不保留图片和复杂排版）
- MOBI / AZW / AZW3：暂未实现

**阅读体验**
- 翻页：鼠标点击、滚轮、方向键、PageUp / PageDown
- 逐行滚动、自动翻页（翻页 / 滚动两种模式）
- 章节目录自动解析、自定义正则；点击“目录”直接切换全阅读区目录，选中章节或按 Enter 返回正文，Esc 关闭目录
- 搜索（Ctrl+F）、书签（Ctrl+M）、进度跳转（Ctrl+G）
- 编辑模式（Ctrl+E）：仅 TXT 支持，直接修改页面文本

**显示与窗口**
- 字体、字号、行距、段距、首行缩进、压缩空行、Word wrap
- 背景颜色 / 背景图片、窗口透明度
- 全屏（F11）、置顶（Alt+T）、隐藏边框（F12）、隐藏窗口（Alt+H）
- 关键字标签高亮
- 所有快捷键可自定义

**系统集成**
- 文件关联：双击 txt / epub 直接打开
- 最小化到系统托盘
- 阅读记录列表不设数量上限、阅读进度自动记忆；文件菜单直接显示书籍列表，右键书名就地确认删除记录，保留本地书籍文件，删除或取消时列表保持展开

## 构建

```bash
# 依赖
sudo pacman -S qt6-base libarchive cmake

# 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 运行
./build/reader
```

## 安装

### 从 GitHub Release 安装（CachyOS / Arch Linux）

适用于 **x86_64** 的 CachyOS / Arch Linux。打开
[GitHub Releases](https://github.com/wz150432/Reader-CachyOS/releases)，选择所需版本，
下载附件中的 `.pkg.tar.zst` 安装包（不是 `Source code` 源码压缩包）。
如果该版本还没有安装包附件，请使用下方的源码构建和手动安装方法。

在下载目录打开终端，执行下面的命令；将文件名替换为实际下载的安装包名称：

```bash
sudo pacman -U ./reader-cachyos-git-版本号-1-x86_64.pkg.tar.zst
```

pacman 会安装程序及所需的 Qt 6、libarchive 依赖。安装完成后，可在应用菜单搜索
`Reader`，或在终端运行：

```bash
reader
# 直接打开一本书
reader /路径/书籍.epub
```

### 升级

从 Releases 下载新版安装包，再执行 `sudo pacman -U ./新版安装包.pkg.tar.zst`，
即可覆盖升级，无需先卸载。手动下载的 Release 安装包需要自行下载新版更新。

### 卸载（通过 pacman 安装的版本）

```bash
sudo pacman -R reader-cachyos-git
```

卸载会移除程序、应用菜单入口、图标和软件包提供的文件类型定义；保留个人设置、
阅读记录、书签以及本地书籍。当前 `PKGBUILD` 的包名是 `reader-cachyos-git`，
卸载时使用这个包名，而不是启动命令 `reader`。

### 从源码手动安装

先按上方“构建”步骤编译，再在项目根目录执行：

```bash
./packaging/install.sh --user      # 安装到 ~/.local
./packaging/install.sh --system    # 安装到 /usr/local（需要 sudo）
```

此方式安装的文件不由 pacman 管理，不能使用上面的 pacman 卸载命令。

### AUR（Arch / CachyOS）暂时未上线

```bash
paru -S reader-cachyos-git
```

## 项目结构

```
src/core/          核心库（编码识别、章节解析、分页引擎、书签、设置）
src/app/           应用层（主窗口、阅读视图、各设置对话框）
tests/             单元测试（Qt Test）
packaging/         PKGBUILD、desktop 文件、图标、安装脚本
```

## 技术栈

- C++17 + Qt 6（Widgets）
- CMake >= 3.21
- Qt Test 单元测试

## 致谢

本项目的功能与交互设计致敬并参考 [binbyu/Reader](https://github.com/binbyu/Reader)（作者：binbyu）。原项目是一款优秀的 Windows 桌面阅读器，本项目仅作个人学习与使用，将其中的本地阅读体验带到 Linux 原生环境。

感谢 binbyu 的开源分享。
