#include "translation-coordinator.h"
#include "lyrics-document-reader.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QThread>
#include <QTimer>
#include <atomic>

namespace relaxer {
namespace {
std::atomic<quint64> nextIdentity{0};
quint64 identity(){return ++nextIdentity;}
bool fail(QString *error,const QString &message){if(error)*error=message;return false;}
QString usageText(const ai::TokenUsage &usage){
    return usage.available?QStringLiteral("费用按 DeepSeek 实际计费；本次 token：输入 %1，输出 %2，总计 %3。")
        .arg(usage.inputTokens).arg(usage.outputTokens).arg(usage.totalTokens):
        QStringLiteral("本次 token 用量未知；费用以 DeepSeek 实际账单为准。");
}
}
bool TranslationSummary::terminal() const {
    return state==TranslationState::Saved||state==TranslationState::Cancelled||state==TranslationState::Failed||state==TranslationState::Invalidated;
}
bool TranslationSummary::pending() const {
    return state==TranslationState::Queued||state==TranslationState::Running||state==TranslationState::NeedsAttention;
}
TranslationCoordinator::TranslationCoordinator(Library *library,ai::AiRuntime *runtime,QObject *parent,
                                               std::unique_ptr<TranslationBrowserBackend> browser)
    :QObject(parent),m_library(library),m_runtime(runtime),m_browser(std::move(browser)){
    qRegisterMetaType<TranslationSummary>();
    if(!m_browser)m_browser=std::make_unique<TranslationBrowserAdapter>(library?library->root():QString());
    if(runtime&&runtime->requests()){
        connect(runtime->requests(),&ai::AiRequestCoordinator::completionFinished,this,
            [this](quint64 id,const QString &,const ai::ChatCompletionResult &result){completeAi(id,result);});
        connect(runtime,&QObject::destroyed,this,[this]{
            for(const auto &task:m_tasks.values())if(task->value.method==TranslationMethod::DeepSeek&&!task->value.terminal())
                finish(task,TranslationState::Invalidated,QStringLiteral("翻译失败"),QStringLiteral("AI 服务已关闭"),"runtime-destroyed");
        });
    }
    connect(library,&QObject::destroyed,this,[this]{shutdown();});
    connect(m_browser.get(),&TranslationBrowserBackend::completed,this,&TranslationCoordinator::completeBrowser,Qt::QueuedConnection);
    connect(m_browser.get(),&TranslationBrowserBackend::attention,this,[this](quint64 token,const QString &reason){
        const auto task=m_tasks.value(m_active);
        if(!task||task->segmentToken!=token||task->value.state!=TranslationState::Running)return;
        task->value.state=TranslationState::NeedsAttention;task->value.status=QStringLiteral("等待人工验证");task->value.details=reason;publish(task);
    },Qt::QueuedConnection);
    connect(m_browser.get(),&TranslationBrowserBackend::resumed,this,[this](quint64 token){
        const auto task=m_tasks.value(m_active);
        if(!task||task->segmentToken!=token||task->value.state!=TranslationState::NeedsAttention)return;
        task->value.state=TranslationState::Running;task->value.status=QStringLiteral("正在翻译");publish(task);
    },Qt::QueuedConnection);
}
TranslationCoordinator::~TranslationCoordinator(){shutdown();}
bool TranslationCoordinator::ownerThread() const {
    return QThread::currentThread()==thread()&&(!m_library||m_library->thread()==thread())&&
        (!m_runtime||m_runtime->thread()==thread())&&(!m_browser||m_browser->thread()==thread());
}
QByteArray TranslationCoordinator::configurationDigest() const {
    if(!m_runtime)return {};
    const auto configuration=m_runtime->configuration();const auto task=m_runtime->translationConfiguration();
    const QJsonObject snapshot{{"providerId",task.providerId},{"modelId",task.modelId},
        {"thinkingEnabled",task.thinkingEnabled},{"reasoningEffort",task.reasoningEffort},
        {"targetLanguage",configuration.targetLanguage},{"translationStylePrompt",configuration.translationStylePrompt},
        {"systemPrompt",ai::lyrics::systemPrompt()}};
    return QCryptographicHash::hash(QJsonDocument(snapshot).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);
}
quint64 TranslationCoordinator::prepareAi(qint64 id,QString *error){return prepare(id,TranslationMethod::DeepSeek,{}, {},error);}
quint64 TranslationCoordinator::prepareBrowser(qint64 id,const QString &provider,const QString &language,QString *error){
    return prepare(id,TranslationMethod::Browser,provider,language,error);
}
quint64 TranslationCoordinator::prepare(qint64 id,TranslationMethod method,const QString &provider,const QString &language,QString *error){
    if(error)error->clear();
    auto reject=[error](const QString &message)->quint64{fail(error,message);return 0;};
    if(!ownerThread())return reject(QStringLiteral("翻译服务必须在 Library 所属线程调用"));
    if(m_stopping||!m_library)return reject(QStringLiteral("翻译服务已关闭"));
    if(m_tasks.size()>=32)return reject(QStringLiteral("翻译任务已达上限，请释放已结束任务"));
    const auto track=m_library->track(id);
    if(!track.id||track.lyrics.isEmpty())return reject(QStringLiteral("没有已保存的原文歌词，无法翻译"));
    const auto document=LyricsDocumentReader::readFile(m_library->absolute(track.lyrics));
    if(!document.opened||document.tooLarge||!document.complete)return reject(QStringLiteral("歌词文件无法完整读取或超过 4 MB。"));
    const auto prepared=ai::lyrics::prepare(document.bytes,track.title,track.artist);
    if(!prepared.ok())return reject(prepared.error);
    auto task=std::make_shared<Task>();task->track=track;task->source=document.bytes;task->sourceLanguage=language;
    auto &value=task->value;value.trackId=id;value.trackHash=track.hash;value.lyricsPath=track.lyrics;
    value.method=method;value.prepared=prepared;value.sourceDigest=prepared.sourceDigest;
    value.totalLines=int(prepared.lines.size());value.targetLanguage="zh-CN";
    value.status=QStringLiteral("等待确认");
    if(method==TranslationMethod::DeepSeek){
        if(!m_runtime||!m_runtime->requests())return reject(QStringLiteral("AI 服务不可用。请从主窗口打开歌词翻译。"));
        task->configuration=m_runtime->translationConfiguration();
        const auto validation=m_runtime->validateConfiguration(task->configuration);
        if(!validation.isEmpty())return reject(validation);
        const auto *model=m_runtime->model(task->configuration.providerId,task->configuration.modelId);
        if(!model)return reject(QStringLiteral("所选模型不可用，请在 AI 服务设置中刷新模型列表。"));
        const auto service=m_runtime->configuration();
        value.targetLanguage=service.targetLanguage.trimmed().isEmpty()?QStringLiteral("zh-CN"):service.targetLanguage.trimmed();
        if(value.targetLanguage!="zh-CN")return reject(QStringLiteral("当前只支持简体中文目标语言，请检查 AI 翻译设置；本次未发送请求。"));
        auto &request=task->request;request.modelId=task->configuration.modelId;
        request.messages=ai::lyrics::chatMessages(value.targetLanguage,service.translationStylePrompt,prepared);
        request.thinkingEnabled=task->configuration.thinkingEnabled;request.reasoningEffort=task->configuration.reasoningEffort;
        request.maxTokens=model->capabilities.maxOutputTokens;
        if(model->capabilities.contextWindow<=0||request.maxTokens<=0||
           ai::lyrics::inputTokenUpperBound(request.messages)>model->capabilities.contextWindow-request.maxTokens)
            return reject(QStringLiteral("整首歌词与输出上限无法放入所选模型的已确认上下文窗口；本次未发送请求。请手动选择兼容模型或精简歌词后再试。"));
        value.providerId=task->configuration.providerId;value.configurationDigest=configurationDigest();
    }else{
        if(provider!="bing"&&provider!="baidu")return reject(QStringLiteral("不支持的浏览器翻译来源"));
        value.providerId=provider;
        value.configurationDigest=QCryptographicHash::hash((provider+'\n'+language+"\nzh-CN").toUtf8(),QCryptographicHash::Sha256);
    }
    task->saveSnapshot=m_library->lyricsSaveSnapshot(id);
    if(!task->saveSnapshot.valid)return reject(task->saveSnapshot.error);
    // Snapshot creation itself does file I/O: ensure bytes and save guard agree.
    QString snapshotError;
    if(!current(task,&snapshotError))return reject(snapshotError);
    value.taskId=identity();value.generation=++m_generation;
    m_tasks.insert(value.taskId,task);publish(task);return value.taskId;
}
bool TranslationCoordinator::current(const TaskPtr &task,QString *error) const {
    if(!ownerThread()||!m_library)return fail(error,QStringLiteral("歌曲身份已失效，未保存译文"));
    const auto track=m_library->track(task->value.trackId);
    if(!track.id||track.id!=task->track.id||track.hash!=task->track.hash||track.title!=task->track.title||
       track.artist!=task->track.artist||track.lyrics!=task->track.lyrics)
        return fail(error,QStringLiteral("歌曲身份或歌词文件已变化，旧译文已丢弃"));
    const auto file=LyricsDocumentReader::readFile(m_library->absolute(track.lyrics));
    if(!file.opened||file.tooLarge||!file.complete)return fail(error,QStringLiteral("当前歌词无法完整读取，旧译文已丢弃"));
    if(QCryptographicHash::hash(file.bytes,QCryptographicHash::Sha256)!=task->value.sourceDigest)
        return fail(error,QStringLiteral("歌词内容已变化，旧译文已丢弃"));
    if(task->value.method==TranslationMethod::DeepSeek&&(!m_runtime||configurationDigest()!=task->value.configurationDigest))
        return fail(error,QStringLiteral("AI 翻译配置已变化，请重新翻译"));
    if(!m_library->lyricsSnapshotMatches(task->saveSnapshot,error))return false;
    if(error)error->clear();
    return true;
}
TranslationSummary TranslationCoordinator::summary(quint64 id) const {
    if(!ownerThread())return {};
    const auto task=m_tasks.value(id);return task?task->value:TranslationSummary{};
}
void TranslationCoordinator::publish(const TaskPtr &task){
    const auto value=task->value;
    QTimer::singleShot(0,this,[this,value]{
        QPointer<TranslationCoordinator> guard(this);emit taskChanged(value);
        if(guard&&value.terminal())emit finished(value);
    });
}
void TranslationCoordinator::finish(const TaskPtr &task,TranslationState state,const QString &status,const QString &details,const QString &code){
    if(task->value.terminal())return;
    const auto request=task->requestId;task->requestId=0;task->segmentToken=0;
    m_queue.removeAll(task->value.taskId);
    if(m_active==task->value.taskId){
        m_active=0;if(task->value.method==TranslationMethod::Browser)m_browser->cancel();
    }
    task->value.state=state;task->value.status=status;task->value.details=details;task->value.errorCode=code;
    task->source.clear();task->merged.clear();task->request={};task->value.translations.clear();task->value.prepared={};
    // Retire business identity before request cancel emits its completion signal.
    if(request&&m_runtime&&m_runtime->requests()->isPending(request))m_runtime->requests()->cancel(request);
    publish(task);schedule();
}
bool TranslationCoordinator::start(quint64 id,QString *error){
    if(!ownerThread())return fail(error,QStringLiteral("翻译服务线程错误"));
    const auto task=m_tasks.value(id);
    if(m_stopping||!task||task->value.state!=TranslationState::WaitingForConsent)return false;
    QString message;
    if(!current(task,&message)){finish(task,TranslationState::Invalidated,QStringLiteral("未发送"),message,"snapshot");return fail(error,message);}
    task->value.state=TranslationState::Queued;task->value.status=QStringLiteral("等待翻译");
    m_queue.enqueue(id);publish(task);schedule();if(error)error->clear();
    return true;
}
void TranslationCoordinator::schedule(){
    if(m_stopping||m_pumpScheduled)return;
    m_pumpScheduled=true;QTimer::singleShot(0,this,[this]{m_pumpScheduled=false;pump();});
}
void TranslationCoordinator::pump(){
    if(m_active||m_stopping)return;
    while(!m_queue.isEmpty()){
        const auto task=m_tasks.value(m_queue.dequeue());if(!task||task->value.state!=TranslationState::Queued)continue;
        QString error;if(!current(task,&error)){finish(task,TranslationState::Invalidated,QStringLiteral("未发送"),error,"snapshot");continue;}
        if(task->value.method==TranslationMethod::DeepSeek){
            error=m_runtime->validateConfiguration(task->configuration);
            if(!error.isEmpty()){finish(task,TranslationState::Failed,QStringLiteral("翻译失败"),error,"configuration");continue;}
        }
        m_active=task->value.taskId;task->value.state=TranslationState::Running;
        task->value.status=QStringLiteral("正在翻译");task->value.details=QStringLiteral("已发送一次整首歌词请求，正在等待 DeepSeek 响应，可取消。");
        if(task->value.method==TranslationMethod::DeepSeek)
            task->requestId=m_runtime->requests()->complete(task->configuration.providerId,task->request);
        else dispatchSegment(task);
        publish(task);return;
    }
}
void TranslationCoordinator::dispatchSegment(const TaskPtr &task){
    task->segmentToken=identity();const auto index=task->value.completedLines;
    m_browser->translate(task->segmentToken,task->value.providerId,task->value.prepared.lines.at(index).text,task->sourceLanguage);
}
void TranslationCoordinator::completeAi(quint64 requestId,const ai::ChatCompletionResult &result){
    const auto task=m_tasks.value(m_active);
    if(!task||task->value.method!=TranslationMethod::DeepSeek||!requestId||requestId!=task->requestId||task->value.state!=TranslationState::Running)return;
    task->requestId=0;const auto usage=usageText(result.usage);QString error;
    if(!current(task,&error)){finish(task,TranslationState::Invalidated,QStringLiteral("翻译失败"),QStringLiteral("翻译失败：")+error+"。 "+usage,"snapshot");return;}
    if(!result.ok()){finish(task,TranslationState::Failed,QStringLiteral("翻译失败"),QStringLiteral("翻译失败：")+result.error.message+"。 "+usage,result.error.code);return;}
    const auto validation=ai::lyrics::validateResponse(result.content,task->value.prepared);
    if(!validation.ok()){finish(task,TranslationState::Failed,QStringLiteral("翻译失败"),QStringLiteral("翻译失败：")+validation.error+"。 "+usage,"response");return;}
    ready(task,validation.translations,usage);
}
void TranslationCoordinator::completeBrowser(quint64 token,const QString &text,const QString &error){
    const auto task=m_tasks.value(m_active);
    if(!task||task->value.method!=TranslationMethod::Browser||task->segmentToken!=token||!token||
       (task->value.state!=TranslationState::Running&&task->value.state!=TranslationState::NeedsAttention))return;
    task->segmentToken=0;
    if(!error.isEmpty()){finish(task,TranslationState::Failed,QStringLiteral("翻译失败"),error,"browser");return;}
    // Reuse the strict complete-response validator even for each browser line.
    auto single=task->value.prepared;single.lines={single.lines.at(task->value.completedLines)};single.lines[0].id=0;
    const auto content=QString::fromUtf8(QJsonDocument(QJsonObject{{"translations",QJsonArray{QJsonObject{
        {"id",single.lines.first().id},{"text",text}}}}}).toJson(QJsonDocument::Compact));
    const auto validation=ai::lyrics::validateResponse(content,single);
    if(!validation.ok()){finish(task,TranslationState::Failed,QStringLiteral("翻译失败"),validation.error,"partial");return;}
    task->value.translations.append(validation.translations.first());++task->value.completedLines;
    task->value.state=TranslationState::Running;
    if(task->value.completedLines==task->value.totalLines){ready(task,task->value.translations);return;}
    publish(task);QTimer::singleShot(0,this,[this,task]{
        if(m_active==task->value.taskId&&task->value.state==TranslationState::Running)dispatchSegment(task);
    });
}
void TranslationCoordinator::ready(const TaskPtr &task,const QStringList &translations,const QString &usage){
    QString error;
    if(!current(task,&error)){finish(task,TranslationState::Invalidated,QStringLiteral("翻译失败"),error,"snapshot");return;}
    task->merged=ai::lyrics::merge(task->source,task->value.prepared,translations,&error);
    if(task->merged.isEmpty()){finish(task,TranslationState::Failed,QStringLiteral("翻译失败"),error,"merge");return;}
    task->value.translations=translations;task->value.completedLines=task->value.totalLines;
    task->value.state=TranslationState::CompletedPendingSave;task->value.status=QStringLiteral("翻译完成");
    task->value.details=QStringLiteral("翻译完成，请核对双栏预览后再保存。 ");
    const auto masked=translations.count(ai::lyrics::blockedLinePlaceholder());
    if(masked)task->value.details+=QStringLiteral("译文含 %1 行屏蔽占位，请核对；原文保留。 ").arg(masked);
    task->value.details+=usage;task->request={};m_active=0;publish(task);schedule();
}
bool TranslationCoordinator::cancel(quint64 id){
    if(!ownerThread())return false;
    const auto task=m_tasks.value(id);
    if(!task||task->value.terminal()||task->value.hasPreview())return false;
    finish(task,TranslationState::Cancelled,QStringLiteral("已取消"),QStringLiteral("本地翻译任务已取消；远端可能继续处理或计费。"));return true;
}
bool TranslationCoordinator::discard(quint64 id){
    if(!ownerThread())return false;
    const auto task=m_tasks.value(id);
    if(!task||task->value.terminal())return false;
    finish(task,TranslationState::Cancelled,QStringLiteral("已取消"),QStringLiteral("已明确放弃本地翻译任务或未保存结果。"));return true;
}
bool TranslationCoordinator::resume(quint64 id){
    if(!ownerThread())return false;
    const auto task=m_tasks.value(id);
    if(!task||m_active!=id||task->value.state!=TranslationState::NeedsAttention)return false;
    QString error;
    if(!current(task,&error)){finish(task,TranslationState::Invalidated,QStringLiteral("翻译失败"),error,"snapshot");return false;}
    // Change state before backend call: repeated clicks cannot dispatch twice.
    task->value.state=TranslationState::Running;task->value.status=QStringLiteral("正在翻译");publish(task);m_browser->resume();return true;
}
bool TranslationCoordinator::save(quint64 id,QString *error){
    if(!ownerThread())return fail(error,QStringLiteral("翻译服务线程错误"));
    const auto task=m_tasks.value(id);
    if(!task||!task->value.hasPreview())return false;
    QString message;if(!current(task,&message)){finish(task,TranslationState::Invalidated,QStringLiteral("未保存"),message,"snapshot");return fail(error,message);}
    if(!m_library->saveLyricsChecked(task->value.trackId,task->merged,task->saveSnapshot,true)){
        message=m_library->lastError().isEmpty()?QStringLiteral("歌词保存失败，原歌词仍保留"):m_library->lastError();
        task->value.status=QStringLiteral("未保存");task->value.details=QStringLiteral("未保存：")+message;task->value.errorCode="save";publish(task);return fail(error,message);
    }
    finish(task,TranslationState::Saved,QStringLiteral("已保存"),QStringLiteral("双语歌词已保存，原文与时间戳保持不变。"));if(error)error->clear();
    return true;
}
bool TranslationCoordinator::forget(quint64 id){
    if(!ownerThread())return false;
    const auto task=m_tasks.value(id);
    if(!task||!task->value.terminal())return false;
    return m_tasks.remove(id)>0;
}
void TranslationCoordinator::shutdown(){
    if(!ownerThread())return;
    if(m_stopping)return;
    m_stopping=true;m_queue.clear();
    for(const auto &task:m_tasks.values())if(!task->value.terminal())
        finish(task,TranslationState::Cancelled,QStringLiteral("已取消"),QStringLiteral("翻译服务已关闭"));
    if(m_browser)m_browser->shutdown();
}
}
