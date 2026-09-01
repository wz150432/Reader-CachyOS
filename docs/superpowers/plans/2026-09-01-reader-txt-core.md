# Reader TXT 阅读核心（第一阶段）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 CachyOS 上交付一个可用的原生 Linux TXT 小说阅读器核心：多编码识别、分页排版、翻页、章节目录、阅读进度记忆与基础显示设置。

**Architecture:** C++17 + Qt 6 Widgets。核心逻辑（编码识别、章节解析、分页引擎、设置、缓存）与 UI 分离，核心层不依赖 Widgets，可独立单元测试。UI 层（主窗口、阅读视图、显示设置对话框）调用核心层接口。

**Tech Stack:** C++17、Qt 6（Core/Gui/Widgets/Test）、CMake ≥ 3.21、Ninja、glibc iconv（GB18030 转码）。

**Spec:** [2026-09-01-reader-cachyos-design.md](../specs/2026-09-01-reader-cachyos-design.md)

## Global Constraints

- C++17 标准，Qt 6 Widgets（本机 Qt 6.11.2），CMake ≥ 3.21，Ninja 生成器
- 完全离线，无任何联网代码
- 界面文案为简体中文（菜单与设计文档第 5 节一致）
- 设置文件 `~/.config/Reader/config.json`，进度文件 `~/.local/share/Reader/cache.json`，均为 UTF-8 JSON，写入用临时文件+改名保证原子性
- 所有单元测试以 `QT_QPA_PLATFORM=offscreen` 运行
- 提交信息按 Conventional Commits（`feat:` / `test:` / `docs:` / `chore:`）

## File Structure

```
CMakeLists.txt
src/main.cpp                      # 应用入口
src/core/TextCodec.h/.cpp         # 文本编码识别与解码（UTF 家族 + GB18030）
src/core/ChapterParser.h/.cpp     # 章节标题解析（默认规则 + 自定义正则）
src/core/Book.h/.cpp              # 书籍抽象接口 + 按扩展名工厂
src/core/TextBook.h/.cpp          # TXT 书籍实现
src/core/Page.h/.cpp              # 分页排版引擎（QTextLayout）
src/core/Settings.h/.cpp          # 显示设置数据 + JSON 持久化
src/core/Cache.h/.cpp             # 阅读进度缓存 + JSON 持久化
src/app/ReadingView.h/.cpp        # 自绘阅读视图（翻页/输入/渲染）
src/app/SettingsDialog.h/.cpp     # 基础显示设置对话框（第一阶段）
src/app/MainWindow.h/.cpp         # 主窗口（菜单/目录侧栏/文件打开/进度保存）
tests/tst_textcodec.cpp
tests/tst_chapterparser.cpp
tests/tst_page.cpp
tests/tst_settings.cpp
tests/tst_cache.cpp
tests/tst_readingview.cpp
tests/tst_mainwindow.cpp
```

## 模块接口约定（后续任务引用）

```cpp
// core/TextCodec.h
namespace reader {
enum class TextEncoding { Utf8, Utf8Bom, Utf16LE, Utf16BE, Gb18030, Unknown };
struct DecodeResult { TextEncoding encoding = TextEncoding::Unknown; QString text; bool ok = false; };
class TextCodec {
public:
    static TextEncoding detect(const QByteArray &bytes);
    static DecodeResult decode(const QByteArray &bytes);
    static DecodeResult decode(const QByteArray &bytes, TextEncoding encoding);
    static QString encodingName(TextEncoding encoding);
};
}

// core/ChapterParser.h
namespace reader {
struct Chapter { QString title; int charOffset = 0; };
class ChapterParser {
public:
    static QVector<Chapter> parseDefault(const QString &text);
    static QVector<Chapter> parseRegex(const QString &text, const QRegularExpression &re);
};
}

// core/Book.h
namespace reader {
class Book {
public:
    virtual ~Book() = default;
    virtual bool open(const QString &filePath, QString *error = nullptr) = 0;
    virtual QString title() const = 0;
    virtual const QVector<Chapter> &chapters() const = 0;
    virtual QString chapterText(int chapterIndex) const = 0;
    static std::shared_ptr<Book> create(const QString &filePath, QString *error = nullptr);
    const QString &filePath() const { return m_filePath; }
protected:
    QString m_filePath;
};
}

// core/Page.h
namespace reader {
struct PageLayoutParams {
    QFont font{QStringLiteral("Noto Sans CJK SC"), 12};
    QFont titleFont{QStringLiteral("Noto Sans CJK SC"), 15};
    bool useSameFont = false;
    QColor textColor{QColor(0x33, 0x33, 0x33)};
    int lineGap = 4;
    int paragraphGap = 8;
    bool firstLineIndent = true;
    bool compressBlankLines = false;
    bool wordWrap = true;
    QColor bgColor{QColor(0xF7, 0xF0, 0xE2)};
    int margin = 24;
};
struct PageContent { QVector<int> paragraphIndex; QVector<int> lineIndex; QVector<QPointF> positions; qreal lineHeight = 0; };
class Page {
public:
    void setParams(const PageLayoutParams &params);
    void setViewSize(int width, int height);
    void setText(const QString &text);
    int pageCount() const { return m_pages.size(); }
    int currentPage() const { return m_current; }
    bool goToPage(int page);
    bool nextPage();
    bool prevPage();
    qreal progress() const;
    void jumpToProgress(qreal p);
    const PageContent &content(int page) const { return m_pages.at(page); }
    int lineCount(int page) const { return m_pages.at(page).paragraphIndex.size(); }
    const QTextLayout &paragraph(int index) const { return *m_paragraphs.at(index); }
private:
    void repaginate();
    int m_viewWidth = 0, m_viewHeight = 0;
    PageLayoutParams m_params;
    QString m_text;
    QVector<PageContent> m_pages;
    std::vector<std::unique_ptr<QTextLayout>> m_paragraphs;
    int m_current = 0;
};
}

// core/Settings.h
namespace reader {
struct DisplaySettings {
    QFont font{QStringLiteral("Noto Sans CJK SC"), 12};
    QFont titleFont{QStringLiteral("Noto Sans CJK SC"), 15};
    bool useSameFont = false;
    QColor textColor{QColor(0x33, 0x33, 0x33)};
    QColor bgColor{QColor(0xF7, 0xF0, 0xE2)};
    int lineGap = 4;
    int paragraphGap = 8;
    bool firstLineIndent = true;
    bool compressBlankLines = false;
    bool chapterPageBreak = false;
    bool wordWrap = true;
    int margin = 24;
};
class Settings {
public:
    explicit Settings(const QString &configFilePath = QString());
    DisplaySettings display;
    void load();
    void save() const;
    static QString defaultConfigFilePath();
private:
    QString m_path;
};
}

// core/Cache.h
namespace reader {
struct BookProgress { QString filePath; int chapterIndex = 0; int pageIndex = 0; qint64 lastOpened = 0; };
class Cache {
public:
    explicit Cache(const QString &cacheFilePath = QString());
    void load();
    void save() const;
    void upsertProgress(const BookProgress &p);
    std::optional<BookProgress> progress(const QString &filePath) const;
    QStringList recentFiles() const;
    void clearAll();
    static QString defaultCacheFilePath();
private:
    QString m_path;
    QVector<BookProgress> m_progress;
};
}

// app/ReadingView.h（信号与公开方法）
class ReadingView : public QWidget {
    Q_OBJECT
public:
    explicit ReadingView(QWidget *parent = nullptr);
    void setBook(std::shared_ptr<reader::Book> book);
    void setSettings(const reader::DisplaySettings &settings);
    int currentChapter() const { return m_chapter; }
    int currentPage() const { return m_page.currentPage(); }
    int pageCount() const { return m_page.pageCount(); }
    void goToChapter(int index);
    void goToPage(int page);
    void refreshLayout();
signals:
    void chapterChanged(int index);
    void pageChanged(int index);
protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
private:
    void loadChapter();
    std::shared_ptr<reader::Book> m_book;
    reader::DisplaySettings m_settings;
    reader::Page m_page;
    int m_chapter = 0;
    bool m_hasBook = false;
};

// app/MainWindow.h（测试用公开方法）
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void openBook(const QString &path);
    QString currentBookTitle() const;
    int tocItemCount() const;
private:
    void buildMenus();
    void populateToc();
    void updateTitle();
    void saveProgress();
    reader::ReadingView *m_view = nullptr;
    QTreeWidget *m_toc = nullptr;
    reader::Cache m_cache;
    reader::Settings m_settings;
    std::shared_ptr<reader::Book> m_book;
    QString m_currentPath;
};
```

---

### Task 1: 项目骨架（CMake + 空主窗口）

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.cpp`
- Create: `src/app/MainWindow.h`
- Create: `src/app/MainWindow.cpp`

**Interfaces:**
- Consumes: 无
- Produces: 可构建的 `reader` 可执行文件；`reader::MainWindow` 类（后续任务扩展）

- [ ] **Step 1: 创建最小可构建工程**

`CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.21)
project(ReaderCachyOS VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
qt_standard_project_setup()

qt_add_executable(reader
    src/main.cpp
    src/app/MainWindow.cpp src/app/MainWindow.h
)
target_include_directories(reader PRIVATE src)
target_link_libraries(reader PRIVATE Qt6::Widgets)
```

`src/main.cpp`：

```cpp
#include <QApplication>
#include "app/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Reader"));
    app.setOrganizationName(QStringLiteral("Reader"));
    reader::MainWindow w;
    w.resize(960, 720);
    w.show();
    return app.exec();
}
```

`src/app/MainWindow.h`：

```cpp
#pragma once
#include <QMainWindow>

namespace reader {

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
};

}
```

`src/app/MainWindow.cpp`：

```cpp
#include "app/MainWindow.h"

namespace reader {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Reader"));
}

}
```

- [ ] **Step 2: 配置并构建**

Run: `cmake -S . -B build -G Ninja && cmake --build build`
Expected: 构建成功，生成 `build/reader`

- [ ] **Step 3: 冒烟运行**

Run: `QT_QPA_PLATFORM=offscreen timeout 3 ./build/reader; test $? -eq 124`
Expected: 退出码为 124（程序启动后保持运行直到被 timeout 终止）

- [ ] **Step 4: 提交**

```bash
git add CMakeLists.txt src/main.cpp src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat: 项目骨架（CMake + 空主窗口）"
```

---

### Task 2: TextCodec 编码识别与解码

**Files:**
- Create: `src/core/TextCodec.h`
- Create: `src/core/TextCodec.cpp`
- Test: `tests/tst_textcodec.cpp`
- Modify: `CMakeLists.txt`（加入 reader_core 静态库与测试）

**Interfaces:**
- Consumes: 无
- Produces: `reader::TextCodec`，签名见"模块接口约定"；测试命令 `tst_textcodec`

- [ ] **Step 1: 写失败测试**

`tests/tst_textcodec.cpp`：

```cpp
#include <QtTest>
#include "core/TextCodec.h"

using namespace reader;

class TestTextCodec : public QObject
{
    Q_OBJECT
private slots:
    void utf8Bom();
    void utf8Plain();
    void utf16le();
    void utf16be();
    void gb18030();
    void asciiIsUtf8();
};

void TestTextCodec::utf8Bom()
{
    const QByteArray bytes = QByteArray::fromHex("efbbbf") + QStringLiteral("你好").toUtf8();
    const DecodeResult r = TextCodec::decode(bytes);
    QVERIFY(r.ok);
    QCOMPARE(r.encoding, TextEncoding::Utf8Bom);
    QCOMPARE(r.text, QStringLiteral("你好"));
}

void TestTextCodec::utf8Plain()
{
    const QByteArray bytes = QStringLiteral("第一章 测试").toUtf8();
    const DecodeResult r = TextCodec::decode(bytes);
    QVERIFY(r.ok);
    QCOMPARE(r.encoding, TextEncoding::Utf8);
    QCOMPARE(r.text, QStringLiteral("第一章 测试"));
}

void TestTextCodec::utf16le()
{
    QByteArray data = QByteArray::fromHex("fffe");
    const QString text = QStringLiteral("你好");
    for (const QChar c : text)
        data.append(char(c.unicode() & 0xff)).append(char(c.unicode() >> 8));
    const DecodeResult r = TextCodec::decode(data);
    QVERIFY(r.ok);
    QCOMPARE(r.encoding, TextEncoding::Utf16LE);
    QCOMPARE(r.text, text);
}

void TestTextCodec::utf16be()
{
    QByteArray data = QByteArray::fromHex("feff");
    const QString text = QStringLiteral("你好");
    for (const QChar c : text)
        data.append(char(c.unicode() >> 8)).append(char(c.unicode() & 0xff));
    const DecodeResult r = TextCodec::decode(data);
    QVERIFY(r.ok);
    QCOMPARE(r.encoding, TextEncoding::Utf16BE);
    QCOMPARE(r.text, text);
}

void TestTextCodec::gb18030()
{
    // "第一章 测试" 的 GBK 字节
    const QByteArray bytes = QByteArray::fromHex("b5dad2bbd5c220b2e2cad4");
    const DecodeResult r = TextCodec::decode(bytes);
    QVERIFY(r.ok);
    QCOMPARE(r.encoding, TextEncoding::Gb18030);
    QCOMPARE(r.text, QStringLiteral("第一章 测试"));
}

void TestTextCodec::asciiIsUtf8()
{
    const QByteArray bytes = "hello world\n";
    const DecodeResult r = TextCodec::decode(bytes);
    QVERIFY(r.ok);
    QCOMPARE(r.encoding, TextEncoding::Utf8);
    QCOMPARE(r.text, QStringLiteral("hello world\n"));
}

QTEST_APPLESS_MAIN(TestTextCodec)
#include "tst_textcodec.moc"
```

`CMakeLists.txt` 修改：把测试目标加入（本任务先只注册 textcodec，其余测试目标在对应任务中加入）：

```cmake
enable_testing()
qt_add_executable(tst_textcodec tests/tst_textcodec.cpp)
target_link_libraries(tst_textcodec PRIVATE reader_core Qt6::Test)
add_test(NAME textcodec COMMAND tst_textcodec)
set_tests_properties(textcodec PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

其中 `reader_core` 暂为空库占位（本任务 Step 3 填内容）：

```cmake
add_library(reader_core STATIC)
target_include_directories(reader_core PUBLIC src)
target_link_libraries(reader_core PUBLIC Qt6::Core Qt6::Gui)
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`TextCodec` 未定义，链接错误或断言失败）

- [ ] **Step 3: 实现 TextCodec**

`src/core/TextCodec.h`：

```cpp
#pragma once
#include <QByteArray>
#include <QString>

namespace reader {

enum class TextEncoding { Utf8, Utf8Bom, Utf16LE, Utf16BE, Gb18030, Unknown };

struct DecodeResult
{
    TextEncoding encoding = TextEncoding::Unknown;
    QString text;
    bool ok = false;
};

class TextCodec
{
public:
    static TextEncoding detect(const QByteArray &bytes);
    static DecodeResult decode(const QByteArray &bytes);
    static DecodeResult decode(const QByteArray &bytes, TextEncoding encoding);
    static QString encodingName(TextEncoding encoding);
};

}
```

`src/core/TextCodec.cpp`：

```cpp
#include "core/TextCodec.h"
#include <QStringConverter>
#include <QStringDecoder>
#include <iconv.h>
#include <vector>

namespace reader {

static bool looksLikeUtf8(const QByteArray &bytes)
{
    const QStringDecoder dec(QStringConverter::Utf8);
    return !dec(bytes).contains(QChar::ReplacementCharacter);
}

static QString decodeGb18030(const QByteArray &bytes)
{
    iconv_t cd = iconv_open("UTF-8", "GB18030");
    if (cd == reinterpret_cast<iconv_t>(-1))
        return QString();
    std::vector<char> outBuf(static_cast<size_t>(bytes.size()) * 4 + 16);
    char *inPtr = const_cast<char *>(bytes.constData());
    size_t inLeft = static_cast<size_t>(bytes.size());
    char *outPtr = outBuf.data();
    size_t outLeft = outBuf.size();
    const size_t rc = iconv(cd, &inPtr, &inLeft, &outPtr, &outLeft);
    iconv_close(cd);
    if (rc == static_cast<size_t>(-1))
        return QString();
    return QString::fromUtf8(outBuf.data(), static_cast<qsizetype>(outBuf.size() - outLeft));
}

TextEncoding TextCodec::detect(const QByteArray &bytes)
{
    if (bytes.startsWith("\xEF\xBB\xBF"))
        return TextEncoding::Utf8Bom;
    if (bytes.startsWith("\xFF\xFE"))
        return TextEncoding::Utf16LE;
    if (bytes.startsWith("\xFE\xFF"))
        return TextEncoding::Utf16BE;
    if (looksLikeUtf8(bytes))
        return TextEncoding::Utf8;
    return TextEncoding::Gb18030;
}

DecodeResult TextCodec::decode(const QByteArray &bytes)
{
    return decode(bytes, detect(bytes));
}

DecodeResult TextCodec::decode(const QByteArray &bytes, TextEncoding encoding)
{
    DecodeResult r;
    r.encoding = encoding;
    switch (encoding) {
    case TextEncoding::Utf8Bom:
        r.text = QStringDecoder(QStringConverter::Utf8)(bytes.mid(3));
        r.ok = true;
        break;
    case TextEncoding::Utf8:
        r.text = QStringDecoder(QStringConverter::Utf8)(bytes);
        r.ok = true;
        break;
    case TextEncoding::Utf16LE:
        r.text = QStringDecoder(QStringConverter::Utf16LE)(bytes.mid(2));
        r.ok = true;
        break;
    case TextEncoding::Utf16BE:
        r.text = QStringDecoder(QStringConverter::Utf16BE)(bytes.mid(2));
        r.ok = true;
        break;
    case TextEncoding::Gb18030: {
        r.text = decodeGb18030(bytes);
        r.ok = !r.text.isEmpty();
        break;
    }
    default:
        break;
    }
    return r;
}

QString TextCodec::encodingName(TextEncoding encoding)
{
    switch (encoding) {
    case TextEncoding::Utf8: return QStringLiteral("UTF-8");
    case TextEncoding::Utf8Bom: return QStringLiteral("UTF-8 BOM");
    case TextEncoding::Utf16LE: return QStringLiteral("UTF-16 LE");
    case TextEncoding::Utf16BE: return QStringLiteral("UTF-16 BE");
    case TextEncoding::Gb18030: return QStringLiteral("GB18030");
    default: return QStringLiteral("未知");
    }
}

}
```

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS（若 `gb18030` 用例失败，先核对 GBK 字节串，必要时用 `iconv` 修正后重跑）

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/core/TextCodec.h src/core/TextCodec.cpp tests/tst_textcodec.cpp
git commit -m "feat: 文本编码识别与解码（UTF 家族 + GB18030）"
```

---

### Task 3: ChapterParser 章节解析

**Files:**
- Create: `src/core/ChapterParser.h`
- Create: `src/core/ChapterParser.cpp`
- Test: `tests/tst_chapterparser.cpp`
- Modify: `CMakeLists.txt`（reader_core 加入 ChapterParser；注册 tst_chapterparser）

**Interfaces:**
- Consumes: 无
- Produces: `reader::Chapter`、`reader::ChapterParser`，签名见"模块接口约定"；测试命令 `tst_chapterparser`

- [ ] **Step 1: 写失败测试**

`tests/tst_chapterparser.cpp`：

```cpp
#include <QtTest>
#include "core/ChapterParser.h"

using namespace reader;

class TestChapterParser : public QObject
{
    Q_OBJECT
private slots:
    void defaultChapters();
    void defaultIgnoresPlainLines();
    void standaloneJianZi();
    void invalidChapterNumber();
    void regexChapters();
};

void TestChapterParser::defaultChapters()
{
    const QString text = QStringLiteral(
        "第一章 相遇\n"
        "正文内容\n"
        "第二章 离别\n"
        "正文内容\n"
        "第100章 终章\n"
        "楔子 前言部分\n"
        "序章 开始");
    const QVector<Chapter> chapters = ChapterParser::parseDefault(text);
    QCOMPARE(chapters.size(), 5);
    QCOMPARE(chapters.at(0).title, QStringLiteral("第一章 相遇"));
    QCOMPARE(chapters.at(1).title, QStringLiteral("第二章 离别"));
    QCOMPARE(chapters.at(2).title, QStringLiteral("第100章 终章"));
    QCOMPARE(chapters.at(3).title, QStringLiteral("楔子 前言部分"));
    QCOMPARE(chapters.at(4).title, QStringLiteral("序章 开始"));
    QVERIFY(chapters.at(0).charOffset < chapters.at(1).charOffset);
}

void TestChapterParser::defaultIgnoresPlainLines()
{
    const QString text = QStringLiteral("这是一段普通文字\n没有章节标题\n希望不会误判");
    QCOMPARE(ChapterParser::parseDefault(text).size(), 0);
}

void TestChapterParser::standaloneJianZi()
{
    const QString text = QStringLiteral("楔子\n正文\n第一章 开始");
    const QVector<Chapter> chapters = ChapterParser::parseDefault(text);
    QCOMPARE(chapters.size(), 2);
    QCOMPARE(chapters.at(0).title, QStringLiteral("楔子"));
}

void TestChapterParser::invalidChapterNumber()
{
    const QString text = QStringLiteral("第X章 不是章节\n正文");
    QCOMPARE(ChapterParser::parseDefault(text).size(), 0);
}

void TestChapterParser::regexChapters()
{
    const QString text = QStringLiteral(
        "第一章 相遇\n"
        "正文\n"
        "第二章 离别\n"
        "正文\n"
        "尾声\n");
    const QRegularExpression re(QStringLiteral("第[0-9一二三四五六七八九十]+章.*"));
    const QVector<Chapter> chapters = ChapterParser::parseRegex(text, re);
    QCOMPARE(chapters.size(), 2);
    QCOMPARE(chapters.at(0).title, QStringLiteral("第一章 相遇"));
    QCOMPARE(chapters.at(1).title, QStringLiteral("第二章 离别"));
}

QTEST_APPLESS_MAIN(TestChapterParser)
#include "tst_chapterparser.moc"
```

`CMakeLists.txt` 追加：

```cmake
qt_add_executable(tst_chapterparser tests/tst_chapterparser.cpp)
target_link_libraries(tst_chapterparser PRIVATE reader_core Qt6::Test)
add_test(NAME chapterparser COMMAND tst_chapterparser)
set_tests_properties(chapterparser PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`ChapterParser` 未定义）

- [ ] **Step 3: 实现 ChapterParser**

`src/core/ChapterParser.h`：

```cpp
#pragma once
#include <QRegularExpression>
#include <QString>
#include <QVector>

namespace reader {

struct Chapter
{
    QString title;
    int charOffset = 0; // 章节在整本书解码文本中的字符偏移
};

class ChapterParser
{
public:
    static QVector<Chapter> parseDefault(const QString &text);
    static QVector<Chapter> parseRegex(const QString &text, const QRegularExpression &re);
};

}
```

`src/core/ChapterParser.cpp`（默认解析规则对齐原版：`第[数字|中文数字]章/节/卷/部`、`楔子`、`序章`）：

```cpp
#include "core/ChapterParser.h"

namespace reader {

static const QString kValidChapterChars =
    QStringLiteral(" \t0123456789零一二三四五六七八九十百千万亿壹贰叁肆伍陆柒捌玖拾佰仟萬億两\u3000");

static bool isChapterNumber(const QString &s)
{
    if (s.isEmpty())
        return false;
    for (const QChar c : s) {
        if (!kValidChapterChars.contains(c))
            return false;
    }
    return true;
}

static bool isSep(const QChar &c)
{
    return c == QChar(0x20) || c == QChar(0x09) || c == QChar(0x3000) || c == QChar(0xA0)
        || c == QChar(0xFF1A) || c == QChar(0x3A);
}

QVector<Chapter> ChapterParser::parseDefault(const QString &text)
{
    QVector<Chapter> chapters;
    const QStringList lines = text.split(QLatin1Char('\n'));
    int offset = 0;
    for (const QString &line : lines) {
        int idx1 = -1;
        int idx2 = -1;
        bool found = false;
        for (int i = 0; i < line.size(); ++i) {
            if (line.at(i) == QChar(0x7B2C)) // 第
                idx1 = i;
            const QChar next = (i + 1 < line.size()) ? line.at(i + 1) : QChar();
            if (idx1 >= 0 && (next.isNull() || isSep(next))) {
                const QChar c = line.at(i);
                if (c == QChar(0x5377) || c == QChar(0x7AE0) || c == QChar(0x90E8) || c == QChar(0x8282)) {
                    idx2 = i;
                    found = true;
                    break;
                }
            }
            // 楔子 / 序章（行内独立词，后跟分隔符或行尾）
            if (idx1 == -1 && i + 1 < line.size()
                && line.at(i) == QChar(0x6954) && line.at(i + 1) == QChar(0x5B50)) { // 楔子
                const bool rest = (i + 2 == line.size()) || isSep(line.at(i + 2));
                if (rest) {
                    idx1 = i;
                    idx2 = line.size() - 1;
                    found = true;
                    break;
                }
            }
            if (idx1 == -1 && i + 1 < line.size()
                && line.at(i) == QChar(0x5E8F) && line.at(i + 1) == QChar(0x7AE0)) { // 序章
                const bool rest = (i + 2 == line.size()) || isSep(line.at(i + 2));
                if (rest) {
                    idx1 = i;
                    idx2 = line.size() - 1;
                    found = true;
                    break;
                }
            }
        }
        bool isChapter = false;
        if (found) {
            if (line.at(idx1) == QChar(0x6954) || line.at(idx1) == QChar(0x5E8F)) { // 楔 / 序
                isChapter = true;
            } else if (isChapterNumber(line.mid(idx1 + 1, idx2 - idx1 - 1))) {
                isChapter = true;
            }
        }
        if (isChapter) {
            Chapter ch;
            ch.title = line.mid(idx1);
            ch.charOffset = offset + idx1;
            chapters.append(ch);
        }
        offset += line.size() + 1; // +1 对应换行符
    }
    return chapters;
}

QVector<Chapter> ChapterParser::parseRegex(const QString &text, const QRegularExpression &re)
{
    QVector<Chapter> chapters;
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        Chapter ch;
        ch.title = m.captured(0).trimmed();
        ch.charOffset = m.capturedStart(0);
        chapters.append(ch);
    }
    return chapters;
}

}
```

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/core/ChapterParser.h src/core/ChapterParser.cpp tests/tst_chapterparser.cpp
git commit -m "feat: 章节目录解析（默认规则 + 正则）"
```

---

### Task 4: Book / TextBook 书籍加载

**Files:**
- Create: `src/core/Book.h`
- Create: `src/core/Book.cpp`
- Create: `src/core/TextBook.h`
- Create: `src/core/TextBook.cpp`
- Test: `tests/tst_textbook.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `TextCodec`、`ChapterParser`
- Produces: `reader::Book`、`reader::TextBook`、`Book::create`，签名见"模块接口约定"；测试命令 `tst_textbook`

- [ ] **Step 1: 写失败测试**

`tests/tst_textbook.cpp`：

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/Book.h"
#include "core/TextBook.h"

using namespace reader;

class TestTextBook : public QObject
{
    Q_OBJECT
private slots:
    void opensUtf8();
    void opensGb18030();
    void chapterTextSlices();
    void noChaptersYieldsWholeText();
    void factoryTxtOnly();
};

static QString writeTemp(const QTemporaryDir &dir, const QString &name, const QByteArray &bytes)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(bytes);
    return path;
}

void TestTextBook::opensUtf8()
{
    QTemporaryDir dir;
    const QString path = writeTemp(dir, "a.txt",
        QStringLiteral("第一章 相遇\n正文\n第二章 离别\n正文").toUtf8());
    TextBook book;
    QString err;
    QVERIFY(book.open(path, &err));
    QCOMPARE(book.chapters().size(), 2);
    QCOMPARE(book.chapterText(0).trimmed(), QStringLiteral("第一章 相遇\n正文"));
}

void TestTextBook::opensGb18030()
{
    QTemporaryDir dir;
    const QString path = writeTemp(dir, "b.txt", QByteArray::fromHex("b5dad2bbd5c220b2e2cad4"));
    TextBook book;
    QVERIFY(book.open(path));
    QCOMPARE(book.encoding(), TextEncoding::Gb18030);
    QCOMPARE(book.chapterText(0).trimmed(), QStringLiteral("第一章 测试"));
}

void TestTextBook::chapterTextSlices()
{
    QTemporaryDir dir;
    const QString path = writeTemp(dir, "c.txt",
        QStringLiteral("第一章 甲\nAAAA\n第二章 乙\nBBBB").toUtf8());
    TextBook book;
    QVERIFY(book.open(path));
    QCOMPARE(book.chapters().size(), 2);
    QVERIFY(book.chapterText(0).contains(QStringLiteral("AAAA")));
    QVERIFY(!book.chapterText(0).contains(QStringLiteral("BBBB")));
    QVERIFY(book.chapterText(1).contains(QStringLiteral("BBBB")));
}

void TestTextBook::noChaptersYieldsWholeText()
{
    QTemporaryDir dir;
    const QString path = writeTemp(dir, "d.txt", QStringLiteral("无章节的正文内容").toUtf8());
    TextBook book;
    QVERIFY(book.open(path));
    QCOMPARE(book.chapters().size(), 1);
    QCOMPARE(book.chapterText(0).trimmed(), QStringLiteral("无章节的正文内容"));
}

void TestTextBook::factoryTxtOnly()
{
    QTemporaryDir dir;
    const QString path = writeTemp(dir, "e.txt", "hi".toUtf8());
    QString err;
    auto book = Book::create(path, &err);
    QVERIFY(book);
    QCOMPARE(book->title(), QStringLiteral("e"));
    const QString bad = writeTemp(dir, "f.epub", "fake".toUtf8());
    auto badBook = Book::create(bad, &err);
    QVERIFY(!badBook);
    QVERIFY(!err.isEmpty());
}

QTEST_APPLESS_MAIN(TestTextBook)
#include "tst_textbook.moc"
```

`CMakeLists.txt` 追加（本任务时 reader_core 只包含已创建的文件，Page/Settings/Cache 在 Task 5/6/7 再追加）：

```cmake
add_library(reader_core STATIC
    src/core/TextCodec.cpp src/core/TextCodec.h
    src/core/ChapterParser.cpp src/core/ChapterParser.h
    src/core/Book.cpp src/core/Book.h
    src/core/TextBook.cpp src/core/TextBook.h)

qt_add_executable(tst_textbook tests/tst_textbook.cpp)
target_link_libraries(tst_textbook PRIVATE reader_core Qt6::Test)
add_test(NAME textbook COMMAND tst_textbook)
set_tests_properties(textbook PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`Book` / `TextBook` 未定义）

- [ ] **Step 3: 实现 Book 与 TextBook**

`src/core/Book.h`：

```cpp
#pragma once
#include <QString>
#include <QVector>
#include <memory>
#include "core/ChapterParser.h"

namespace reader {

class Book
{
public:
    virtual ~Book() = default;
    virtual bool open(const QString &filePath, QString *error = nullptr) = 0;
    virtual QString title() const = 0;
    virtual const QVector<Chapter> &chapters() const = 0;
    virtual QString chapterText(int chapterIndex) const = 0;
    static std::shared_ptr<Book> create(const QString &filePath, QString *error = nullptr);
    const QString &filePath() const { return m_filePath; }

protected:
    QString m_filePath;
};

}
```

`src/core/Book.cpp`：

```cpp
#include "core/Book.h"
#include "core/TextBook.h"
#include <QFileInfo>

namespace reader {

std::shared_ptr<Book> Book::create(const QString &filePath, QString *error)
{
    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == QStringLiteral("txt")) {
        auto book = std::make_shared<TextBook>();
        if (!book->open(filePath, error))
            return nullptr;
        return book;
    }
    if (error)
        *error = QStringLiteral("暂不支持该格式（当前版本支持 TXT）");
    return nullptr;
}

}
```

`src/core/TextBook.h`：

```cpp
#pragma once
#include "core/Book.h"
#include "core/TextCodec.h"

namespace reader {

class TextBook final : public Book
{
public:
    bool open(const QString &filePath, QString *error = nullptr) override;
    QString title() const override;
    const QVector<Chapter> &chapters() const override { return m_chapters; }
    QString chapterText(int chapterIndex) const override;
    TextEncoding encoding() const { return m_encoding; }

private:
    QString m_text;
    QVector<Chapter> m_chapters;
    TextEncoding m_encoding = TextEncoding::Unknown;
};

}
```

`src/core/TextBook.cpp`：

```cpp
#include "core/TextBook.h"
#include <QFile>
#include <QFileInfo>

namespace reader {

bool TextBook::open(const QString &filePath, QString *error)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    const QByteArray bytes = f.readAll();
    const DecodeResult r = TextCodec::decode(bytes);
    if (!r.ok) {
        if (error)
            *error = QStringLiteral("无法识别文件编码");
        return false;
    }
    m_filePath = filePath;
    m_encoding = r.encoding;
    m_text = r.text;
    m_chapters = ChapterParser::parseDefault(m_text);
    if (m_chapters.isEmpty()) {
        Chapter ch;
        ch.title = QFileInfo(filePath).completeBaseName();
        ch.charOffset = 0;
        m_chapters.append(ch);
    }
    return true;
}

QString TextBook::title() const
{
    return QFileInfo(m_filePath).completeBaseName();
}

QString TextBook::chapterText(int chapterIndex) const
{
    if (m_chapters.isEmpty())
        return m_text;
    if (chapterIndex < 0 || chapterIndex >= m_chapters.size())
        return QString();
    const int start = m_chapters.at(chapterIndex).charOffset;
    const int end = (chapterIndex + 1 < m_chapters.size())
        ? m_chapters.at(chapterIndex + 1).charOffset
        : m_text.size();
    return m_text.mid(start, end - start);
}

}
```

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/core/Book.h src/core/Book.cpp src/core/TextBook.h src/core/TextBook.cpp tests/tst_textbook.cpp
git commit -m "feat: TXT 书籍加载（编码识别 + 章节切分）"
```

---

### Task 5: Page 分页排版引擎

**Files:**
- Create: `src/core/Page.h`
- Create: `src/core/Page.cpp`
- Test: `tests/tst_page.cpp`
- Modify: `CMakeLists.txt`（reader_core 加入 Page；注册 tst_page）

**Interfaces:**
- Consumes: 无（仅 Qt）
- Produces: `reader::PageLayoutParams`、`reader::PageContent`、`reader::Page`，签名见"模块接口约定"；测试命令 `tst_page`

- [ ] **Step 1: 写失败测试**

`tests/tst_page.cpp`：

```cpp
#include <QtTest>
#include <QImage>
#include "core/Page.h"

using namespace reader;

class TestPage : public QObject
{
    Q_OBJECT
private slots:
    void singlePage();
    void multiplePages();
    void navigationBounds();
    void progressMonotonic();
    void compressBlankLines();
};

void TestPage::singlePage()
{
    Page page;
    PageLayoutParams p;
    page.setParams(p);
    page.setViewSize(800, 1000);
    page.setText(QStringLiteral("第一章 测试\n内容一\n内容二"));
    QCOMPARE(page.pageCount(), 1);
    QVERIFY(page.lineCount(0) >= 3);
}

void TestPage::multiplePages()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 18);
    page.setParams(p);
    page.setViewSize(400, 120);
    QString text;
    for (int i = 0; i < 200; ++i)
        text += QStringLiteral("第%1行 测试文字测试文字\n").arg(i);
    page.setText(text);
    QVERIFY(page.pageCount() > 1);
}

void TestPage::navigationBounds()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 18);
    page.setParams(p);
    page.setViewSize(400, 120);
    QString text;
    for (int i = 0; i < 100; ++i)
        text += QStringLiteral("第%1行 测试\n").arg(i);
    page.setText(text);
    const int count = page.pageCount();
    QVERIFY(count > 1);
    QVERIFY(!page.goToPage(-1));
    QVERIFY(!page.goToPage(count));
    QVERIFY(page.goToPage(0));
    QCOMPARE(page.currentPage(), 0);
    QVERIFY(page.nextPage());
    QCOMPARE(page.currentPage(), 1);
    QVERIFY(page.prevPage());
    QCOMPARE(page.currentPage(), 0);
    QVERIFY(page.goToPage(count - 1));
    QVERIFY(!page.nextPage());
}

void TestPage::progressMonotonic()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 18);
    page.setParams(p);
    page.setViewSize(400, 120);
    QString text;
    for (int i = 0; i < 100; ++i)
        text += QStringLiteral("第%1行 测试\n").arg(i);
    page.setText(text);
    const int count = page.pageCount();
    QVERIFY(count > 1);
    const qreal first = page.progress();
    QVERIFY(page.nextPage());
    QVERIFY(page.progress() > first);
    page.jumpToProgress(1.0);
    QCOMPARE(page.currentPage(), count - 1);
    page.jumpToProgress(0.0);
    QCOMPARE(page.currentPage(), 0);
}

void TestPage::compressBlankLines()
{
    Page page;
    PageLayoutParams p;
    p.font = QFont(QStringLiteral("Noto Sans CJK SC"), 18);
    page.setParams(p);
    page.setViewSize(400, 200);
    QString text = QStringLiteral("第一段\n");
    for (int i = 0; i < 30; ++i)
        text += QStringLiteral("\n");
    text += QStringLiteral("第二段\n");

    p.compressBlankLines = false;
    page.setParams(p);
    page.setText(text);
    const int without = page.pageCount();

    p.compressBlankLines = true;
    page.setParams(p);
    page.setText(text);
    const int with = page.pageCount();

    QVERIFY(with < without);
}

QTEST_APPLESS_MAIN(TestPage)
#include "tst_page.moc"
```

`CMakeLists.txt` 追加：

```cmake
add_library(reader_core STATIC
    src/core/TextCodec.cpp src/core/TextCodec.h
    src/core/ChapterParser.cpp src/core/ChapterParser.h
    src/core/Book.cpp src/core/Book.h
    src/core/TextBook.cpp src/core/TextBook.h
    src/core/Page.cpp src/core/Page.h)

qt_add_executable(tst_page tests/tst_page.cpp)
target_link_libraries(tst_page PRIVATE reader_core Qt6::Test)
add_test(NAME page COMMAND tst_page)
set_tests_properties(page PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`Page` 未定义）

- [ ] **Step 3: 实现 Page**

`src/core/Page.h`：

```cpp
#pragma once
#include <QColor>
#include <QFont>
#include <QPointF>
#include <QString>
#include <QTextLayout>
#include <QVector>

namespace reader {

struct PageLayoutParams
{
    QFont font{QStringLiteral("Noto Sans CJK SC"), 12};
    QFont titleFont{QStringLiteral("Noto Sans CJK SC"), 15};
    bool useSameFont = false;
    QColor textColor{QColor(0x33, 0x33, 0x33)};
    int lineGap = 4;
    int paragraphGap = 8;
    bool firstLineIndent = true;
    bool compressBlankLines = false;
    bool wordWrap = true;
    QColor bgColor{QColor(0xF7, 0xF0, 0xE2)};
    int margin = 24;
};

struct PageContent
{
    QVector<QTextLayout> layouts;
    QVector<QPointF> positions;
    qreal lineHeight = 0;
};

class Page
{
public:
    void setParams(const PageLayoutParams &params);
    void setViewSize(int width, int height);
    void setText(const QString &text);

    int pageCount() const { return m_pages.size(); }
    int currentPage() const { return m_current; }
    bool goToPage(int page);
    bool nextPage();
    bool prevPage();
    qreal progress() const;
    void jumpToProgress(qreal p);
    const PageContent &content(int page) const { return m_pages.at(page); }

private:
    void repaginate();
    int m_viewWidth = 0;
    int m_viewHeight = 0;
    PageLayoutParams m_params;
    QString m_text;
    QVector<PageContent> m_pages;
    int m_current = 0;
};

}
```

`src/core/Page.cpp`：

```cpp
#include "core/Page.h"
#include <QTextOption>
#include <algorithm>

namespace reader {

void Page::setParams(const PageLayoutParams &params)
{
    m_params = params;
    repaginate();
}

void Page::setViewSize(int width, int height)
{
    if (m_viewWidth == width && m_viewHeight == height)
        return;
    m_viewWidth = width;
    m_viewHeight = height;
    repaginate();
}

void Page::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    repaginate();
}

void Page::repaginate()
{
    m_pages.clear();
    m_current = 0;
    if (m_text.isEmpty() || m_viewWidth <= 0 || m_viewHeight <= 0)
        return;

    const int usableWidth = qMax(20, m_viewWidth - 2 * m_params.margin);
    const qreal usableHeight = qMax(20, m_viewHeight - 2 * m_params.margin);
    const QFont bodyFont = m_params.font;
    const QFont titleFont = m_params.useSameFont ? m_params.font : m_params.titleFont;
    const QTextOption::WrapMode wrap = m_params.wordWrap
        ? QTextOption::WrapAtWordBoundaryOrAnywhere
        : QTextOption::NoWrap;

    struct Para { QTextLayout layout; };
    QVector<Para> paras;
    const QStringList paragraphs = m_text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    for (int p = 0; p < paragraphs.size(); ++p) {
        QString paraText = paragraphs.at(p);
        if (m_params.compressBlankLines && paraText.trimmed().isEmpty())
            continue;
        if (m_params.firstLineIndent && !paraText.isEmpty())
            paraText = QStringLiteral("\u3000\u3000") + paraText;
        const QFont &f = (p == 0) ? titleFont : bodyFont;
        QTextLayout layout(paraText, f);
        QTextOption opt;
        opt.setWrapMode(wrap);
        layout.setTextOption(opt);
        layout.beginLayout();
        qreal y = 0;
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(usableWidth);
            line.setPosition(QPointF(0, y));
            y += line.height() + m_params.lineGap;
        }
        layout.endLayout();
        paras.append({std::move(layout)});
    }
    if (paras.isEmpty())
        return;

    struct LineRef { int para; int line; };
    QVector<LineRef> allLines;
    for (int i = 0; i < paras.size(); ++i) {
        for (int j = 0; j < paras.at(i).layout.lineCount(); ++j)
            allLines.append({i, j});
    }

    int i = 0;
    while (i < allLines.size()) {
        PageContent page;
        qreal y = 0;
        int lastPara = -1;
        bool any = false;
        while (i < allLines.size()) {
            const LineRef &ref = allLines.at(i);
            const QTextLayout &layout = paras.at(ref.para).layout;
            const qreal lineH = layout.lineAt(ref.line).height() + m_params.lineGap;
            if (any && y + lineH > usableHeight)
                break;
            if (ref.line == 0 && any && ref.para != lastPara) {
                if (y + m_params.paragraphGap + lineH > usableHeight)
                    break;
                y += m_params.paragraphGap;
            }
            page.layouts.append(QTextLayout(layout));
            page.positions.append(QPointF(0, y));
            page.lineHeight = qMax(page.lineHeight, lineH);
            y += lineH;
            lastPara = ref.para;
            any = true;
            ++i;
        }
        if (!any)
            break;
        m_pages.append(page);
    }
    if (m_pages.isEmpty()) {
        PageContent page;
        page.layouts.append(QTextLayout(paras.first().layout));
        page.positions.append(QPointF(0, 0));
        m_pages.append(page);
    }
}

bool Page::goToPage(int page)
{
    if (page < 0 || page >= m_pages.size())
        return false;
    m_current = page;
    return true;
}

bool Page::nextPage()
{
    return goToPage(m_current + 1);
}

bool Page::prevPage()
{
    return goToPage(m_current - 1);
}

qreal Page::progress() const
{
    if (m_pages.isEmpty())
        return 0.0;
    return qreal(m_current) / qreal(m_pages.size());
}

void Page::jumpToProgress(qreal p)
{
    if (m_pages.isEmpty())
        return;
    m_current = qBound(0, static_cast<int>(p * m_pages.size()), m_pages.size() - 1);
}

}
```

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS（若字体度量导致页数断言不稳，将测试中的字号/视口尺寸调整为 18pt/400×120，保证至少 2 页且差距显著）

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/core/Page.h src/core/Page.cpp tests/tst_page.cpp
git commit -m "feat: 分页排版引擎（行距/段距/缩进/压缩空行/换行）"
```

---

### Task 6: Settings 显示设置持久化

**Files:**
- Create: `src/core/Settings.h`
- Create: `src/core/Settings.cpp`
- Test: `tests/tst_settings.cpp`
- Modify: `CMakeLists.txt`（reader_core 加入 Settings；注册 tst_settings）

**Interfaces:**
- Consumes: 无
- Produces: `reader::DisplaySettings`、`reader::Settings`，签名见"模块接口约定"；测试命令 `tst_settings`

- [ ] **Step 1: 写失败测试**

`tests/tst_settings.cpp`：

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/Settings.h"

using namespace reader;

class TestSettings : public QObject
{
    Q_OBJECT
private slots:
    void defaultsWhenMissing();
    void saveLoadRoundtrip();
    void corruptJsonFallsBackToDefaults();
};

void TestSettings::defaultsWhenMissing()
{
    QTemporaryDir dir;
    Settings s(dir.filePath(QStringLiteral("config.json")));
    s.load();
    QCOMPARE(s.display.font.family(), QStringLiteral("Noto Sans CJK SC"));
    QCOMPARE(s.display.lineGap, 4);
    QCOMPARE(s.display.bgColor, QColor(0xF7, 0xF0, 0xE2));
    QVERIFY(s.display.wordWrap);
}

void TestSettings::saveLoadRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    Settings s(path);
    s.load();
    s.display.font.setFamily(QStringLiteral("Serif"));
    s.display.font.setPointSize(20);
    s.display.lineGap = 10;
    s.display.paragraphGap = 16;
    s.display.bgColor = QColor(0x11, 0x22, 0x33);
    s.display.textColor = QColor(0xAA, 0xBB, 0xCC);
    s.display.firstLineIndent = false;
    s.display.compressBlankLines = true;
    s.display.chapterPageBreak = true;
    s.display.wordWrap = false;
    s.save();

    Settings t(path);
    t.load();
    QCOMPARE(t.display.font.family(), QStringLiteral("Serif"));
    QCOMPARE(t.display.font.pointSize(), 20);
    QCOMPARE(t.display.lineGap, 10);
    QCOMPARE(t.display.paragraphGap, 16);
    QCOMPARE(t.display.bgColor, QColor(0x11, 0x22, 0x33));
    QCOMPARE(t.display.textColor, QColor(0xAA, 0xBB, 0xCC));
    QVERIFY(!t.display.firstLineIndent);
    QVERIFY(t.display.compressBlankLines);
    QVERIFY(t.display.chapterPageBreak);
    QVERIFY(!t.display.wordWrap);
}

void TestSettings::corruptJsonFallsBackToDefaults()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write("{ not json !!");
    f.close();
    Settings s(path);
    s.load();
    QCOMPARE(s.display.lineGap, 4);
}

QTEST_APPLESS_MAIN(TestSettings)
#include "tst_settings.moc"
```

`CMakeLists.txt` 追加：

```cmake
add_library(reader_core STATIC
    src/core/TextCodec.cpp src/core/TextCodec.h
    src/core/ChapterParser.cpp src/core/ChapterParser.h
    src/core/Book.cpp src/core/Book.h
    src/core/TextBook.cpp src/core/TextBook.h
    src/core/Page.cpp src/core/Page.h
    src/core/Settings.cpp src/core/Settings.h)

qt_add_executable(tst_settings tests/tst_settings.cpp)
target_link_libraries(tst_settings PRIVATE reader_core Qt6::Test)
add_test(NAME settings COMMAND tst_settings)
set_tests_properties(settings PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`Settings` 未定义）

- [ ] **Step 3: 实现 Settings**

`src/core/Settings.h`：

```cpp
#pragma once
#include <QColor>
#include <QFont>
#include <QJsonObject>
#include <QString>

namespace reader {

struct DisplaySettings
{
    QFont font{QStringLiteral("Noto Sans CJK SC"), 12};
    QFont titleFont{QStringLiteral("Noto Sans CJK SC"), 15};
    bool useSameFont = false;
    QColor textColor{QColor(0x33, 0x33, 0x33)};
    QColor bgColor{QColor(0xF7, 0xF0, 0xE2)};
    int lineGap = 4;
    int paragraphGap = 8;
    bool firstLineIndent = true;
    bool compressBlankLines = false;
    bool chapterPageBreak = false;
    bool wordWrap = true;
    int margin = 24;
};

class Settings
{
public:
    explicit Settings(const QString &configFilePath = QString());
    DisplaySettings display;
    void load();
    void save() const;
    static QString defaultConfigFilePath();

private:
    void readDisplay(const QJsonObject &o);
    QJsonObject writeDisplay() const;
    QString m_path;
};

}
```

`src/core/Settings.cpp`：

```cpp
#include "core/Settings.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace reader {

Settings::Settings(const QString &configFilePath)
    : m_path(configFilePath.isEmpty() ? defaultConfigFilePath() : configFilePath)
{
}

QString Settings::defaultConfigFilePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("Reader/config.json"));
}

void Settings::load()
{
    QFile f(m_path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    readDisplay(doc.object().value(QStringLiteral("display")).toObject());
}

void Settings::save() const
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    QJsonObject root;
    root.insert(QStringLiteral("display"), writeDisplay());
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.commit();
}

void Settings::readDisplay(const QJsonObject &o)
{
    auto readBool = [&o](const char *key, bool fallback) {
        return o.contains(QLatin1String(key)) ? o.value(QLatin1String(key)).toBool() : fallback;
    };
    auto readInt = [&o](const char *key, int fallback) {
        return o.contains(QLatin1String(key)) ? o.value(QLatin1String(key)).toInt() : fallback;
    };
    auto readColor = [&o](const char *key, const QColor &fallback) {
        return o.contains(QLatin1String(key))
            ? QColor(o.value(QLatin1String(key)).toString())
            : fallback;
    };
    const QString fam = o.value(QStringLiteral("font_family")).toString();
    const QString titleFam = o.value(QStringLiteral("title_font_family")).toString();
    if (!fam.isEmpty())
        display.font.setFamily(fam);
    if (!titleFam.isEmpty())
        display.titleFont.setFamily(titleFam);
    if (o.contains(QStringLiteral("font_size")))
        display.font.setPointSize(o.value(QStringLiteral("font_size")).toInt());
    if (o.contains(QStringLiteral("title_font_size")))
        display.titleFont.setPointSize(o.value(QStringLiteral("title_font_size")).toInt());
    display.useSameFont = readBool("use_same_font", display.useSameFont);
    display.textColor = readColor("text_color", display.textColor);
    display.bgColor = readColor("bg_color", display.bgColor);
    display.lineGap = readInt("line_gap", display.lineGap);
    display.paragraphGap = readInt("paragraph_gap", display.paragraphGap);
    display.firstLineIndent = readBool("first_line_indent", display.firstLineIndent);
    display.compressBlankLines = readBool("compress_blank_lines", display.compressBlankLines);
    display.chapterPageBreak = readBool("chapter_page_break", display.chapterPageBreak);
    display.wordWrap = readBool("word_wrap", display.wordWrap);
    display.margin = readInt("margin", display.margin);
}

QJsonObject Settings::writeDisplay() const
{
    QJsonObject o;
    o.insert(QStringLiteral("font_family"), display.font.family());
    o.insert(QStringLiteral("font_size"), display.font.pointSize());
    o.insert(QStringLiteral("title_font_family"), display.titleFont.family());
    o.insert(QStringLiteral("title_font_size"), display.titleFont.pointSize());
    o.insert(QStringLiteral("use_same_font"), display.useSameFont);
    o.insert(QStringLiteral("text_color"), display.textColor.name());
    o.insert(QStringLiteral("bg_color"), display.bgColor.name());
    o.insert(QStringLiteral("line_gap"), display.lineGap);
    o.insert(QStringLiteral("paragraph_gap"), display.paragraphGap);
    o.insert(QStringLiteral("first_line_indent"), display.firstLineIndent);
    o.insert(QStringLiteral("compress_blank_lines"), display.compressBlankLines);
    o.insert(QStringLiteral("chapter_page_break"), display.chapterPageBreak);
    o.insert(QStringLiteral("word_wrap"), display.wordWrap);
    o.insert(QStringLiteral("margin"), display.margin);
    return o;
}

}
```

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/core/Settings.h src/core/Settings.cpp tests/tst_settings.cpp
git commit -m "feat: 显示设置数据与 JSON 持久化"
```

---

### Task 7: Cache 阅读进度缓存

**Files:**
- Create: `src/core/Cache.h`
- Create: `src/core/Cache.cpp`
- Test: `tests/tst_cache.cpp`
- Modify: `CMakeLists.txt`（reader_core 加入 Cache；注册 tst_cache）

**Interfaces:**
- Consumes: 无
- Produces: `reader::BookProgress`、`reader::Cache`，签名见"模块接口约定"；测试命令 `tst_cache`

- [ ] **Step 1: 写失败测试**

`tests/tst_cache.cpp`：

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include "core/Cache.h"

using namespace reader;

class TestCache : public QObject
{
    Q_OBJECT
private slots:
    void missingFileIsEmpty();
    void upsertAndRoundtrip();
    void recentFilesOrdering();
    void clearAll();
};

void TestCache::missingFileIsEmpty()
{
    QTemporaryDir dir;
    Cache c(dir.filePath(QStringLiteral("cache.json")));
    c.load();
    QVERIFY(!c.progress(QStringLiteral("/x/a.txt")).has_value());
    QVERIFY(c.recentFiles().isEmpty());
}

void TestCache::upsertAndRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cache.json"));
    {
        Cache c(path);
        c.load();
        c.upsertProgress({QStringLiteral("/books/a.txt"), 2, 5, 1000});
        c.upsertProgress({QStringLiteral("/books/a.txt"), 3, 7, 2000});
        c.save();
    }
    {
        Cache c(path);
        c.load();
        const auto p = c.progress(QStringLiteral("/books/a.txt"));
        QVERIFY(p.has_value());
        QCOMPARE(p->chapterIndex, 3);
        QCOMPARE(p->pageIndex, 7);
        QCOMPARE(p->lastOpened, 2000);
    }
}

void TestCache::recentFilesOrdering()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cache.json"));
    Cache c(path);
    c.load();
    c.upsertProgress({QStringLiteral("/books/a.txt"), 0, 0, 100});
    c.upsertProgress({QStringLiteral("/books/b.txt"), 0, 0, 300});
    c.upsertProgress({QStringLiteral("/books/c.txt"), 0, 0, 200});
    c.save();
    Cache d(path);
    d.load();
    const QStringList recent = d.recentFiles();
    QCOMPARE(recent, QStringList({QStringLiteral("/books/b.txt"),
                                  QStringLiteral("/books/c.txt"),
                                  QStringLiteral("/books/a.txt")}));
}

void TestCache::clearAll()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cache.json"));
    Cache c(path);
    c.load();
    c.upsertProgress({QStringLiteral("/books/a.txt"), 1, 1, 100});
    c.save();
    c.clearAll();
    c.save();
    Cache d(path);
    d.load();
    QVERIFY(!d.progress(QStringLiteral("/books/a.txt")).has_value());
}

QTEST_APPLESS_MAIN(TestCache)
#include "tst_cache.moc"
```

`CMakeLists.txt` 追加：

```cmake
add_library(reader_core STATIC
    src/core/TextCodec.cpp src/core/TextCodec.h
    src/core/ChapterParser.cpp src/core/ChapterParser.h
    src/core/Book.cpp src/core/Book.h
    src/core/TextBook.cpp src/core/TextBook.h
    src/core/Page.cpp src/core/Page.h
    src/core/Settings.cpp src/core/Settings.h
    src/core/Cache.cpp src/core/Cache.h)

qt_add_executable(tst_cache tests/tst_cache.cpp)
target_link_libraries(tst_cache PRIVATE reader_core Qt6::Test)
add_test(NAME cache COMMAND tst_cache)
set_tests_properties(cache PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`Cache` 未定义）

- [ ] **Step 3: 实现 Cache**

`src/core/Cache.h`：

```cpp
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace reader {

struct BookProgress
{
    QString filePath;
    int chapterIndex = 0;
    int pageIndex = 0;
    qint64 lastOpened = 0;
};

class Cache
{
public:
    explicit Cache(const QString &cacheFilePath = QString());
    void load();
    void save() const;
    void upsertProgress(const BookProgress &p);
    std::optional<BookProgress> progress(const QString &filePath) const;
    QStringList recentFiles() const;
    void clearAll();
    static QString defaultCacheFilePath();

private:
    QString m_path;
    QVector<BookProgress> m_progress;
};

}
```

`src/core/Cache.cpp`：

```cpp
#include "core/Cache.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

namespace reader {

Cache::Cache(const QString &cacheFilePath)
    : m_path(cacheFilePath.isEmpty() ? defaultCacheFilePath() : cacheFilePath)
{
}

QString Cache::defaultCacheFilePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("cache.json"));
}

void Cache::load()
{
    m_progress.clear();
    QFile f(m_path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    const QJsonArray arr = doc.object().value(QStringLiteral("progress")).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        BookProgress p;
        p.filePath = o.value(QStringLiteral("file")).toString();
        p.chapterIndex = o.value(QStringLiteral("chapter")).toInt();
        p.pageIndex = o.value(QStringLiteral("page")).toInt();
        p.lastOpened = static_cast<qint64>(o.value(QStringLiteral("last_opened")).toDouble());
        if (!p.filePath.isEmpty())
            m_progress.append(p);
    }
}

void Cache::save() const
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    QJsonArray arr;
    for (const BookProgress &p : m_progress) {
        QJsonObject o;
        o.insert(QStringLiteral("file"), p.filePath);
        o.insert(QStringLiteral("chapter"), p.chapterIndex);
        o.insert(QStringLiteral("page"), p.pageIndex);
        o.insert(QStringLiteral("last_opened"), static_cast<double>(p.lastOpened));
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("progress"), arr);
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.commit();
}

void Cache::upsertProgress(const BookProgress &p)
{
    for (BookProgress &item : m_progress) {
        if (item.filePath == p.filePath) {
            item = p;
            return;
        }
    }
    m_progress.append(p);
}

std::optional<BookProgress> Cache::progress(const QString &filePath) const
{
    for (const BookProgress &p : m_progress) {
        if (p.filePath == filePath)
            return p;
    }
    return std::nullopt;
}

QStringList Cache::recentFiles() const
{
    QStringList files;
    for (const BookProgress &p : m_progress)
        files.append(p.filePath);
    std::stable_sort(files.begin(), files.end(),
                     [this](const QString &a, const QString &b) {
                         return progress(a)->lastOpened > progress(b)->lastOpened;
                     });
    return files;
}

void Cache::clearAll()
{
    m_progress.clear();
}

}
```

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/core/Cache.h src/core/Cache.cpp tests/tst_cache.cpp
git commit -m "feat: 阅读进度缓存（进度 + 最近列表）"
```

---

### Task 8: ReadingView 阅读视图

**Files:**
- Create: `src/app/ReadingView.h`
- Create: `src/app/ReadingView.cpp`
- Test: `tests/tst_readingview.cpp`
- Modify: `CMakeLists.txt`（reader 可执行文件加入 ReadingView；注册 tst_readingview）

**Interfaces:**
- Consumes: `reader::Book`、`reader::Page`、`reader::DisplaySettings`
- Produces: `reader::ReadingView`，接口见"模块接口约定"；测试命令 `tst_readingview`

- [ ] **Step 1: 写失败测试**

`tests/tst_readingview.cpp`：

```cpp
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include "app/ReadingView.h"
#include "core/Book.h"

using namespace reader;

class TestReadingView : public QObject
{
    Q_OBJECT
private slots:
    void setBookAndNavigate();
    void keyAndWheelNavigation();
};

static std::shared_ptr<Book> makeBook(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("novel.txt"));
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    QString text;
    for (int i = 0; i < 50; ++i)
        text += QStringLiteral("第%1章 章节\n").arg(i + 1) + QStringLiteral("正文内容\n");
    f.write(text.toUtf8());
    QString err;
    return Book::create(path, &err);
}

void TestReadingView::setBookAndNavigate()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    QVERIFY(book);
    ReadingView view;
    view.setSettings(DisplaySettings());
    view.setBook(book);
    view.resize(600, 800);
    QCOMPARE(view.pageCount(), 1);
    QCOMPARE(view.currentChapter(), 0);
    view.goToChapter(3);
    QCOMPARE(view.currentChapter(), 3);
    QCOMPARE(view.currentPage(), 0);
    QCOMPARE(view.pageCount(), 1);
}

void TestReadingView::keyAndWheelNavigation()
{
    QTemporaryDir dir;
    auto book = makeBook(dir);
    ReadingView view;
    DisplaySettings s;
    s.font = QFont(QStringLiteral("Noto Sans CJK SC"), 18);
    view.setSettings(s);
    view.setBook(book);
    view.resize(300, 100);
    QVERIFY(view.pageCount() > 1);
    const int first = view.currentPage();
    QTest::keyClick(&view, Qt::Key_Right);
    QCOMPARE(view.currentPage(), first + 1);
    QTest::keyClick(&view, Qt::Key_Left);
    QCOMPARE(view.currentPage(), first);
    QWheelEvent wheel(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, -120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &wheel);
    QCOMPARE(view.currentPage(), first + 1);
}

QTEST_MAIN(TestReadingView)
#include "tst_readingview.moc"
```

`CMakeLists.txt` 修改：

```cmake
qt_add_executable(reader
    src/main.cpp
    src/app/MainWindow.cpp src/app/MainWindow.h
    src/app/ReadingView.cpp src/app/ReadingView.h
)
target_include_directories(reader PRIVATE src)
target_link_libraries(reader PRIVATE reader_core Qt6::Widgets)

qt_add_executable(tst_readingview tests/tst_readingview.cpp)
target_link_libraries(tst_readingview PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME readingview COMMAND tst_readingview)
set_tests_properties(readingview PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`ReadingView` 未定义）

- [ ] **Step 3: 实现 ReadingView**

`src/app/ReadingView.h`：

```cpp
#pragma once
#include <QWidget>
#include <memory>
#include "core/Book.h"
#include "core/Page.h"
#include "core/Settings.h"

namespace reader {

class ReadingView : public QWidget
{
    Q_OBJECT
public:
    explicit ReadingView(QWidget *parent = nullptr);
    void setBook(std::shared_ptr<Book> book);
    void setSettings(const DisplaySettings &settings);
    int currentChapter() const { return m_chapter; }
    int currentPage() const { return m_page.currentPage(); }
    int pageCount() const { return m_page.pageCount(); }
    void goToChapter(int index);
    void goToPage(int page);
    void refreshLayout();

signals:
    void chapterChanged(int index);
    void pageChanged(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void loadChapter();
    std::shared_ptr<Book> m_book;
    DisplaySettings m_settings;
    Page m_page;
    int m_chapter = 0;
    bool m_hasBook = false;
};

}
```

`src/app/ReadingView.cpp`：

```cpp
#include "app/ReadingView.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace reader {

ReadingView::ReadingView(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
}

void ReadingView::setBook(std::shared_ptr<Book> book)
{
    m_book = std::move(book);
    m_hasBook = bool(m_book);
    m_chapter = 0;
    if (m_hasBook)
        loadChapter();
    else
        m_page.setText(QString());
    update();
}

void ReadingView::setSettings(const DisplaySettings &settings)
{
    m_settings = settings;
    refreshLayout();
}

void ReadingView::goToChapter(int index)
{
    if (!m_hasBook)
        return;
    const QVector<Chapter> &chapters = m_book->chapters();
    if (index < 0 || index >= chapters.size())
        return;
    m_chapter = index;
    loadChapter();
    emit chapterChanged(index);
}

void ReadingView::goToPage(int page)
{
    if (m_page.goToPage(page)) {
        update();
        emit pageChanged(m_page.currentPage());
    }
}

void ReadingView::refreshLayout()
{
    m_page.setParams(toPageParams(m_settings));
    m_page.setViewSize(width(), height());
    if (m_hasBook)
        m_page.setText(m_book->chapterText(m_chapter));
    update();
}

void ReadingView::loadChapter()
{
    if (!m_hasBook)
        return;
    m_page.setParams(toPageParams(m_settings));
    m_page.setViewSize(width(), height());
    m_page.setText(m_book->chapterText(m_chapter));
    m_page.goToPage(0);
    emit pageChanged(0);
    update();
}

void ReadingView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), m_settings.bgColor);
    if (!m_hasBook || m_page.pageCount() == 0) {
        painter.setPen(QColor(140, 140, 140));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("打开一本 TXT 小说开始阅读（Ctrl+O）"));
        return;
    }
    painter.setPen(m_settings.textColor);
    const PageContent &content = m_page.content(m_page.currentPage());
    for (int i = 0; i < content.paragraphIndex.size(); ++i) {
        const QTextLayout &layout = m_page.paragraph(content.paragraphIndex.at(i));
        layout.lineAt(content.lineIndex.at(i)).draw(&painter, content.positions.at(i));
    }
    painter.setPen(QColor(128, 128, 128));
    painter.drawText(rect().adjusted(0, 0, -12, -8), Qt::AlignRight | Qt::AlignBottom,
                     QStringLiteral("%1 / %2").arg(m_page.currentPage() + 1).arg(m_page.pageCount()));
}

void ReadingView::resizeEvent(QResizeEvent *event)
{
    const qreal keep = m_page.progress();
    m_page.setViewSize(width(), height());
    m_page.jumpToProgress(keep);
    QWidget::resizeEvent(event);
    update();
    emit pageChanged(m_page.currentPage());
}

void ReadingView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_page.nextPage()) {
        emit pageChanged(m_page.currentPage());
        update();
    } else if (event->button() == Qt::RightButton && m_page.prevPage()) {
        emit pageChanged(m_page.currentPage());
        update();
    }
    QWidget::mousePressEvent(event);
}

void ReadingView::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    bool moved = delta < 0 ? m_page.nextPage() : m_page.prevPage();
    if (moved) {
        emit pageChanged(m_page.currentPage());
        update();
    }
    event->accept();
}

void ReadingView::keyPressEvent(QKeyEvent *event)
{
    bool moved = false;
    switch (event->key()) {
    case Qt::Key_Right:
    case Qt::Key_PageDown:
        moved = m_page.nextPage();
        break;
    case Qt::Key_Left:
    case Qt::Key_PageUp:
        moved = m_page.prevPage();
        break;
    case Qt::Key_Space:
        moved = m_page.nextPage();
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    if (moved) {
        emit pageChanged(m_page.currentPage());
        update();
    }
}

}
```

> 注：`ReadingView.cpp` 中用到 `toPageParams`，该辅助函数将 `DisplaySettings` 映射为 `PageLayoutParams`，定义如下（加入 `ReadingView.cpp` 的匿名命名空间）：

```cpp
namespace {
reader::PageLayoutParams toPageParams(const reader::DisplaySettings &s)
{
    reader::PageLayoutParams p;
    p.font = s.font;
    p.titleFont = s.titleFont;
    p.useSameFont = s.useSameFont;
    p.textColor = s.textColor;
    p.lineGap = s.lineGap;
    p.paragraphGap = s.paragraphGap;
    p.firstLineIndent = s.firstLineIndent;
    p.compressBlankLines = s.compressBlankLines;
    p.wordWrap = s.wordWrap;
    p.bgColor = s.bgColor;
    p.margin = s.margin;
    return p;
}
}
```

（`toPageParams` 放在 `ReadingView.cpp` 顶部 include 之后、`namespace reader` 之前。）

- [ ] **Step 4: 重新构建并运行测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt src/app/ReadingView.h src/app/ReadingView.cpp tests/tst_readingview.cpp
git commit -m "feat: 阅读视图（渲染/翻页/键盘与滚轮）"
```

---

### Task 9: SettingsDialog + MainWindow 集成

**Files:**
- Create: `src/app/SettingsDialog.h`
- Create: `src/app/SettingsDialog.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Test: `tests/tst_mainwindow.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Settings`、`Cache`、`ReadingView`、`Book`
- Produces: `reader::SettingsDialog`；`MainWindow::openBook`、`currentBookTitle`、`tocItemCount`（测试用）

- [ ] **Step 1: 写失败测试**

`tests/tst_mainwindow.cpp`：

```cpp
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include "app/MainWindow.h"
#include "core/Cache.h"

using namespace reader;

class TestMainWindow : public QObject
{
    Q_OBJECT
private slots:
    void openBookPopulatesTocAndTitle();
    void pageChangeSavesProgress();
};

static QString makeTxt(const QTemporaryDir &dir, const QString &name)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    QString text;
    for (int i = 0; i < 10; ++i)
        text += QStringLiteral("第%1章 章节\n正文内容\n").arg(i + 1);
    f.write(text.toUtf8());
    return path;
}

void TestMainWindow::openBookPopulatesTocAndTitle()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString path = makeTxt(dir, QStringLiteral("test_book.txt"));
    w.openBook(path);
    QCOMPARE(w.tocItemCount(), 10);
    QVERIFY(w.currentBookTitle().contains(QStringLiteral("第一章 章节")));
}

void TestMainWindow::pageChangeSavesProgress()
{
    QTemporaryDir dir;
    MainWindow w;
    w.show();
    const QString path = makeTxt(dir, QStringLiteral("save_book.txt"));
    w.openBook(path);
    QTest::keyClick(w.findChild<QWidget *>(QStringLiteral("readingView")), Qt::Key_Right);
    QTest::qWait(50);
    const QString cachePath = Cache::defaultCacheFilePath();
    Cache c(cachePath);
    c.load();
    const auto p = c.progress(path);
    QVERIFY(p.has_value());
    QVERIFY(p->pageIndex >= 0);
}

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
```

> 注：为避免测试写入真实的 `~/.config` / `~/.local/share`，把 `tst_mainwindow.cpp` 末尾的 `QTEST_MAIN(TestMainWindow)` 替换为下面的自定义入口（`#include <QTemporaryDir>` 已具备）：

```cpp
int main(int argc, char *argv[])
{
    QTemporaryDir tmp;
    qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
    QApplication app(argc, argv);
    TestMainWindow tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}
```

`CMakeLists.txt` 修改：

```cmake
qt_add_executable(reader
    src/main.cpp
    src/app/MainWindow.cpp src/app/MainWindow.h
    src/app/ReadingView.cpp src/app/ReadingView.h
    src/app/SettingsDialog.cpp src/app/SettingsDialog.h
)
target_include_directories(reader PRIVATE src)

qt_add_executable(tst_mainwindow tests/tst_mainwindow.cpp)
target_link_libraries(tst_mainwindow PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME mainwindow COMMAND tst_mainwindow)
set_tests_properties(mainwindow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL（`SettingsDialog` / `MainWindow` 新方法未定义或链接错误）

- [ ] **Step 3: 实现 SettingsDialog**

`src/app/SettingsDialog.h`：

```cpp
#pragma once
#include <QDialog>
#include "core/Settings.h"

class QFontComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;

namespace reader {

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(Settings *settings, QWidget *parent = nullptr);

private slots:
    void pickBackgroundColor();
    void pickTextColor();
    void accept() override;

private:
    Settings *m_settings;
    QFontComboBox *m_fontCombo;
    QSpinBox *m_sizeSpin;
    QSpinBox *m_lineGapSpin;
    QCheckBox *m_firstLineIndent;
    QPushButton *m_bgButton;
    QPushButton *m_textButton;
};

}
```

`src/app/SettingsDialog.cpp`：

```cpp
#include "app/SettingsDialog.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace reader {

SettingsDialog::SettingsDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("显示设置"));
    m_fontCombo = new QFontComboBox(this);
    m_fontCombo->setCurrentFont(m_settings->display.font);
    m_sizeSpin = new QSpinBox(this);
    m_sizeSpin->setRange(6, 72);
    m_sizeSpin->setValue(m_settings->display.font.pointSize());
    m_lineGapSpin = new QSpinBox(this);
    m_lineGapSpin->setRange(0, 40);
    m_lineGapSpin->setValue(m_settings->display.lineGap);
    m_firstLineIndent = new QCheckBox(QStringLiteral("首行缩进"), this);
    m_firstLineIndent->setChecked(m_settings->display.firstLineIndent);
    m_bgButton = new QPushButton(this);
    m_textButton = new QPushButton(this);
    const auto setColor = [](QPushButton *b, const QColor &c) {
        QPixmap pm(24, 24);
        pm.fill(c);
        b->setIcon(QIcon(pm));
        b->setText(c.name());
    };
    setColor(m_bgButton, m_settings->display.bgColor);
    setColor(m_textButton, m_settings->display.textColor);
    connect(m_bgButton, &QPushButton::clicked, this, &SettingsDialog::pickBackgroundColor);
    connect(m_textButton, &QPushButton::clicked, this, &SettingsDialog::pickTextColor);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("字体"), m_fontCombo);
    form->addRow(QStringLiteral("字号"), m_sizeSpin);
    form->addRow(QStringLiteral("行距"), m_lineGapSpin);
    form->addRow(QStringLiteral("背景色"), m_bgButton);
    form->addRow(QStringLiteral("文字颜色"), m_textButton);
    form->addRow(QString(), m_firstLineIndent);

    auto *ok = new QPushButton(QStringLiteral("确定"), this);
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    connect(ok, &QPushButton::clicked, this, &SettingsDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttons);
}

void SettingsDialog::pickBackgroundColor()
{
    const QColor c = QColorDialog::getColor(m_settings->display.bgColor, this, QStringLiteral("选择背景色"));
    if (c.isValid()) {
        m_settings->display.bgColor = c;
        QPixmap pm(24, 24);
        pm.fill(c);
        m_bgButton->setIcon(QIcon(pm));
        m_bgButton->setText(c.name());
    }
}

void SettingsDialog::pickTextColor()
{
    const QColor c = QColorDialog::getColor(m_settings->display.textColor, this, QStringLiteral("选择文字颜色"));
    if (c.isValid()) {
        m_settings->display.textColor = c;
        QPixmap pm(24, 24);
        pm.fill(c);
        m_textButton->setIcon(QIcon(pm));
        m_textButton->setText(c.name());
    }
}

void SettingsDialog::accept()
{
    m_settings->display.font.setFamily(m_fontCombo->currentFont().family());
    m_settings->display.font.setPointSize(m_sizeSpin->value());
    m_settings->display.lineGap = m_lineGapSpin->value();
    m_settings->display.firstLineIndent = m_firstLineIndent->isChecked();
    m_settings->save();
    QDialog::accept();
}

}
```

- [ ] **Step 4: 扩展 MainWindow**

`src/app/MainWindow.h`（完整替换）：

```cpp
#pragma once
#include <QMainWindow>
#include <memory>
#include "core/Book.h"
#include "core/Cache.h"
#include "core/Settings.h"

class QTreeWidget;

namespace reader {

class ReadingView;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void openBook(const QString &path);
    QString currentBookTitle() const;
    int tocItemCount() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onChapterChanged(int index);
    void onPageChanged(int index);

private:
    void buildMenus();
    void populateToc();
    void updateTitle();
    void saveProgress();
    ReadingView *m_view = nullptr;
    QTreeWidget *m_toc = nullptr;
    Cache m_cache;
    Settings m_settings;
    std::shared_ptr<Book> m_book;
    QString m_currentPath;
};

}
```

`src/app/MainWindow.cpp`（完整替换）：

```cpp
#include "app/MainWindow.h"
#include "app/ReadingView.h"
#include "app/SettingsDialog.h"
#include <QCloseEvent>
#include <QDateTime>
#include <QDockWidget>
#include <QFileDialog>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QTreeWidget>

namespace reader {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_cache.load();
    m_settings.load();
    m_view = new ReadingView(this);
    m_view->setObjectName(QStringLiteral("readingView"));
    m_view->setSettings(m_settings.display);
    setCentralWidget(m_view);
    connect(m_view, &ReadingView::chapterChanged, this, &MainWindow::onChapterChanged);
    connect(m_view, &ReadingView::pageChanged, this, &MainWindow::onPageChanged);

    auto *dock = new QDockWidget(QStringLiteral("目录"), this);
    dock->setObjectName(QStringLiteral("tocDock"));
    m_toc = new QTreeWidget(dock);
    m_toc->setHeaderHidden(true);
    dock->setWidget(m_toc);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    connect(m_toc, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        m_view->goToChapter(item->data(0, Qt::UserRole).toInt());
    });

    buildMenus();
    updateTitle();
    resize(960, 720);
}

void MainWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    QAction *open = file->addAction(QStringLiteral("打开(&O)"));
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("打开"), QString(),
            QStringLiteral("书籍文件 (*.txt);;所有文件 (*)"));
        if (!path.isEmpty())
            openBook(path);
    });
    QAction *clearRecent = file->addAction(QStringLiteral("清空(&C)"));
    clearRecent->setEnabled(false);
    clearRecent->setToolTip(QStringLiteral("第三阶段开放"));
    QAction *quit = file->addAction(QStringLiteral("退出(&X)"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    QMenu *tocMenu = menuBar()->addMenu(QStringLiteral("目录(&V)"));
    QAction *tocToggle = tocMenu->addAction(QStringLiteral("显示/隐藏目录"));
    connect(tocToggle, &QAction::triggered, this, [this] {
        if (QDockWidget *dock = findChild<QDockWidget *>(QStringLiteral("tocDock")))
            dock->setVisible(!dock->isVisible());
    });

    QMenu *bookmark = menuBar()->addMenu(QStringLiteral("书签(&M)"));
    QAction *bm = bookmark->addAction(QStringLiteral("添加书签"));
    bm->setEnabled(false);
    bm->setToolTip(QStringLiteral("第二阶段开放"));

    QMenu *settings = menuBar()->addMenu(QStringLiteral("设置(&S)"));
    QAction *display = settings->addAction(QStringLiteral("显示设置"));
    connect(display, &QAction::triggered, this, [this] {
        SettingsDialog dlg(&m_settings, this);
        if (dlg.exec() == QDialog::Accepted) {
            m_view->setSettings(m_settings.display);
            m_view->refreshLayout();
        }
    });
    settings->addAction(QStringLiteral("基本设置"))->setEnabled(false);
    settings->addAction(QStringLiteral("高级设置"))->setEnabled(false);
    settings->addAction(QStringLiteral("按键设置"))->setEnabled(false);
    settings->addAction(QStringLiteral("标签设置"))->setEnabled(false);
    settings->addSeparator();
    settings->addAction(QStringLiteral("还原默认设置"))->setEnabled(false);

    QMenu *help = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    QAction *about = help->addAction(QStringLiteral("关于 ..."));
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(this, QStringLiteral("关于"),
            QStringLiteral("Reader（CachyOS 原生版）\n\n"
                           "本地 TXT/EPUB/MOBI 阅读器，功能参考开源项目 binbyu/Reader。\n"
                           "本版本为第一阶段：TXT 阅读核心。"));
    });
}

void MainWindow::openBook(const QString &path)
{
    QString err;
    auto book = Book::create(path, &err);
    if (!book) {
        QMessageBox::warning(this, QStringLiteral("无法打开"), err);
        return;
    }
    saveProgress();
    m_book = std::move(book);
    m_currentPath = path;
    m_view->setBook(m_book);
    populateToc();
    if (const auto p = m_cache.progress(path)) {
        m_view->goToChapter(p->chapterIndex);
        m_view->goToPage(p->pageIndex);
    }
    updateTitle();
    m_view->setFocus();
}

void MainWindow::populateToc()
{
    m_toc->clear();
    if (!m_book)
        return;
    const QVector<Chapter> &chapters = m_book->chapters();
    for (int i = 0; i < chapters.size(); ++i) {
        auto *item = new QTreeWidgetItem(m_toc);
        item->setText(0, chapters.at(i).title);
        item->setData(0, Qt::UserRole, i);
    }
}

void MainWindow::onChapterChanged(int index)
{
    if (!m_book || index < 0 || index >= m_book->chapters().size())
        return;
    if (QTreeWidgetItem *item = m_toc->topLevelItem(index)) {
        m_toc->setCurrentItem(item);
        m_toc->scrollToItem(item);
    }
    updateTitle();
    saveProgress();
}

void MainWindow::onPageChanged(int)
{
    saveProgress();
}

void MainWindow::updateTitle()
{
    QString title;
    if (m_book && !m_book->chapters().isEmpty())
        title = m_book->chapters().at(m_view->currentChapter()).title;
    if (title.isEmpty())
        title = m_book ? m_book->title() : QStringLiteral("Reader");
    setWindowTitle(QStringLiteral("%1 - Reader").arg(title));
}

void MainWindow::saveProgress()
{
    if (m_currentPath.isEmpty())
        return;
    m_cache.upsertProgress({m_currentPath, m_view->currentChapter(), m_view->currentPage(),
                            QDateTime::currentSecsSinceEpoch()});
    m_cache.save();
}

QString MainWindow::currentBookTitle() const
{
    if (!m_book || m_book->chapters().isEmpty())
        return QString();
    return m_book->chapters().at(m_view->currentChapter()).title;
}

int MainWindow::tocItemCount() const
{
    return m_toc->topLevelItemCount();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveProgress();
    QMainWindow::closeEvent(event);
}

}
```

- [ ] **Step 5: 重新构建并运行全部测试**

Run: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS（含 `tst_mainwindow` 两个用例）

- [ ] **Step 6: 手工验证清单（有显示环境时）**

Run: `./build/reader`

逐项检查：
1. 菜单与设计一致：文件(打开/退出)、目录、书签(禁用)、设置(显示设置可用，其余禁用)、帮助(关于)
2. 打开一个 GBK/GB2312 编码 TXT，正文不乱码；再打开 UTF-8 TXT
3. 目录树列出章节；点击目录跳转；窗口标题显示章节名
4. 鼠标左键下一页、右键上一页；滚轮翻页；←/→ 翻页；空格下一页
5. 设置→显示设置：改字体/字号/行距/背景色/文字颜色，立即生效并保存
6. 关闭程序重新打开同一本书，恢复到上次章节与页码
7. 调整窗口大小，当前阅读进度大致保持
8. 打开 epub/mobi 文件时提示"暂不支持该格式"

- [ ] **Step 7: 提交**

```bash
git add CMakeLists.txt src/app/SettingsDialog.h src/app/SettingsDialog.cpp src/app/MainWindow.h src/app/MainWindow.cpp tests/tst_mainwindow.cpp
git commit -m "feat: 主窗口集成（菜单/目录/设置/进度记忆）"
```

---

## 自审记录

- 规范覆盖：设计文档 8 节第一阶段全部条目均有对应任务（骨架=Task1、编码=Task2、目录=Task3、TXT 加载=Task4、分页=Task5、设置=Task6、进度=Task7、阅读视图=Task8、主窗口集成=Task9）。
- 占位符扫描：无 TBD/TODO；所有代码步骤含完整代码。
- 类型一致性：`BookProgress`、`DisplaySettings`、`PageLayoutParams`、`Chapter` 等类型名在任务间一致；`ReadingView::goToPage/goToChapter`、`MainWindow::openBook/tocItemCount/currentBookTitle` 签名在 Task 8/9 与测试中一致。
- 已知取舍：第一阶段滚轮/方向键按整页翻页；逐行滚动、段距对话框、书签、搜索等在第二/三阶段计划中实现（符合设计文档 8 节阶段划分）。
