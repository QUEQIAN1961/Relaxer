#pragma once

#include "ai-provider.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>

namespace relaxer::ai {

class DeepSeekProvider final : public AiProvider {
    Q_OBJECT
  public:
    explicit DeepSeekProvider(QUrl baseUrl = QUrl(QStringLiteral("https://api.deepseek.com")),
                              QObject *parent = nullptr);
    struct MockEndpoint { QUrl url; };
    explicit DeepSeekProvider(MockEndpoint endpoint, QObject *parent = nullptr);
    ~DeepSeekProvider() override;
    QString providerId() const override;
    bool usesMockEndpoint() const override { return m_mockEndpoint; }
    bool cancel(quint64 requestId) override;
    QUrl endpoint(const QString &path) const;
    void setTimeoutMs(int timeoutMs);
    int timeoutMs() const { return m_timeoutMs; }

  private:
    enum class Kind { Models, Completion };
    struct Pending {
        Kind kind = Kind::Models;
        QPointer<QNetworkReply> reply;
        QPointer<QTimer> timer;
    };
    quint64 listModels(const QString &apiKey) override;
    quint64 complete(const QString &apiKey, const ChatCompletionRequest &request) override;
    QString endpointError() const;
    bool m_mockEndpoint = false;
    QNetworkAccessManager m_network;
    QUrl m_baseUrl;
    QHash<quint64, Pending> m_pending;
    quint64 m_nextRequestId = 0;
    int m_timeoutMs = 30000;
    quint64 send(Kind kind, const QString &apiKey, const QByteArray &body = {});
    void finish(quint64 requestId);
};

} // namespace relaxer::ai
