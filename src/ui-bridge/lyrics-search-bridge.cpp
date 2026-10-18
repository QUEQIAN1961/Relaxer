#include "lyrics-search-bridge.h"
#include "domain/lyric-match.h"
namespace relaxer {
LyricsSearchBridge::LyricsSearchBridge(Library *library,PlaybackFacade *facade,LyricsSearchCoordinator *service,QObject *parent)
    :QAbstractListModel(parent),m_library(library),m_facade(facade),m_service(service){
    connect(facade,&PlaybackFacade::trackChanged,this,&LyricsSearchBridge::resetForTrack);
    connect(service,&LyricsSearchCoordinator::candidatesChanged,this,[this](const LyricsCandidateSnapshot &snapshot,quint64){
        const auto current=m_library->track(m_facade->current().id);
        if(snapshot.trackId!=current.id||snapshot.trackHash!=current.hash)return;
        beginResetModel();m_snapshot=snapshot;m_selected={};m_selectedKey.clear();endResetModel();emit changed();
    });
    connect(service,&LyricsSearchCoordinator::summaryChanged,this,[this](const LyricsSearchSummary &s){m_status=s.status;emit changed();});
}
int LyricsSearchBridge::rowCount(const QModelIndex &p)const{return p.isValid()?0:m_snapshot.records.size();}
QHash<int,QByteArray> LyricsSearchBridge::roleNames()const{return {{CandidateKey,"candidateKey"},{Title,"title"},{Artist,"artist"},{Album,"album"},{Source,"source"},{Timed,"timed"}};}
QVariant LyricsSearchBridge::data(const QModelIndex &index,int role)const{
    if(!index.isValid()||index.row()<0||index.row()>=m_snapshot.records.size())return {};
    const auto r=m_snapshot.records[index.row()].toObject();
    switch(role){case CandidateKey:return LyricsSearchCoordinator::candidateIdentity(r);case Title:return r["trackName"].toString();case Artist:return r["artistName"].toString();case Album:return r["albumName"].toString();case Source:return r["source"].toString("LRCLIB");case Timed:return !r["syncedLyrics"].toString().isEmpty();default:return {};}
}
void LyricsSearchBridge::resetForTrack(){
    m_service->stop();const auto id=m_facade->current().id;
    // loadCached is a synchronous read API; it intentionally emits no signal.
    if(id)m_service->loadCached({id});
    beginResetModel();m_snapshot=id?m_service->candidates(id):LyricsCandidateSnapshot{};m_selected={};m_selectedKey.clear();m_guard={};endResetModel();
    m_status="本地缓存；手动搜索才连接歌词来源";emit changed();
}
bool LyricsSearchBridge::search(bool confirmed){
    if(!confirmed||!m_facade->current().id){m_status="请先确认联网检索当前歌曲";emit changed();return false;}
    if(busy())return false;
    const auto track=m_library->track(m_facade->current().id);LyricsSearchRequest request;
    request.original=track;request.match=track;request.manual=true;request.replace=true;request.saveSnapshot=m_library->lyricsSaveSnapshot(track.id);
    if(!request.saveSnapshot.valid){m_status=request.saveSnapshot.error;emit changed();return false;}
    const bool ok=m_service->start({request})!=0;emit changed();return ok;
}
void LyricsSearchBridge::stop(){m_service->stop();emit changed();}
bool LyricsSearchBridge::select(int row){
    if(row<0||row>=m_snapshot.records.size()||m_snapshot.trackId!=m_facade->current().id)return false;
    const auto track=m_library->track(m_snapshot.trackId);if(track.hash!=m_snapshot.trackHash)return false;
    m_selected=m_snapshot.records[row].toObject();m_guard=m_library->lyricsSaveSnapshot(track.id);
    m_selectedKey=QString::number(track.id)+':'+QString::number(m_snapshot.revision)+':'+LyricsSearchCoordinator::candidateIdentity(m_selected);
    emit changed();return true;
}
QString LyricsSearchBridge::previewText()const{
    const auto timed=m_selected["syncedLyrics"].toString();return timed.isEmpty()?m_selected["plainLyrics"].toString():timed;
}
bool LyricsSearchBridge::saveSelected(const QString &key,bool confirmed){
    if(!confirmed||key.isEmpty()||key!=m_selectedKey||m_guard.id!=m_facade->current().id)return false;
    auto bytes=previewText().toUtf8();if(m_selected["instrumental"].toBool())bytes=QString("纯音乐，请欣赏").toUtf8();
    QString error;
    if(!LyricMatchPolicy::rawLyricsAllowed(bytes,&error)||!m_library->lyricsSnapshotMatches(m_guard,&error)){
        m_status=error;emit changed();return false;
    }
    const bool ok=m_library->saveLyricsChecked(m_guard.id,bytes,m_guard);
    m_status=ok?"歌词已保存":m_library->lastError();m_selectedKey.clear();emit changed();return ok;
}
}
