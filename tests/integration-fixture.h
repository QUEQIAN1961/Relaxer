#pragma once
#include "ai/ai-runtime.h"
#include "import-coordinator.h"
#include "import-fixture.h"
#include "lyricslookup.h"
#include "playback-facade.h"
#include <QApplication>
#include <QDataStream>
#include <QFile>
#include <QJsonDocument>
#include <QMessageBox>
#include <QSemaphore>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <qscopeguard.h>
namespace integration_fixture {
using namespace relaxer;
using namespace relaxer::ai;
class FakeCredentialStore final : public CredentialStore {
  public:
    bool store(const QString &providerId, const QString &secret, QString *error) override {
        if (providerId.isEmpty() || secret.isEmpty()) {
            if (error)
                *error = QStringLiteral("invalid test credential");
            return false;
        }
        m_values.insert(providerId, secret);
        if (error)
            error->clear();
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
        if (error)
            error->clear();
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
            if (!socket)
                continue;
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
        if (headerEnd < 0)
            return;
        qint64 length = 0;
        for (const auto &line : buffer.left(headerEnd).split('\n'))
            if (line.toLower().startsWith("content-length:"))
                length = line.mid(line.indexOf(':') + 1).trimmed().toLongLong();
        if (buffer.size() < headerEnd + 4 + length)
            return;
        m_requests.append(buffer.left(headerEnd + 4 + length));
        if (m_hold) {
            m_held.append(socket);
            return;
        }
        respond(socket);
    }
    void respond(QTcpSocket *socket) {
        if (!socket || m_responses.isEmpty())
            return;
        const auto response = m_responses.takeFirst();
        QByteArray bytes = "HTTP/1.1 " + QByteArray::number(response.first) + " Result\r\n";
        bytes +=
            "Content-Type: application/json\r\nContent-Length: " + QByteArray::number(response.second.size());
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
    model.capabilities.reasoningEfforts = {QStringLiteral("low"), QStringLiteral("high"),
                                           QStringLiteral("max")};
    model.protocol = {true, true, true, true};
    return model;
}

QByteArray completion(const QString &content, const QString &finishReason = QStringLiteral("stop")) {
    const QJsonObject message{
        {"role", "assistant"}, {"content", content}, {"reasoning_content", "must not be used"}};
    const QJsonArray choices{
        QJsonObject{{"index", 0}, {"finish_reason", finishReason}, {"message", message}}};
    const QJsonObject root{
        {"choices", choices},
        {"usage", QJsonObject{{"prompt_tokens", 17}, {"completion_tokens", 23}, {"total_tokens", 40}}}};
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray translationJson(const QList<QPair<int, QString>> &items) {
    QJsonArray translations;
    for (const auto &item : items)
        translations.append(QJsonObject{{"id", item.first}, {"text", item.second}});
    return QJsonDocument(QJsonObject{{"translations", translations}}).toJson(QJsonDocument::Compact);
}

bool configureRuntime(AiRuntime &runtime, QString *error = nullptr) {
    runtime.acceptModelCatalog(QStringLiteral("deepseek"), {confirmedFlash()});
    AiServiceConfiguration configuration = runtime.configuration();
    configuration.defaults = {QStringLiteral("deepseek"), QStringLiteral("deepseek-flash"), false, {}};
    configuration.translationUsesDefaults = true;
    configuration.targetLanguage = QStringLiteral("zh-CN");
    configuration.translationStylePrompt = QStringLiteral("自然简洁，保留歌手的情绪。");
    if (!runtime.saveConfiguration(configuration, error))
        return false;
    return runtime.credentials()->store(QStringLiteral("deepseek"), QStringLiteral("test-only-fake-key"),
                                        error);
}

inline QByteArray bytes(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
inline bool wave(const QString &path, int serial = 1, int milliseconds = 10000) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    const quint32 size = milliseconds * 16;
    QDataStream out(&f);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + size);
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2)
        << quint16(16);
    out.writeRawData("data", 4);
    out << size;
    QByteArray pcm(size, '\0');
    for (int n = 0; n < 4; ++n)
        pcm[n] = char((serial >> (8 * n)) & 255);
    out.writeRawData(pcm.constData(), pcm.size());
    return out.status() == QDataStream::Ok;
}
inline QVector<qint64> seed(Library &lib, int count = 3, int duration = 10000) {
    QVector<qint64> ids;
    for (int n = 0; n < count; ++n) {
        Track t;
        t.title = QString("Integration song %1").arg(n);
        t.artist = "Test Artist";
        t.album = "Test Album";
        t.hash = QString("integration-%1").arg(n);
        t.file = QString("Music/fixture-%1.wav").arg(n);
        t.duration = duration;
        t.coverOrigin = "none";
        if (!wave(lib.absolute(t.file), n + 1, duration))
            return {};
        t.id = lib.insert(t);
        if (!t.id)
            return {};
        ids << t.id;
    }
    return ids;
}
inline void consent(LyricsLookup &dialog) {
    auto *classification=dialog.findChild<QPushButton *>("lyricsNeedsTranslation");
    if(!classification->isChecked())classification->click();
    QVERIFY(classification->isChecked());
    QTimer::singleShot(0, &dialog, [&dialog] {
        if (auto *box = dialog.findChild<QDialog *>("lyricsAiCloudConsent"))
            box->findChild<QPushButton *>("lyricsAiConsentSend")->click();
    });
    dialog.findChild<QPushButton *>("lyricsTranslate")->click();
}
inline void select(LyricsLookup &dialog, qint64 id) {
    auto *rows = dialog.findChild<QListWidget *>("lyricsTracks");
    for (int n = 0; n < rows->count(); ++n)
        if (rows->item(n)->data(Qt::UserRole).toLongLong() == id) {
            rows->setCurrentRow(n);
            return;
        }
}
inline QJsonObject candidate(const Track &t, const QString &text = "Hello world\nSecond line") {
    return {{"id", double(t.id)},
            {"trackName", t.title},
            {"artistName", t.artist},
            {"albumName", t.album},
            {"duration", t.duration / 1000.},
            {"plainLyrics", text},
            {"syncedLyrics",
             QString("[00:01.00]%1\n[00:02.00]%2").arg(text.section('\n', 0, 0), text.section('\n', 1, 1))}};
}
inline QVector<LyricsSearchRequest> requests(Library &lib, const QVector<qint64> &ids) {
    QVector<LyricsSearchRequest> result;
    for (auto id : ids) {
        LyricsSearchRequest r;
        r.original = lib.track(id);
        r.match = r.original;
        r.manual = true;
        r.replace = true;
        r.saveSnapshot = lib.lyricsSaveSnapshot(id);
        result << r;
    }
    return result;
}
inline void answers(TranslationHttpMock &server, const QJsonArray &records, int count = 200) {
    for (int n = 0; n < count; ++n)
        server.enqueue(QJsonDocument(records).toJson(QJsonDocument::Compact));
}
struct ImportGate {
    QSemaphore entered, released;
    ~ImportGate() { released.release(1000); }
};
class GatedImporter final : public Importer {
    std::shared_ptr<ImportGate> gate;

  public:
    GatedImporter(const QString &root, const QStringList &paths, std::shared_ptr<ImportGate> g)
        : Importer(root, paths), gate(std::move(g)) {}
    void run() override {
        gate->entered.release();
        gate->released.acquire();
        Importer::run();
    }
};
class GatedImport final : public ImportCoordinator {
    Library *library;

  public:
    std::shared_ptr<ImportGate> gate = std::make_shared<ImportGate>();
    explicit GatedImport(Library *lib) : ImportCoordinator(lib), library(lib) {}

  protected:
    Importer *createImporter(const ImportRequest &r, const std::shared_ptr<std::atomic_bool> &) override {
        auto *worker = new GatedImporter(library->root(), r.sources, gate);
        worker->setMaximumWorkers(r.maximumWorkers);
        return worker;
    }
};
} // namespace integration_fixture
