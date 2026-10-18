#include "app/ReadingView.h"
#include "core/online/OnlineReadingController.h"
#include <QTemporaryDir>
#include <QtTest>
using namespace reader;
class ReadingTest : public QObject {
  Q_OBJECT
private slots:
  void bridgeDefaultHandshakeIsBounded() {
    qputenv("READER_FIXTURE_HANG_HELLO", "1");
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() + "/online_fixture");
    QSignalSpy errors(&bridge, &OnlineEngineBridge::error);
    QElapsedTimer elapsed;
    elapsed.start();
    bridge.request("toc", {}, "hung-hello");
    while (errors.isEmpty() && elapsed.elapsed() < 6500)
      QTest::qWait(10);
    qunsetenv("READER_FIXTURE_HANG_HELLO");
    QCOMPARE(errors.size(), 1);
    QCOMPARE(errors.first()[2].toString(), QString("Online engine handshake timed out"));
    QVERIFY(elapsed.elapsed() < 6000);
    bridge.setExecutablePath({});
    bridge.request("validate", {}, "teardown");
    QTRY_COMPARE(errors.size(), 2);
  }
  void bridgeSafeErrors_data() {
    QTest::addColumn<QString>("raw");
    QTest::addColumn<QString>("expected");
    for (const auto message : {"HTTP 403", "HTTP 404", "HTTP 429", "HTTP 503", "Browser challenge",
                               "Empty content", "Invalid XPath", "Pagination limit", "Network failure", "Rate limit exceeded"})
      QTest::newRow(message) << QString(message) << QString(message);
    for (const auto message : {"HTTP 403 Cookie=secret", "HTTP 999", "HTTP 403\n", "Empty content secret", "Cookie=secret"})
      QTest::newRow(message) << QString(message) << QString("Online request failed");
  }
  void bridgeSafeErrors() {
    QFETCH(QString, raw);
    QFETCH(QString, expected);
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() + "/online_fixture");
    QSignalSpy errors(&bridge, &OnlineEngineBridge::error);
    bridge.request("chapter", {{"fixtureError", raw}}, "error");
    QTRY_COMPARE(errors.size(), 1);
    QCOMPARE(errors.first()[2].toString(), expected);
    bridge.setExecutablePath({});
    bridge.request("validate", {}, "teardown");
    QTRY_COMPARE(errors.size(), 2);
  }
  void bridgeRejectsBrokenProcesses_data() {
    QTest::addColumn<QString>("mode");
    for (const auto &mode : {"crash", "timeout", "malformed", "oversize"})
      QTest::newRow(mode) << QString(mode);
  }
  void bridgeRejectsBrokenProcesses() {
    QFETCH(QString, mode);
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    bridge.setTimeoutMs(250);
    QSignalSpy errors(&bridge, &OnlineEngineBridge::error);
    QSignalSpy results(&bridge, &OnlineEngineBridge::result);
    QString id = bridge.request("chapter", {{"mode", mode}}, "session");
    QTRY_COMPARE(errors.size(), 1);
    QCOMPARE(errors[0][0].toString(), id);
    QCOMPARE(errors[0][1].toString(), QString("session"));
    QVERIFY(results.isEmpty());
    bridge.setExecutablePath({});
    bridge.request("validate", {}, "teardown");
    QTRY_COMPARE(errors.size(), 2);
  }
  void bridgeFailedStartThenImmediateRestart() {
    OnlineEngineBridge bridge;
    bridge.setTimeoutMs(1000);
    bridge.setExecutablePath("/missing/reader-online");
    QSignalSpy results(&bridge, &OnlineEngineBridge::result);
    QSignalSpy errors(&bridge, &OnlineEngineBridge::error);
    auto old = bridge.request("chapter", {}, "old");
    QString replacement;
    QTimer::singleShot(0, &bridge, [&] {
      bridge.stop();
      bridge.setExecutablePath(QCoreApplication::applicationDirPath() + "/online_fixture");
      replacement = bridge.request("chapter", {{"url", "https://example.org/new"}}, "new");
    });
    QTRY_COMPARE_WITH_TIMEOUT(results.size(), 1, 2000);
    QCOMPARE(results[0][0].toString(), replacement);
    QCOMPARE(results[0][1].toString(), QString("new"));
    QCOMPARE(errors.size(), 1);
    QCOMPARE(errors[0][0].toString(), old);
    bridge.setExecutablePath({});
    auto teardown = bridge.request("validate", {}, "teardown");
    QTRY_COMPARE(errors.size(), 2);
    QCOMPARE(errors.last()[0].toString(), teardown);
  }
  void bridgeImmediateRestartPreservesNewRequest() {
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() + "/online_fixture");
    QSignalSpy results(&bridge, &OnlineEngineBridge::result);
    QSignalSpy errors(&bridge, &OnlineEngineBridge::error);
    bridge.request("chapter", {{"url", "https://example.org/first"}}, "first");
    QTRY_COMPARE(results.size(), 1);
    for (int i = 0; i < 3; ++i) {
      bridge.stop();
      auto id = bridge.request("chapter", {{"url", "https://example.org/restarted"}}, "new-session");
      QTRY_COMPARE_WITH_TIMEOUT(results.size(), i + 2, 2000);
      QCOMPARE(results.last()[0].toString(), id);
      QCOMPARE(results.last()[1].toString(), QString("new-session"));
      QVERIFY(errors.isEmpty());
    }
    bridge.setExecutablePath({});
    auto teardown = bridge.request("validate", {}, "teardown");
    QTRY_COMPARE(errors.size(), 1);
    QCOMPARE(errors.last()[0].toString(), teardown);
  }
  void bridgeMissingRetryAndCancel() {
    OnlineEngineBridge bridge;
    QSignalSpy errors(&bridge, &OnlineEngineBridge::error);
    QSignalSpy results(&bridge, &OnlineEngineBridge::result);
    bridge.request("chapter", {}, "book");
    QVERIFY(errors.isEmpty());
    QTRY_COMPARE(errors.size(), 1);
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    bridge.request("chapter", {{"url", "https://example.org/1"}}, "book");
    QTRY_COMPARE(results.size(), 1);
    auto id =
        bridge.request("chapter", {{"url", "https://example.org/2"}}, "book");
    bridge.cancel(id);
    QTest::qWait(50);
    QCOMPARE(results.size(), 1);
  }
  void scrollAcrossCachedChapterKeepsObserversTransactional() {
    QTemporaryDir d;
    QJsonArray chapters;
    for (int i = 0; i < 3; ++i)
      chapters.append(
          QJsonObject{{"title", QString::number(i)},
                      {"url", QString("https://example.org/%1").arg(i)}});
    auto b = OnlineBook::createManifest(d.path(), BookSource::preset(),
                                        "https://example.org/book", "Book",
                                        chapters);
    QVERIFY(b);
    QVERIFY(b->saveChapter(0, "first"));
    QVERIFY(b->saveChapter(1, "second"));
    ReadingView view;
    view.resize(400, 200);
    view.setBook(b);
    QSignalSpy chapterSignals(&view, &ReadingView::chapterChanged);
    QSignalSpy pageSignals(&view, &ReadingView::pageChanged);
    QList<QList<int>> requestStates;
    connect(&view, &ReadingView::chapterRequested, this, [&](int target) {
      requestStates.append({target, view.currentChapter(), view.currentPage(),
                            view.pendingChapter()});
    });
    QVERIFY(!view.scrollByPixels(1000000));
    QCOMPARE(view.currentChapter(), 0);
    QCOMPARE(view.currentPage(), 0);
    QCOMPARE(view.pendingChapter(), 2);
    QVERIFY(chapterSignals.isEmpty());
    QVERIFY(pageSignals.isEmpty());
    QCOMPARE(requestStates, (QList<QList<int>>{{2, 0, 0, 2}}));
    QVERIFY(b->saveChapter(2, "third"));
    view.setChapterReady(2);
    QCOMPARE(chapterSignals.size(), 1);
    QCOMPARE(chapterSignals[0][0].toInt(), 2);
    QCOMPARE(view.currentChapter(), 2);
    view.goToChapter(0);
    chapterSignals.clear();
    pageSignals.clear();
    QList<QList<int>> commits;
    connect(&view, &ReadingView::chapterChanged, this, [&](int index) {
      commits.append({index, view.currentChapter(), view.currentPage(),
                      view.pendingChapter()});
    });
    QVERIFY(view.scrollByPixels(1000000));
    QCOMPARE(commits, (QList<QList<int>>{{2, 2, 0, -1}}));
    QCOMPARE(chapterSignals.size(), 1);
  }
  void pageUpFromPendingLandsOnCachedLastPage() {
    QTemporaryDir d;
    auto b = OnlineBook::createManifest(
        d.path(), BookSource::preset(), "https://example.org/book", "Book",
        QJsonArray{
            QJsonObject{{"title", "One"}, {"url", "https://example.org/1"}},
            QJsonObject{{"title", "Two"}, {"url", "https://example.org/2"}}});
    QVERIFY(b);
    QVERIFY(b->saveChapter(0, QString("long text\n\n").repeated(100)));
    ReadingView view;
    view.resize(400, 200);
    view.setBook(b);
    QVERIFY(view.pageCount() > 1);
    const int lastPage = view.pageCount() - 1;
    view.goToChapter(1);
    QList<QList<int>> commits;
    connect(&view, &ReadingView::chapterChanged, this, [&](int index) {
      commits.append({index, view.currentChapter(), view.currentPage(),
                      view.pendingChapter()});
    });
    QSignalSpy pages(&view, &ReadingView::pageChanged);
    view.pageUp();
    QCOMPARE(view.currentPage(), lastPage);
    QCOMPARE(view.pendingChapter(), -1);
    QCOMPARE(commits, (QList<QList<int>>{{0, 0, lastPage, -1}}));
    QCOMPARE(pages.size(), 1);
    QCOMPARE(pages[0][0].toInt(), lastPage);
  }
  void malformedBackgroundChapterReportsFailure() {
    QTemporaryDir d;
    auto b = OnlineBook::createManifest(
        d.path(), BookSource::preset(), "https://example.org/book", "Book",
        QJsonArray{
            QJsonObject{{"title", "One"}, {"url", "https://example.org/1"}},
            QJsonObject{{"title", "Two"},
                        {"url", "https://example.org/invalid-body"}}});
    QVERIFY(b);
    QVERIFY(b->saveChapter(0, "cached"));
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    OnlineReadingController controller(&bridge);
    QSignalSpy background(&controller,
                          &OnlineReadingController::backgroundStatus);
    QSignalSpy foregroundErrors(&controller,
                                &OnlineReadingController::currentError);
    controller.setBook(b);
    controller.requestChapter(0);
    QTRY_COMPARE_WITH_TIMEOUT(background.size(), 1, 1000);
    QCOMPARE(background[0][0].toInt(), -1);
    QCOMPARE(background[0][1].toInt(), 1);
    QVERIFY(foregroundErrors.isEmpty());
    QVERIFY(!b->chapterAvailable(1));
  }
  void pixelBoundaryDoesNotCommitPartialScroll() {
    QTemporaryDir d;
    auto b = OnlineBook::createManifest(
        d.path(), BookSource::preset(), "https://example.org/book", "Book",
        QJsonArray{
            QJsonObject{{"title", "One"}, {"url", "https://example.org/1"}},
            QJsonObject{{"title", "Two"}, {"url", "https://example.org/2"}}});
    QVERIFY(b);
    QVERIFY(b->saveChapter(0, QString("paragraph text\n\n").repeated(300)));
    ReadingView view;
    view.resize(400, 200);
    view.setBook(b);
    QVERIFY(view.pageCount() > 1);
    QVERIFY(!view.scrollByPixels(1000000));
    QCOMPARE(view.pendingChapter(), 1);
    QCOMPARE(view.currentChapter(), 0);
    QCOMPARE(view.currentPage(), 0);
    QCOMPARE(view.pixelOffset(), 0.0);
  }
  void automaticPagingWaitsForReadiness() {
    QTemporaryDir d;
    auto b = OnlineBook::createManifest(
        d.path(), BookSource::preset(), "https://example.org/book", "Book",
        QJsonArray{
            QJsonObject{{"title", "One"}, {"url", "https://example.org/1"}},
            QJsonObject{{"title", "Two"}, {"url", "https://example.org/2"}}});
    QVERIFY(b);
    QVERIFY(b->saveChapter(0, "short"));
    ReadingView view;
    view.resize(400, 200);
    view.setBook(b);
    auto timer = view.findChild<QTimer *>();
    QVERIFY(timer);
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
    QCOMPARE(view.pendingChapter(), 1);
    QCOMPARE(view.currentChapter(), 0);
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
    QCOMPARE(view.currentChapter(), 0);
    QCOMPARE(view.currentPage(), 0);
    QVERIFY(
        b->saveChapter(1, QString("long paragraph text\n\n").repeated(100)));
    view.setChapterReady(1);
    QVERIFY(view.pageCount() > 1);
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
    QCOMPARE(view.currentChapter(), 1);
    QCOMPARE(view.currentPage(), 1);
  }
  void pendingViewKeepsCommittedPosition() {
    QTemporaryDir d;
    QJsonArray chapters;
    for (int i = 0; i < 8; ++i)
      chapters.append(
          QJsonObject{{"title", QString::number(i)},
                      {"url", QString("https://example.org/%1").arg(i)}});
    auto b = OnlineBook::createManifest(d.path(), BookSource::preset(),
                                        "https://example.org/book", "Test",
                                        chapters);
    QVERIFY(b);
    QVERIFY(b->saveChapter(0, "readable"));
    ReadingView view;
    view.resize(400, 400);
    view.setBook(b);
    view.goToChapter(5);
    QCOMPARE(view.currentChapter(), 0);
    QCOMPARE(view.pendingChapter(), 5);
    QVERIFY(view.committedChapterReady());
    view.pageDown();
    QCOMPARE(view.pendingChapter(), 6);
    QCOMPARE(view.currentChapter(), 0);
    QVERIFY(!view.scrollByPixels(100000));
    QCOMPARE(view.currentChapter(), 0);
    QVERIFY(b->saveChapter(5, "stale"));
    view.setChapterReady(5);
    QCOMPARE(view.currentChapter(), 0);
    QVERIFY(b->saveChapter(6, "ready"));
    view.setChapterReady(6);
    QCOMPARE(view.currentChapter(), 6);
    QCOMPARE(view.pendingChapter(), -1);
    view.setSearchWholeBook(true);
    QVERIFY(!view.findNext("missing"));
    QCOMPARE(view.currentChapter(), 6);
  }
  void switchingBookIgnoresOldReplies() {
    QTemporaryDir d;
    QJsonArray chapters;
    for (int i = 0; i < 20; ++i)
      chapters.append(
          QJsonObject{{"title", QString::number(i)},
                      {"url", QString("https://example.org/%1").arg(i)}});
    QJsonArray slowChapters;
    for (int i = 0; i < 20; ++i)
      slowChapters.append(
          QJsonObject{{"title", QString::number(i)},
                      {"url", QString("https://example.org/slow/%1").arg(i)}});
    auto a =
        OnlineBook::createManifest(d.path(), BookSource::preset(),
                                   "https://example.org/a", "A", slowChapters);
    auto b = OnlineBook::createManifest(d.path(), BookSource::preset(),
                                        "https://example.org/b", "B", chapters);
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    OnlineReadingController controller(&bridge);
    QSignalSpy ready(&bridge, &OnlineEngineBridge::ready);
    controller.setBook(a);
    controller.requestChapter(10);
    QTRY_COMPARE(ready.size(), 1);
    QTest::qWait(10);
    controller.setBook(b);
    controller.requestChapter(3);
    QTRY_VERIFY(b->chapterAvailable(6));
    QCOMPARE(controller.book(), b);
    QCOMPARE(controller.targetChapter(), 3);
    QVERIFY(!a->chapterAvailable(10));
    QVERIFY(!b->chapterAvailable(10));
  }
  void jumpPrioritizesMissingForegroundWithinOldWindow() {
    QTemporaryDir d;
    QJsonArray chapters;
    for (int i = 0; i < 20; ++i)
      chapters.append(
          QJsonObject{{"title", QString::number(i)},
                      {"url", QString("https://example.org/%1").arg(i)}});
    auto b = OnlineBook::createManifest(d.path(), BookSource::preset(),
                                        "https://example.org/a", "A", chapters);
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    OnlineReadingController controller(&bridge);
    QList<int> requests;
    connect(&controller, &OnlineReadingController::chapterRequestStarted, this,
            [&](int i) { requests.append(i); });
    controller.setBook(b);
    controller.requestChapter(10);
    controller.requestChapter(9);
    QCOMPARE(requests, (QList<int>{10, 9}));
    QTRY_VERIFY(b->chapterAvailable(12));
  }
  void redirectedChapterRetainsRequestedCacheIdentity() {
    QTemporaryDir d;
    auto b = OnlineBook::createManifest(
        d.path(), BookSource::preset(), "https://example.org/book", "Book",
        QJsonArray{QJsonObject{{"title", "One"},
                               {"url", "https://example.org/redirect"}}});
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    OnlineReadingController controller(&bridge);
    controller.setBook(b);
    controller.requestChapter(0);
    QTRY_VERIFY_WITH_TIMEOUT(b->chapterAvailable(0), 1000);
    QCOMPARE(b->chapterUrl(0), QString("https://example.org/redirect"));
  }
  void foregroundPromotesInFlight() {
    QTemporaryDir d;
    QJsonArray chapters;
    for (int i = 0; i < 20; ++i)
      chapters.append(
          QJsonObject{{"title", QString::number(i)},
                      {"url", QString("https://example.org/%1").arg(i)}});
    auto b = OnlineBook::createManifest(d.path(), BookSource::preset(),
                                        "https://example.org/a", "A", chapters);
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    OnlineReadingController controller(&bridge);
    QList<int> requests;
    connect(&controller, &OnlineReadingController::chapterRequestStarted, this,
            [&](int i) {
              requests.append(i);
              if (i == 11)
                controller.requestChapter(11);
            });
    controller.setBook(b);
    controller.requestChapter(10);
    QTRY_VERIFY(b->chapterAvailable(14));
    QCOMPARE(requests, (QList<int>{10, 11, 12, 13, 14}));
    QCOMPARE(controller.targetChapter(), 11);
  }
  void windowAndCache() {
    QTemporaryDir d;
    QJsonArray chapters;
    for (int i = 0; i < 20; ++i)
      chapters.append(
          QJsonObject{{"title", QString::number(i)},
                      {"url", QString("https://example.org/%1").arg(i)}});
    auto b = OnlineBook::createManifest(d.path(), BookSource::preset(),
                                        "https://example.org/book", "Test",
                                        chapters);
    QVERIFY(b);
    OnlineEngineBridge bridge;
    bridge.setExecutablePath(QCoreApplication::applicationDirPath() +
                             "/online_fixture");
    OnlineReadingController controller(&bridge);
    QList<int> requested;
    connect(&controller, &OnlineReadingController::chapterRequestStarted, this,
            [&](int i) { requested.append(i); });
    controller.setBook(b);
    controller.requestChapter(10);
    QTRY_VERIFY(b->chapterAvailable(13));
    QCOMPARE(requested, (QList<int>{10, 11, 12, 13}));
    controller.requestChapter(11);
    QTRY_VERIFY(b->chapterAvailable(14));
    QCOMPARE(requested, (QList<int>{10, 11, 12, 13, 14}));
    controller.close();
    requested.clear();
    controller.setBook(b);
    controller.requestChapter(10);
    QTest::qWait(50);
    QVERIFY(requested.isEmpty());
  }
};
QTEST_MAIN(ReadingTest)
#include "tst_onlinereading.moc"
