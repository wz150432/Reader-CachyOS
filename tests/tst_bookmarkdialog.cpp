#include <QtTest>
#include <QSignalSpy>
#include "app/BookmarkDialog.h"

using namespace reader;

class TestBookmarkDialog : public QObject
{
    Q_OBJECT
private slots:
    void deleteSelectedBookmarkEmitsRemoval();
    void jumpSelectedBookmarkEmitsJump();
};

void TestBookmarkDialog::deleteSelectedBookmarkEmitsRemoval()
{
    BookmarkDialog dlg({
        Bookmark{QStringLiteral("/b/a.txt"), 0, 0, QStringLiteral("第一章"), 100},
        Bookmark{QStringLiteral("/b/a.txt"), 1, 4, QStringLiteral("第二章"), 101},
        Bookmark{QStringLiteral("/b/a.txt"), 2, 7, QStringLiteral("第三章"), 102}
    });
    QSignalSpy spy(&dlg, &BookmarkDialog::deleteRequested);
    dlg.setCurrentRow(1);
    dlg.deleteSelectedBookmark();
    QCOMPARE(spy.count(), 1);
    const Bookmark removed = spy.takeFirst().at(0).value<Bookmark>();
    QCOMPARE(removed.created, 101);
    QCOMPARE(dlg.count(), 2);
}

void TestBookmarkDialog::jumpSelectedBookmarkEmitsJump()
{
    BookmarkDialog dlg({
        Bookmark{QStringLiteral("/b/a.txt"), 3, 9, QStringLiteral("第四章"), 200}
    });
    QSignalSpy spy(&dlg, &BookmarkDialog::jumpRequested);
    dlg.setCurrentRow(0);
    dlg.jumpToSelectedBookmark();
    QCOMPARE(spy.count(), 1);
    const Bookmark target = spy.takeFirst().at(0).value<Bookmark>();
    QCOMPARE(target.chapterIndex, 3);
    QCOMPARE(target.pageIndex, 9);
}

QTEST_MAIN(TestBookmarkDialog)
#include "tst_bookmarkdialog.moc"
