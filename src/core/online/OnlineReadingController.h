#pragma once
#include "OnlineBook.h"
#include "OnlineEngineBridge.h"
namespace reader {
class OnlineReadingController : public QObject {
  Q_OBJECT
public:
  explicit OnlineReadingController(OnlineEngineBridge *,
                                   QObject *parent = nullptr);
  void setBook(std::shared_ptr<OnlineBook>);
  void requestChapter(int index);
  void retry();
  void cancel();
  void close();
  int targetChapter() const { return m_target; }
  QString session() const { return m_session; }
  std::shared_ptr<OnlineBook> book() const { return m_book; }
signals:
  void currentReady(int index);
  void currentError(int index, const QString &error);
  void backgroundStatus(int completed, int total);
  void chapterRequestStarted(int index);

private:
  void pump();
  OnlineEngineBridge *m_bridge;
  std::shared_ptr<OnlineBook> m_book;
  QString m_session, m_request;
  int m_target = -1, m_inflight = -1;
  bool m_cancelled = false;
};
} // namespace reader
