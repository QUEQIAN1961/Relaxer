#include "ai-bridge.h"
namespace relaxer {
AiBridge::AiBridge(ai::AiRuntime *runtime,QObject *parent):QObject(parent),m_runtime(runtime){
    resetDraft();connect(runtime,&ai::AiRuntime::configurationChanged,this,&AiBridge::resetDraft);
    connect(runtime,&ai::AiRuntime::modelCatalogFinished,this,[this](quint64 task,const QString &,const QList<ai::ModelInfo> &,const ai::ProviderError &error){
        if(task!=m_request)return;m_request=0;status(error.isEmpty()?QString("模型已刷新，请明确选择并保存"):error.message);
    });
}
void AiBridge::status(const QString &text){m_status=text;emit changed();}
void AiBridge::resetDraft(){
    const auto &c=m_runtime->configuration();
    m_draft={{"providerId",c.defaults.providerId},{"modelId",c.defaults.modelId},{"thinkingEnabled",c.defaults.thinkingEnabled},
        {"reasoningEffort",c.defaults.reasoningEffort},{"translationUsesDefaults",c.translationUsesDefaults},
        {"translationProviderId",c.translationOverride.providerId},{"translationModelId",c.translationOverride.modelId},
        {"translationThinkingEnabled",c.translationOverride.thinkingEnabled},{"translationReasoningEffort",c.translationOverride.reasoningEffort},
        {"targetLanguage",c.targetLanguage},{"translationStylePrompt",c.translationStylePrompt}};
    emit changed();
}
QVariantList AiBridge::models()const {
    QVariantList result;
    for(const auto &m:m_runtime->models())result.append(QVariantMap{{"id",m.id},{"providerId",m.providerId},
        {"label",m.displayName+" · "+ai::modelAvailabilityText(m.availability)},
        {"availability",int(m.availability)},{"reasoningEfforts",m.capabilities.reasoningEfforts}});
    return result;
}
QStringList AiBridge::defaultEfforts()const{
    const auto *model=m_runtime->model(m_draft["providerId"].toString(),m_draft["modelId"].toString());
    auto efforts=model?model->capabilities.reasoningEfforts:QStringList{};efforts.removeAll("off");return efforts;
}
QStringList AiBridge::translationEfforts()const{
    const auto *model=m_runtime->model(m_draft["translationProviderId"].toString(),m_draft["translationModelId"].toString());
    auto efforts=model?model->capabilities.reasoningEfforts:QStringList{};efforts.removeAll("off");return efforts;
}
bool AiBridge::edit(const QString &key,const QVariant &value){if(!m_draft.contains(key))return false;m_draft[key]=value;emit changed();return true;}
bool AiBridge::save(){
    auto c=m_runtime->configuration();
    if(m_draft["targetLanguage"].toString()!="zh-CN"){status("当前整首AI翻译仅支持简体中文目标语言");return false;}
    for(const auto &key:{"providerId","translationProviderId"}){
        const auto id=m_draft[key].toString();if(!id.isEmpty()&&id!="deepseek"){status("供应商无效");return false;}
    }
    c.defaults={m_draft["providerId"].toString(),m_draft["modelId"].toString(),m_draft["thinkingEnabled"].toBool(),m_draft["reasoningEffort"].toString()};
    c.translationUsesDefaults=m_draft["translationUsesDefaults"].toBool();
    c.translationOverride={m_draft["translationProviderId"].toString(),m_draft["translationModelId"].toString(),m_draft["translationThinkingEnabled"].toBool(),m_draft["translationReasoningEffort"].toString()};
    c.targetLanguage="zh-CN";c.translationStylePrompt=m_draft["translationStylePrompt"].toString();
    QString error;if(!m_runtime->saveConfiguration(c,&error)){status(error);return false;}
    status("AI配置已保存 · 保存配置不会发送请求");return true;
}
bool AiBridge::storeKey(QString secret){
    QString error;auto normalized=ai::normalizedApiKey(secret,&error);secret.fill(QChar());
    if(normalized.isEmpty()){status("API Key为空或包含非法字符");return false;}
    const bool ok=m_runtime->credentials()&&m_runtime->credentials()->store("deepseek",normalized,&error);normalized.fill(QChar());
    if(ok)m_credentialStatus="凭据已安全保存";
    status(ok?"凭据已保存；未发送网络请求":"凭据保存失败");return ok;
}
bool AiBridge::removeKey(){
    QString error;const bool ok=m_runtime->credentials()&&m_runtime->credentials()->remove("deepseek",&error);
    if(ok)m_credentialStatus="未保存凭据";status(ok?"凭据已移除":"凭据移除失败");return ok;
}
bool AiBridge::checkCredential(){
    QString error;const bool found=m_runtime->hasCredential("deepseek",&error);
    m_credentialStatus=found?"已保存凭据":error.isEmpty()?"未保存凭据":"无法访问凭据存储";emit changed();return found;
}
bool AiBridge::refreshModels(bool confirmed){
    if(!confirmed){status("请先确认连接DeepSeek并读取模型目录");return false;}
    if(busy())return false;
    m_request=m_runtime->requests()->fetchModels("deepseek");status("正在读取模型目录；不会发起歌词翻译");return m_request!=0;
}
}
