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
        m_result={{"added",s.added},{"duplicates",s.duplicates},{"failed",s.failed},{"ignoredPaths",s.ignoredPaths},
            {"cancelled",s.cancelled},{"partialSuccess",s.partialSuccess},{"unstarted",s.unstarted},
            {"errors",s.errors},{"fatalError",s.fatalError},{"playlistFailures",s.playlistFailures}};
        m_status=QString("%1 · 新增%2，重复%3，失败%4，未开始%5").arg(s.cancelled?"已取消":s.partialSuccess?"部分完成":"导入完成").arg(s.added).arg(s.duplicates).arg(s.failed).arg(s.unstarted);
        if(!s.fatalError.isEmpty())m_status+=" · "+s.fatalError;
        // The existing coordinator deliberately batches Library signals.
        // Notify projections once at its committed/thread-exit barrier.
        if(s.added||s.duplicates)emit contentsChanged();emit changed();
    });
    connect(coordinator,&ImportCoordinator::requestRejected,this,[this](const QString &error){m_status=error;emit changed();});
    connect(coordinator,&ImportCoordinator::shutdownReady,this,[this]{emit changed();emit readyToClose();});
}
bool ImportBridge::start(const QList<QUrl> &urls,const QString &playlistKey){
    if(!m_coordinator||busy()||m_closing){m_status="已有导入任务或正在关闭";emit changed();return false;}
    ImportRequest request;bool valid=false;request.targetPlaylist=playlistKey.toLongLong(&valid);
    if(!valid||request.targetPlaylist<0){m_status="歌单身份无效";emit changed();return false;}
    if(request.targetPlaylist>0){bool exists=false;for(const auto &p:m_library->playlists())exists|=p.id==request.targetPlaylist;if(!exists){m_status="歌单不存在";emit changed();return false;}}
    for(const auto &url:urls){if(!url.isLocalFile()){m_status="只能导入本地文件或文件夹";emit changed();return false;}request.sources.append(url.toLocalFile());}
    if(request.sources.isEmpty()){m_status="请选择文件或文件夹";emit changed();return false;}
    m_current=0;m_total=0;m_result.clear();m_status="正在扫描";
    m_task=m_coordinator->start(request);if(!m_task)m_status=m_coordinator->lastError();emit changed();return m_task!=0;
}
bool ImportBridge::cancel(){
    if(!m_coordinator||!m_task)return false;
    const bool accepted=m_coordinator->cancel(m_task);if(accepted){m_status="正在取消，等待后台任务安全退出";emit changed();}return accepted;
}
void ImportBridge::beginShutdown(){m_closing=true;emit changed();if(m_coordinator)m_coordinator->shutdown();else emit readyToClose();}
}
