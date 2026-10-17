#pragma once
#include "ai/ai-runtime.h"
#include "library.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
using namespace relaxer::ai;
namespace {
class FakeCredentialStore final : public CredentialStore {
  public:
    bool store(const QString &providerId, const QString &secret, QString *error) override {
        if (providerId.isEmpty() || secret.isEmpty()) {
            if (error) *error = QStringLiteral("invalid test credential");
            return false;
        }
        m_values.insert(providerId, secret);
        if (error) error->clear();
        return true;
    }
    CredentialReadResult read(const QString &providerId) override {
        CredentialReadResult result;
        result.ok = true;
        result.found = m_values.contains(providerId);
        result.secret = m_values.value(providerId);
        return result;
    }
    bool remove(const QString &providerId, QString *error) override {
        m_values.remove(providerId);
        if (error) error->clear();
        return true;
    }
    bool supported() const override { return true; }
    bool allowsMockRequests() const override { return true; }
  private:
    QHash<QString, QString> m_values;
};

class TranslationHttpMock final : public QObject {
  public:
    explicit TranslationHttpMock(QObject *parent = nullptr) : QObject(parent) {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (m_server.hasPendingConnections()) {
                auto *socket = m_server.nextPendingConnection();
                m_buffers.insert(socket, {});
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] { consume(socket); });
                connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                    m_held.removeAll(socket);
                    m_buffers.remove(socket);
                    socket->deleteLater();
                });
            }
        });
    }
    ~TranslationHttpMock() override {
        for (auto *socket : m_buffers.keys()) {
            if (!socket) continue;
            disconnect(socket, nullptr, this, nullptr);
            socket->abort();
        }
    }
    bool listen() { return m_server.listen(QHostAddress::LocalHost); }
    QUrl baseUrl() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort())); }
    int requestCount() const { return m_requests.size(); }
    int heldCount() const { return m_held.size(); }
    QByteArray request(int index) const { return m_requests.value(index); }
    QByteArray requestBody(int index) const {
        const auto bytes = request(index);
        const auto split = bytes.indexOf("\r\n\r\n");
        return split < 0 ? QByteArray() : bytes.mid(split + 4);
    }
    void enqueue(QByteArray body, int status = 200) { m_responses.append({status, std::move(body)}); }
    void setHold(bool value) { m_hold = value; }
    bool releaseHeld() {
        while (!m_held.isEmpty()) {
            auto *socket = m_held.takeFirst();
            if (socket && socket->state() == QAbstractSocket::ConnectedState) {
                respond(socket);
                return true;
            }
        }
        return false;
    }
    bool releaseHeldAt(int index) {
        if(index<0||index>=m_held.size())return false;
        auto *socket=m_held.takeAt(index);
        if(!socket||socket->state()!=QAbstractSocket::ConnectedState)return false;
        respond(socket);return true;
    }
    void disconnectHeld() { for(auto *socket:m_held)socket->disconnectFromHost(); }
  private:
    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QList<QTcpSocket *> m_held;
    QList<QByteArray> m_requests;
    QList<QPair<int, QByteArray>> m_responses;
    bool m_hold = false;

    void consume(QTcpSocket *socket) {
        auto &buffer = m_buffers[socket];
        buffer += socket->readAll();
        const auto headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) return;
        qint64 length = 0;
        for (const auto &line : buffer.left(headerEnd).split('\n'))
            if (line.toLower().startsWith("content-length:"))
                length = line.mid(line.indexOf(':') + 1).trimmed().toLongLong();
        if (buffer.size() < headerEnd + 4 + length) return;
        m_requests.append(buffer.left(headerEnd + 4 + length));
        if (m_hold) {
            m_held.append(socket);
            return;
        }
        respond(socket);
    }
    void respond(QTcpSocket *socket) {
        if (!socket || m_responses.isEmpty()) return;
        const auto response = m_responses.takeFirst();
        QByteArray bytes = "HTTP/1.1 " + QByteArray::number(response.first) + " Result\r\n";
        bytes += "Content-Type: application/json\r\nContent-Length: " + QByteArray::number(response.second.size());
        bytes += "\r\nConnection: close\r\n\r\n" + response.second;
        socket->write(bytes);
        socket->flush();
        socket->disconnectFromHost();
    }
};

ModelInfo confirmedFlash(qint64 context = 32768) {
    ModelInfo model;
    model.providerId = QStringLiteral("deepseek");
    model.id = QStringLiteral("deepseek-flash");
    model.displayName = QStringLiteral("DeepSeek Flash");
    model.availability = ModelAvailability::Available;
    model.capabilities.inputModalities = {QStringLiteral("text")};
    model.capabilities.outputModalities = {QStringLiteral("text")};
    model.capabilities.contextWindow = context;
    model.capabilities.maxOutputTokens = 8192;
    model.capabilities.reasoningMetadataKnown = true;
    model.capabilities.reasoningEfforts = {QStringLiteral("low"), QStringLiteral("high"), QStringLiteral("max")};
    model.protocol = {true, true, true, true};
    return model;
}

QByteArray completion(const QString &content, const QString &finishReason = QStringLiteral("stop")) {
    const QJsonObject message{{"role", "assistant"}, {"content", content}, {"reasoning_content", "must not be used"}};
    const QJsonArray choices{QJsonObject{{"index", 0}, {"finish_reason", finishReason}, {"message", message}}};
    const QJsonObject root{{"choices", choices},
                           {"usage", QJsonObject{{"prompt_tokens", 17}, {"completion_tokens", 23}, {"total_tokens", 40}}}};
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray translationJson(const QList<QPair<int, QString>> &items) {
    QJsonArray translations;
    for (const auto &item : items)
        translations.append(QJsonObject{{"id", item.first}, {"text", item.second}});
    return QJsonDocument(QJsonObject{{"translations", translations}}).toJson(QJsonDocument::Compact);
}

Track seed(Library &library, const QString &title, const QString &key, const QString &artist = QStringLiteral("Test Artist")) {
    Track track;
    track.title = title;
    track.artist = artist;
    track.album = QStringLiteral("AI Translation Tests");
    track.hash = key;
    track.file = QStringLiteral("Music/%1.wav").arg(key);
    track.coverOrigin = QStringLiteral("none");
    QDir().mkpath(library.absolute(QStringLiteral("Music")));
    QFile audio(library.absolute(track.file));
    if (!audio.open(QIODevice::WriteOnly)) return {};
    audio.write("test audio");
    audio.close();
    track.id = library.insert(track);
    return library.track(track.id);
}

bool configureRuntime(AiRuntime &runtime, QString *error = nullptr) {
    runtime.acceptModelCatalog(QStringLiteral("deepseek"), {confirmedFlash()});
    AiServiceConfiguration configuration = runtime.configuration();
    configuration.defaults = {QStringLiteral("deepseek"), QStringLiteral("deepseek-flash"), false, {}};
    configuration.translationUsesDefaults = true;
    configuration.targetLanguage = QStringLiteral("zh-CN");
    configuration.translationStylePrompt = QStringLiteral("自然简洁，保留歌手的情绪。");
    if (!runtime.saveConfiguration(configuration, error)) return false;
    return runtime.credentials()->store(QStringLiteral("deepseek"), QStringLiteral("test-only-fake-key"), error);
}

bool readFile(const QString &path, QByteArray *bytes) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    *bytes = file.readAll();
    return file.error() == QFileDevice::NoError;
}

}
