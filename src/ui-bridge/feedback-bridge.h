#pragma once
#include <QObject>
#include <QVariantMap>
#include <QVariantList>

namespace relaxer {
// Shell-owned presentation adapter. It never retries, saves or makes business decisions.
class FeedbackBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(QVariantList history READ history NOTIFY changed)
    Q_PROPERTY(bool visible READ visible NOTIFY changed)
public:
    explicit FeedbackBridge(QObject *parent = nullptr) : QObject(parent) {}
    QVariantMap current() const { return m_current; }
    QVariantList history() const { return m_history; }
    bool visible() const { return m_visible && !m_closing; }
    void post(const QString &source, const QString &category, const QString &message,
              const QString &details = {}, const QString &operation = {}, const QString &target = {},
              bool retryable = false, bool actionRequired = false, int code = 0);
    void clearSource(const QString &source);
    Q_INVOKABLE void dismiss();
    Q_INVOKABLE void setClosing(bool closing);
    Q_INVOKABLE void presented(const QString &eventId);
signals:
    void changed();
private:
    quint64 m_sequence = 0;
    QVariantMap m_current;
    QVariantList m_history;
    bool m_visible = false, m_closing = false;
};
}
