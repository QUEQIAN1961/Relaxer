#include "translation-bridge.h"
namespace relaxer {
TranslationBridge::TranslationBridge(Library *library,PlaybackFacade *facade,TranslationCoordinator *service,LyricsModel *lyrics,QObject *parent)
    :QObject(parent),m_library(library),m_facade(facade),m_service(service),m_lyrics(lyrics){
    connect(service,&TranslationCoordinator::taskChanged,this,[this](const TranslationSummary &value){
        if(m_tasks.value(value.trackId)!=value.taskId)return;
        if(value.trackId==m_facade->current().id)refresh();else emit changed();
    });
    connect(facade,&PlaybackFacade::trackChanged,this,[this]{
        if(m_consent&&m_service->summary(m_consent).trackId!=m_facade->current().id)declineConsent();
        m_error.clear();refresh();
    });
    connect(library,&Library::changed,this,&TranslationBridge::refresh);
}
TranslationSummary TranslationBridge::summary()const{return m_service->summary(m_tasks.value(m_facade->current().id));}
QString TranslationBridge::taskKey()const{const auto s=summary();return s.taskId?QString::number(s.taskId):QString();}
bool TranslationBridge::unfinished()const{for(auto task:m_tasks)if(!m_service->summary(task).terminal())return true;return false;}
QString TranslationBridge::state()const {
    const auto s=summary();if(!s.taskId)return "NoTask";
    switch(s.state){
    case TranslationState::WaitingForConsent:return "WaitingForConsent";case TranslationState::Queued:return "Queued";
    case TranslationState::Running:return "Running";case TranslationState::NeedsAttention:return "NeedsAttention";
    case TranslationState::CompletedPendingSave:return "CompletedPendingSave";case TranslationState::Saved:return "Saved";
    case TranslationState::Cancelled:return "Cancelled";case TranslationState::Failed:return "Failed";case TranslationState::Invalidated:return "Invalidated";
    }return {};
}
QString TranslationBridge::status()const{return !m_error.isEmpty()?m_error:summary().taskId?summary().status:QString("已有译文从本地读取；云端翻译需逐次明确授权");}
QString TranslationBridge::details()const{return summary().details;}
int TranslationBridge::classification()const{return int(m_library->translationJudgment(m_facade->current().id));}
void TranslationBridge::refresh(){m_lyrics->presentTranslation(summary());emit changed();}
bool TranslationBridge::classify(int value){
    if(!m_facade->current().id||value<0||value>2)return false;
    const bool ok=m_library->setTranslationJudgment(m_facade->current().id,TranslationJudgment(value));refresh();return ok;
}
QString TranslationBridge::prepared(quint64 task,const QString &error){
    m_error=error;
    if(task){
        m_tasks.insert(m_facade->current().id,task);m_consent=task;
        // Retire only old terminal summaries; pending/preview state is retained.
        if(m_tasks.size()>32)for(auto it=m_tasks.begin();it!=m_tasks.end();){
            if(it.value()!=task&&m_service->summary(it.value()).terminal()){m_service->forget(it.value());it=m_tasks.erase(it);}else ++it;
        }
    }
    refresh();return task?QString::number(task):QString();
}
bool TranslationBridge::canPrepare(){
    if(summary().taskId&&!summary().terminal()){m_error="已有任务或未保存预览，请先保存、取消或明确放弃";emit changed();return false;}
    for(auto it=m_tasks.begin();it!=m_tasks.end();){
        const auto old=m_service->summary(it.value());
        if(old.terminal()||!old.taskId){m_service->forget(it.value());it=m_tasks.erase(it);}else ++it;
    }
    return true;
}
QString TranslationBridge::prepareAi(){if(!canPrepare())return {};QString error;const auto task=m_service->prepareAi(m_facade->current().id,&error);return prepared(task,error);}
QString TranslationBridge::prepareBrowser(const QString &provider,const QString &language){if(!canPrepare())return {};QString error;const auto task=m_service->prepareBrowser(m_facade->current().id,provider,language,&error);return prepared(task,error);}
bool TranslationBridge::start(const QString &exactConsentTask){
    bool valid=false;const auto task=exactConsentTask.toULongLong(&valid);
    if(!valid||!task||task!=m_consent||task!=m_tasks.value(m_facade->current().id)||m_service->summary(task).state!=TranslationState::WaitingForConsent){m_error="授权身份已失效，本次未发送";emit changed();return false;}
    m_consent=0;m_error.clear();const bool ok=m_service->start(task,&m_error);refresh();return ok;
}
void TranslationBridge::declineConsent(){if(m_consent)m_service->discard(m_consent);m_consent=0;refresh();}
bool TranslationBridge::cancel(){const bool ok=m_service->cancel(summary().taskId);refresh();return ok;}
bool TranslationBridge::discard(){const auto task=summary().taskId;const bool ok=m_service->discard(task);if(m_consent==task)m_consent=0;refresh();return ok;}
bool TranslationBridge::save(){m_error.clear();const bool ok=m_service->save(summary().taskId,&m_error);m_lyrics->reload();refresh();return ok;}
bool TranslationBridge::resume(){const bool ok=m_service->resume(summary().taskId);refresh();return ok;}
}
