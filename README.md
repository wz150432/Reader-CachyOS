# Reader CachyOS (Niri)
> 致敬 [binbyu/Reader](https://github.com/binbyu/Reader) -- 一款简洁好用的 Windows 桌面阅读器。
> 本项目将其核心阅读体验移植到 Linux 原生环境，献给每一位在 CachyOS / Arch 上安静读书的人。

## 这是什么

Reader CachyOS 是一款**默认离线、可选在线阅读**的桌面阅读器，功能设计参考 [binbyu/Reader](https://github.com/binbyu/Reader) 的本地阅读部分，使用 C++17 + Qt 6 从零重写，原生运行于 CachyOS / Arch Linux（Wayland / niri）。

基础程序保留本地阅读体验；在线搜索和按章阅读通过独立、需明确安装的可选组件提供。未安装或停用组件时，启动和取消安装均不会请求书源网络。

## 功能

**阅读格式**
- TXT（自动识别 UTF-8 / UTF-16 / GB2312 / GBK 等编码）
- MOBI / AZW / AZW3（未加密，支持 PalmDOC、HUFF/CDIC、KF8）
- FB2（读取书名、嵌套章节与正文，以文字阅读为主）
- EPUB（EPUB 2 / 3 文字阅读，按书内顺序加载章节，支持目录标题；不保留图片和复杂排版）

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
- 文件关联：双击 txt / epub / mobi / azw3 / fb2 直接打开
- 最小化到系统托盘
- 阅读记录列表不设数量上限、阅读进度自动记忆；文件菜单直接显示书籍列表，右键书名就地确认删除记录，保留本地书籍文件，删除或取消时列表保持展开

## 构建

```bash
# 依赖
sudo pacman -S qt6-base libarchive cmake

# 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release -DREADER_BUILD_ONLINE_ENGINE=OFF
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


### 性能与依赖

隐藏窗口时暂停自动翻页，恢复窗口后继续；后台不轮询鼠标或窗口位置。
Niri 全局快捷键优先使用独立的 `reader-control` 小程序，只发送本地指令，不初始化 Qt 图形环境，也不增加常驻进程。
Niri 使用本地事件订阅及异步命令，隐藏时断开订阅；阅读进度最多每 300 ms
写盘一次，切书和退出时立即保存。窗口和菜单栏状态仍会恢复。

MOBI/AZW3 使用单独的 libmobi 共享库（LGPL-3.0-or-later），固定版本源码及
构建适配位于 third_party/libmobi，不需要联网构建或安装 Calibre。
构建需要 C/C++ 编译器、Qt 6、libarchive 和 zlib。
发布时必须包含 bin/reader-control、lib/reader/libreader_mobi.so 和对应许可证；请使用安装脚本，
不要单独复制 reader 可执行文件。DRM 书籍会明确提示，图片、复杂版式和扫描书不在
文字导入功能范围内。


## 可选在线小说阅读

“文件 → 在线阅读”在组件未安装或已停用时打开组件管理器；安装并启用后直接打开在线书库。
有真实官方元数据的发布版本可点击“下载并启用”，看到版本、体积和位置后确认安装。
源码构建未传入元数据时显示“未配置组件下载”；本地构建的空 URL 元数据也仅支持本地导入。
点击“导入本地组件…”依次选择 `.json` 和对应 `.tar.gz`，确认后进行大小、SHA-256、解压和协议版本校验。
取消选择或取消确认不会联网。组件停用、卸载入口在组件管理器内；卸载组件保留书源和缓存。
组件包只能来自官方 HTTPS GitHub Release 地址；自定义书源只提供内容，不能提供可执行程序。

内置中文维基文库书源可搜索书名，必要时选择版本，再选目录中的章节打开；也可直接输入作品 URL。
目录按章节估算进度。每次先取当前章，再依次缓存其后**恰好 3 个存在的章节**，不会下载整本书。
切章、重试和取消均有界面操作；在线书籍搜索仅查已缓存章节。已缓存内容在离线、组件停用或缺失时仍可从阅读记录打开，
未缓存章节会显示安装/启用或重试提示。书签和进度保存在本地；刷新目录若会丢失当前章节或书签对应的 URL 会保留旧目录并提示。

数据位于 `${XDG_DATA_HOME:-$HOME/.local/share}/Reader/Reader/online/`：`component/` 放组件，
`sources.json` 放书源，`books/` 放目录清单与章节缓存。Cookie 和请求头只保存在私有权限的书源文件中，
不写日志或命令参数。请仅对有权访问的内容配置凭据，并保留来源链接与相关作品授权信息。

### 自定义 HTML / XPath 书源

在线书库 → 书源管理：内置书源不可覆盖，可“复制”为自定义书源，或“新建”空白书源。
每个书源都可启用/停用，表单支持以下字段：

| 范围 | 字段与含义 |
| --- | --- |
| 基本 | `id`、`name`、`kind=html`、`baseUrl`、`enabled` |
| 搜索请求 | `searchUrl`、`searchMethod=GET/POST`、`searchBody`；搜索模板支持 `{keyword}` 与 `{page}` |
| 搜索结果 | `resultXPath` 定位每个结果；`titleXPath`、`linkXPath`、`authorXPath` 相对每个结果取值 |
| 目录 | `tocUrlXPath` 从作品页定位目录；`chapterXPath` 定位章节链接；`tocNextXPath` 定位下一目录页；`reverseToc` 反转目录 |
| 正文 | `contentXPath`、`removeXPath`、`contentNextXPath`；移除脚本、样式、导航及指定节点，保留段落和换行 |
| 请求 | `encoding=auto/UTF-8/GB18030`、`cookie`、JSON 对象 `headers`、`intervalMs`（默认 1000，最低 500） |

例如结果容器 `//div[@class='result']`、相对标题 `.//a`、链接 `.//a/@href`、
章节 `//div[@id='list']//a`、正文 `//div[@id='content']`。相对链接基于最终响应 URL 解析。
没有 `searchUrl` 的书源使用直接 URL 导入。先保存检查字段与 XPath，再用“测试”执行搜索/目录/所选单章预览，
确认标题、链接、正文和分页规则正确；预览不预取后续章。反爬验证或失效规则会显示错误，不绕过登录或验证码。

每请求默认超时 20 秒、正文响应最多 8 MiB；正文分页最多 20 页，目录最多 100 页及 20,000 个去重章节。
跨站重定向不携带自定义 Cookie/认证头。网页结构变化可能需要调整规则。

### 独立组件构建、导入与验证

基础构建只需要现有 Qt 6、libarchive、zlib 和编译工具，不增加 libxml2 开发依赖。
现有 libarchive 可能间接依赖 libxml2 运行库；可选解析器单独携带私有 libxml2 及许可证。
在 Arch/CachyOS 上构建可选组件另需 `libxml2` 开发文件（`sudo pacman -S libxml2`）。

```bash
# 基础程序：默认 OFF，明确写出便于核对
cmake -S . -B build-online-base -DCMAKE_BUILD_TYPE=Release -DREADER_BUILD_ONLINE_ENGINE=OFF
cmake --build build-online-base -j3

# 独立组件：版本必须与元数据和真实 hello 一致
cmake -S . -B build-online-engine -DCMAKE_BUILD_TYPE=Release \
  -DREADER_BUILD_ONLINE_ENGINE=ON -DREADER_ONLINE_VERSION=0.1.0
cmake --build build-online-engine --target reader-online tst_onlineengine -j3
python3 packaging/build-online-component.py \
  --engine build-online-engine/reader-online --base build-online-base/reader \
  --version 0.1.0 --app-version 0.1.0 --output-dir release
# 生成 release/reader-online-0.1.0-linux-x86_64.tar.gz 和同名 .json
# 然后在“文件 → 在线阅读 → 导入本地组件…”选择上述 JSON 和压缩包

# 将实际元数据嵌入基础程序（本地空 URL 仍禁用远程下载）
cmake -S . -B build-online-base -DREADER_BUILD_ONLINE_ENGINE=OFF \
  -DREADER_ONLINE_METADATA="$PWD/release/reader-online-0.1.0-linux-x86_64.json"
cmake --build build-online-base -j3

python3 -m unittest discover -s tests -p test_online_packaging.py
QT_QPA_PLATFORM=offscreen ctest --test-dir build-online-base --output-on-failure
READER_ONLINE_ARCHIVE="$PWD/release/reader-online-0.1.0-linux-x86_64.tar.gz" \
READER_ONLINE_MANIFEST="$PWD/release/reader-online-0.1.0-linux-x86_64.json" \
  build-online-base/tst_onlineartifact
# 显式联网烟测：两个作品、真实安装组件、当前+后3章、停用组件后的缓存重开
READER_ONLINE_LIVE=1 \
READER_ONLINE_ARCHIVE="$PWD/release/reader-online-0.1.0-linux-x86_64.tar.gz" \
READER_ONLINE_MANIFEST="$PWD/release/reader-online-0.1.0-linux-x86_64.json" \
  build-online-base/tst_onlineartifact
```

`packaging/build-release.sh [包版本]` 在本地先构建并验证独立组件，再构建与测试基础 `.pkg.tar.zst`，
输出组件、元数据、源码和 `SHA256SUMS`；基础包内没有 `reader-online`。源码快照排除构建目录、release、graft 和 `.superpowers` 工作记录。
没有指定真实发布 URL 时，这是可本地导入的发布产物。公开的一键下载还需维护者发布真实附件，
然后以该附件的官方 HTTPS URL 设置 `READER_ONLINE_URL` 重新构建基础包；生成器的 `--url` 也遵循相同验证。
脚本不会上传。元数据中的 `appVersion` 必须与程序版本一致，当前为 `0.1.0`；组件只支持 Linux x86_64，
依赖系统 Qt 6/基础程序运行库，不是跨发行版静态二进制。
