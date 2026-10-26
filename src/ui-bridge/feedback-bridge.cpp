#include "feedback-bridge.h"
#include <QDateTime>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(relaxerFeedback, "relaxer.feedback")
namespace relaxer {
void FeedbackBridge::post(const QString &source, const QString &category, const QString &message,
                          const QString &details, const QString &operation, const QString &target,
                          bool retryable, bool actionRequired, int code) {
    if (m_closing || message.isEmpty()) return;
    const auto id = QString::number(++m_sequence);
    const QVariantMap event = {{"eventId", id}, {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"source", source}, {"category", category}, {"message", message.left(512)},
        {"details", details.left(16384)}, {"operationId", operation.isEmpty() ? id : operation},
        {"targetId", target}, {"retryable", retryable}, {"actionRequired", actionRequired},
        {"code", code}, {"presented", false}, {"detailsTruncated", details.size() > 16384}};
    m_history.prepend(event);
    while (m_history.size() > 20) m_history.removeLast();
    // Silent decisions must not erase an unrelated visible failure.
    if (category != "Cancellation" && category != "ConfirmationRequired") {
        m_current = event; m_visible = true;
    }
    // Metadata only: no titles, paths, lyric text, provider response, keys or tokens.
    qCInfo(relaxerFeedback).noquote() << event.value("timestamp").toString()
        << "event=" + id << "source=" + source << "category=" + category
        << "operation=" + event.value("operationId").toString() << "target=" + target << "code=" << code
        << (category == "Cancellation" || category == "ConfirmationRequired" ? "ui=local-decision" : "ui=pending");
    emit changed();
}
void FeedbackBridge::clearSource(const QString &source) {
    if (m_current.value("source") == source) dismiss();
}
void FeedbackBridge::dismiss() {
    if (!m_visible) return;
    m_visible = false; emit changed();
}
void FeedbackBridge::setClosing(bool closing) {
    if (m_closing == closing) return;
    m_closing = closing;
    if (closing) m_visible = false;
    emit changed();
}
void FeedbackBridge::presented(const QString &id) {
    if (!visible() || id != m_current.value("eventId").toString() || m_current.value("presented").toBool()) return;
    m_current["presented"] = true;
    for (auto &entry : m_history) if(entry.toMap().value("eventId").toString()==id) {entry=m_current;break;}
    qCInfo(relaxerFeedback).noquote() << "event=" + id << "ui=presented";
    // No changed(): acknowledging presentation must not restart the display timer.
}
}
