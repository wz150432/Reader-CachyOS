#include "core/online/OnlineComponentManager.h"
#include <QCryptographicHash>
#include <QFile>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
#include <archive.h>
#include <archive_entry.h>
using namespace reader;
class ComponentTest : public QObject {
  Q_OBJECT

private:
  QByteArray helper() {
    QFile f(qEnvironmentVariable("READER_TEST_ONLINE_HELPER",
                                 QCoreApplication::applicationDirPath() +
                                     "/online_fixture"));
    if (!f.open(QIODevice::ReadOnly))
      return {};
    return f.readAll();
  }
  QString package(const QString &path, const QByteArray &data,
                  const QString &name = "reader-online", int link = 0) {
    auto a = archive_write_new();
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, QFile::encodeName(path).constData());
    auto e = archive_entry_new();
    archive_entry_set_pathname(e, name.toUtf8());
    archive_entry_set_filetype(e, link == 1 ? AE_IFLNK : AE_IFREG);
    archive_entry_set_perm(e, 0755);
    archive_entry_set_size(e, link ? 0 : data.size());
    if (link == 1)
      archive_entry_set_symlink(e, "/tmp/escape");
    if (link == 2)
      archive_entry_set_hardlink(e, "other");
    archive_write_header(a, e);
    if (!link)
      archive_write_data(a, data.constData(), data.size());
    archive_entry_free(e);
    archive_write_close(a);
    archive_write_free(a);
    return path;
  }
  QJsonObject metadata(const QString &path, qint64 unpacked) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
      return {};
    auto bytes = f.readAll();
    return {{"version", "0.1.0"},
            {"protocol", 1},
            {"appVersion", "0.1.0"},
            {"platform", "linux"},
            {"arch", "x86_64"},
            {"entrypoint", "reader-online"},
            {"url", "https://github.com/wz150432/Reader-CachyOS/releases/"
                    "download/v0.1.0/online.tar"},
            {"size", bytes.size()},
            {"unpackedSize", unpacked},
            {"sha256",
             QString::fromLatin1(
                 QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
                     .toHex())}};
  }
private slots:
  void incompatibleMetadata() {
    QTemporaryDir d;
    OnlineComponentManager m(d.path() + "/component", "0.1.0");
    auto p = package(d.path() + "/p.tar", helper());
    auto j = metadata(p, helper().size());
    QString error;
    QVERIFY(m.validateMetadata(j, &error));
    for (auto key :
         {"protocol", "platform", "arch", "appVersion", "entrypoint", "url"}) {
      auto bad = j;
      bad[key] = "wrong";
      QVERIFY2(!m.validateMetadata(bad, &error), key);
    }
    QVERIFY(!m.isBusy());
    QVERIFY(!m.isInstalled());
  }
  void localImportHasNoDownload() {
    QTemporaryDir d;
    auto p = package(d.path() + "/local.tar", helper());
    auto j = metadata(p, helper().size());
    j["url"] = "";
    OnlineComponentManager m(d.path() + "/component", "0.1.0", j);
    QString error;
    QVERIFY2(m.validateMetadata(j, &error), qPrintable(error));
    QVERIFY(!m.downloadAvailable());
    QVERIFY(!m.download(&error));
    QVERIFY(!m.isBusy());
    QSignalSpy done(&m, &OnlineComponentManager::finished);
    QVERIFY2(m.importPackage(p, j, &error), qPrintable(error));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
    QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
    QVERIFY(m.isEnabled());
    OnlineComponentManager reopened(d.path() + "/component", "0.1.0", j);
    QVERIFY(reopened.isEnabled());
    QVERIFY(!reopened.downloadAvailable());
  }
  void safeInstallAndPreservation() {
    QTemporaryDir d;
    auto data = helper();
    QVERIFY(!data.isEmpty());
    auto p = package(d.path() + "/p.tar", data);
    auto j = metadata(p, data.size());
    OnlineComponentManager m(d.path() + "/component", "0.1.0");
    QSignalSpy done(&m, &OnlineComponentManager::finished);
    QString error;
    QVERIFY(m.importPackage(p, j, &error));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
    QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
    QVERIFY(m.isEnabled());
    QVERIFY(QFileInfo::exists(m.executablePath()));
    auto original = m.executablePath();
    auto bad = j;
    bad["sha256"] = QString(64, '0');
    QVERIFY(m.importPackage(p, bad));
    QTRY_COMPARE(done.size(), 2);
    QVERIFY(!done[1][0].toBool());
    QCOMPARE(m.executablePath(), original);
    QFile cache(d.path() + "/chapter.json");
    QVERIFY(cache.open(QIODevice::WriteOnly));
    cache.write("keep");
    cache.close();
    QVERIFY(m.setEnabled(false));
    QVERIFY(!m.isEnabled());
    QVERIFY(m.isInstalled());
    QVERIFY(m.setEnabled(true));
    QVERIFY(m.uninstall());
    QVERIFY(!m.isInstalled());
    QVERIFY(QFileInfo::exists(cache.fileName()));
  }
  void rejectsArchiveEscapes() {
    QTemporaryDir d;
    auto data = helper();
    OnlineComponentManager m(d.path() + "/component", "0.1.0");
    QSignalSpy done(&m, &OnlineComponentManager::finished);
    for (auto name : {"../escape", "/tmp/escape", "reader-online"}) {
      auto p = package(d.path() + "/bad.tar", data, name,
                       QString(name) == "reader-online");
      QVERIFY(m.importPackage(p, metadata(p, data.size())));
      QTRY_COMPARE(done.size(), 1);
      QVERIFY(!done[0][0].toBool());
      QVERIFY(!m.isInstalled());
      done.clear();
    }
  }
  void immediateReinstall() {
    QTemporaryDir d;
    auto data = helper();
    auto p = package(d.path() + "/p.tar", data);
    auto j = metadata(p, data.size());
    OnlineComponentManager m(d.path() + "/component", "0.1.0");
    QSignalSpy done(&m, &OnlineComponentManager::finished);
    for (int i = 0; i < 2; ++i) {
      QVERIFY(m.importPackage(p, j));
      QTRY_COMPARE_WITH_TIMEOUT(done.size(), i + 1, 10000);
      QVERIFY2(done[i][0].toBool(), qPrintable(done[i][1].toString()));
    }
    QVERIFY(m.isEnabled());
  }
  void rejectsHardlinksAndArchiveLimits() {
    QTemporaryDir d;
    auto data = helper();
    OnlineComponentManager m(d.path() + "/component", "0.1.0");
    QSignalSpy done(&m, &OnlineComponentManager::finished);
    auto hard = package(d.path() + "/hard.tar", data, "reader-online", 2);
    QVERIFY(m.importPackage(hard, metadata(hard, data.size())));
    QTRY_COMPARE(done.size(), 1);
    QVERIFY(!done[0][0].toBool());
    auto oversized = package(d.path() + "/oversized.tar", data);
    auto limits = metadata(oversized, 1);
    QVERIFY(m.importPackage(oversized, limits));
    QTRY_COMPARE(done.size(), 2);
    QVERIFY(!done[1][0].toBool());
    QString countPath = d.path() + "/count.tar";
    auto a = archive_write_new();
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, QFile::encodeName(countPath));
    for (int i = 0; i < 257; ++i) {
      auto e = archive_entry_new();
      archive_entry_set_pathname(e, QString("file-%1").arg(i).toUtf8());
      archive_entry_set_filetype(e, AE_IFREG);
      archive_entry_set_perm(e, 0644);
      archive_entry_set_size(e, 1);
      archive_write_header(a, e);
      archive_write_data(a, "x", 1);
      archive_entry_free(e);
    }
    archive_write_close(a);
    archive_write_free(a);
    QVERIFY(m.importPackage(countPath, metadata(countPath, 257)));
    QTRY_COMPARE(done.size(), 3);
    QVERIFY(!done[2][0].toBool());
    QVERIFY(!m.isInstalled());
  }
  void failedHandshakeKeepsPriorActivation() {
    QTemporaryDir d;
    auto data = helper();
    auto p = package(d.path() + "/p.tar", data);
    auto j = metadata(p, data.size());
    OnlineComponentManager m(d.path() + "/component", "0.1.0");
    QSignalSpy done(&m, &OnlineComponentManager::finished);
    QVERIFY(m.importPackage(p, j));
    QTRY_COMPARE(done.size(), 1);
    QVERIFY(done[0][0].toBool());
    QString previous = m.executablePath();
    j["version"] = "0.2.0";
    QVERIFY(m.importPackage(p, j));
    QTRY_COMPARE(done.size(), 2);
    QVERIFY(!done[1][0].toBool());
    QCOMPARE(m.executablePath(), previous);
  }
  void refusesSymlinkedActivationRecord() {
    QTemporaryDir d;
    auto data = helper();
    auto p = package(d.path() + "/p.tar", data);
    OnlineComponentManager m(d.path() + "/component", "0.1.0");
    QSignalSpy done(&m, &OnlineComponentManager::finished);
    QVERIFY(m.importPackage(p, metadata(p, data.size())));
    QTRY_COMPARE(done.size(), 1);
    QVERIFY(done[0][0].toBool());
    QString record = m.rootPath() + "/active.json";
    QString external = d.path() + "/external.json";
    QVERIFY(QFile::rename(record, external));
    QVERIFY(QFile::link(external, record));
    QVERIFY(!m.isInstalled());
    QVERIFY(!m.uninstall());
    QVERIFY(QFileInfo::exists(external));
  }
  void cancelDuringHandshakeThenReinstall() {
    if (!qEnvironmentVariable("READER_TEST_ONLINE_HELPER").isEmpty())
      QSKIP("Delayed handshake uses controlled fixture only");
    QTemporaryDir d;
    auto data = helper();
    auto p = package(d.path() + "/p.tar", data);
    auto j = metadata(p, data.size());
    auto marker = d.path() + "/hello-started";
    auto previous = qgetenv("READER_FIXTURE_HELLO_MARKER");
    auto restore = qScopeGuard([&] {
      if (previous.isNull())
        qunsetenv("READER_FIXTURE_HELLO_MARKER");
      else
        qputenv("READER_FIXTURE_HELLO_MARKER", previous);
    });
    qputenv("READER_FIXTURE_HELLO_MARKER", marker.toUtf8());
    OnlineComponentManager manager(d.path() + "/component", "0.1.0");
    QSignalSpy done(&manager, &OnlineComponentManager::finished);
    QVERIFY(manager.importPackage(p, j));
    QTRY_VERIFY(QFileInfo::exists(marker));
    manager.cancel();
    QTRY_COMPARE(done.size(), 1);
    QVERIFY(!done[0][0].toBool());
    QVERIFY(!manager.isInstalled());
    QVERIFY(QFile::remove(marker));
    QVERIFY(manager.importPackage(p, j));
    QTRY_COMPARE(done.size(), 2);
    QVERIFY2(done[1][0].toBool(), qPrintable(done[1][1].toString()));
    QVERIFY(manager.isEnabled());
  }
  void cancellation() {
    QTemporaryDir d;
    auto data = helper();
    auto p = package(d.path() + "/p.tar", data);
    OnlineComponentManager m(d.path() + "/component", "0.1.0");
    QSignalSpy done(&m, &OnlineComponentManager::finished);
    QVERIFY(m.importPackage(p, metadata(p, data.size())));
    m.cancel();
    QTRY_COMPARE(done.size(), 1);
    QVERIFY(!done[0][0].toBool());
    QVERIFY(!m.isInstalled());
  }
};
QTEST_GUILESS_MAIN(ComponentTest)
#include "tst_onlinecomponent.moc"
