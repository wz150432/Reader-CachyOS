# Task 1 — optional source engine

Implemented optional `reader-online`, independent parser/client library, process-level Qt test target, deterministic local HTTP fixtures, and opt-in Wikisource smoke. `cmake/OnlineEngine.cmake` defaults OFF and only discovers libxml2 and Qt Concurrent when ON. The existing CMakeLists.txt include hunk is intentionally left unstaged for controller isolation.

## Wire contract and implementation

UTF-8 newline-delimited JSON with 16 MiB request/reply limit; protocol=1, string ID/session, hello version 0.1.0, search, toc, chapter, cancel target ID, quit. Replies echo ID/session and contain ok plus result or fixed sanitized error. Cancellation acknowledges the cancel request, emits no target success, aborts active replies, and ignores delayed parse callbacks. Malformed JSON/unknown operation fail; missing-ID quit no longer bypasses validation. stdin EOF exits. No protocol additions.

HTML uses libxml2 NONET XPath, final-response URL resolution, relative item fields, script/style/nav and configured node removal, paragraph/br preservation, charset/GB18030 decoding, TOC indirection, ordered deduplication/reversal, cycle and pagination limits (20 content pages, 100 TOC pages, 20,000 chapters). HTTP uses asynchronous QtNetwork, 20-second deadline, 8 MiB response bound, 1-second default / 500-ms minimum pacing, five redirects maximum, HTTPS downgrade rejection, origin-gated headers/cookies, disabled automatic cookie persistence, and two temporary retries maximum. Parsing runs with QtConcurrent. MediaWiki uses query/search and parse/text, separates likely chapters and editions, removes common navigation and retains attribution URLs.

Current-plus-three scheduling and executable installation trust belong to later host tasks; this standalone engine only fetches explicitly requested search/TOC/chapter pages and their bounded pagination. It does no startup networking.

## RED / GREEN evidence

The interrupted implementation and tests already existed on resumption. No original missing-implementation RED log was located; I cannot claim to have observed Step 2's initial configure/build failure. Existing fixture RED observed with `cmake --build build-online-engine --target tst_onlineengine -j3 && ctest --test-dir build-online-engine -R '^onlineengine$' --output-on-failure`: 11 passed, 2 failed — wikiEditions returned two choices instead of one; wikiNavigation retained 下一回. Added navigation removal and confirmed green.

Added actual-process malformedQuit regression, then ran `build-online-engine/tst_onlineengine malformedQuit`: failed because missing-ID quit returned ok=true. Required a nonempty ID before handling quit; final full suite passes.

After live smoke exposed 下一回▶, changed the deterministic fixture to the decorated label and ran `build-online-engine/tst_onlineengine wikiNavigation`: failed on retained navigation. Normalized common arrow decorations before matching navigation labels; final full suite passes.

Also added fractional-version rejection and real cross-origin redirect tests (127.0.0.1 → localhost), proving final response URL attribution and absence of custom Authorization/cookie secrets on the redirected request. These passed against existing behavior; no production change was made for them.

## Verification

- `cmake --build build-online-engine -j3`: full configured application/engine/test build succeeded.
- `ctest --test-dir build-online-engine -R '^onlineengine$' --output-on-failure`: passed, ~8.3 seconds. Tests exercise real QProcess and local TCP responses: handshake, malformed/unknown operations, relative URL search, paginated TOC deduplication, paragraphs/entities/script exclusion, browser challenge, invalid XPath, cycle, >8 MiB, 404 distinction, HTTP charset, TOC indirection, editions/navigation, cancellation, EOF, strict malformed quit, and redirect credentials.
- `cmake -S . -B /tmp/reader-engine-default-off -DCMAKE_DISABLE_FIND_PACKAGE_LibXml2=TRUE`: succeeded; cache confirms engine OFF and LibXml2 disable variable unused (no discovery). This verifies configuration independence; no fresh OFF application rebuild was performed.
- `git diff --check`: clean.
- `python3 tests/online/live-smoke.py build-online-engine/reader-online`: real search → TOC → first chapter succeeded for 红楼梦 (120 chapters; 8,253 chars) and 三国演义 (120 chapters; 4,812 chars). Opt-in, outside CTest. Fresh final run is recorded below after completion.

## Self-review / remaining concerns

MediaWiki link classification is heuristic: parent-title-prefixed links with Chinese chapter markers or English Chapter become chapters; other matches become edition choices. Works with unrelated chapter page names, numeric-only labels or unusual navigation may need explicit rules. Common navigation links/classes are stripped, but residual directory/title text and unrecognized navigation variants can remain. Only two live works were smoke-tested. HTML search hasMore is false because the source schema has no search-next selector; callers may request explicit page numbers. Redirect secret isolation is tested; timeout duration, retry count, and all pagination ceilings are implemented but not separately stress-tested. POSIX stdin notifier implementation targets this Linux application; Windows portability is unverified. XML diagnostic errors may go to stderr, never protocol stdout. No original RED evidence was invented.

Only task-owned new files are staged/committed. All pre-existing dirty application, packaging, README and CMakeLists.txt changes remain outside the commit.

Final verification after all fixes: full `cmake --build build-online-engine -j3` succeeded; focused CTest passed in 8.27 seconds. Final live smoke again succeeded for both works: 红楼梦 120 chapters / 8,253 characters and 三国演义 120 chapters / 4,800 characters (decorated previous/next navigation removed). Final smoke log: `/tmp/reader-task1-live-final.log`; build log: `/tmp/reader-task1-build.log` (temporary evidence, not shipped artifacts).

## Review fix round 1 — redirect methods and complete envelope

Verified both Important findings against current code: redirects only POSTed when redirect count was zero; session was neither validated by submit nor by the quit fast path. Added tests before changing production code.

Local TCP regressions send actual form POSTs through 301/302/303/307/308 redirects. They assert final method and unchanged `keyword=test&page=2` for 307/308, and GET without body for 301/302/303. Twelve process regressions cover missing/numeric/object session on hello/chapter/cancel/quit, assert rejection and no HTTP request. Existing cancellation fixture now supplies the required session.

Implementation carries method state across redirects and retries; 307/308 preserve POST/body and 301/302/303 convert POST to GET. The same complete-envelope validator gates submit and the quit fast path: numeric protocol exactly 1, nonempty string ID, string session. Invalid requests cannot remove an active job sharing their ID. Existing origin checks for headers/cookies are unchanged and their regression still passes. Self-review confirmed pagination starts fresh GET requests and retry preserves the current redirected method.

RED command (exit 14):
```sh
cmake --build build-online-engine --target tst_onlineengine -j3 > /tmp/reader-task1-round1-build-red.log 2>&1 && build-online-engine/tst_onlineengine postRedirects envelope > /tmp/reader-task1-round1-red.log 2>&1
```
Exact RED test output:
```text
********* Start testing of EngineTest *********
Config: Using QtTest library 6.12.0, Qt 6.12.0 (x86_64-little_endian-lp64 shared (dynamic) release build; by GCC 16.2.1 20260810), cachyos unknown
PASS   : EngineTest::initTestCase()
PASS   : EngineTest::postRedirects(301)
PASS   : EngineTest::postRedirects(302)
PASS   : EngineTest::postRedirects(303)
FAIL!  : EngineTest::postRedirects(307) 'lastRequest.startsWith("POST /search ")' returned FALSE. (GET /search HTTP/1.1?
Host: 127.0.0.1:38053?
User-Agent: ReaderCachyOS/0.1 (https://github.com/wz150432/Reader-CachyOS)?
Connection: Keep-Alive?
Accept-Encoding: zstd, br, gzip, deflate?
Accept-Language: en,*?
?
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(171)]
FAIL!  : EngineTest::postRedirects(308) 'lastRequest.startsWith("POST /search ")' returned FALSE. (GET /search HTTP/1.1?
Host: 127.0.0.1:38053?
User-Agent: ReaderCachyOS/0.1 (https://github.com/wz150432/Reader-CachyOS)?
Connection: Keep-Alive?
Accept-Encoding: zstd, br, gzip, deflate?
Accept-Language: en,*?
?
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(171)]
FAIL!  : EngineTest::envelope(hello-missing) 'rejected' returned FALSE. ({"id":"bad","ok":true,"protocol":1,"result":{"protocol":1,"version":"0.1.0"},"session":""}
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(211)]
FAIL!  : EngineTest::envelope(hello-number) 'rejected' returned FALSE. ({"id":"bad","ok":true,"protocol":1,"result":{"protocol":1,"version":"0.1.0"},"session":""}
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(211)]
FAIL!  : EngineTest::envelope(hello-object) 'rejected' returned FALSE. ({"id":"bad","ok":true,"protocol":1,"result":{"protocol":1,"version":"0.1.0"},"session":""}
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(211)]
FAIL!  : EngineTest::envelope(chapter-missing) 'rejected' returned FALSE. ({"id":"bad","ok":true,"protocol":1,"result":{"text":"甲&乙\n\n丙\n丁","title":"","url":"http://127.0.0.1:38053/c1"},"session":""}
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(211)]
FAIL!  : EngineTest::envelope(chapter-number) 'rejected' returned FALSE. ({"id":"bad","ok":true,"protocol":1,"result":{"text":"甲&乙\n\n丙\n丁","title":"","url":"http://127.0.0.1:38053/c1"},"session":""}
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(211)]
FAIL!  : EngineTest::envelope(chapter-object) 'rejected' returned FALSE. ({"id":"bad","ok":true,"protocol":1,"result":{"text":"甲&乙\n\n丙\n丁","title":"","url":"http://127.0.0.1:38053/c1"},"session":""}
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(211)]
FAIL!  : EngineTest::envelope(cancel-missing) 'rejected' returned FALSE. ({"id":"bad","ok":true,"protocol":1,"result":{},"session":""}
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(211)]
FAIL!  : EngineTest::envelope(cancel-number) 'rejected' returned FALSE. ({"id":"bad","ok":true,"protocol":1,"result":{},"session":""}
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(211)]
FAIL!  : EngineTest::envelope(cancel-object) 'rejected' returned FALSE. ({"id":"bad","ok":true,"protocol":1,"result":{},"session":""}
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(211)]
FAIL!  : EngineTest::envelope(quit-missing) 'rejected' returned FALSE. ({"id":"bad","ok":true,"protocol":1,"result":{},"session":null}
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(211)]
FAIL!  : EngineTest::envelope(quit-number) 'rejected' returned FALSE. ({"id":"bad","ok":true,"protocol":1,"result":{},"session":7}
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(211)]
FAIL!  : EngineTest::envelope(quit-object) 'rejected' returned FALSE. ({"id":"bad","ok":true,"protocol":1,"result":{},"session":{}}
)
   Loc: [/home/wz150432/Documents/GitHub/Reader-CachyOS/tests/tst_onlineengine.cpp(211)]
PASS   : EngineTest::cleanupTestCase()
Totals: 5 passed, 14 failed, 0 skipped, 0 blacklisted, 4918ms
********* Finished testing of EngineTest *********
```

GREEN command (exit 0):
```sh
cmake --build build-online-engine --target tst_onlineengine -j3 > /tmp/reader-task1-round1-build-green.log 2>&1 && build-online-engine/tst_onlineengine postRedirects envelope > /tmp/reader-task1-round1-green.log 2>&1 && ctest --test-dir build-online-engine -R '^onlineengine$' --output-on-failure
```
Exact targeted GREEN output:
```text
********* Start testing of EngineTest *********
Config: Using QtTest library 6.12.0, Qt 6.12.0 (x86_64-little_endian-lp64 shared (dynamic) release build; by GCC 16.2.1 20260810), cachyos unknown
PASS   : EngineTest::initTestCase()
PASS   : EngineTest::postRedirects(301)
PASS   : EngineTest::postRedirects(302)
PASS   : EngineTest::postRedirects(303)
PASS   : EngineTest::postRedirects(307)
PASS   : EngineTest::postRedirects(308)
PASS   : EngineTest::envelope(hello-missing)
PASS   : EngineTest::envelope(hello-number)
PASS   : EngineTest::envelope(hello-object)
PASS   : EngineTest::envelope(chapter-missing)
PASS   : EngineTest::envelope(chapter-number)
PASS   : EngineTest::envelope(chapter-object)
PASS   : EngineTest::envelope(cancel-missing)
PASS   : EngineTest::envelope(cancel-number)
PASS   : EngineTest::envelope(cancel-object)
PASS   : EngineTest::envelope(quit-missing)
PASS   : EngineTest::envelope(quit-number)
PASS   : EngineTest::envelope(quit-object)
PASS   : EngineTest::cleanupTestCase()
Totals: 19 passed, 0 failed, 0 skipped, 0 blacklisted, 4897ms
********* Finished testing of EngineTest *********
```
Exact final focused CTest output:
```text
Test project /home/wz150432/Documents/GitHub/Reader-CachyOS/build-online-engine
    Start 26: onlineengine
1/1 Test #26: onlineengine .....................   Passed   13.25 sec

100% tests passed out of 1

Total Test time (real) =  13.25 sec
```

The two Important findings are fixed. Previously documented classification/navigation and portability limits remain. No live smoke was repeated for this protocol/redirect-only review round.
