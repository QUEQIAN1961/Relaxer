#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QProcess>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <functional>
#include <memory>
class QLockFile;
namespace relaxer {
// A dedicated, visible browser profile. Never attaches to the user's personal browser.
class BrowserTranslation : public QObject {
    Q_OBJECT
public:
    explicit BrowserTranslation(const QString &dataRoot,QObject *parent=nullptr);
    ~BrowserTranslation() override;
    void translate(quint64 token,const QString &provider,const QString &text,const QString &sourceLanguage);
    void cancel();
    void resume();
    void showBrowser();
    bool waiting() const { return m_waiting; }
    static QString browserExecutable();
    static QUrl pageUrl(const QString &provider,const QString &text,const QString &language);
    static QString readScript(const QString &provider,const QString &text);
signals:
    void completed(quint64 token,const QString &translation,const QString &error);
    void needsAttention(const QString &reason);
    void resumed();
private:
    QString m_root,m_profile,m_provider,m_text,m_language,m_target,m_session,m_candidate;
    QString m_path;QByteArray m_bytes,m_fragments,m_key;
    QProcess m_process;QTcpSocket m_socket;QTimer m_timer;QElapsedTimer m_clock,m_stable;
    std::unique_ptr<QLockFile> m_lock;
    quint64 m_token=0,m_epoch=0;int m_serial=0;
    bool m_ready=false,m_busy=false,m_waiting=false,m_polling=false,m_submitted=false;
    bool m_handshake=false,m_closing=false;int m_fragmentOpcode=0;
    QHash<int,std::function<void(const QJsonObject &)>> m_commands;
    QHash<QString,QString> m_cache;
    void launch();void tick();void consume();void poll();void submit();
    void frame(int opcode,const QByteArray &payload);
    void command(const QString &method,const QJsonObject &params,std::function<void(const QJsonObject &)> callback={},bool session=true);
    void evaluate(const QString &script,std::function<void(const QJsonObject &)> callback);
    void finish(const QString &text,const QString &error={});
    void attention(const QString &reason);
    void disconnectBrowser();
};
}
