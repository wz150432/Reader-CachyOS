#include "OnlineComponentDialog.h"
#include "app/UiStyle.h"
#include "core/online/OnlineComponentManager.h"
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
namespace reader {
OnlineComponentDialog::OnlineComponentDialog(OnlineComponentManager *m,
                                             QWidget *p)
    : MenuPanel(p), m_manager(m) {
  setWindowTitle(tr("在线阅读组件"));
  setObjectName("onlineComponentDialog");
  setStyleSheet(readerUiStyle());
  resize(420, 200);
  setAttribute(Qt::WA_DeleteOnClose, false);
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 14, 16, 14);
  layout->setSpacing(8);
  auto title = new QLabel(tr("安装在线阅读组件"), this);
  title->setProperty("sectionHeading", true);
  layout->addWidget(title);
  m_details = new QLabel(this);
  m_details->setWordWrap(true);
  m_details->setTextFormat(Qt::PlainText);
  m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(m_details);
  m_status = new QLabel(this);
  m_status->setWordWrap(true);
  m_status->setObjectName("componentStatus");
  layout->addWidget(m_status);
  m_progress = new QProgressBar(this);
  layout->addWidget(m_progress);
  auto row = new QHBoxLayout;
  layout->addLayout(row);
  m_download = new QPushButton(tr("安装"), this);
  m_download->setObjectName("downloadComponent");
  m_download->setDefault(true);
  row->addWidget(m_download);
  m_import = new QPushButton(tr("本地安装…"), this);
  m_import->setObjectName("importComponent");
  row->addWidget(m_import);
  row->addStretch();
  auto manage = new QHBoxLayout;
  layout->addLayout(manage);
  m_enable = new QPushButton(this);
  m_enable->setObjectName("enableComponent");
  manage->addWidget(m_enable);
  m_uninstall = new QPushButton(tr("卸载组件"), this);
  manage->addWidget(m_uninstall);
  manage->addStretch();
  m_cancel = new QPushButton(tr("取消安装"), this);
  manage->addWidget(m_cancel);
  auto close = new QPushButton(tr("关闭"), this);
  close->setObjectName("closeComponent");
  manage->addWidget(close);
  connect(close, &QPushButton::clicked, this, &QDialog::close);
  connect(m_cancel, &QPushButton::clicked, m, &OnlineComponentManager::cancel);
  connect(m_download, &QPushButton::clicked, this, [this] {
    QString e;
    if (!m_manager->download(&e))
      m_status->setText(e);
    refresh();
  });
  connect(m_import, &QPushButton::clicked, this,
          &OnlineComponentDialog::importLocal);
  connect(m_enable, &QPushButton::clicked, this, [this] {
    QString e;
    if (!m_manager->setEnabled(!m_manager->isEnabled(), &e)) {
      m_status->setText(e);
    } else if (m_manager->isEnabled()) {
      accept();
      emit ready();
    }
  });
  connect(m_uninstall, &QPushButton::clicked, this, [this] {
    if (QMessageBox::question(
            this, tr("卸载在线组件"),
            tr("卸载可执行组件？书源、书籍和章节缓存会保留。")) !=
        QMessageBox::Yes)
      return;
    QString e;
    if (!m_manager->uninstall(&e))
      m_status->setText(e);
  });
  connect(m, &OnlineComponentManager::changed, this,
          &OnlineComponentDialog::refresh);
  connect(m, &OnlineComponentManager::progress, this,
          [this](qint64 n, qint64 total) {
            m_progress->setRange(0, 100);
            m_progress->setValue(total > 0 ? int(n * 100 / total) : 0);
            m_status->setText(
                tr("正在下载 %1 / %2 KiB").arg(n / 1024).arg(total / 1024));
          });
  connect(m, &OnlineComponentManager::finished, this,
          [this](bool ok, const QString &error) {
            m_status->setText(ok ? QString() : error);
            refresh();
            if (ok) {
              accept();
              emit ready();
            }
          });
  refresh();
}
void OnlineComponentDialog::refresh() {
  bool busy = m_manager->isBusy(), installed = m_manager->isInstalled();
  auto j = installed ? m_manager->installedMetadata() : m_manager->metadata();
  m_details->setText(tr("版本 %1 · 下载 %2 KiB")
      .arg(j["version"].toString(tr("未提供")))
      .arg(j["size"].toInteger() / 1024));
  m_details->setToolTip(tr("来源：%1\n安装位置：%2")
      .arg(j["url"].toString(), m_manager->rootPath()));
  m_download->setVisible(!installed);
  m_download->setEnabled(!busy && m_manager->downloadAvailable());
  m_import->setEnabled(!busy);
  m_enable->setVisible(installed);
  m_uninstall->setVisible(installed);
  m_enable->setEnabled(!busy);
  m_uninstall->setEnabled(!busy);
  m_enable->setText(m_manager->isEnabled() ? tr("停用组件") : tr("启用组件"));
  m_cancel->setVisible(busy);
  m_progress->setVisible(busy);
  if (busy) {
    m_progress->setRange(0, 0);
    m_status->setText(tr("正在校验、解压并测试组件…"));
  } else if (m_status->text().isEmpty())
    m_status->setText(
        installed ? (m_manager->isEnabled() ? tr("已启用")
                                            : tr("已停用；缓存章节仍可阅读"))
                  : m_manager->downloadAvailable()
                      ? tr("点击安装后即可搜索和阅读在线书籍。")
                      : tr("暂无下载地址，可选择本地组件安装。"));
}
void OnlineComponentDialog::importLocal() {
  QString meta = QFileDialog::getOpenFileName(this, tr("选择官方组件元数据"),
                                              {}, tr("JSON (*.json)"));
  if (meta.isEmpty())
    return;
  QFile f(meta);
  if (!f.open(QIODevice::ReadOnly) || f.size() > 16384) {
    m_status->setText(tr("无法读取组件元数据"));
    return;
  }
  auto j = QJsonDocument::fromJson(f.readAll()).object();
  QString e;
  if (!m_manager->validateMetadata(j, &e)) {
    m_status->setText(e);
    return;
  }
  QString path = QFileDialog::getOpenFileName(
      this, tr("选择对应组件压缩包"), {},
      tr("组件包 (*.tar *.tar.gz *.tar.zst *.zip);;所有文件 (*)"));
  if (path.isEmpty())
    return;
  if (QMessageBox::question(
          this, tr("安装本地组件"),
          tr("将安装并运行组件 %1（%2 KiB），以 SHA-256 "
             "校验并测试协议。\n位置：%3\n确认安装您信任的官方组件？")
              .arg(j["version"].toString())
              .arg(j["size"].toInteger() / 1024)
              .arg(m_manager->rootPath())) != QMessageBox::Yes)
    return;
  if (!m_manager->importPackage(path, j, &e))
    m_status->setText(e);
  refresh();
}
} // namespace reader
