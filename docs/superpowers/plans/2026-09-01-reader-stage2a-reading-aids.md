# Reader 第二阶段 2a：阅读辅助与显示设置补全 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为第一阶段 TXT 阅读器补全阅读辅助（搜索、书签、全书进度百分比跳转）与显示/按键设置（背景图、透明度、取色器、段距、压缩空行、Word wrap、章前分页、章节字体、快捷键自定义），并把默认快捷键接入原版习惯（Ctrl+F / Ctrl+M / Ctrl+G / Ctrl+E / F11 / F12 / Alt+T / Alt+H 等）。

**Architecture:** 延续第一阶段分层：核心层（Keyset 快捷键、Cache 书签、Page 行滚动与字符范围、Settings 新字段）全部带单元测试；应用层（ReadingView 接线、SettingsDialog/KeysetDialog/BookmarkDialog、MainWindow 集成）以核心层为数据源，UI 行为用 QtTest 覆盖关键路径。

**Tech Stack:** C++17、Qt 6（Core/Gui/Widgets/Test）、CMake ≥ 3.21、Ninja。

**Spec:** [2026-09-01-reader-cachyos-design.md](../specs/2026-09-01-reader-cachyos-design.md)（第 2.1 节搜索/书签/进度跳转/显示设置/快捷键；第 8 节第二阶段）

## Global Constraints

- C++17，Qt 6 Widgets（本机 6.11.2），CMake ≥ 3.21，Ninja
- 完全离线；界面文案简体中文
- 设置仍存 `~/.config/Reader/config.json`（新增顶层 `keys` 对象与 display 内 `bg_image`、`window_alpha` 字段）；缺失字段回退默认，保证与第一阶段配置向后兼容
- 默认快捷键对齐原版：搜索 Ctrl+F、进度跳转 Ctrl+G、添加书签 Ctrl+M、编辑模式 Ctrl+E、自动翻页 空格、字号放大 Ctrl+=、字号缩小 Ctrl+-、全屏 F11、隐藏边框 F12、置顶 Alt+T、隐藏窗口 Alt+H、打开 Ctrl+O、退出 Ctrl+Q、翻页 ←/→、逐行 ↑/↓、章节 Ctrl+←/→
- 所有测试以 `QT_QPA_PLATFORM=offscreen` 运行；提交信息 Conventional Commits

## File Structure

```
src/core/Keyset.h/.cpp              # 新增：快捷键映射与 JSON 持久化
src/core/Cache.h/.cpp               # 修改：书签存储
src/core/Page.h/.cpp                # 修改：逐行滚动 + 每行/每页字符范围
src/core/Settings.h/.cpp            # 修改：bgImagePath/windowAlpha/keyset 字段
src/core/Book.h/.cpp                # 修改：totalCharCount()
src/core/TextBook.h/.cpp            # 修改：实现 totalCharCount()
src/app/ReadingView.h/.cpp          # 修改：快捷键接线/滚轮逐行/搜索高亮/全书进度跳转/背景图
src/app/SettingsDialog.h/.cpp       # 修改：新增设置项与取色器
src/app/KeysetDialog.h/.cpp         # 新增：按键设置对话框
src/app/BookmarkDialog.h/.cpp       # 新增：书签列表对话框
src/app/MainWindow.h/.cpp           # 修改：菜单启用/搜索条/跳转对话框/还原默认
tests/tst_keyset.cpp                # 新增
tests/tst_bookmarks.cpp             # 新增
tests/tst_page2.cpp                 # 新增
tests/tst_settings2.cpp             # 新增
tests/tst_readingview2.cpp          # 新增
tests/tst_mainwindow2.cpp           # 新增
```

## 模块接口约定（后续任务引用）

```cpp
// core/Keyset.h
namespace reader {
enum class KeyAction {
    PageUp, PageDown, LineUp, LineDown, ChapterUp, ChapterDown,
    Search, Jump, AddBookmark, EditMode, AutoPage,
    FontZoomIn, FontZoomOut, Fullscreen, HideBorder, AlwaysOnTop,
    HideWindow, OpenFile, Quit
};
class Keyset {
public:
    Keyset() { reset(); }
    QKeySequence shortcut(KeyAction action) const { return m_keys.value(action); }
    void setShortcut(KeyAction action, const QKeySequence &seq) { m_keys.insert(action, seq); }
    static QKeySequence defaultShortcut(KeyAction action);
    void reset();
    void load(const QJsonObject &o);
    QJsonObject save() const;
    QList<KeyAction> actions() const { return m_keys.keys(); }
private:
    QMap<KeyAction, QKeySequence> m_keys;
    static const QMap<KeyAction, QKeySequence> &defaults();
};
}

// core/Cache.h（新增）
struct Bookmark { QString filePath; int chapterIndex = 0; int pageIndex = 0; QString title; qint64 created = 0; };
// class Cache 新增：
//   QVector<Bookmark> bookmarks(const QString &filePath) const;
//   void addBookmark(const Bookmark &b);
//   void removeBookmark(const QString &filePath, qint64 created);

// core/Page.h（新增成员）
//   int lineOffset() const;
//   void resetLineOffset();
//   int linesOnCurrentPage() const;
//   bool nextLine(); bool prevLine(); bool scrollLines(int delta);
//   QPair<int,int> charRange(int page) const;   // [start,end) 章节内字符区间
//   int pageForChar(int charPos) const;
// struct PageContent 新增：QVector<QPair<int,int>> lineCharRange;

// core/Settings.h
// struct DisplaySettings 新增：QString bgImagePath; int windowAlpha = 255;
// class Settings 新增成员：Keyset keyset;

// core/Book.h
// virtual qint64 totalCharCount() const = 0;

// app/ReadingView.h 新增
//   void setKeyset(const Keyset &keyset);
//   bool findNext(const QString &keyword, bool forward = true);
//   void jumpToBookProgress(qreal progress);   // 0..1
//   void clearMatch();
// signals 新增：searchRequested() / jumpRequested() / bookmarkRequested() /
//   autoPageRequested() / displaySettingsChanged(const reader::DisplaySettings &)

// app/MainWindow.h 新增（测试用）
//   void addBookmarkForCurrentBook();
//   void resetSettings();
```

---

### Task 1: Keyset 快捷键核心

**Files:**
- Create: `src/core/Keyset.h`
- Create: `src/core/Keyset.cpp`
- Test: `tests/tst_keyset.cpp`
- Modify: `CMakeLists.txt`（reader_core 加入 Keyset；注册 tst_keyset）

**Interfaces:**
- Consumes: 无
- Produces: `reader::KeyAction`、`reader::Keyset`，签名见"模块接口约定"；测试命令 `tst_keyset`

- [ ] **Step 1: 写失败测试**

`tests/tst_keyset.cpp`：

```cpp
#include <QtTest>
#include "core/Keyset.h"

using namespace reader;

class TestKeyset : public QObject
{
    Q_OBJECT
private slots:
    void defaults();
    void saveLoadRoundtrip();
    void resetRestores();
};

void TestKeyset::defaults()
{
    Keyset k;
    QCOMPARE(k.shortcut(KeyAction::Search), QKeySequence(QStringLiteral("Ctrl+F")));
    QCOMPARE(k.shortcut(KeyAction::Jump), QKeySequence(QStringLiteral("Ctrl+G")));
    QCOMPARE(k.shortcut(KeyAction::AddBookmark), QKeySequence(QStringLiteral("Ctrl+M")));
    QCOMPARE(k.shortcut(KeyAction::PageDown), QKeySequence(Qt::Key_Right));
    QCOMPARE(k.shortcut(KeyAction::LineDown), QKeySequence(Qt::Key_Down));
    QCOMPARE(k.shortcut(KeyAction::ChapterDown), QKeySequence(Qt::CTRL | Qt::Key_Right));
    QCOMPARE(k.shortcut(KeyAction::Fullscreen), QKeySequence(Qt::Key_F11));
    QCOMPARE(k.shortcut(KeyAction::HideBorder), QKeySequence(Qt::Key_F12));
    QCOMPARE(k.shortcut(KeyAction::AlwaysOnTop), QKeySequence(QStringLiteral("Alt+T")));
    QCOMPARE(k.shortcut(KeyAction::HideWindow), QKeySequence(QStringLiteral("Alt+H")));
}

void TestKeyset::saveLoadRoundtrip()
{
    Keyset k;
    k.setShortcut(KeyAction::Search, QKeySequence(QStringLiteral("Alt+F")));
    k.setShortcut(KeyAction::PageDown, QKeySequence(QStringLiteral("PgDown")));
    const QJsonObject saved = k.save();
    Keyset t;
    t.load(saved);
    QCOMPARE(t.shortcut(KeyAction::Search), QKeySequence(QStringLiteral("Alt+F")));
    QCOMPARE(t.shortcut(KeyAction::PageDown), QKeySequence(QStringLiteral("PgDown")));
    QCOMPARE(t.shortcut(KeyAction::Fullscreen), QKeySequence(Qt::Key_F11));
}

void TestKeyset::resetRestores()
{
    Keyset k;
    k.setShortcut(KeyAction::Search, QKeySequence(QStringLiteral("Alt+F")));
    k.reset();
    QCOMPARE(k.shortcut(KeyAction::Search), QKeySequence(QStringLiteral("Ctrl+F")));
}

QTEST_APPLESS_MAIN(TestKeyset)
#include "tst_keyset.moc"
```

`CMakeLists.txt`：

```cmake
add_library(reader_core STATIC
    src/core/TextCodec.cpp src/core/TextCodec.h
    src/core/ChapterParser.cpp src/core/ChapterParser.h
    src/core/Book.cpp src/core/Book.h
    src/core/TextBook.cpp src/core/TextBook.h
    src/core/Page.cpp src/core/Page.h
    src/core/Settings.cpp src/core/Settings.h
    src/core/Cache.cpp src/core/Cache.h
    src/core/Keyset.cpp src/core/Keyset.h)

qt_add_executable(tst_keyset tests/tst_keyset.cpp)
target_link_libraries(tst_keyset PRIVATE reader_core Qt6::Test)
add_test(NAME keyset COMMAND tst_keyset)
set_tests_properties(keyset PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`Keyset` 未定义）

- [ ] **Step 3: 实现 Keyset**

`src/core/Keyset.h`：

```cpp
#pragma once
#include <QJsonObject>
#include <QKeySequence>
#include <QMap>

namespace reader {

enum class KeyAction {
    PageUp, PageDown, LineUp, LineDown, ChapterUp, ChapterDown,
    Search, Jump, AddBookmark, EditMode, AutoPage,
    FontZoomIn, FontZoomOut, Fullscreen, HideBorder, AlwaysOnTop,
    HideWindow, OpenFile, Quit
};

class Keyset
{
public:
    Keyset() { reset(); }
    QKeySequence shortcut(KeyAction action) const { return m_keys.value(action); }
    void setShortcut(KeyAction action, const QKeySequence &seq) { m_keys.insert(action, seq); }
    static QKeySequence defaultShortcut(KeyAction action);
    void reset();
    void load(const QJsonObject &o);
    QJsonObject save() const;
    QList<KeyAction> actions() const { return m_keys.keys(); }

private:
    QMap<KeyAction, QKeySequence> m_keys;
    static const QMap<KeyAction, QKeySequence> &defaults();
};

}
```

`src/core/Keyset.cpp`：

```cpp
#include "core/Keyset.h"

namespace reader {

static QMap<KeyAction, QKeySequence> makeDefaults()
{
    QMap<KeyAction, QKeySequence> d;
    d.insert(KeyAction::PageUp, QKeySequence(Qt::Key_Left));
    d.insert(KeyAction::PageDown, QKeySequence(Qt::Key_Right));
    d.insert(KeyAction::LineUp, QKeySequence(Qt::Key_Up));
    d.insert(KeyAction::LineDown, QKeySequence(Qt::Key_Down));
    d.insert(KeyAction::ChapterUp, QKeySequence(Qt::CTRL | Qt::Key_Left));
    d.insert(KeyAction::ChapterDown, QKeySequence(Qt::CTRL | Qt::Key_Right));
    d.insert(KeyAction::Search, QKeySequence(QStringLiteral("Ctrl+F")));
    d.insert(KeyAction::Jump, QKeySequence(QStringLiteral("Ctrl+G")));
    d.insert(KeyAction::AddBookmark, QKeySequence(QStringLiteral("Ctrl+M")));
    d.insert(KeyAction::EditMode, QKeySequence(QStringLiteral("Ctrl+E")));
    d.insert(KeyAction::AutoPage, QKeySequence(Qt::Key_Space));
    d.insert(KeyAction::FontZoomIn, QKeySequence(QStringLiteral("Ctrl+=")));
    d.insert(KeyAction::FontZoomOut, QKeySequence(QStringLiteral("Ctrl+-")));
    d.insert(KeyAction::Fullscreen, QKeySequence(Qt::Key_F11));
    d.insert(KeyAction::HideBorder, QKeySequence(Qt::Key_F12));
    d.insert(KeyAction::AlwaysOnTop, QKeySequence(QStringLiteral("Alt+T")));
    d.insert(KeyAction::HideWindow, QKeySequence(QStringLiteral("Alt+H")));
    d.insert(KeyAction::OpenFile, QKeySequence(QStringLiteral("Ctrl+O")));
    d.insert(KeyAction::Quit, QKeySequence(QStringLiteral("Ctrl+Q")));
    return d;
}

const QMap<KeyAction, QKeySequence> &Keyset::defaults()
{
    static const QMap<KeyAction, QKeySequence> d = makeDefaults();
    return d;
}

QKeySequence Keyset::defaultShortcut(KeyAction action)
{
    return defaults().value(action);
}

void Keyset::reset()
{
    m_keys = defaults();
}

void Keyset::load(const QJsonObject &o)
{
    reset();
    const QStringList names = o.keys();
    for (const QString &name : names) {
        bool ok = false;
        const int v = name.toInt(&ok);
        if (!ok)
            continue;
        const KeyAction a = static_cast<KeyAction>(v);
        if (!m_keys.contains(a))
            continue;
        const QString seq = o.value(name).toString();
        if (!seq.isEmpty())
            m_keys.insert(a, QKeySequence(seq));
    }
}

QJsonObject Keyset::save() const
{
    QJsonObject o;
    for (auto it = m_keys.cbegin(); it != m_keys.cend(); ++it)
        o.insert(QString::number(int(it.key())), it.value().toString());
    return o;
}

}
```

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/core/Keyset.h src/core/Keyset.cpp tests/tst_keyset.cpp
git commit -m "feat: 快捷键映射核心（默认对齐原版）"
```

---

### Task 2: Cache 书签存储

**Files:**
- Modify: `src/core/Cache.h`
- Modify: `src/core/Cache.cpp`
- Test: `tests/tst_bookmarks.cpp`
- Modify: `CMakeLists.txt`（注册 tst_bookmarks）

**Interfaces:**
- Consumes: 无
- Produces: `reader::Bookmark` 与 Cache 书签接口，签名见"模块接口约定"；测试命令 `tst_bookmarks`

- [ ] **Step 1: 写失败测试**

`tests/tst_bookmarks.cpp`：

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include "core/Cache.h"

using namespace reader;

class TestBookmarks : public QObject
{
    Q_OBJECT
private slots:
    void addAndRoundtrip();
    void removeBookmark();
    void emptyForUnknownFile();
};

void TestBookmarks::addAndRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cache.json"));
    {
        Cache c(path);
        c.load();
        c.addBookmark({QStringLiteral("/b/a.txt"), 2, 5, QStringLiteral("第二章 标题"), 1000});
        c.addBookmark({QStringLiteral("/b/a.txt"), 1, 0, QStringLiteral("第一章 标题"), 900});
        c.addBookmark({QStringLiteral("/b/other.txt"), 0, 0, QStringLiteral("别书"), 800});
        c.save();
    }
    {
        Cache c(path);
        c.load();
        const QVector<Bookmark> marks = c.bookmarks(QStringLiteral("/b/a.txt"));
        QCOMPARE(marks.size(), 2);
        QCOMPARE(marks.at(0).title, QStringLiteral("第一章 标题"));
        QCOMPARE(marks.at(1).pageIndex, 5);
        QCOMPARE(c.bookmarks(QStringLiteral("/b/other.txt")).size(), 1);
    }
}

void TestBookmarks::removeBookmark()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cache.json"));
    Cache c(path);
    c.load();
    c.addBookmark({QStringLiteral("/b/a.txt"), 0, 1, QStringLiteral("标记"), 100});
    c.removeBookmark(QStringLiteral("/b/a.txt"), 100);
    QVERIFY(c.bookmarks(QStringLiteral("/b/a.txt")).isEmpty());
}

void TestBookmarks::emptyForUnknownFile()
{
    QTemporaryDir dir;
    Cache c(dir.filePath(QStringLiteral("cache.json")));
    c.load();
    QVERIFY(c.bookmarks(QStringLiteral("/nope.txt")).isEmpty());
}

QTEST_APPLESS_MAIN(TestBookmarks)
#include "tst_bookmarks.moc"
```

`CMakeLists.txt`：

```cmake
qt_add_executable(tst_bookmarks tests/tst_bookmarks.cpp)
target_link_libraries(tst_bookmarks PRIVATE reader_core Qt6::Test)
add_test(NAME bookmarks COMMAND tst_bookmarks)
set_tests_properties(bookmarks PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`Bookmark` 未定义）

- [ ] **Step 3: 实现书签**

`src/core/Cache.h` 在 `BookProgress` 之后加入：

```cpp
struct Bookmark
{
    QString filePath;
    int chapterIndex = 0;
    int pageIndex = 0;
    QString title;
    qint64 created = 0;
};
```

`class Cache` 的 public 区加入：

```cpp
    QVector<Bookmark> bookmarks(const QString &filePath) const;
    void addBookmark(const Bookmark &b);
    void removeBookmark(const QString &filePath, qint64 created);
```

private 区加入：`QVector<Bookmark> m_bookmarks;`

`src/core/Cache.cpp`：

- `load()` 中解析 `bookmarks` 数组（字段：file/chapter/page/title/created），追加到 `m_bookmarks`：

```cpp
    const QJsonArray marks = doc.object().value(QStringLiteral("bookmarks")).toArray();
    for (const QJsonValue &v : marks) {
        const QJsonObject o = v.toObject();
        Bookmark b;
        b.filePath = o.value(QStringLiteral("file")).toString();
        b.chapterIndex = o.value(QStringLiteral("chapter")).toInt();
        b.pageIndex = o.value(QStringLiteral("page")).toInt();
        b.title = o.value(QStringLiteral("title")).toString();
        b.created = static_cast<qint64>(o.value(QStringLiteral("created")).toDouble());
        if (!b.filePath.isEmpty())
            m_bookmarks.append(b);
    }
```

- `save()` 中写入 `bookmarks` 数组：

```cpp
    QJsonArray marks;
    for (const Bookmark &b : m_bookmarks) {
        QJsonObject o;
        o.insert(QStringLiteral("file"), b.filePath);
        o.insert(QStringLiteral("chapter"), b.chapterIndex);
        o.insert(QStringLiteral("page"), b.pageIndex);
        o.insert(QStringLiteral("title"), b.title);
        o.insert(QStringLiteral("created"), static_cast<double>(b.created));
        marks.append(o);
    }
    root.insert(QStringLiteral("bookmarks"), marks);
```

- 三个新方法：

```cpp
QVector<Bookmark> Cache::bookmarks(const QString &filePath) const
{
    QVector<Bookmark> out;
    for (const Bookmark &b : m_bookmarks) {
        if (b.filePath == filePath)
            out.append(b);
    }
    std::stable_sort(out.begin(), out.end(), [](const Bookmark &a, const Bookmark &b) {
        if (a.chapterIndex != b.chapterIndex)
            return a.chapterIndex < b.chapterIndex;
        return a.pageIndex < b.pageIndex;
    });
    return out;
}

void Cache::addBookmark(const Bookmark &b)
{
    m_bookmarks.append(b);
}

void Cache::removeBookmark(const QString &filePath, qint64 created)
{
    for (int i = m_bookmarks.size() - 1; i >= 0; --i) {
        if (m_bookmarks.at(i).filePath == filePath && m_bookmarks.at(i).created == created)
            m_bookmarks.removeAt(i);
    }
}
```

- `Cache::clearAll()` 增加 `m_bookmarks.clear();`

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/core/Cache.h src/core/Cache.cpp tests/tst_bookmarks.cpp
git commit -m "feat: 书签存储（按书管理，JSON 持久化）"
```

---

### Task 3: Page 逐行滚动与字符范围

**Files:**
- Modify: `src/core/Page.h`
- Modify: `src/core/Page.cpp`
- Test: `tests/tst_page2.cpp`
- Modify: `CMakeLists.txt`（注册 tst_page2）

**Interfaces:**
- Consumes: `PageContent`（第一阶段）
- Produces: `Page::lineOffset/nextLine/prevLine/scrollLines/linesOnCurrentPage/charRange/pageForChar`、`PageContent::lineCharRange`，签名见"模块接口约定"；测试命令 `tst_page2`

- [ ] **Step 1: 写失败测试**

`tests/tst_page2.cpp`：

```cpp
#include <QtTest>
#include "core/Page.h"

using namespace reader;

class TestPage2 : public QObject
{
    Q_OBJECT
private slots:
    void lineScrolling();
    void scrollLinesAcrossPages();
    void charRangesPartitionText();
    void pageForCharFindsPage();
};

static QString longText()
{
    QString text;
    for (int i = 0; i < 200; ++i)
        text += QStringLiteral("第%1章 测试\n正文内容测试\n").arg(i + 1);
    return text;
}

void TestPage2::lineScrolling()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 12);
    page.setParams(p);
    page.setViewSize(300, 120);
    page.setText(longText());
    QVERIFY(page.pageCount() > 1);
    QCOMPARE(page.lineOffset(), 0);
    QVERIFY(page.nextLine());
    QCOMPARE(page.lineOffset(), 1);
    QVERIFY(page.prevLine());
    QCOMPARE(page.lineOffset(), 0);
}

void TestPage2::scrollLinesAcrossPages()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 12);
    page.setParams(p);
    page.setViewSize(300, 80);
    page.setText(longText());
    QVERIFY(page.pageCount() > 2);
    const int firstPage = page.currentPage();
    const int firstLines = page.linesOnCurrentPage();
    QVERIFY(page.scrollLines(firstLines));
    QVERIFY(page.currentPage() >= firstPage + 1);
    QVERIFY(page.scrollLines(-firstLines));
    QCOMPARE(page.currentPage(), firstPage);
}

void TestPage2::charRangesPartitionText()
{
    Page page;
    PageLayoutParams p;
    page.setParams(p);
    page.setViewSize(500, 200);
    const QString text = longText();
    page.setText(text);
    const int count = page.pageCount();
    QVERIFY(count > 1);
    for (int i = 0; i + 1 < count; ++i) {
        const QPair<int, int> a = page.charRange(i);
        const QPair<int, int> b = page.charRange(i + 1);
        QVERIFY(a.second <= b.first);
    }
    const QPair<int, int> last = page.charRange(count - 1);
    QVERIFY(last.second > last.first);
}

void TestPage2::pageForCharFindsPage()
{
    Page page;
    PageLayoutParams p;
    page.setParams(p);
    page.setViewSize(500, 200);
    const QString text = longText();
    page.setText(text);
    const int count = page.pageCount();
    QVERIFY(page.pageForChar(0) == 0);
    QVERIFY(page.pageForChar(text.size() - 1) == count - 1);
    const int mid = page.pageForChar(text.size() / 2);
    const QPair<int, int> range = page.charRange(mid);
    QVERIFY(text.size() / 2 >= range.first);
    QVERIFY(text.size() / 2 < range.second);
}

QTEST_MAIN(TestPage2)
#include "tst_page2.moc"
```

`CMakeLists.txt`：

```cmake
qt_add_executable(tst_page2 tests/tst_page2.cpp)
target_link_libraries(tst_page2 PRIVATE reader_core Qt6::Test)
add_test(NAME page2 COMMAND tst_page2)
set_tests_properties(page2 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`nextLine` 等未定义）

- [ ] **Step 3: 实现**

`src/core/Page.h`：

- `PageContent` 增加一行：`QVector<QPair<int, int>> lineCharRange;`
- `class Page` public 增加：

```cpp
    int lineOffset() const { return m_lineOffset; }
    void resetLineOffset() { m_lineOffset = 0; }
    int linesOnCurrentPage() const { return m_pages.isEmpty() ? 0 : m_pages.at(m_current).paragraphIndex.size(); }
    bool nextLine();
    bool prevLine();
    bool scrollLines(int delta);
    QPair<int, int> charRange(int page) const { return m_pageCharRange.value(page); }
    int pageForChar(int charPos) const;
```

- private 增加：

```cpp
    int m_lineOffset = 0;
    QVector<QPair<int, int>> m_pageCharRange;
    std::vector<std::pair<int, int>> m_paragraphInfo; // (章节内源字符起点, 首行缩进字符数)
```

`src/core/Page.cpp`：

- `repaginate()` 开头（`m_paragraphs.clear();` 旁）增加：

```cpp
    m_paragraphInfo.clear();
    m_pageCharRange.clear();
    m_lineOffset = 0;
```

- 段落循环：把 `const QStringList paragraphs = m_text.split(...)` 之后的循环改为同时记录源起点与缩进（`running` 记录章节内字符偏移，`part.size() + 1` 含换行）：

```cpp
    int running = 0;
    for (int p = 0; p < paragraphs.size(); ++p) {
        const QString part = paragraphs.at(p);
        const int partStart = running;
        running += part.size() + 1;
        if (m_params.compressBlankLines && part.trimmed().isEmpty())
            continue;
        QString paraText = part;
        const int indent = (m_params.firstLineIndent && !part.isEmpty()) ? 2 : 0;
        if (indent)
            paraText = QStringLiteral("\u3000\u3000") + paraText;
        const QFont &f = (p == 0) ? titleFont : bodyFont;
        auto layout = std::make_unique<QTextLayout>(paraText, f);
        QTextOption opt;
        opt.setWrapMode(wrap);
        layout->setTextOption(opt);
        layout->beginLayout();
        qreal y = 0;
        while (true) {
            QTextLine line = layout->createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(usableWidth);
            line.setPosition(QPointF(0, y));
            y += line.height() + m_params.lineGap;
        }
        layout->endLayout();
        m_paragraphs.push_back(std::move(layout));
        m_paragraphInfo.push_back({partStart, indent});
    }
```

> 说明：替换第一阶段中"先 split 再循环"的段落构建块；`bodyFont/titleFont/wrap` 变量保持不变。

- 分页内层循环中，在 `page.lineIndex.append(ref.line);` 之后追加字符范围计算：

```cpp
            const QTextLine tl = layout.lineAt(ref.line);
            const int indent = m_paragraphInfo.at(ref.para).second;
            const int srcStart = qMax(0, tl.textStart() - indent);
            const int srcEnd = qMax(0, tl.textStart() + tl.textLength() - indent);
            page.lineCharRange.append({m_paragraphInfo.at(ref.para).first + srcStart,
                                       m_paragraphInfo.at(ref.para).first + srcEnd});
```

- 每页结束 `m_pages.append(page);` 后追加：

```cpp
        m_pageCharRange.append({page.lineCharRange.constFirst().first,
                                page.lineCharRange.constLast().second});
```

- `goToPage` 成功分支增加 `m_lineOffset = 0;`
- `jumpToProgress` 内设置 `m_current` 后增加 `m_lineOffset = 0;`
- 新增方法：

```cpp
bool Page::nextLine()
{
    if (m_pages.isEmpty())
        return false;
    if (m_lineOffset + 1 < linesOnCurrentPage()) {
        ++m_lineOffset;
        return true;
    }
    if (m_current + 1 < m_pages.size()) {
        ++m_current;
        m_lineOffset = 0;
        return true;
    }
    return false;
}

bool Page::prevLine()
{
    if (m_pages.isEmpty())
        return false;
    if (m_lineOffset > 0) {
        --m_lineOffset;
        return true;
    }
    if (m_current > 0) {
        --m_current;
        m_lineOffset = linesOnCurrentPage() - 1;
        return true;
    }
    return false;
}

bool Page::scrollLines(int delta)
{
    bool any = false;
    while (delta > 0) {
        if (!nextLine())
            break;
        --delta;
        any = true;
    }
    while (delta < 0) {
        if (!prevLine())
            break;
        ++delta;
        any = true;
    }
    return any;
}

int Page::pageForChar(int charPos) const
{
    for (int i = 0; i < m_pageCharRange.size(); ++i) {
        const QPair<int, int> r = m_pageCharRange.at(i);
        if (charPos >= r.first && charPos < r.second)
            return i;
    }
    if (!m_pageCharRange.isEmpty() && charPos < m_pageCharRange.constFirst().first)
        return 0;
    return m_pageCharRange.size() - 1;
}
```

> 注：`m_pageCharRange.value(page)` 对越界返回默认 `(0,0)`，与 `charRange()` 语义一致。

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS（若 `charRangesPartitionText` 因段落跳过导致相邻页区间重叠，调整测试数据为无压缩空行的连续文本）

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/core/Page.h src/core/Page.cpp tests/tst_page2.cpp
git commit -m "feat: 分页引擎逐行滚动与字符范围"
```

---

### Task 4: Settings 扩展（背景图/透明度/快捷键持久化）

**Files:**
- Modify: `src/core/Settings.h`
- Modify: `src/core/Settings.cpp`
- Modify: `src/core/Book.h`
- Modify: `src/core/Book.cpp`
- Modify: `src/core/TextBook.h`
- Modify: `src/core/TextBook.cpp`
- Test: `tests/tst_settings2.cpp`
- Modify: `CMakeLists.txt`（注册 tst_settings2）

**Interfaces:**
- Consumes: `Keyset`
- Produces: `DisplaySettings::bgImagePath/windowAlpha`、`Settings::keyset`、`Book::totalCharCount`；测试命令 `tst_settings2`

- [ ] **Step 1: 写失败测试**

`tests/tst_settings2.cpp`：

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include "core/Settings.h"

using namespace reader;

class TestSettings2 : public QObject
{
    Q_OBJECT
private slots:
    void newFieldsRoundtrip();
    void keysetPersistsWithSettings();
    void missingFieldsFallBackToDefaults();
};

void TestSettings2::newFieldsRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    s.display.bgImagePath = QStringLiteral("/tmp/bg.png");
    s.display.windowAlpha = 128;
    s.save();
    Settings t(path);
    t.load();
    QCOMPARE(t.display.bgImagePath, QStringLiteral("/tmp/bg.png"));
    QCOMPARE(t.display.windowAlpha, 128);
}

void TestSettings2::keysetPersistsWithSettings()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    s.keyset.setShortcut(KeyAction::Search, QKeySequence(QStringLiteral("Alt+F")));
    s.save();
    Settings t(path);
    t.load();
    QCOMPARE(t.keyset.shortcut(KeyAction::Search), QKeySequence(QStringLiteral("Alt+F")));
}

void TestSettings2::missingFieldsFallBackToDefaults()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    QCOMPARE(s.display.windowAlpha, 255);
    QVERIFY(s.display.bgImagePath.isEmpty());
    QCOMPARE(s.keyset.shortcut(KeyAction::Search), QKeySequence(QStringLiteral("Ctrl+F")));
}

QTEST_APPLESS_MAIN(TestSettings2)
#include "tst_settings2.moc"
```

`CMakeLists.txt`：

```cmake
qt_add_executable(tst_settings2 tests/tst_settings2.cpp)
target_link_libraries(tst_settings2 PRIVATE reader_core Qt6::Test)
add_test(NAME settings2 COMMAND tst_settings2)
set_tests_properties(settings2 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（字段不存在 / `totalCharCount` 未定义）

- [ ] **Step 3: 实现**

`src/core/Settings.h`：

- include 增加 `"core/Keyset.h"`
- `DisplaySettings` 增加：

```cpp
    QString bgImagePath;
    int windowAlpha = 255;
```

- `class Settings` public 增加成员：`Keyset keyset;`

`src/core/Settings.cpp`：

- `load()` 中 `readDisplay(...)` 后增加：

```cpp
    keyset.load(doc.object().value(QStringLiteral("keys")).toObject());
```

- `save()` 中 `root.insert(QStringLiteral("display"), writeDisplay());` 后增加：

```cpp
    root.insert(QStringLiteral("keys"), keyset.save());
```

- `readDisplay` 增加（放在 `display.margin = ...` 之后）：

```cpp
    display.bgImagePath = o.value(QStringLiteral("bg_image")).toString();
    display.windowAlpha = readInt("window_alpha", display.windowAlpha);
```

- `writeDisplay` 增加：

```cpp
    o.insert(QStringLiteral("bg_image"), display.bgImagePath);
    o.insert(QStringLiteral("window_alpha"), display.windowAlpha);
```

`src/core/Book.h`：`chapterText` 之后增加纯虚函数：

```cpp
    virtual qint64 totalCharCount() const = 0;
```

`src/core/TextBook.h`：`chapterText` 声明后增加：

```cpp
    qint64 totalCharCount() const override { return m_text.size(); }
```

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/core/Settings.h src/core/Settings.cpp src/core/Book.h src/core/TextBook.h tests/tst_settings2.cpp
git commit -m "feat: 设置扩展（背景图/透明度/快捷键持久化/全书字数）"
```

---

### Task 5: ReadingView 扩展（快捷键/逐行滚动/搜索/进度跳转/背景图）

**Files:**
- Modify: `src/app/ReadingView.h`
- Modify: `src/app/ReadingView.cpp`
- Test: `tests/tst_readingview2.cpp`
- Modify: `CMakeLists.txt`（注册 tst_readingview2，含 ReadingView 源码）

**Interfaces:**
- Consumes: `Keyset`、`Page`（Task 3）、`Book::totalCharCount`、`DisplaySettings`
- Produces: `ReadingView` 新接口与信号，见"模块接口约定"；测试命令 `tst_readingview2`

- [ ] **Step 1: 写失败测试**

`tests/tst_readingview2.cpp`：

```cpp
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include "app/ReadingView.h"
#include "core/Book.h"

using namespace reader;

class TestReadingView2 : public QObject
{
    Q_OBJECT
private slots:
    void customKeysetTriggersSignals();
    void wheelScrollsByLine();
    void findNextJumpsAndHighlights();
    void jumpToBookProgressWholeBook();
};

static std::shared_ptr<Book> makeBook(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("novel.txt"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return nullptr;
    QString text;
    for (int i = 0; i < 30; ++i) {
        text += QStringLiteral("第%1章 章节\n").arg(i + 1);
        if (i == 5)
            text += QStringLiteral("独一无二的线索词\n");
        text += QStringLiteral("正文内容测试\n").repeated(8);
    }
    f.write(text.toUtf8());
    f.close();
    QString err;
    return Book::create(path, &err);
}

void TestReadingView2::customKeysetTriggersSignals()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    QVERIFY(book);
    ReadingView view;
    view.resize(500, 600);
    view.setSettings(DisplaySettings());
    Keyset k;
    k.setShortcut(KeyAction::Search, QKeySequence(QStringLiteral("F6")));
    view.setKeyset(k);
    view.setBook(book);
    QSignalSpy spy(&view, &ReadingView::searchRequested);
    QTest::keyClick(&view, Qt::Key_F6);
    QCOMPARE(spy.count(), 1);
}

void TestReadingView2::wheelScrollsByLine()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    ReadingView view;
    DisplaySettings s;
    s.font = QFont(QStringLiteral("Noto Sans CJK SC"), 16);
    view.setSettings(s);
    view.setBook(book);
    view.resize(300, 100);
    QVERIFY(view.pageCount() > 1);
    QCOMPARE(view.lineOffset(), 0);
    QWheelEvent wheel(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, -120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &wheel);
    QVERIFY(view.lineOffset() > 0 || view.currentPage() > 0);
}

void TestReadingView2::findNextJumpsAndHighlights()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    QVERIFY(book);
    ReadingView view;
    view.setSettings(DisplaySettings());
    view.setBook(book);
    view.resize(500, 600);
    view.goToChapter(5);
    QVERIFY(view.findNext(QStringLiteral("独一无二的线索词")));
    QVERIFY(view.currentMatchStart() >= 0);
    QCOMPARE(view.currentChapter(), 5);
    const QString text = book->chapterText(5);
    const QPair<int, int> range = view.currentPageCharRange();
    QVERIFY(view.currentMatchStart() >= range.first);
    QVERIFY(view.currentMatchStart() < range.second);
}

void TestReadingView2::jumpToBookProgressWholeBook()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    QVERIFY(book);
    ReadingView view;
    view.setSettings(DisplaySettings());
    view.setBook(book);
    view.resize(500, 600);
    view.jumpToBookProgress(0.0);
    QCOMPARE(view.currentChapter(), 0);
    QCOMPARE(view.currentPage(), 0);
    view.jumpToBookProgress(1.0);
    QCOMPARE(view.currentChapter(), book->chapters().size() - 1);
    view.jumpToBookProgress(0.5);
    QVERIFY(view.currentChapter() > 0);
}

QTEST_MAIN(TestReadingView2)
#include "tst_readingview2.moc"
```

`CMakeLists.txt`：

```cmake
qt_add_executable(tst_readingview2
    tests/tst_readingview2.cpp
    src/app/ReadingView.cpp src/app/ReadingView.h
)
target_include_directories(tst_readingview2 PRIVATE src)
target_link_libraries(tst_readingview2 PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME readingview2 COMMAND tst_readingview2)
set_tests_properties(readingview2 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

> 注：测试用到 `view.lineOffset()` 与 `view.currentPageCharRange()` —— 在 ReadingView 上透传 Page 的两个方法（见 Step 3）。

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（新接口未定义）

- [ ] **Step 3: 实现**

`src/app/ReadingView.h`：

- include 增加 `"core/Keyset.h"`
- public 增加：

```cpp
    void setKeyset(const Keyset &keyset) { m_keyset = keyset; }
    bool findNext(const QString &keyword, bool forward = true);
    void jumpToBookProgress(qreal progress);
    void clearMatch() { m_matchStart = -1; m_matchEnd = -1; update(); }
    int currentMatchStart() const { return m_matchStart; }
    int currentMatchEnd() const { return m_matchEnd; }
    int lineOffset() const { return m_page.lineOffset(); }
    QPair<int, int> currentPageCharRange() const { return m_page.charRange(m_page.currentPage()); }
```

- signals 增加：

```cpp
    void searchRequested();
    void jumpRequested();
    void bookmarkRequested();
    void autoPageRequested();
    void displaySettingsChanged(const reader::DisplaySettings &settings);
```

- private 增加：

```cpp
    Keyset m_keyset;
    QPixmap m_bgPixmap;
    QString m_bgImagePath;
    int m_matchStart = -1;
    int m_matchEnd = -1;
    void nextChapter();
    void prevChapter();
    void fontZoom(int delta);
```

`src/app/ReadingView.cpp`：

- include 增加 `<QPixmap>`。
- `setSettings` 中，在 `m_settings = settings;` 后增加背景图加载：

```cpp
    if (m_bgImagePath != settings.bgImagePath) {
        m_bgImagePath = settings.bgImagePath;
        m_bgPixmap = QPixmap(m_bgImagePath);
    }
```

- `paintEvent` 改为：

```cpp
void ReadingView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    if (!m_bgPixmap.isNull())
        painter.drawPixmap(rect(), m_bgPixmap);
    else
        painter.fillRect(rect(), m_settings.bgColor);
    if (!m_hasBook || m_page.pageCount() == 0) {
        painter.setPen(QColor(140, 140, 140));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("打开一本 TXT 小说开始阅读（Ctrl+O）"));
        return;
    }
    painter.setPen(m_settings.textColor);
    const PageContent &content = m_page.content(m_page.currentPage());
    const int from = m_page.lineOffset();
    for (int i = from; i < content.paragraphIndex.size(); ++i) {
        const QTextLayout &layout = m_page.paragraph(content.paragraphIndex.at(i));
        layout.lineAt(content.lineIndex.at(i)).draw(&painter, content.positions.at(i));
    }
    if (m_matchStart >= 0 && m_matchEnd > m_matchStart) {
        for (int i = from; i < content.paragraphIndex.size(); ++i) {
            const QPair<int, int> range = content.lineCharRange.at(i);
            if (m_matchStart >= range.second || m_matchEnd <= range.first)
                continue;
            const QTextLayout &layout = m_page.paragraph(content.paragraphIndex.at(i));
            const QTextLine line = layout.lineAt(content.lineIndex.at(i));
            const int len = line.textLength();
            const int p1 = qBound(0, m_matchStart - range.first, len);
            const int p2 = qBound(0, m_matchEnd - range.first, len);
            if (p2 <= p1)
                continue;
            const qreal x1 = line.cursorToX(p1);
            const qreal x2 = line.cursorToX(p2);
            painter.fillRect(QRectF(content.positions.at(i).x() + x1,
                                    content.positions.at(i).y(),
                                    x2 - x1, line.height()),
                             QColor(0xFF, 0xE0, 0x66));
        }
    }
    painter.setPen(QColor(128, 128, 128));
    painter.drawText(rect().adjusted(0, 0, -12, -8), Qt::AlignRight | Qt::AlignBottom,
                     QStringLiteral("%1 / %2").arg(m_page.currentPage() + 1).arg(m_page.pageCount()));
}
```

- `goToPage`/`loadChapter`/`resizeEvent`/`mousePressEvent` 在跳页后无需额外处理（`Page::goToPage` 已重置行偏移）；`wheelEvent` 改为逐行：

```cpp
void ReadingView::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (m_page.scrollLines(delta < 0 ? 1 : -1)) {
        emit pageChanged(m_page.currentPage());
        update();
    }
    event->accept();
}
```

- `keyPressEvent` 替换为 Keyset 匹配：

```cpp
void ReadingView::keyPressEvent(QKeyEvent *event)
{
    const QKeySequence seq(event->keyCombination());
    const auto is = [this, &seq](KeyAction a) {
        return m_keyset.shortcut(a).matches(seq) == QKeySequence::ExactMatch;
    };
    bool moved = false;
    if (is(KeyAction::PageDown)) {
        moved = m_page.nextPage();
    } else if (is(KeyAction::PageUp)) {
        moved = m_page.prevPage();
    } else if (is(KeyAction::LineDown)) {
        moved = m_page.nextLine();
    } else if (is(KeyAction::LineUp)) {
        moved = m_page.prevLine();
    } else if (is(KeyAction::ChapterDown)) {
        nextChapter();
    } else if (is(KeyAction::ChapterUp)) {
        prevChapter();
    } else if (is(KeyAction::FontZoomIn)) {
        fontZoom(1);
    } else if (is(KeyAction::FontZoomOut)) {
        fontZoom(-1);
    } else if (is(KeyAction::AutoPage)) {
        emit autoPageRequested();
    } else if (is(KeyAction::Search)) {
        emit searchRequested();
    } else if (is(KeyAction::Jump)) {
        emit jumpRequested();
    } else if (is(KeyAction::AddBookmark)) {
        emit bookmarkRequested();
    } else {
        QWidget::keyPressEvent(event);
        return;
    }
    if (moved) {
        emit pageChanged(m_page.currentPage());
        update();
    }
}
```

- 新增私有方法：

```cpp
void ReadingView::nextChapter()
{
    if (m_hasBook && m_chapter + 1 < m_book->chapters().size())
        goToChapter(m_chapter + 1);
}

void ReadingView::prevChapter()
{
    if (m_hasBook && m_chapter > 0)
        goToChapter(m_chapter - 1);
}

void ReadingView::fontZoom(int delta)
{
    const int size = qBound(6, m_settings.font.pointSize() + delta, 72);
    if (size == m_settings.font.pointSize())
        return;
    m_settings.font.setPointSize(size);
    refreshLayout();
    emit displaySettingsChanged(m_settings);
}

bool ReadingView::findNext(const QString &keyword, bool forward)
{
    if (!m_hasBook || keyword.isEmpty())
        return false;
    const QString text = m_book->chapterText(m_chapter);
    const QPair<int, int> cur = m_page.charRange(m_page.currentPage());
    int idx = -1;
    if (forward)
        idx = text.indexOf(keyword, cur.second);
    else
        idx = text.lastIndexOf(keyword, qMax(0, cur.first - 1));
    if (idx < 0)
        idx = forward ? text.indexOf(keyword) : text.lastIndexOf(keyword);
    if (idx < 0)
        return false;
    m_matchStart = idx;
    m_matchEnd = idx + keyword.size();
    const int page = m_page.pageForChar(idx);
    m_page.goToPage(page);
    emit pageChanged(page);
    update();
    return true;
}

void ReadingView::jumpToBookProgress(qreal progress)
{
    if (!m_hasBook)
        return;
    const qint64 total = m_book->totalCharCount();
    if (total <= 0)
        return;
    const qint64 target = qint64(progress * total);
    const QVector<Chapter> &ch = m_book->chapters();
    int ci = 0;
    for (int i = 0; i < ch.size(); ++i) {
        const qint64 cstart = ch.at(i).charOffset;
        const qint64 cend = (i + 1 < ch.size()) ? ch.at(i + 1).charOffset : total;
        if (target >= cstart && target < cend) {
            ci = i;
            break;
        }
    }
    goToChapter(ci);
    const qint64 cstart = ch.at(ci).charOffset;
    const qint64 cend = (ci + 1 < ch.size()) ? ch.at(ci + 1).charOffset : total;
    const qreal frac = (cend > cstart) ? qreal(target - cstart) / qreal(cend - cstart) : 0.0;
    m_page.jumpToProgress(frac);
    emit pageChanged(m_page.currentPage());
    update();
}
```

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS（若 `wheelScrollsByLine` 断言受字体度量影响，把字号调至 16、视口 300×100 后仍不稳时，将章节正文 repeated 次数加到 12）

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/app/ReadingView.h src/app/ReadingView.cpp tests/tst_readingview2.cpp
git commit -m "feat: 阅读视图快捷键/逐行滚动/搜索高亮/全书进度跳转/背景图"
```

---

### Task 6: SettingsDialog 设置项补全

**Files:**
- Modify: `src/app/SettingsDialog.h`
- Modify: `src/app/SettingsDialog.cpp`

**Interfaces:**
- Consumes: `DisplaySettings`（含 Task 4 新字段）
- Produces: 完整"显示设置"对话框

- [ ] **Step 1: 扩展头文件**

`src/app/SettingsDialog.h` 增加槽与成员：

```cpp
private slots:
    void pickScreenColor();
    void pickBackgroundImage();
    void clearBackgroundImage();
    void updateAlphaLabel(int value);

private:
    QSpinBox *m_paragraphGapSpin;
    QCheckBox *m_compressBlankLines;
    QCheckBox *m_wordWrap;
    QCheckBox *m_chapterPageBreak;
    QFontComboBox *m_titleFontCombo;
    QSpinBox *m_titleSizeSpin;
    QCheckBox *m_useSameFont;
    QPushButton *m_bgImageButton;
    QSlider *m_alphaSlider;
    QLabel *m_alphaLabel;
```

前置声明增加 `class QSlider; class QLabel;`

- [ ] **Step 2: 扩展实现**

`src/app/SettingsDialog.cpp`：

- include 增加 `#include <QFileDialog>`、`#include <QFileInfo>`、`#include <QGuiApplication>`、`#include <QScreen>`、`#include <QCursor>`、`#include <QSlider>`、`#include <QLabel>`
- 构造函数在既有控件之后追加：

```cpp
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
    m_alphaSlider->setRange(1, 255);
    m_alphaSlider->setValue(m_settings->display.windowAlpha);
    m_alphaLabel = new QLabel(this);
    connect(m_alphaSlider, &QSlider::valueChanged, this, &SettingsDialog::updateAlphaLabel);
    updateAlphaLabel(m_settings->display.windowAlpha);
```

- `form` 追加行：

```cpp
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
    auto *pickScreen = new QPushButton(QStringLiteral("屏幕取色"), this);
    connect(pickScreen, &QPushButton::clicked, this, &SettingsDialog::pickScreenColor);
    form->addRow(QStringLiteral("背景色"), m_bgButton);
    form->addRow(QStringLiteral("取色"), pickScreen);
```

> 注：原构造函数中已有 `form->addRow(QStringLiteral("背景色"), m_bgButton);` 一行，将其替换为上面两行（背景色 + 屏幕取色）；`accept()` 前保持原逻辑。

- `accept()` 增加：

```cpp
    m_settings->display.paragraphGap = m_paragraphGapSpin->value();
    m_settings->display.compressBlankLines = m_compressBlankLines->isChecked();
    m_settings->display.wordWrap = m_wordWrap->isChecked();
    m_settings->display.chapterPageBreak = m_chapterPageBreak->isChecked();
    m_settings->display.titleFont.setFamily(m_titleFontCombo->currentFont().family());
    m_settings->display.titleFont.setPointSize(m_titleSizeSpin->value());
    m_settings->display.useSameFont = m_useSameFont->isChecked();
    m_settings->display.windowAlpha = m_alphaSlider->value();
```

- 新槽：

```cpp
void SettingsDialog::pickScreenColor()
{
    hide();
    QCoreApplication::processEvents();
    const QScreen *screen = QGuiApplication::primaryScreen();
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
```

- [ ] **Step 3: 构建与手工验证**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ./build/reader`

逐项检查：设置→显示设置 中段距/压缩空行/Word wrap/章前分页/章节字体/正文标题同字体/背景图片选择与清除/透明度滑杆/屏幕取色 均可用，确认后设置保存并在下次启动生效。

- [ ] **Step 4: 提交**

```bash
git add src/app/SettingsDialog.h src/app/SettingsDialog.cpp
git commit -m "feat: 显示设置补全（段距/换行/章节字体/背景图/透明度/取色）"
```

---

### Task 7: KeysetDialog 按键设置对话框

**Files:**
- Create: `src/app/KeysetDialog.h`
- Create: `src/app/KeysetDialog.cpp`
- Modify: `CMakeLists.txt`（reader 与 tst_mainwindow2 目标加入 KeysetDialog 源码；本任务先只加 reader）

**Interfaces:**
- Consumes: `Settings::keyset`
- Produces: `reader::KeysetDialog`

- [ ] **Step 1: 实现头文件**

`src/app/KeysetDialog.h`：

```cpp
#pragma once
#include <QDialog>
#include "core/Settings.h"

class QTableWidget;

namespace reader {

class KeysetDialog : public QDialog
{
    Q_OBJECT
public:
    explicit KeysetDialog(Settings *settings, QWidget *parent = nullptr);

private slots:
    void editShortcut();
    void restoreDefaults();
    void accept() override;

private:
    void reload();
    Settings *m_settings;
    QTableWidget *m_table;
};

}
```

- [ ] **Step 2: 实现**

`src/app/KeysetDialog.cpp`：

```cpp
#include "app/KeysetDialog.h"
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace reader {

static QString actionName(KeyAction a)
{
    switch (a) {
    case KeyAction::PageUp: return QStringLiteral("上一页");
    case KeyAction::PageDown: return QStringLiteral("下一页");
    case KeyAction::LineUp: return QStringLiteral("上一行");
    case KeyAction::LineDown: return QStringLiteral("下一行");
    case KeyAction::ChapterUp: return QStringLiteral("上一章");
    case KeyAction::ChapterDown: return QStringLiteral("下一章");
    case KeyAction::Search: return QStringLiteral("搜索");
    case KeyAction::Jump: return QStringLiteral("进度跳转");
    case KeyAction::AddBookmark: return QStringLiteral("添加书签");
    case KeyAction::EditMode: return QStringLiteral("编辑模式");
    case KeyAction::AutoPage: return QStringLiteral("自动翻页");
    case KeyAction::FontZoomIn: return QStringLiteral("字号放大");
    case KeyAction::FontZoomOut: return QStringLiteral("字号缩小");
    case KeyAction::Fullscreen: return QStringLiteral("全屏");
    case KeyAction::HideBorder: return QStringLiteral("隐藏边框");
    case KeyAction::AlwaysOnTop: return QStringLiteral("窗口置顶");
    case KeyAction::HideWindow: return QStringLiteral("隐藏窗口");
    case KeyAction::OpenFile: return QStringLiteral("打开文件");
    case KeyAction::Quit: return QStringLiteral("退出");
    }
    return QString();
}

KeysetDialog::KeysetDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("按键设置"));
    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("功能"), QStringLiteral("快捷键")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    reload();

    auto *edit = new QPushButton(QStringLiteral("修改选中项"), this);
    auto *reset = new QPushButton(QStringLiteral("恢复默认"), this);
    auto *ok = new QPushButton(QStringLiteral("确定"), this);
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    connect(edit, &QPushButton::clicked, this, &KeysetDialog::editShortcut);
    connect(reset, &QPushButton::clicked, this, &KeysetDialog::restoreDefaults);
    connect(ok, &QPushButton::clicked, this, &KeysetDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(edit);
    buttons->addWidget(reset);
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_table);
    layout->addLayout(buttons);
    resize(420, 480);
}

void KeysetDialog::reload()
{
    const QList<KeyAction> actions = m_settings->keyset.actions();
    m_table->setRowCount(actions.size());
    int row = 0;
    for (const KeyAction a : actions) {
        auto *name = new QTableWidgetItem(actionName(a));
        auto *seq = new QTableWidgetItem(m_settings->keyset.shortcut(a).toString());
        name->setData(Qt::UserRole, int(a));
        m_table->setItem(row, 0, name);
        m_table->setItem(row, 1, seq);
        ++row;
    }
}

void KeysetDialog::editShortcut()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;
    const KeyAction a = static_cast<KeyAction>(m_table->item(row, 0)->data(Qt::UserRole).toInt());
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("设置快捷键"));
    auto *edit = new QKeySequenceEdit(m_settings->keyset.shortcut(a), &dlg);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto *layout = new QVBoxLayout(&dlg);
    layout->addWidget(edit);
    layout->addWidget(box);
    if (dlg.exec() == QDialog::Accepted && !edit->keySequence().isEmpty()) {
        m_settings->keyset.setShortcut(a, edit->keySequence());
        reload();
    }
}

void KeysetDialog::restoreDefaults()
{
    if (QMessageBox::question(this, QStringLiteral("恢复默认"),
            QStringLiteral("确定将快捷键恢复为默认设置？")) != QMessageBox::Yes)
        return;
    m_settings->keyset.reset();
    reload();
}

void KeysetDialog::accept()
{
    m_settings->save();
    QDialog::accept();
}

}
```

`CMakeLists.txt`（reader 目标加入）：

```cmake
qt_add_executable(reader
    src/main.cpp
    src/app/MainWindow.cpp src/app/MainWindow.h
    src/app/ReadingView.cpp src/app/ReadingView.h
    src/app/SettingsDialog.cpp src/app/SettingsDialog.h
    src/app/KeysetDialog.cpp src/app/KeysetDialog.h
)
```

- [ ] **Step 3: 构建验证**

Run: `cmake -S . -B build -G Ninja && cmake --build build`
Expected: 构建成功

- [ ] **Step 4: 提交**

```bash
git add CMakeLists.txt src/app/KeysetDialog.h src/app/KeysetDialog.cpp
git commit -m "feat: 按键设置对话框（自定义快捷键/恢复默认）"
```

---

### Task 8: MainWindow 集成与收尾

**Files:**
- Create: `src/app/BookmarkDialog.h`
- Create: `src/app/BookmarkDialog.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Test: `tests/tst_mainwindow2.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ReadingView` 新信号、`Settings::keyset`、`Cache` 书签、`KeysetDialog`
- Produces: `MainWindow::addBookmarkForCurrentBook/resetSettings`（测试用）、`reader::BookmarkDialog`

- [ ] **Step 1: 写失败测试**

`tests/tst_mainwindow2.cpp`：

```cpp
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include "app/MainWindow.h"
#include "core/Cache.h"

using namespace reader;

class TestMainWindow2 : public QObject
{
    Q_OBJECT
private slots:
    void addBookmarkPersists();
    void resetSettingsRestoresDefaults();
};

static QString makeTxt(const QTemporaryDir &dir, const QString &name)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    QString text;
    for (int i = 0; i < 10; ++i)
        text += QStringLiteral("第%1章 章节\n正文内容\n").arg(i + 1);
    f.write(text.toUtf8());
    f.close();
    return path;
}

void TestMainWindow2::addBookmarkPersists()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString path = makeTxt(dir, QStringLiteral("book.txt"));
    w.openBook(path);
    w.addBookmarkForCurrentBook();
    QTest::qWait(30);
    Cache c(Cache::defaultCacheFilePath());
    c.load();
    const QVector<Bookmark> marks = c.bookmarks(path);
    QCOMPARE(marks.size(), 1);
    QCOMPARE(marks.at(0).chapterIndex, w.currentChapter());
}

void TestMainWindow2::resetSettingsRestoresDefaults()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    w.resetSettings();
    QTest::qWait(30);
    Settings s(Settings::defaultConfigFilePath());
    s.load();
    QCOMPARE(s.keyset.shortcut(KeyAction::Search), QKeySequence(QStringLiteral("Ctrl+F")));
    QCOMPARE(s.display.bgColor, QColor(Qt::white));
}

// 注意：resetSettings() 本身不弹确认框；确认框放在菜单 lambda 里（见 Step 4），
// 否则离屏测试会阻塞在模态对话框。

int main(int argc, char *argv[])
{
    QTemporaryDir tmp;
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
    QApplication app(argc, argv);
    TestMainWindow2 tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_mainwindow2.moc"
```

> 注：测试需要 `MainWindow::currentChapter()` 透传（见 Step 3）。

`CMakeLists.txt`：

```cmake
qt_add_executable(reader
    src/main.cpp
    src/app/MainWindow.cpp src/app/MainWindow.h
    src/app/ReadingView.cpp src/app/ReadingView.h
    src/app/SettingsDialog.cpp src/app/SettingsDialog.h
    src/app/KeysetDialog.cpp src/app/KeysetDialog.h
    src/app/BookmarkDialog.cpp src/app/BookmarkDialog.h
)

qt_add_executable(tst_mainwindow2
    tests/tst_mainwindow2.cpp
    src/app/MainWindow.cpp src/app/MainWindow.h
    src/app/ReadingView.cpp src/app/ReadingView.h
    src/app/SettingsDialog.cpp src/app/SettingsDialog.h
    src/app/KeysetDialog.cpp src/app/KeysetDialog.h
    src/app/BookmarkDialog.cpp src/app/BookmarkDialog.h
)
target_include_directories(tst_mainwindow2 PRIVATE src)
target_link_libraries(tst_mainwindow2 PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME mainwindow2 COMMAND tst_mainwindow2)
set_tests_properties(mainwindow2 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`BookmarkDialog`/新方法未定义）

- [ ] **Step 3: 实现 BookmarkDialog**

`src/app/BookmarkDialog.h`：

```cpp
#pragma once
#include <QDialog>
#include "core/Cache.h"

class QListWidget;

namespace reader {

class BookmarkDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BookmarkDialog(const QVector<Bookmark> &bookmarks, QWidget *parent = nullptr);
    int selectedBookmarkIndex() const { return m_selected; }

signals:
    void jumpRequested(int bookmarkIndex);

private slots:
    void onDoubleClicked();

private:
    QListWidget *m_list;
    int m_selected = -1;
};

}
```

`src/app/BookmarkDialog.cpp`：

```cpp
#include "app/BookmarkDialog.h"
#include <QListWidget>
#include <QVBoxLayout>

namespace reader {

BookmarkDialog::BookmarkDialog(const QVector<Bookmark> &bookmarks, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("书签"));
    m_list = new QListWidget(this);
    for (const Bookmark &b : bookmarks) {
        const QString text = QStringLiteral("%1（第 %2 页）")
            .arg(b.title.isEmpty() ? QStringLiteral("未命名") : b.title)
            .arg(b.pageIndex + 1);
        m_list->addItem(text);
    }
    connect(m_list, &QListWidget::itemDoubleClicked, this, &BookmarkDialog::onDoubleClicked);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_list);
    resize(360, 420);
}

void BookmarkDialog::onDoubleClicked()
{
    m_selected = m_list->currentRow();
    if (m_selected >= 0)
        emit jumpRequested(m_selected);
}

}
```

- [ ] **Step 4: 扩展 MainWindow**

`src/app/MainWindow.h`：

- include 增加 `"core/Keyset.h"`
- public 增加：

```cpp
    void addBookmarkForCurrentBook();
    void resetSettings();
    int currentChapter() const { return m_view->currentChapter(); }
```

- private slots 增加：

```cpp
    void onSearchRequested();
    void onJumpRequested();
    void onBookmarkRequested();
    void onDisplaySettingsChanged(const DisplaySettings &settings);
```

- private 增加：

```cpp
    void openBookmarkList();
    void applyKeyset();
```

`src/app/MainWindow.cpp`：

- include 增加 `"app/KeysetDialog.h"`、`"app/BookmarkDialog.h"`、`<QInputDialog>`、`<QLineEdit>`、`<QToolBar>`、`<QSpinBox>`、`<QDialogButtonBox>`、`<QVBoxLayout>`
- 构造函数：`m_view->setSettings(...)` 后增加 `m_view->setKeyset(m_settings.keyset);` 与信号连接：

```cpp
    connect(m_view, &ReadingView::searchRequested, this, &MainWindow::onSearchRequested);
    connect(m_view, &ReadingView::jumpRequested, this, &MainWindow::onJumpRequested);
    connect(m_view, &ReadingView::bookmarkRequested, this, &MainWindow::onBookmarkRequested);
    connect(m_view, &ReadingView::displaySettingsChanged, this, &MainWindow::onDisplaySettingsChanged);
```

- 新增搜索条（构造函数中 `addDockWidget` 之后）：

```cpp
    auto *searchBar = addToolBar(QStringLiteral("搜索"));
    searchBar->setObjectName(QStringLiteral("searchBar"));
    searchBar->setMovable(false);
    m_searchEdit = new QLineEdit(searchBar);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索当前章节（Enter 下一个，Esc 关闭）"));
    searchBar->addWidget(m_searchEdit);
    searchBar->setVisible(false);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this] {
        m_view->findNext(m_searchEdit->text());
    });
```

  `MainWindow.h` 增加成员 `QLineEdit *m_searchEdit = nullptr;` 与前置声明 `class QLineEdit;`

- 菜单调整：
  - 书签菜单：`bm` 改为可用并连接 `onBookmarkRequested`，另加"书签列表"动作连接 `openBookmarkList`：

```cpp
    QAction *bm = bookmark->addAction(QStringLiteral("添加书签"));
    connect(bm, &QAction::triggered, this, &MainWindow::onBookmarkRequested);
    QAction *bmList = bookmark->addAction(QStringLiteral("书签列表"));
    connect(bmList, &QAction::triggered, this, &MainWindow::openBookmarkList);
```

  - 设置菜单："按键设置"动作连接 KeysetDialog；"还原默认设置"动作连接 `resetSettings`：

```cpp
    QAction *keysetAction = settings->addAction(QStringLiteral("按键设置"));
    connect(keysetAction, &QAction::triggered, this, [this] {
        KeysetDialog dlg(&m_settings, this);
        if (dlg.exec() == QDialog::Accepted)
            applyKeyset();
    });
    QAction *resetAction = settings->addAction(QStringLiteral("还原默认设置"));
    connect(resetAction, &QAction::triggered, this, [this] {
        if (QMessageBox::question(this, QStringLiteral("还原默认设置"),
                QStringLiteral("确定恢复所有默认设置？")) != QMessageBox::Yes)
            return;
        resetSettings();
    });
```

  并删除原 `settings->addAction(QStringLiteral("按键设置"))->setEnabled(false);` 与 `settings->addAction(QStringLiteral("还原默认设置"))->setEnabled(false);` 两行。

  - 打开/退出动作的快捷键改用 `applyKeyset()` 统一设置。

- 新槽实现：

```cpp
void MainWindow::onSearchRequested()
{
    if (QToolBar *bar = findChild<QToolBar *>(QStringLiteral("searchBar"))) {
        bar->setVisible(true);
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    }
}

void MainWindow::onJumpRequested()
{
    if (!m_book)
        return;
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("跳转到进度"));
    auto *spin = new QSpinBox(&dlg);
    spin->setRange(0, 100);
    spin->setSuffix(QStringLiteral(" %"));
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto *layout = new QVBoxLayout(&dlg);
    layout->addWidget(spin);
    layout->addWidget(box);
    if (dlg.exec() == QDialog::Accepted)
        m_view->jumpToBookProgress(spin->value() / 100.0);
}

void MainWindow::onBookmarkRequested()
{
    addBookmarkForCurrentBook();
}

void MainWindow::addBookmarkForCurrentBook()
{
    if (m_currentPath.isEmpty() || !m_book)
        return;
    const int ci = m_view->currentChapter();
    QString title;
    if (ci >= 0 && ci < m_book->chapters().size())
        title = m_book->chapters().at(ci).title;
    m_cache.addBookmark({m_currentPath, ci, m_view->currentPage(), title,
                         QDateTime::currentSecsSinceEpoch()});
    m_cache.save();
}

void MainWindow::openBookmarkList()
{
    if (m_currentPath.isEmpty())
        return;
    const QVector<Bookmark> marks = m_cache.bookmarks(m_currentPath);
    BookmarkDialog dlg(marks, this);
    connect(&dlg, &BookmarkDialog::jumpRequested, this, [this, &dlg, marks](int idx) {
        if (idx >= 0 && idx < marks.size()) {
            m_view->goToChapter(marks.at(idx).chapterIndex);
            m_view->goToPage(marks.at(idx).pageIndex);
        }
    });
    dlg.exec();
}

void MainWindow::onDisplaySettingsChanged(const DisplaySettings &settings)
{
    m_settings.display = settings;
    m_settings.save();
    setWindowOpacity(settings.windowAlpha / 255.0);
}

void MainWindow::applyKeyset()
{
    m_view->setKeyset(m_settings.keyset);
    if (QAction *open = findChild<QAction *>(QStringLiteral("actOpen")))
        open->setShortcut(m_settings.keyset.shortcut(KeyAction::OpenFile));
    if (QAction *quit = findChild<QAction *>(QStringLiteral("actQuit")))
        quit->setShortcut(m_settings.keyset.shortcut(KeyAction::Quit));
}

void MainWindow::resetSettings()
{
    Settings fresh;
    m_settings.display = fresh.display;
    m_settings.keyset.reset();
    m_settings.save();
    m_view->setSettings(m_settings.display);
    m_view->setKeyset(m_settings.keyset);
    applyKeyset();
    setWindowOpacity(m_settings.display.windowAlpha / 255.0);
}
```

- `buildMenus` 中打开/退出动作设置 objectName 并在末尾调用 `applyKeyset()`：

```cpp
    open->setObjectName(QStringLiteral("actOpen"));
    quit->setObjectName(QStringLiteral("actQuit"));
    ...
    applyKeyset();
```

- `openBook` 末尾：`m_view->setKeyset(m_settings.keyset);` 与 `setWindowOpacity(m_settings.display.windowAlpha / 255.0);`
- 显示设置对话框 accept 后：`setWindowOpacity(m_settings.display.windowAlpha / 255.0);`
- `onChapterChanged`/`onPageChanged` 不变。
- 构造函数末尾 `resize(960, 720);` 前保持原逻辑。

`MainWindow.h` 增加成员：

```cpp
    QLineEdit *m_searchEdit = nullptr;
```

- [ ] **Step 5: 重新构建并运行全部测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 6: 手工验证清单**

Run: `./build/reader`

1. Ctrl+F 弹出搜索条，输入关键字回车跳到匹配页并黄色高亮；再按 Ctrl+F 可继续
2. Ctrl+G 输入百分比跳到对应章节
3. Ctrl+M 添加书签；菜单"书签列表"双击跳转
4. 设置→显示设置：新设置项生效并保存（段距/压缩空行/Word wrap/章节字体/背景图/透明度/取色）
5. 设置→按键设置：改 Ctrl+F 为其他键后生效；恢复默认恢复
6. 设置→还原默认设置：确认后界面恢复默认
7. 滚轮逐行滚动；↑/↓ 逐行；←/→ 翻页；Ctrl+←/→ 章节；Ctrl+=/- 字号并保存
8. 重启程序：快捷键、显示设置、书签、搜索不残留

- [ ] **Step 7: 提交**

```bash
git add CMakeLists.txt src/app/BookmarkDialog.h src/app/BookmarkDialog.cpp src/app/MainWindow.h src/app/MainWindow.cpp tests/tst_mainwindow2.cpp
git commit -m "feat: 主窗口集成搜索/书签/进度跳转/按键设置/还原默认"
```

---

## 自审记录

- 规范覆盖：设计文档 2.1 节搜索/书签/进度跳转/显示设置/快捷键全部落到任务（搜索=T5/T8、书签=T2/T8、进度跳转=T5/T8、显示设置补全=T4/T6、快捷键=T1/T7）；2b 部分（自动翻页/标签/窗口行为/托盘）留在后续计划。
- 占位符扫描：无 TBD/TODO；所有代码步骤含完整代码。
- 类型一致性：`KeyAction`/`Keyset`/`Bookmark`/`Page::charRange`/`DisplaySettings::windowAlpha`/`Book::totalCharCount` 名称在任务间一致；`ReadingView` 新接口与测试签名一致。
- 已知取舍：搜索范围为当前章节（与原版按全书搜索不同，作为 2a 取舍，后续可扩展）；"章前分页"因每章独立分页天然成立，设置仅持久化。
