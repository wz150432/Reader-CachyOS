# Reader CachyOS

> 致敬 [binbyu/Reader](https://github.com/binbyu/Reader) -- 一款简洁好用的 Windows 桌面阅读器。
> 本项目将其核心阅读体验移植到 Linux 原生环境，献给每一位在 CachyOS / Arch 上安静读书的人。

## 这是什么

Reader CachyOS 是一款**完全离线**的桌面阅读器，功能设计参考 [binbyu/Reader](https://github.com/binbyu/Reader) 的本地阅读部分，使用 C++17 + Qt 6 从零重写，原生运行于 CachyOS / Arch Linux（Wayland / niri）。

不做在线书源、不做联网爬虫，只专注于一件事：**打开一本书，安静地读完它。**

## 功能

**阅读格式**
- TXT（自动识别 UTF-8 / UTF-16 / GB2312 / GBK 等编码）
- EPUB
- MOBI / AZW / AZW3

**阅读体验**
- 翻页：鼠标点击、滚轮、方向键、PageUp / PageDown
- 逐行滚动、自动翻页（翻页 / 滚动两种模式）
- 章节目录自动解析、自定义正则
- 搜索（Ctrl+F）、书签（Ctrl+M）、进度跳转（Ctrl+G）
- 编辑模式（Ctrl+E）：直接修改页面文本

**显示与窗口**
- 字体、字号、行距、段距、首行缩进、压缩空行、Word wrap
- 背景颜色 / 背景图片、窗口透明度
- 全屏（F11）、置顶（Alt+T）、隐藏边框（F12）、隐藏窗口（Alt+H）
- 关键字标签高亮
- 所有快捷键可自定义

**系统集成**
- 文件关联：双击 txt / epub / mobi 直接打开
- 最小化到系统托盘
- 最近阅读列表、阅读进度自动记忆

## 构建

```bash
# 依赖
sudo pacman -S qt6-base cmake

# 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 运行
./build/reader
```

## 安装

### 手动安装

```bash
./packaging/install.sh --user      # 安装到 ~/.local
./packaging/install.sh --system    # 安装到 /usr/local（需要 sudo）
```

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
