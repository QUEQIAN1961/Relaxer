#include "translation-browser-backend.h"
#include <utility>

namespace relaxer {
TranslationBrowserAdapter::TranslationBrowserAdapter(QString root,QObject *parent)
    :TranslationBrowserBackend(parent),m_root(std::move(root)){}
void TranslationBrowserAdapter::translate(quint64 token,const QString &provider,const QString &text,const QString &language){
    if(!m_browser){
        m_browser=std::make_unique<BrowserTranslation>(m_root);
        connect(m_browser.get(),&BrowserTranslation::completed,this,[this](quint64 t,const QString &s,const QString &e){
            if(t!=m_token)return;
            m_token=0;emit completed(t,s,e);
        });
        connect(m_browser.get(),&BrowserTranslation::needsAttention,this,[this](const QString &reason){if(m_token)emit attention(m_token,reason);});
        connect(m_browser.get(),&BrowserTranslation::resumed,this,[this]{if(m_token)emit resumed(m_token);});
    }
    m_token=token;m_browser->translate(token,provider,text,language);
}
void TranslationBrowserAdapter::cancel(){m_token=0;if(m_browser)m_browser->cancel();}
void TranslationBrowserAdapter::resume(){if(m_browser&&m_token)m_browser->resume();}
void TranslationBrowserAdapter::shutdown(){cancel();m_browser.reset();}
}
