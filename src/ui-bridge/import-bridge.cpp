#include "import-bridge.h"
#include "library.h"
namespace relaxer {
ImportBridge::ImportBridge(Library *library,ImportCoordinator *coordinator,QObject *parent)
    :QObject(parent),m_library(library),m_coordinator(coordinator){
    connect(coordinator,&ImportCoordinator::progress,this,[this](quint64 task,int current,int total,const QString &name){
        if(task!=m_task)return;m_current=current;m_total=total;m_status=name;emit changed();
    });
    connect(coordinator,&ImportCoordinator::finished,this,[this](const ImportSummary &s){
        if(s.taskId!=m_task)return;
        m_task=0;m_current=s.processed;m_total=s.totalFiles;
        const QString category = s.cancelled ? "Cancellation" :
            (s.failed || s.playlistFailures || !s.fatalError.isEmpty()) ? (s.added+s.duplicates>0 ? "Warning" : "Error") :
            s.ignoredPaths>0 ? "Warning" : "Information";
        const int omitted = qMax(0,s.failed+s.playlistFailures-int(s.errors.size()));
        m_result={{"taskId",QString::number(s.taskId)},{"category",category},{"total",s.totalFiles},{"processed",s.processed},
            {"skipped",s.skipped+s.duplicates},{"omittedErrors",omitted},{"added",s.added},{"duplicates",s.duplicates},{"failed",s.failed},{"ignoredPaths",s.ignoredPaths},
            {"cancelled",s.cancelled},{"partialSuccess",s.partialSuccess},{"unstarted",s.unstarted},
            {"errors",s.errors},{"fatalError",s.fatalError},{"playlistFailures",s.playlistFailures}};
        m_status=QString("%1 · 新增%2，重复%3，失败%4，未开始%5").arg(s.cancelled?"已取消":category=="Error"?"导入失败":s.partialSuccess?"部分完成":"导入完成").arg(s.added).arg(s.duplicates).arg(s.failed).arg(s.unstarted);
        if(s.ignoredPaths>0){
            if(!s.cancelled&&s.fatalError.isEmpty()&&s.added+s.duplicates+s.failed==0)
                m_status+=" · 未找到可导入的音频文件，部分路径可能不存在或已被忽略。";
            else m_status+=QString(" · 已忽略%1个路径").arg(s.ignoredPaths);
        }
        if(!s.fatalError.isEmpty())m_status+=" · "+s.fatalError;
        QString details=QString("扫描到 %1 项；已处理 %2 项；新增 %3；重复 %4；失败 %5；未开始 %6；忽略路径 %7；加入歌单失败 %8。")
            .arg(s.totalFiles).arg(s.processed).arg(s.added).arg(s.duplicates).arg(s.failed).arg(s.unstarted).arg(s.ignoredPaths).arg(s.playlistFailures);
        if(!s.fatalError.isEmpty())details+='\n'+s.fatalError;
        for(const auto &error:s.errors)details+='\n'+error.left(1500);
        if(omitted)details+=QString("\n详情仅保留前 %1 项，另有 %2 项未展示。").arg(s.errors.size()).arg(omitted);
        if(s.ignoredPaths)details+="\n忽略路径由扫描器报告，可能不存在或类型不受支持；扫描器未提供逐路径原因。";
        m_result["details"]=details;
        if(!m_closing)emit completed(category,QString::number(s.taskId),m_status,details);
        // The existing coordinator deliberately batches Library signals.
        // Notify projections once at its committed/thread-exit barrier.
        if(s.added||s.duplicates)emit contentsChanged();emit changed();
    });
    connect(coordinator,&ImportCoordinator::requestRejected,this,[this](const QString &error){m_status=error;emit changed();if(!m_closing)emit rejected(error);});
    connect(coordinator,&ImportCoordinator::artworkStarted,this,[this](int){m_artworkResult.clear();emit changed();});
    connect(coordinator,&ImportCoordinator::artworkCompleted,this,[this](const ArtworkSummary &s){
        const QString category=s.cancelled?"Cancellation":s.failed||s.invalid||s.stale||!s.errors.isEmpty()?"Warning":"Information";
        const QString message=QString("旧封面检查完成：恢复 %1，保留旧图 %2，占位 %3").arg(s.recovered).arg(s.retained).arg(s.placeholder);
        QString details=QString("计划 %1；已处理 %2；恢复 %3；无内嵌来源 %4；图片损坏 %5；失败 %6；目标变化 %7；未完成 %8；取消 %9。")
            .arg(s.total).arg(s.processed).arg(s.recovered).arg(s.noSource).arg(s.invalid).arg(s.failed).arg(s.stale).arg(s.unstarted).arg(s.cancelled);
        for(const auto &error:s.errors)details+='\n'+error;
        const int omitted=qMax(0,s.failed+s.invalid+s.stale-int(s.errors.size()));
        if(omitted)details+=QString("\n另有 %1 项未展开；详情最多保留8项。").arg(omitted);
        m_artworkResult={{"taskId",QString::number(s.taskId)},{"total",s.total},{"processed",s.processed},{"recovered",s.recovered},
            {"retained",s.retained},{"placeholder",s.placeholder},{"noSource",s.noSource},{"invalid",s.invalid},{"failed",s.failed},
            {"stale",s.stale},{"cancelled",s.cancelled},{"unstarted",s.unstarted},{"errors",s.errors},{"details",details},{"category",category}};
        if(!m_closing)emit artworkCompleted(category,QString::number(s.taskId),message,details);
        if(s.recovered)emit contentsChanged();emit changed();
    });
    connect(coordinator,&ImportCoordinator::shutdownReady,this,[this]{emit changed();emit readyToClose();});
}
bool ImportBridge::start(const QList<QUrl> &urls,const QString &playlistKey){
    if(!m_coordinator||busy()||m_closing){m_status="已有导入任务或正在关闭";emit changed();if(!m_closing)emit rejected(m_status);return false;}
    ImportRequest request;bool valid=false;request.targetPlaylist=playlistKey.toLongLong(&valid);
    if(!valid||request.targetPlaylist<0){m_status="歌单身份无效";emit changed();if(!m_closing)emit rejected(m_status);return false;}
    if(request.targetPlaylist>0){bool exists=false;for(const auto &p:m_library->playlists())exists|=p.id==request.targetPlaylist;if(!exists){m_status="歌单不存在";emit changed();if(!m_closing)emit rejected(m_status);return false;}}
    for(const auto &url:urls){if(!url.isLocalFile()){m_status="只能导入本地文件或文件夹";emit changed();if(!m_closing)emit rejected(m_status);return false;}request.sources.append(url.toLocalFile());}
    if(request.sources.isEmpty()){m_status="请选择文件或文件夹";emit changed();if(!m_closing)emit rejected(m_status);return false;}
    m_current=0;m_total=0;m_result.clear();m_status="正在扫描";emit started();
    m_task=m_coordinator->start(request);if(!m_task)m_status=m_coordinator->lastError();emit changed();return m_task!=0;
}
bool ImportBridge::cancel(){
    if(!m_coordinator||!m_task)return false;
    const bool accepted=m_coordinator->cancel(m_task);if(accepted){m_status="正在取消，等待后台任务安全退出";emit changed();}return accepted;
}
void ImportBridge::beginShutdown(){m_closing=true;emit changed();if(m_coordinator)m_coordinator->shutdown();else emit readyToClose();}
}
