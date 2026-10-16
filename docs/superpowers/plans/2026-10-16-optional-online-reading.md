# Optional Online Reading Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Provide an optional downloadable Chinese novel engine, configurable sources, and reading with three chapters prefetched.

**Architecture:** The base Qt application owns installation, settings, cached books and reading navigation. An optional `reader-online` process owns HTTP and HTML/XPath parsing and communicates using bounded newline JSON. Disk manifests let existing progress and bookmarks work offline.

**Tech Stack:** C++17, Qt6 Widgets/Network/Test/Concurrent, libarchive; libxml2 only in the optional engine; Python3 release tooling.

**Spec:** `docs/superpowers/specs/2026-10-16-online-novel-reading-design.md`

## Global Constraints

- Basic application builds without libxml2, optional engine option defaults OFF.
- Current chapter first, then exactly the next 3 existing chapters; no full-book fetching.
- No network on startup or cancelled installation when component is absent/disabled.
- Default timeout 20 seconds, response limit 8 MiB, request interval 1 second with minimum 0.5 seconds.
- Content pagination maximum 20 pages; TOC maximum 100 pages and 20,000 unique chapters.
- Only official HTTPS release URLs can supply executable packages; custom sources supply content only.
- Source/cache/manifest writes atomic; credentials private and never logged or passed in argv.
- Preserve all pre-existing dirty edits. Commit only new feature files or precisely staged new feature hunks. Never commit a whole previously dirty file.
- Build reviewable local artifacts; do not publish assets remotely.

## Protocol and file map

`src/online/engine/` contains the optional executable, pure parser and asynchronous source client. `src/core/online/` contains source store, book/cache, process bridge, component manager and scheduling. `src/app/online/` contains the library, source and installation UI. `cmake/OnlineEngine.cmake` and `cmake/OnlineHost.cmake` register targets without duplicating the existing application source lists. `packaging/build-online-component.py` produces independent archives and immutable release metadata.

Wire messages are single JSON objects per line, UTF-8, maximum 16 MiB per message. Request schema:

```json
{"protocol":1,"id":"17","session":"book-1","op":"search","source":{"id":"wikisource-zh","kind":"mediawiki","name":"中文维基文库","baseUrl":"https://zh.wikisource.org"},"keyword":"红楼梦","page":1,"url":""}
```

Operations: `hello`, `search`, `toc`, `chapter`, `cancel`, `quit`. A cancel message contains `target` request ID. Reply echoes `protocol`, `id`, `session`, has `ok` boolean and either `result` object or sanitized `error` string. Hello result: `{"protocol":1,"version":"0.1.0"}`. Search result: `{"books":[{"title":"…","url":"https://…","author":"…"}],"hasMore":false}`. TOC result: `{"title":"…","url":"…","chapters":[{"title":"第一章","url":"https://…"}],"choices":[]}`; `choices` uses the same title/url model when a version must be chosen. Chapter result: `{"title":"…","url":"…","text":"paragraph\n\nparagraph"}`. Unknown/malformed operations fail without executing anything.

HTML source fields (all JSON strings unless specified): `id`, `kind=html`, `name`, `baseUrl`, `searchUrl`, `searchMethod=GET|POST`, `searchBody`, `resultXPath`, `titleXPath`, `linkXPath`, `authorXPath`, `tocUrlXPath`, `chapterXPath`, `contentXPath`, `removeXPath`, `tocNextXPath`, `contentNextXPath`, `encoding=auto|UTF-8|GB18030`, `cookie`, `headers` object, `intervalMs` integer, `reverseToc` bool, `enabled` bool. Missing search URL permits direct URL import. Relative field XPath applies inside each search item; links resolve against the final response URL. Preset is immutable but copyable.

### Task 1: Optional source engine

**Files:** Create `src/online/engine/{main.cpp,HtmlRuleParser.h,HtmlRuleParser.cpp,NovelSourceClient.h,NovelSourceClient.cpp}`, `cmake/OnlineEngine.cmake`, `tests/tst_onlineengine.cpp` and fixture files under `tests/online/`. Append one include to `CMakeLists.txt`.

**Interfaces:** Consumes the protocol and source schema above. Produces `reader-online` with the protocol above and a standalone optional-engine test target. No host API dependency.

- [ ] **Step 1: Write failing tests for real extraction and network behavior.** A Qt local TCP server serves search results with `href='../book/1'`, a two-page TOC including a repeated chapter, and chapter HTML containing `<p>甲&amp;乙</p><script>bad()</script><p>丙<br>丁</p>`. Assert final absolute URLs, ordered deduplication, `甲&乙` and separate paragraphs, no `bad()`. Invalid XPath and a 200 browser challenge must fail. Cancellation must not emit a success, cyclic pagination must fail, 8 MiB excess and 404 must return distinguishable errors. Handshake and malformed protocol tested via actual QProcess.

```cpp
QVERIFY(!text.contains("bad()"));
QVERIFY(text.contains(QString::fromUtf8("甲&乙")));
QCOMPARE(chapters.size(), 2);
```

- [ ] **Step 2: Configure with `-DREADER_BUILD_ONLINE_ENGINE=ON` and run the test target; observe missing implementation failure.** Record command and failure in report.
- [ ] **Step 3: Implement parser and engine.** Use libxml2 HTML parser with NONET and XPath; delete script/style and removal nodes, preserve paragraph and br boundaries. Validate XPath and URL input. Use QtNetwork asynchronously, bounded redirects, no secrets across origins, timeouts, body limit, per-source pacing and at most two temporary network retries. Pagination tracks URLs and enforces limits. Parse off main event loop with QtConcurrent. MediaWiki adapter uses `action=query&list=search` and `action=parse&prop=text`; robustly identifies chapter links versus edition choices, strips navigation and tracks attribution URL. Decode charset including GB18030. Engine stdin QSocketNotifier accumulates bounded lines; stdout only protocol, exit on stdin EOF. Cancel aborts replies and ignores pending parse completions.

```cmake
option(READER_BUILD_ONLINE_ENGINE "Build optional online reading component" OFF)
if(READER_BUILD_ONLINE_ENGINE)
  find_package(LibXml2 REQUIRED)
  find_package(Qt6 REQUIRED COMPONENTS Concurrent)
  # engine target links only Qt Core/Network/Concurrent and LibXml2
endif()
```

- [ ] **Step 4: Run engine tests until green and perform live Wikisource smoke for two works.** Real search→TOC/choices→chapter; record network failures honestly. Keep live smoke opt-in, outside CTest.
- [ ] **Step 5: Self-review and commit only newly created task files.** Leave the previously dirty `CMakeLists.txt` hunk unstaged for controller isolation. Report commands/results and exact protocol additions if needed.

### Task 2: Base host, component installation and reader UI

**Files:** Create `src/core/online/` and `src/app/online/` classes named in spec, `cmake/OnlineHost.cmake`, `tests/tst_onlinehost.cpp`, `tests/tst_onlinereading.cpp`, `tests/tst_onlinecomponent.cpp`; modify `src/core/Book.{h,cpp}`, `src/app/ReadingView.{h,cpp}`, `src/app/MainWindow.{h,cpp}`, `CMakeLists.txt` incrementally.

**Interfaces:** Consumes Task 1 process protocol and source schema. Produces cached `.rnovel` books, an “在线阅读” action, configurable library/source dialog, prefetch controller, and safe component install/import API for Task 3 artifacts. Release metadata schema uses `version`, `protocol`, `appVersion`, `platform=linux`, `arch=x86_64`, `url`, `size`, `unpackedSize`, `sha256`, `entrypoint=reader-online`.

- [ ] **Step 1: Write failing behavior tests.** With a controlled protocol process/local server, request chapter 10 of a 20 chapter book, assert actual network/content requests are precisely 10,11,12,13; move11 adds14. Jump3, then switch book and deliver stale response: active book/position stay unchanged. Missing chapter never changes committed progress, and cached reopen makes no fetch. Source save/load rejects corrupt JSON without overwrite and secrets mode is 0600. Installer rejects wrong hash, traversal/symlink archive, oversized files, wrong protocol/platform; successful packaged real engine handshake activates and uninstall preserves cache. Cancellation leaves no activation. Test actual ReadingView next-chapter waiting and offline error recovery.

```cpp
QCOMPARE(requested, (QList<int>{10, 11, 12, 13}));
view.goToChapter(5); // absent: leave old committed chapter until ready
QCOMPARE(view.currentChapter(), 0);
QVERIFY(QFileInfo::exists(savedChapterPath)); // after uninstall
```

- [ ] **Step 2: Run each new target before implementation and record expected failures.** Use `QT_QPA_PLATFORM=offscreen` for Widgets tests.
- [ ] **Step 3: Implement models, bridge, installer and controller.** Book availability is a virtual method default true for local formats; cached OnlineBook getters never fetch. Stable source+URL hash manifest and per-chapter URL hashes, atomic writes and integrity validation. Bridge invokes exact managed executable, performs hello before queued operations, bounded messages, session/request IDs and crash errors. Installer only begins download from explicit UI acceptance; no network constructor; validates metadata/package limits/types before extraction, starts hello with timeout and atomically writes activation only after success. Keep active version on failure. Import local archive with chosen metadata supports development; unavailable metadata shows disabled download with explanation. Installed disabled state enables without download; component management displays version/size, disable/uninstall stop bridge and preserve content.

  Library uses source selector, search/paging, direct URL, results and TOC/version selection, open cached books, refresh TOC with stable position mapping. Source dialog groups exact schema fields, copy preset, CRUD, validation and stepwise search/TOC/chapter preview without prefetch; private credentials and test errors. Register all host widget sources on reader and MainWindow test targets via cmake helper. Match existing Qt widget style. MainWindow adds online menu and connects chosen manifest to existing openBook/recent/bookmarks. ReadingView requests absent targets, shows separate loading/error/cancel/retry UI, retains committed chapter/progress, resumes paging after ready, target-based rapid navigation. Estimated progress uses TOC, cached-only book search, forbid TXT editing. Controller serializes requests and current-target priority, resets three-chapter queue on jumps and book/session changes.

- [ ] **Step 4: Run host, reader and installer tests and existing CTest suite.** Verify base OFF configure never calls find_package LibXml2, base reader runtime doesn't depend on it; identify pre-existing failures separately from regressions.
- [ ] **Step 5: Self-review; commit only new task files and precisely isolated feature patches.** Never use `git add .`, `git commit -a` or whole-file staging of earlier dirty files. Report screenshot/manual UI route and limitations honestly.

### Task 3: Standalone artifacts, integration validation and docs

**Files:** Create `packaging/build-online-component.py`, release-metadata CMake/header integration, `tests/test_online_packaging.py`, optional live smoke utility; modify `packaging/build-release.sh`, `README.md` and relevant install descriptions incrementally.

**Interfaces:** Consumes Task 1 built engine, Task 2 metadata/install schema. Produces `reader-online-<version>-linux-x86_64.tar.gz` with helper/private new dependencies/licenses and metadata JSON with real SHA256/size/limits. Source builds without supplied metadata remain explicit “未配置组件下载”; release builds pass metadata with `-DREADER_ONLINE_METADATA=<absolute-file>` so base embeds it.

- [ ] **Step 1: Write failing packaging integration tests.** Run the Python tool against a controlled executable and dependencies, inspect archive no system absolute paths, metadata SHA256/length match actual bytes and entrypoint is executable; malformed/non-HTTPS public URL exits nonzero. Round-trip real artifact through host installer in isolated XDG directory. Confirm base installer omits helper and libxml2.

```python
assert metadata["size"] == archive.stat().st_size
assert metadata["sha256"] == hashlib.sha256(archive.read_bytes()).hexdigest()
assert all(not p.startswith("/") and ".." not in p.split("/") for p in members)
```

- [ ] **Step 2: Run `python -m unittest discover -s tests -p test_online_packaging.py` and observe expected missing tool failure.**
- [ ] **Step 3: Implement package generator and release integration.** Copy libxml2 and new non-base transitive libraries based on actual ELF dependencies, set origin-relative runtime path, include licenses, make archive/metadata deterministic where possible, refuse unsupported arch or fake remote URLs. Build component before base release then pass generated metadata, keep engine out of base PKGBUILD/install. Without a user-specified real release URL support a local artifact/import manifest, mark unavailable download instead of inventing a release asset URL. Update README with installed/absent/disabled paths, custom XPath fields/test steps, three-chapter behavior, cached offline reading, build OFF/ON commands and artifact generation/import commands.

- [ ] **Step 4: Build real base and engine, run all CTest tests, packaging tests and two-work live smoke.** Use temp XDG dirs, actually import local package and run protocol. Capture GUI library/source/loading/error screenshots where GUI access is available; retain useful evidence and list inaccessible checks explicitly.
- [ ] **Step 5: Review task and feature; save test evidence and artifacts.** Commit only feature changes. Final user answer says implemented behavior, tests and the fact remote publishing is still required to give public releases real one-click downloads.
