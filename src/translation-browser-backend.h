#pragma once
#include "browser-translation.h"
#include <memory>

namespace relaxer {
// Transport seam: all notifications carry the exact submitted segment token.
// A mock implements this interface without a browser, credentials or website.
class TranslationBrowserBackend : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void translate(quint64 token,const QString &provider,const QString &text,const QString &language)=0;
    virtual void cancel()=0;
    virtual void resume()=0;
    virtual void shutdown()=0;
signals:
    void completed(quint64 token,const QString &text,const QString &error);
    void attention(quint64 token,const QString &reason);
    void resumed(quint64 token);
};

class TranslationBrowserAdapter final : public TranslationBrowserBackend {
    Q_OBJECT
public:
    explicit TranslationBrowserAdapter(QString root,QObject *parent=nullptr);
    void translate(quint64 token,const QString &provider,const QString &text,const QString &language) override;
    void cancel() override;
    void resume() override;
    void shutdown() override;
private:
    QString m_root;
    std::unique_ptr<BrowserTranslation> m_browser;
    quint64 m_token=0;
};
}
