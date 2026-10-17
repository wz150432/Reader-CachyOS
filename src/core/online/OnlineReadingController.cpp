#include "OnlineReadingController.h"
#include <QUrl>
#include <QUuid>
namespace reader {
OnlineReadingController::OnlineReadingController(OnlineEngineBridge *bridge,
                                                 QObject *parent)
    : QObject(parent), m_bridge(bridge) {
  connect(
      bridge, &OnlineEngineBridge::result, this,
      [this](const QString &id, const QString &session, const QJsonObject &r) {
        if (session != m_session || id != m_request || !m_book)
          return;
        int index = m_inflight;
        m_request.clear();
        m_inflight = -1;
        QString e;
        const QUrl resolved(r.value("url").toString());
        if (!r.value("text").isString() || !resolved.isValid() ||
            resolved.host().isEmpty() ||
            (resolved.scheme() != "http" && resolved.scheme() != "https") ||
            !resolved.userInfo().isEmpty() ||
            !m_book->saveChapter(index, r.value("text").toString(), &e)) {
          m_cancelled = true;
          if (index == m_target)
            emit currentError(index, "Invalid chapter response");
          return;
        }
        if (index == m_target)
          emit currentReady(index);
        pump();
      });
  connect(
      bridge, &OnlineEngineBridge::error, this,
      [this](const QString &id, const QString &session, const QString &error) {
        if (session != m_session || id != m_request)
          return;
        int i = m_inflight;
        m_request.clear();
        m_inflight = -1;
        m_cancelled = true;
        if (i == m_target)
          emit currentError(i, error);
        else
          emit backgroundStatus(-1, 3);
      });
}
void OnlineReadingController::setBook(std::shared_ptr<OnlineBook> book) {
  close();
  m_book = std::move(book);
  m_session = QUuid::createUuid().toString(QUuid::WithoutBraces);
}
void OnlineReadingController::requestChapter(int index) {
  if (!m_book || index < 0 || index >= m_book->chapters().size())
    return;
  m_target = index;
  m_cancelled = false;
  if (m_inflight >= 0 &&
      (m_inflight < index || m_inflight > index + 3 ||
       (!m_book->chapterAvailable(index) && m_inflight != index))) {
    m_bridge->cancel(m_request);
    m_request.clear();
    m_inflight = -1;
  }
  if (m_book->chapterAvailable(index))
    emit currentReady(index);
  pump();
}
void OnlineReadingController::pump() {
  if (!m_book || m_cancelled || m_target < 0 || !m_request.isEmpty())
    return;
  int end = qMin(m_target + 3, int(m_book->chapters().size()) - 1);
  int completed = 0;
  for (int i = m_target; i <= end; ++i) {
    if (m_book->chapterAvailable(i)) {
      if (i != m_target)
        ++completed;
      continue;
    }
    m_inflight = i;
    m_request = m_bridge->request(
        "chapter",
        {{"source", m_book->source().toJson()}, {"url", m_book->chapterUrl(i)}},
        m_session);
    emit chapterRequestStarted(i);
    return;
  }
  emit backgroundStatus(completed, end - m_target);
}
void OnlineReadingController::retry() {
  if (m_target >= 0)
    requestChapter(m_target);
}
void OnlineReadingController::cancel() {
  m_cancelled = true;
  QString id = m_request;
  m_request.clear();
  m_inflight = -1;
  if (!id.isEmpty())
    m_bridge->cancel(id);
}
void OnlineReadingController::close() {
  cancel();
  m_session.clear();
  m_book.reset();
  m_target = -1;
}
} // namespace reader
