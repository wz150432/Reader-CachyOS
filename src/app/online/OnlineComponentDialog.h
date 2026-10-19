#pragma once
#include "app/MenuPanel.h"
class QLabel;
class QProgressBar;
class QPushButton;
namespace reader {
class OnlineComponentManager;
class OnlineComponentDialog : public MenuPanel {
  Q_OBJECT

public:
  explicit OnlineComponentDialog(OnlineComponentManager *,
                                 QWidget *parent = nullptr);

signals:
  void ready();

private:
  void refresh();
  void importLocal();
  OnlineComponentManager *m_manager;
  QLabel *m_details, *m_status;
  QProgressBar *m_progress;
  QPushButton *m_download, *m_import, *m_enable, *m_uninstall, *m_cancel;
};
} // namespace reader
