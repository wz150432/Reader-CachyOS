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
