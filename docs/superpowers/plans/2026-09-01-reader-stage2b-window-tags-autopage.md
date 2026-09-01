# Reader 第二阶段 2b：自动翻页、标签高亮、窗口行为与托盘 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 Reader 补全自动翻页（翻页/滚动两种模式）、标签关键字高亮、窗口行为（全屏/置顶/隐藏边框/隐藏窗口）与最小化到托盘，并把"基本设置"对话框落地（自动翻页间隔、滚动速度、关闭行为）。

**Architecture:** 延续 2a：核心层（Settings 基本设置字段、TagSet 规则、Page 滚动步长）带单元测试；应用层（ReadingView 自动翻页定时器、MainWindow 窗口行为、BasicSettingsDialog、TagsetDialog、托盘）接线。

**Tech Stack:** C++17、Qt 6 Widgets、CMake、Ninja。

**Spec:** [2026-09-01-reader-cachyos-design.md](../specs/2026-09-01-reader-cachyos-design.md)（2.1 节自动翻页/标签/窗口行为/托盘/基本设置；8 节第二阶段 2b）

## Global Constraints

- C++17，Qt 6 Widgets，CMake ≥ 3.21，Ninja
- 完全离线；界面简体中文
- 设置存 `~/.config/Reader/config.json`（新增顶层 `behavior` 对象与 `tags` 数组；缺失回退默认，向后兼容）
- 自动翻页默认：翻页模式、间隔 3000ms；滚动模式滚速 1 行/步
- 托盘经 Noctalia/StatusNotifierItem 工作；`QSystemTrayIcon::isSystemTrayAvailable()` 为假时不创建托盘并回退普通退出
- 标签高亮默认关闭；命中关键字用指定背景色高亮
- 提交信息 Conventional Commits

## File Structure

```
src/core/Settings.h/.cpp            # 修改：BehaviorSettings + TagItem + tags
src/core/Page.h/.cpp                # 修改：滚动步长
src/app/ReadingView.h/.cpp          # 修改：自动翻页定时器/标签高亮
src/app/BasicSettingsDialog.h/.cpp  # 新增：基本设置对话框
src/app/TagsetDialog.h/.cpp         # 新增：标签设置对话框
src/app/MainWindow.h/.cpp           # 修改：窗口行为/托盘/菜单接线
tests/tst_settings3.cpp             # 新增
tests/tst_page3.cpp                 # 新增
tests/tst_autopage.cpp              # 新增
tests/tst_tags.cpp                  # 新增
tests/tst_window.cpp                # 新增
```

## 模块接口约定（后续任务引用）

```cpp
// core/Settings.h 新增
struct BehaviorSettings {
    int autoPageIntervalMs = 3000;
    bool autoPageScrollMode = false;   // false=翻页，true=滚动
    int scrollStep = 1;
    bool minimizeToTray = false;
    bool doubleClickHide = true;
};
struct TagItem { QString keyword; QColor fg; QColor bg; bool enabled = true; };
// class Settings 新增： BehaviorSettings behavior; QVector<TagItem> tags;
// 私有 static BehaviorSettings readBehavior(const QJsonObject&); QJsonObject writeBehavior() const;
//      static QVector<TagItem> readTags(const QJsonArray&); QJsonArray writeTags() const;

// core/Page.h 新增
//   void setScrollStep(int step); bool scrollDown(); bool scrollUp();

// app/ReadingView.h 新增
//   void setBehavior(const BehaviorSettings &); void setTags(const QVector<TagItem>&);
//   void startAutoPage(); void stopAutoPage(); bool isAutoPaging() const; void toggleAutoPage();
//   int tagCount() const;
// signals 新增： void behaviorChanged(const reader::BehaviorSettings &);
// private: QTimer *m_autoPageTimer=nullptr; BehaviorSettings m_behavior; QVector<TagItem> m_tags;
//          void onAutoPageTick(); void applyTagHighlight(QPainter&, const PageContent&, int);

// app/MainWindow.h 新增（测试用）
//   void showHideWindow(); void toggleFullscreen(); void toggleAlwaysOnTop();
//   void toggleHideBorder(); void toggleAutoPage(); bool autoPageActive() const;
//   bool windowHiddenForTest() const;
```

---

### Task 3: ReadingView 自动翻页与标签高亮

**Files:**
- Modify: `src/app/ReadingView.h`
- Modify: `src/app/ReadingView.cpp`
- Test: `tests/tst_autopage.cpp`、`tests/tst_tags.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `BehaviorSettings`、`TagItem`、`Page::scrollDown/scrollUp`
- Produces: `ReadingView` 自动翻页/标签接口与信号；测试命令 `tst_autopage`、`tst_tags`

- [ ] **Step 1: 写失败测试**

`tests/tst_autopage.cpp`：

```cpp
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include "app/ReadingView.h"
#include "core/Book.h"

using namespace reader;

class TestAutoPage : public QObject
{
    Q_OBJECT
private slots:
    void toggleStartsAndStops();
    void timerAdvancesPageInFlipMode();
    void timerScrollsInScrollMode();
};

static std::shared_ptr<Book> makeBook(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("novel.txt"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return nullptr;
    QString text;
    for (int i = 0; i < 40; ++i)
        text += QStringLiteral("第%1章 章节\n").arg(i + 1) + QStringLiteral("正文内容测试\n").repeated(8);
    f.write(text.toUtf8());
    f.close();
    QString err;
    return Book::create(path, &err);
}

void TestAutoPage::toggleStartsAndStops()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    QVERIFY(book);
    ReadingView view;
    view.setSettings(DisplaySettings());
    view.setBehavior(BehaviorSettings());
    view.setBook(book);
    view.show();
    view.resize(400, 300);
    QVERIFY(!view.isAutoPaging());
    view.toggleAutoPage();
    QVERIFY(view.isAutoPaging());
    view.toggleAutoPage();
    QVERIFY(!view.isAutoPaging());
}

void TestAutoPage::timerAdvancesPageInFlipMode()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    ReadingView view;
    view.setSettings(DisplaySettings());
    BehaviorSettings b;
    b.autoPageScrollMode = false;
    b.autoPageIntervalMs = 10;
    view.setBehavior(b);
    view.setBook(book);
    view.show();
    view.resize(400, 200);
    QVERIFY(view.pageCount() > 1);
    const int first = view.currentPage();
    view.toggleAutoPage();
    QTRY_VERIFY_WITH_TIMEOUT(view.currentPage() > first, 1000);
}

void TestAutoPage::timerScrollsInScrollMode()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    ReadingView view;
    view.setSettings(DisplaySettings());
    BehaviorSettings b;
    b.autoPageScrollMode = true;
    b.autoPageIntervalMs = 10;
    b.scrollStep = 2;
    view.setBehavior(b);
    view.setBook(book);
    view.show();
    view.resize(400, 150);
    const int before = view.currentPage();
    view.toggleAutoPage();
    QTRY_VERIFY_WITH_TIMEOUT(view.currentPage() > before || view.lineOffset() > 0, 1000);
}

QTEST_MAIN(TestAutoPage)
#include "tst_autopage.moc"
```

`tests/tst_tags.cpp`：

```cpp
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include "app/ReadingView.h"
#include "core/Book.h"

using namespace reader;

class TestTags : public QObject
{
    Q_OBJECT
private slots:
    void disabledTagIgnored();
    void enabledTagStored();
};

void TestTags::disabledTagIgnored()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(QStringLiteral("第一章\n剑光如雪\n").toUtf8());
    f.close();
    QString err;
    auto book = Book::create(path, &err);
    ReadingView view;
    view.setSettings(DisplaySettings());
    view.setBook(book);
    view.setTags({TagItem{QStringLiteral("剑"), QColor("red"), QColor("yellow"), false}});
    view.resize(500, 400);
    QVERIFY(view.tagCount() == 0);
}

void TestTags::enabledTagStored()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("b.txt"));
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(QStringLiteral("第一章\n剑光如雪\n").toUtf8());
    f.close();
    QString err;
    auto book = Book::create(path, &err);
    ReadingView view;
    view.setSettings(DisplaySettings());
    view.setBook(book);
    view.setTags({TagItem{QStringLiteral("剑"), QColor("red"), QColor("yellow"), true}});
    view.resize(500, 400);
    QVERIFY(view.tagCount() > 0);
}

QTEST_MAIN(TestTags)
#include "tst_tags.moc"
```

`CMakeLists.txt`：

```cmake
qt_add_executable(tst_autopage
    tests/tst_autopage.cpp
    src/app/ReadingView.cpp src/app/ReadingView.h
)
target_include_directories(tst_autopage PRIVATE src)
target_link_libraries(tst_autopage PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME autopage COMMAND tst_autopage)
set_tests_properties(autopage PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_tags
    tests/tst_tags.cpp
    src/app/ReadingView.cpp src/app/ReadingView.h
)
target_include_directories(tst_tags PRIVATE src)
target_link_libraries(tst_tags PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME tags COMMAND tst_tags)
set_tests_properties(tags PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（新接口未定义）

- [ ] **Step 3: 实现**

`src/app/ReadingView.h`：include 增加 `<QTimer>`、`<QPainter>`；public 增加：

```cpp
    void setBehavior(const BehaviorSettings &behavior) { m_behavior = behavior; }
    void setTags(const QVector<TagItem> &tags) { m_tags = tags; update(); }
    int tagCount() const { return m_tags.size(); }
    void startAutoPage();
    void stopAutoPage();
    bool isAutoPaging() const { return m_autoPageTimer && m_autoPageTimer->isActive(); }
    void toggleAutoPage();
```

signals 增加 `void behaviorChanged(const reader::BehaviorSettings &behavior);`；private 增加：

```cpp
    QTimer *m_autoPageTimer = nullptr;
    BehaviorSettings m_behavior;
    QVector<TagItem> m_tags;
    void onAutoPageTick();
    void applyTagHighlight(QPainter &painter, const PageContent &content, int fromIndex);
```

`src/app/ReadingView.cpp`：include 增加 `<QTimer>`；构造函数增加：

```cpp
    m_autoPageTimer = new QTimer(this);
    m_autoPageTimer->setInterval(m_behavior.autoPageIntervalMs);
    connect(m_autoPageTimer, &QTimer::timeout, this, &ReadingView::onAutoPageTick);
```

新增方法与标签绘制（代码见"自审记录"下方设计要点：`onAutoPageTick` 按滚动/翻页模式 `scrollDown()`/`nextPage()`；`applyTagHighlight` 遍历启用标签，用 `lineCharRange` 定位并 `fillRect` 背景）。paintEvent 在正文行之后、页码之前调用 `applyTagHighlight(painter, content, from);`。

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/app/ReadingView.h src/app/ReadingView.cpp tests/tst_autopage.cpp tests/tst_tags.cpp
git commit -m "feat: 自动翻页（翻页/滚动）与标签关键字高亮"
```

### Task 4: 基本设置对话框

**Files:**
- Create: `src/app/BasicSettingsDialog.h`
- Create: `src/app/BasicSettingsDialog.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Settings::behavior`
- Produces: `reader::BasicSettingsDialog`

- [ ] **Step 1: 实现头文件**

`src/app/BasicSettingsDialog.h`：

```cpp
#pragma once
#include <QDialog>
#include "core/Settings.h"

class QSpinBox;
class QCheckBox;
class QComboBox;

namespace reader {

class BasicSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BasicSettingsDialog(Settings *settings, QWidget *parent = nullptr);

private slots:
    void accept() override;

private:
    Settings *m_settings;
    QSpinBox *m_intervalSpin;
    QComboBox *m_modeCombo;
    QSpinBox *m_scrollStepSpin;
    QCheckBox *m_minimizeToTray;
    QCheckBox *m_doubleClickHide;
};

}
```

- [ ] **Step 2: 实现**

`src/app/BasicSettingsDialog.cpp`：构建表单（自动翻页间隔 Spin、翻页/滚动 Combo、滚动速度 Spin、两个行为 CheckBox），`accept()` 写回 `m_settings->behavior` 并 `save()`。控件与字段对应：interval→`autoPageIntervalMs`、modeCombo currentData→`autoPageScrollMode`、scrollStep→`scrollStep`、minimizeToTray→`minimizeToTray`、doubleClickHide→`doubleClickHide`。

> 代码模式与 `settings2` 的 SettingsDialog 一致（QFormLayout + 确定/取消），此处不再重复整段模板；实现时参考 `src/app/SettingsDialog.cpp` 的结构。

`CMakeLists.txt` reader 目标加入 `src/app/BasicSettingsDialog.cpp src/app/BasicSettingsDialog.h`。

- [ ] **Step 3: 构建验证**

Run: `cmake -S . -B build -G Ninja && cmake --build build`
Expected: 构建成功

- [ ] **Step 4: 提交**

```bash
git add CMakeLists.txt src/app/BasicSettingsDialog.h src/app/BasicSettingsDialog.cpp
git commit -m "feat: 基本设置对话框（自动翻页/滚动速度/关闭行为）"
```

### Task 5: 标签设置对话框

**Files:**
- Create: `src/app/TagsetDialog.h`
- Create: `src/app/TagsetDialog.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Settings::tags`
- Produces: `reader::TagsetDialog`

- [ ] **Step 1: 实现头文件**

`src/app/TagsetDialog.h`：

```cpp
#pragma once
#include <QDialog>
#include "core/Settings.h"

class QTableWidget;
class QLineEdit;

namespace reader {

class TagsetDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TagsetDialog(Settings *settings, QWidget *parent = nullptr);

private slots:
    void addTag();
    void removeTag();
    void accept() override;

private:
    void reload();
    Settings *m_settings;
    QTableWidget *m_table;
    QLineEdit *m_keywordEdit;
};

}
```

- [ ] **Step 2: 实现**

`src/app/TagsetDialog.cpp`：表格四列（关键字/前景/背景/启用），"添加"读输入行关键字并弹颜色选择默认背景，追加一条启用标签；"删除选中"移除；`reload()` 填充表格（启用列用 `setCheckState`）；`accept()` 把表格回写 `m_settings->tags` 并 `save()`。代码模式同 `KeysetDialog`（QTableWidget + 按钮）。

`CMakeLists.txt` reader 目标加入 `src/app/TagsetDialog.cpp src/app/TagsetDialog.h`。

- [ ] **Step 3: 构建验证**

Run: `cmake -S . -B build -G Ninja && cmake --build build`
Expected: 构建成功

- [ ] **Step 4: 提交**

```bash
git add CMakeLists.txt src/app/TagsetDialog.h src/app/TagsetDialog.cpp
git commit -m "feat: 标签设置对话框（关键字高亮管理）"
```

### Task 6: 窗口行为 + 托盘 + MainWindow 集成

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Test: `tests/tst_window.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ReadingView::toggleAutoPage`、`Settings::behavior/tags`、`BasicSettingsDialog`、`TagsetDialog`
- Produces: 窗口行为方法、托盘；测试命令 `tst_window`

- [ ] **Step 1: 写失败测试**

`tests/tst_window.cpp`：

```cpp
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include "app/MainWindow.h"

using namespace reader;

class TestWindow : public QObject
{
    Q_OBJECT
private slots:
    void hideAndShow();
    void fullscreenToggle();
    void alwaysOnTopToggle();
    void hideBorderToggle();
    void autopageReflectsSettings();
};

void TestWindow::hideAndShow()
{
    MainWindow w;
    w.show();
    w.hide();
    QVERIFY(!w.isVisible());
    w.show();
    QVERIFY(w.isVisible());
}

void TestWindow::fullscreenToggle()
{
    MainWindow w;
    w.show();
    w.toggleFullscreen();
    QVERIFY(w.isFullScreen());
    w.toggleFullscreen();
    QVERIFY(!w.isFullScreen());
}

void TestWindow::alwaysOnTopToggle()
{
    MainWindow w;
    w.show();
    w.toggleAlwaysOnTop();
    QVERIFY(!!(w.windowFlags() & Qt::WindowStaysOnTopHint));
    w.toggleAlwaysOnTop();
    QVERIFY(!(w.windowFlags() & Qt::WindowStaysOnTopHint));
}

void TestWindow::hideBorderToggle()
{
    MainWindow w;
    w.show();
    w.toggleHideBorder();
    QVERIFY(!!(w.windowFlags() & Qt::FramelessWindowHint));
    w.toggleHideBorder();
    QVERIFY(!(w.windowFlags() & Qt::FramelessWindowHint));
}

void TestWindow::autopageReflectsSettings()
{
    MainWindow w;
    w.show();
    w.toggleAutoPage();
    QVERIFY(w.autoPageActive());
    w.toggleAutoPage();
    QVERIFY(!w.autoPageActive());
}

int main(int argc, char *argv[])
{
    QTemporaryDir tmp;
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
    QApplication app(argc, argv);
    TestWindow tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_window.moc"
```

`CMakeLists.txt` reader 目标加入全部对话框与 `tst_window`（含 MainWindow/ReadingView/各对话框源码）。

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（新方法未定义）

- [ ] **Step 3: 实现 MainWindow 扩展**

`src/app/MainWindow.h` public 增加：

```cpp
    void showHideWindow();
    void toggleFullscreen();
    void toggleAlwaysOnTop();
    void toggleHideBorder();
    void toggleAutoPage();
    bool autoPageActive() const;
    bool windowHiddenForTest() const { return !isVisible(); }
```

private 增加 `void createTrayIcon(); void applyWindowOpacity(); QSystemTrayIcon *m_tray = nullptr;`；前置声明 `class QSystemTrayIcon;`。

`src/app/MainWindow.cpp`：

- include 增加 `"app/BasicSettingsDialog.h"`、`"app/TagsetDialog.h"`、`<QSystemTrayIcon>`、`<QMenu>`
- 构造函数：`m_view->setBehavior(m_settings.behavior); m_view->setTags(m_settings.tags);`；末尾 `createTrayIcon();`
- 窗口行为方法（`toggleFullscreen` 用 `isFullScreen()`/`showFullScreen()`/`showNormal()`；置顶/边框用 `setWindowFlag(...)`+`show()`；`showHideWindow` 用 `hide()`/`show()`；`toggleAutoPage` 调 `m_view->toggleAutoPage()`；`autoPageActive` 返回 `m_view->isAutoPaging()`）
- 托盘：`createTrayIcon()` 先查 `QSystemTrayIcon::isSystemTrayAvailable()`，假则直接 return；否则建图标+右键菜单（显示/隐藏、退出）+activated 单击切换
- `closeEvent`：若 `behavior.minimizeToTray && m_tray && m_tray->isVisible()` 则 `event->ignore(); hide();`，否则正常退出
- 菜单接线：把原"基本设置""标签设置"入口改为可用并连接对应对话框；Accept 后 `m_view->setBehavior(...)`/`m_view->setTags(...)` 并保存；窗口动作（全屏/置顶/隐藏边框/隐藏窗口）接到 toggle 方法并设快捷键来自 `Keyset`；自动翻页动作接 `toggleAutoPage`
- `onDisplaySettingsChanged`/`openBook`/显示设置 accept 处的透明度改用 `applyWindowOpacity()`
- `resetSettings` 增加行为与标签重置、`m_view->setBehavior/setTags`、`applyWindowOpacity()`

- [ ] **Step 4: 重新构建并运行全部测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 5: 手工验证清单**

Run: `./build/reader`。逐项检查：空格自动翻页、基本设置改间隔/模式生效、标签高亮显示、F11 全屏、Alt+T 置顶、F12 隐藏边框、Alt+H 隐藏窗口、关闭→托盘→点击恢复、还原默认。

- [ ] **Step 6: 提交**

```bash
git add CMakeLists.txt src/app/MainWindow.h src/app/MainWindow.cpp tests/tst_window.cpp
git commit -m "feat: 窗口行为、托盘与主窗口集成"
```

## 自审记录

- 规范覆盖：自动翻页（Task 3/4）、标签（Task 1/3/5）、窗口行为（Task 3/6 Keyset 接线 + Task 6）、托盘（Task 6）、基本设置（Task 1/4）。
- 占位符说明：Task 4/5 对话框实现沿用既有 SettingsDialog/KeysetDialog 代码模式，未整段重复模板；Task 3 的 `onAutoPageTick`/`applyTagHighlight` 给定了行为要点。执行时（本计划内联执行）按要点写出完整代码并编译验证。
- 已知取舍：标签高亮仅背景色；托盘依赖系统托盘可用性；"隐藏任务栏图标"不实现。


### Task 1: Settings 扩展（基本设置 + 标签规则）

**Files:**
- Modify: `src/core/Settings.h`
- Modify: `src/core/Settings.cpp`
- Test: `tests/tst_settings3.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: 无
- Produces: `reader::BehaviorSettings`、`reader::TagItem`、`Settings::behavior/tags`；测试命令 `tst_settings3`

- [ ] **Step 1: 写失败测试**

`tests/tst_settings3.cpp`：

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include "core/Settings.h"

using namespace reader;

class TestSettings3 : public QObject
{
    Q_OBJECT
private slots:
    void behaviorRoundtrip();
    void tagsRoundtrip();
    void defaultsFilled();
};

void TestSettings3::behaviorRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    s.behavior.autoPageIntervalMs = 2000;
    s.behavior.autoPageScrollMode = true;
    s.behavior.scrollStep = 3;
    s.behavior.minimizeToTray = true;
    s.save();
    Settings t(path);
    t.load();
    QCOMPARE(t.behavior.autoPageIntervalMs, 2000);
    QVERIFY(t.behavior.autoPageScrollMode);
    QCOMPARE(t.behavior.scrollStep, 3);
    QVERIFY(t.behavior.minimizeToTray);
}

void TestSettings3::tagsRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    s.tags = { TagItem{QStringLiteral("剑"), QColor("red"), QColor("yellow"), true},
               TagItem{QStringLiteral("客栈"), QColor("blue"), QColor(), false} };
    s.save();
    Settings t(path);
    t.load();
    QCOMPARE(t.tags.size(), 2);
    QCOMPARE(t.tags.at(0).keyword, QStringLiteral("剑"));
    QCOMPARE(t.tags.at(1).keyword, QStringLiteral("客栈"));
    QVERIFY(!t.tags.at(1).enabled);
}

void TestSettings3::defaultsFilled()
{
    QTemporaryDir dir;
    Settings s(dir.filePath(QStringLiteral("config.json")));
    s.load();
    QCOMPARE(s.behavior.autoPageIntervalMs, 3000);
    QVERIFY(!s.behavior.autoPageScrollMode);
    QCOMPARE(s.behavior.scrollStep, 1);
    QVERIFY(!s.behavior.minimizeToTray);
    QVERIFY(s.tags.isEmpty());
}

QTEST_APPLESS_MAIN(TestSettings3)
#include "tst_settings3.moc"
```

`CMakeLists.txt`：

```cmake
qt_add_executable(tst_settings3 tests/tst_settings3.cpp)
target_link_libraries(tst_settings3 PRIVATE reader_core Qt6::Test)
add_test(NAME settings3 COMMAND tst_settings3)
set_tests_properties(settings3 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（字段未定义）

- [ ] **Step 3: 实现**

`src/core/Settings.h` 在 `DisplaySettings` 之后增加：

```cpp
struct BehaviorSettings
{
    int autoPageIntervalMs = 3000;
    bool autoPageScrollMode = false;
    int scrollStep = 1;
    bool minimizeToTray = false;
    bool doubleClickHide = true;
};

struct TagItem
{
    QString keyword;
    QColor fg;
    QColor bg;
    bool enabled = true;
};
```

`class Settings` public 增加：`BehaviorSettings behavior;` 与 `QVector<TagItem> tags;`

`src/core/Settings.cpp`：

- include 增加 `<QJsonArray>`
- `load()` 中 `keyset.load(...)` 后增加：

```cpp
    behavior = readBehavior(doc.object().value(QStringLiteral("behavior")).toObject());
    tags = readTags(doc.object().value(QStringLiteral("tags")).toArray());
```

- `save()` 中 `root.insert(QStringLiteral("keys"), keyset.save());` 后增加：

```cpp
    root.insert(QStringLiteral("behavior"), writeBehavior());
    root.insert(QStringLiteral("tags"), writeTags());
```

- `Settings` 类 private 增加 4 个方法声明（见"模块接口约定"），并在 cpp 实现（见下方）。

- 实现（示例代码，字段名与 Global Constraints 一致）：

```cpp
static int intOr(const QJsonObject &o, const char *key, int fallback)
{
    const QString k = QLatin1String(key);
    return o.contains(k) ? o.value(k).toInt(fallback) : fallback;
}
static bool boolOr(const QJsonObject &o, const char *key, bool fallback)
{
    const QString k = QLatin1String(key);
    return o.contains(k) ? o.value(k).toBool() : fallback;
}

BehaviorSettings Settings::readBehavior(const QJsonObject &o)
{
    BehaviorSettings b;
    b.autoPageIntervalMs = intOr(o, "auto_page_interval_ms", b.autoPageIntervalMs);
    b.autoPageScrollMode = boolOr(o, "auto_page_scroll_mode", b.autoPageScrollMode);
    b.scrollStep = intOr(o, "scroll_step", b.scrollStep);
    b.minimizeToTray = boolOr(o, "minimize_to_tray", b.minimizeToTray);
    b.doubleClickHide = boolOr(o, "double_click_hide", b.doubleClickHide);
    return b;
}

QJsonObject Settings::writeBehavior() const
{
    QJsonObject o;
    o.insert(QStringLiteral("auto_page_interval_ms"), behavior.autoPageIntervalMs);
    o.insert(QStringLiteral("auto_page_scroll_mode"), behavior.autoPageScrollMode);
    o.insert(QStringLiteral("scroll_step"), behavior.scrollStep);
    o.insert(QStringLiteral("minimize_to_tray"), behavior.minimizeToTray);
    o.insert(QStringLiteral("double_click_hide"), behavior.doubleClickHide);
    return o;
}

QVector<TagItem> Settings::readTags(const QJsonArray &a)
{
    QVector<TagItem> out;
    for (const QJsonValue &v : a) {
        const QJsonObject o = v.toObject();
        TagItem t;
        t.keyword = o.value(QStringLiteral("keyword")).toString();
        t.fg = QColor(o.value(QStringLiteral("fg")).toString());
        t.bg = QColor(o.value(QStringLiteral("bg")).toString());
        if (o.contains(QStringLiteral("enabled")))
            t.enabled = o.value(QStringLiteral("enabled")).toBool();
        if (!t.keyword.isEmpty())
            out.append(t);
    }
    return out;
}

QJsonArray Settings::writeTags() const
{
    QJsonArray a;
    for (const TagItem &t : tags) {
        QJsonObject o;
        o.insert(QStringLiteral("keyword"), t.keyword);
        o.insert(QStringLiteral("fg"), t.fg.isValid() ? t.fg.name() : QString());
        o.insert(QStringLiteral("bg"), t.bg.isValid() ? t.bg.name() : QString());
        o.insert(QStringLiteral("enabled"), t.enabled);
        a.append(o);
    }
    return a;
}
```

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/core/Settings.h src/core/Settings.cpp tests/tst_settings3.cpp
git commit -m "feat: 设置扩展（基本行为配置 + 标签规则）"
```

---

### Task 2: Page 滚动步长

**Files:**
- Modify: `src/core/Page.h`
- Test: `tests/tst_page3.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Page::scrollLines`
- Produces: `Page::setScrollStep/scrollDown/scrollUp`；测试命令 `tst_page3`

- [ ] **Step 1: 写失败测试**

`tests/tst_page3.cpp`：

```cpp
#include <QtTest>
#include "core/Page.h"

using namespace reader;

class TestPage3 : public QObject
{
    Q_OBJECT
private slots:
    void scrollStepScrollsMultipleLines();
};

void TestPage3::scrollStepScrollsMultipleLines()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 12);
    page.setParams(p);
    page.setViewSize(300, 120);
    QString text;
    for (int i = 0; i < 40; ++i)
        text += QStringLiteral("第%1章 测试\n正文内容测试正文内容测试\n").arg(i + 1);
    page.setText(text);
    page.setScrollStep(3);
    QVERIFY(page.scrollDown());
    QVERIFY(page.lineOffset() == 3 || page.currentPage() > 0);
    page.scrollUp();
}

QTEST_MAIN(TestPage3)
#include "tst_page3.moc"
```

`CMakeLists.txt`：

```cmake
qt_add_executable(tst_page3 tests/tst_page3.cpp)
target_link_libraries(tst_page3 PRIVATE reader_core Qt6::Test)
add_test(NAME page3 COMMAND tst_page3)
set_tests_properties(page3 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`setScrollStep` 未定义）

- [ ] **Step 3: 实现**

`src/core/Page.h` private 增加 `int m_scrollStep = 1;`，public 增加：

```cpp
    void setScrollStep(int step) { m_scrollStep = qMax(1, step); }
    bool scrollDown() { return scrollLines(m_scrollStep); }
    bool scrollUp() { return scrollLines(-m_scrollStep); }
```

> 内联即可，`scrollLines` 已在 2a Task 3 实现。

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/core/Page.h tests/tst_page3.cpp
git commit -m "feat: 分页引擎滚动步长"
```

---
