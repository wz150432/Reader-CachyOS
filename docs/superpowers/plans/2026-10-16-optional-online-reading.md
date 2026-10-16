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

### Task 2: Cached books, source settings, process bridge and reading controller

**Files:** Create `src/core/online/{BookSource,BookSourceStore,OnlineBook,OnlineEngineBridge,OnlineReadingController}.{h,cpp}`, `cmake/OnlineHost.cmake`, `tests/tst_onlinehost.cpp`, `tests/tst_onlinereading.cpp`; modify `src/core/Book.{h,cpp}`, `src/app/ReadingView.{h,cpp}`, `CMakeLists.txt` incrementally. Do not add MainWindow/UI installer yet.

**Interfaces:** Consumes Task 1 protocol/source schema. Produces source model/store with read/write JSON, `OnlineBook` implementing Book (with manifest creation/body save/cache existence/source snapshot), `OnlineEngineBridge` hello/request/cancel/result/error and an explicitly supplied executable path, `OnlineReadingController` book/target/session/queue/current-ready/background-status. Report exact public signatures for next task. ReadingView exposes missing chapter request/retry/cancel signals, pending target/readiness/error setters, and committed readiness query. Book availability defaults true for local books and `.rnovel` factory returns cache-only OnlineBook.

- [ ] **Step 1: Write failing behavior tests.** With an actual protocol fixture process, request chapter 10 of a 20-chapter book and assert emitted requests are precisely 10,11,12,13; move11 adds14. Jump3 and deliver stale reply after switching books: new book/position unchanged. Use cache files in temporary directory; cached reopen must not fetch. Malformed/truncated chapter JSON fails cache lookup. Source store corruption must return error without overwriting input, file permissions 0600. Chapter URLs with unsafe titles still write hashed paths. Test bridge malformed/over-limit JSON/crash/cancel/timeout. ReadingView leaves old committed position on missing chapter, rapid navigation follows pending target, pixel scrolling cannot spin or skip, automatic paging resumes only after readiness, and whole-book search only sees cached text.

```cpp
QCOMPARE(requested, (QList<int>{10, 11, 12, 13}));
view.goToChapter(5); // missing; old readable position stays committed
QCOMPARE(view.currentChapter(), 0);
QVERIFY(!book.chapterAvailable(5));
```

- [ ] **Step 2: Run new targets before implementation and record expected failures.** Use `QT_QPA_PLATFORM=offscreen` for Widgets tests.
- [ ] **Step 3: Implement core and incremental reader support.** Stable source+URL hash manifest and per-chapter URL hashes, bounded atomic storage with integrity validation, lazy per-chapter reads. Source snapshot preserved; edit/delete source cannot invalidate cached books. Sources validated (URL, required rules, method, encoding, headers, intervals), preset immutable/copyable, search optional. No host HTML parsing. Bridge invokes exact supplied executable, hello before queued operations, bounded protocol and IDs, no secrets in argv or error output. Controller serializes foreground and 3-ahead requests, promotes in-flight target, resets window on jumps, stale session/ID ignored and stops on switch/close. ReadingView caches/commits only ready chapters, displays separate loading/error/cancel/retry state and retains saved position while waiting; loading text never goes through Page. Preserve all local-format behavior. Unknown total char length uses chapter estimate for online progress; search skips unavailable chapters. Handle all entry points: setBook, goToChapter/page, pageUp/down, scrollByPixels, findNext, jumpToBookProgress and autopaging. Introduce an online identity/readiness Book virtual rather than depending on concrete class in ReadingView.

- [ ] **Step 4: Run focused host/navigation tests and all existing tests.** Confirm no libxml2 in base host linkage. Missing executable must fail asynchronously and permit retry after path is set. Export exact API integration notes.
- [ ] **Step 5: Self-review and commit new files only.** Keep feature deltas to previously dirty files unstaged; controller will isolate against baseline. Full report includes RED/GREEN and public APIs for Task 3.

### Task 3: Optional component installation, online library and source forms

**Files:** Create `src/core/online/OnlineComponentManager.{h,cpp}`, `src/app/online/{OnlineLibraryDialog,BookSourceDialog,OnlineComponentDialog}.{h,cpp}`, `tests/tst_onlinecomponent.cpp`, `tests/tst_onlinedialogs.cpp`; extend `cmake/OnlineHost.cmake`, modify `src/app/MainWindow.{h,cpp}` and `CMakeLists.txt` incrementally.

**Interfaces:** Consumes Task 2 source/book/bridge/controller/ReadingView APIs. Metadata JSON: `version`, `protocol`, `appVersion`, `platform=linux`, `arch=x86_64`, `url`, `size`, `unpackedSize`, `sha256`, `entrypoint=reader-online`. Produces safe install/import manager, embedded metadata support with CMake `READER_ONLINE_METADATA` absolute path, online menu, library and source forms. Absent metadata permits explicit user-selected local archive+metadata import, remote download unavailable.

- [ ] **Step 1: Write failing behavior tests.** Installer rejects wrong hash, traversal/symlink archive, oversized files/counts, wrong protocol/platform/app; valid real helper package handshake activates. Cancellation leaves no activation and old good version survives failures. Uninstall preserves source/cache. GUI tests click online menu with absent metadata, cancel confirmation and assert bridge not running/no download; installed component opens library directly; UI source save validation and selected one-chapter test use actual controlled process. Missing chapter resume must not overwrite old saved progress on startup.

```cpp
QVERIFY(!manager.isInstalled());
QVERIFY(!manager.importPackage(unsafeArchive, metadata, &error));
QVERIFY(!manager.isInstalled());
QVERIFY(QFileInfo::exists(savedChapterPath)); // after uninstall
```

- [ ] **Step 2: Run focused new targets and capture RED before implementation.**
- [ ] **Step 3: Implement installer and Qt Widgets.** Installer never starts network from constructor; download only from explicit UI acceptance. Display real bundled version/size/location/HTTPS source, progress/cancel, single task; enforce exact bytes/hash, bounded archive total/counts and safe regular files/directories, reject absolute/traversal/symlink/devices. Install staging within component root, activate atomically only after asynchronous hello/timeout, retain prior version; exact managed executable path. Enabled/disabled state/version/size, disable/uninstall stop active bridge and requests and keep online data; cached reading remains available. Metadata compatibility enforced before download and on installed reuse. No fake URLs: absent release metadata shows “未配置组件下载” and import option. Local import is explicit action and validates chosen metadata/archive; remote URL can never come from a source.

  Library UI: source selector, search/paging, direct URL, result and TOC/version selection, cached books, manual refresh TOC with stable progress mapping. Source UI: full schema form grouped basic/search/advanced, CRUD/copy preset, enabled switch, inline validation, XPath examples, headers/Cookie protected, stepwise search/TOC/one-chapter preview with no prefetch; no-search source uses URL import. Existing Qt Widgets identity, useful empty/error/loading states and native shortcuts. Read impeccable new-work/craft-floor instructions before UI edits.

  MainWindow owns manager/store/bridge/controller lifetime; “在线阅读” file-menu action; online book manifest uses existing openBook/recent/bookmarks. Bind controller to view missing/ready/error and current chapter, suppress progress saves until real content committed, restore saved chapter/page/pixel only after ready. Source deletion/edit doesn't invalidate existing source snapshot. Online cannot enter TXT edit; progress label says “按章节估算”, search says “已缓存章节”. Component disabled/missing cached book shows explicit enable instruction. Stop helper when no library/online session remains. Register all widget sources on reader and existing MainWindow test targets via CMake helper.

- [ ] **Step 4: Run installer, dialog and MainWindow tests plus full CTest.** Verify default base OFF never finds LibXml2 and runtime has no dependency on it. Use offscreen widgets tests and capture representative forms/installation/loading/error screenshots. Record exact manager/metadata APIs for packaging.
- [ ] **Step 5: Self-review, commit new files only or feature-only isolated patches.** Preserve pre-existing edits. Report actual UI route, tests and any limitations.

### Task 4: Standalone artifacts, integration validation and docs

**Files:** Create `packaging/build-online-component.py`, release-metadata CMake/header integration, `tests/test_online_packaging.py`, optional live smoke utility; modify `packaging/build-release.sh`, `README.md` and relevant install descriptions incrementally.

**Interfaces:** Consumes Task 1 built engine, Task 3 metadata/install schema. Produces `reader-online-<version>-linux-x86_64.tar.gz` with helper/private new dependencies/licenses and metadata JSON with real SHA256/size/limits. Source builds without supplied metadata remain explicit “未配置组件下载”; release builds pass metadata with `-DREADER_ONLINE_METADATA=<absolute-file>` so base embeds it.

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
