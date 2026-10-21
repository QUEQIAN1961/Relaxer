#include "lyrics-search-bridge.h"
#include "domain/lyric-match.h"
#include "lyrics-document-reader.h"
#include <QSet>
#include <QFileInfo>
#include <QCryptographicHash>
namespace relaxer {
LyricsSearchBridge::LyricsSearchBridge(Library *library,PlaybackFacade *facade,LyricsSearchCoordinator *service,QObject *parent)
    :QAbstractListModel(parent),m_library(library),m_facade(facade),m_service(service){
    connect(facade,&PlaybackFacade::trackChanged,this,[this]{if(!m_target){m_service->stop();resetForTrack();}});
    connect(service,&LyricsSearchCoordinator::candidatesChanged,this,[this](const LyricsCandidateSnapshot &snapshot,quint64){
        if(snapshot.records.isEmpty())m_cachedChoices.remove(snapshot.trackId);else m_cachedChoices.insert(snapshot.trackId);
        emit changed();
        const auto current=m_library->track(targetId());
        if(snapshot.trackId!=current.id||snapshot.trackHash!=current.hash)return;
        beginResetModel();m_snapshot=snapshot;m_selected={};m_selectedKey.clear();endResetModel();emit changed();
    });
    connect(service,&LyricsSearchCoordinator::summaryChanged,this,[this](const LyricsSearchSummary &s){m_status=s.status;emit changed();});
    connect(service,&LyricsSearchCoordinator::eventOccurred,this,[this](const LyricsSearchEvent &event){
        m_trackStatus[event.trackId]=event.status;m_trackState[event.trackId]=event.state;
        if(event.state==LyricsSearchState::AwaitingChoice)rememberReview(event.trackId,true);
        else if(event.state==LyricsSearchState::Saved)rememberReview(event.trackId,false);
        emit changed();
    });
    connect(library,&Library::changed,this,[this]{if(!m_targets.isEmpty())m_targetRows=m_library->tracksByIds(m_targets);emit changed();});
}
int LyricsSearchBridge::rowCount(const QModelIndex &p)const{return p.isValid()?0:m_snapshot.records.size();}
QHash<int,QByteArray> LyricsSearchBridge::roleNames()const{return {{CandidateKey,"candidateKey"},{Title,"title"},{Artist,"artist"},{Album,"album"},{Source,"source"},{Timed,"timed"}};}
QVariant LyricsSearchBridge::data(const QModelIndex &index,int role)const{
    if(!index.isValid()||index.row()<0||index.row()>=m_snapshot.records.size())return {};
    const auto r=m_snapshot.records[index.row()].toObject();
    switch(role){case CandidateKey:return LyricsSearchCoordinator::candidateIdentity(r);case Title:return r["trackName"].toString();case Artist:return r["artistName"].toString();case Album:return r["albumName"].toString();case Source:return r["source"].toString("LRCLIB");case Timed:return !r["syncedLyrics"].toString().isEmpty();default:return {};}
}
void LyricsSearchBridge::resetForTrack(){
    const auto id=targetId();
    // loadCached is a synchronous read API; it intentionally emits no signal.
    if(id)m_service->loadCached({id});
    beginResetModel();m_snapshot=id?m_service->candidates(id):LyricsCandidateSnapshot{};m_selected={};m_selectedKey.clear();m_guard={};endResetModel();
    m_status="本地缓存；手动搜索才连接歌词来源";emit changed();
}
bool LyricsSearchBridge::search(bool confirmed){
    const auto track=m_library->track(targetId());
    return searchQuery(track.title,track.artist,{},confirmed);
}
QString LyricsSearchBridge::targetTitle()const{return m_library->track(targetId()).title;}
QString LyricsSearchBridge::targetArtist()const{return m_library->track(targetId()).artist;}
QString LyricsSearchBridge::sourceText()const{
    const auto track=m_library->track(targetId());if(!track.id||track.lyrics.isEmpty())return {};
    const auto file=LyricsDocumentReader::readFile(m_library->absolute(track.lyrics));
    return file.opened&&file.complete&&!file.tooLarge?QString::fromUtf8(file.bytes):QString();
}
void LyricsSearchBridge::rememberReview(qint64 id,bool pending){
    const auto track=m_library->track(id);if(!track.id)return;
    if(pending)m_pendingReviews.insert(id);else m_pendingReviews.remove(id);
    QJsonObject value{{"pending",pending},{"trackHash",track.hash}};
    if(pending){const auto file=LyricsDocumentReader::readFile(m_library->absolute(track.lyrics));
        value["sourceDigest"]=QString::fromLatin1(QCryptographicHash::hash(file.bytes,QCryptographicHash::Sha256).toHex());}
    // Only UI review metadata uses the existing settings store. Candidate text
    // remains exclusively in the coordinator's existing lookup cache.
    m_library->setSetting(QString("qml.lyricsReviewV1.%1").arg(id),value);
}
void LyricsSearchBridge::restoreReviews(const QVector<qint64> &ids){
    if(busy())return;
    for(auto id:ids){
        m_pendingReviews.remove(id);
        m_trackState.remove(id);
        if(!m_cachedChoices.contains(id))continue;
        const auto value=m_library->setting(QString("qml.lyricsReviewV1.%1").arg(id)).toObject();
        const auto track=m_library->track(id);
        if(!value["pending"].toBool()||value["trackHash"].toString()!=track.hash)continue;
        const auto file=LyricsDocumentReader::readFile(m_library->absolute(track.lyrics));
        const auto digest=QString::fromLatin1(QCryptographicHash::hash(file.bytes,QCryptographicHash::Sha256).toHex());
        if(digest==value["sourceDigest"].toString())m_pendingReviews.insert(id);
    }
}
QVariantList LyricsSearchBridge::targetTracks()const{
    QVariantList result;auto ids=m_targets;if(ids.isEmpty()&&targetId())ids.append(targetId());
    const auto tracks=m_targets.isEmpty()?m_library->tracksByIds(ids):m_targetRows;
    for(auto id:ids){const auto track=tracks.value(id);if(!track.id)continue;
        const QFileInfo file(m_library->absolute(track.lyrics));
        const bool saved=!track.lyrics.isEmpty()&&file.isFile()&&file.size()>0;
        QString state=saved?(m_pendingReviews.contains(id)?"choice":"complete"):m_cachedChoices.contains(id)?"choice":"incomplete";
        const auto active=m_trackState.value(id,LyricsSearchState::Cancelled);
        if(busy()&&(active==LyricsSearchState::Searching||active==LyricsSearchState::Queued))state="searching";
        else if(active==LyricsSearchState::CandidatesFound||active==LyricsSearchState::AwaitingChoice)state="choice";
        else if(active==LyricsSearchState::Saved)state=saved?"complete":"incomplete";
        auto label=state=="complete"?QString("已完成"):state=="choice"?QString("待选择歌词版本"):state=="searching"?m_trackStatus.value(id,QString("正在查找")):QString("未完成");
        if(state=="incomplete"&&!m_trackStatus.value(id).isEmpty()&&active!=LyricsSearchState::Searching&&active!=LyricsSearchState::Queued)
            label+=" · "+m_trackStatus.value(id);
        result.append(QVariantMap{{"trackKey",QString::number(id)},{"title",track.title},{"artist",track.artist},
            {"lyricsAvailable",saved},{"status",label},{"searchState",state}});
    }return result;
}
bool LyricsSearchBridge::setTargetTrack(const QString &key){
    bool valid=false;const auto id=key.toLongLong(&valid);
    if(!valid||id<0||(id&&!m_library->track(id).id))return false;
    if(m_target==id)return true;
    if(!id){m_targets.clear();m_targetRows.clear();m_missingOnly=false;}
    m_target=id;resetForTrack();return true;
}
bool LyricsSearchBridge::setTargetTracks(const QStringList &keys,bool missingOnly,bool showCompleted){
    QVector<qint64> input,ids;QSet<qint64> seen;
    for(const auto &key:keys){bool valid=false;const auto id=key.toLongLong(&valid);
        if(!valid||id<=0)return false;
        if(seen.contains(id))continue;seen.insert(id);input.append(id);
    }
    const auto tracks=m_library->tracksByIds(input);if(tracks.size()!=input.size())return false;
    for(auto id:input){const auto track=tracks.value(id);
        if(missingOnly&&!showCompleted&&!track.lyrics.isEmpty()){const QFileInfo file(m_library->absolute(track.lyrics));if(file.isFile()&&file.size()>0)continue;}
        ids.append(id);
    }
    if(!keys.isEmpty()&&ids.isEmpty()){m_status="当前没有缺失歌词的歌曲";emit changed();return false;}
    if(!busy())m_service->loadCached(ids);
    for(auto id:ids)if(!m_service->candidates(id).records.isEmpty())m_cachedChoices.insert(id);
    restoreReviews(ids);
    m_targets=ids;m_targetRows=tracks;m_missingOnly=missingOnly;m_target=ids.isEmpty()?0:ids.first();resetForTrack();return true;
}
bool LyricsSearchBridge::searchQuery(const QString &title,const QString &artist,const QString &fragment,bool confirmed,bool requireTimed){
    return searchDetailed(title,artist,m_library->track(targetId()).album,fragment,confirmed,requireTimed,true);
}
bool LyricsSearchBridge::searchDetailed(const QString &title,const QString &artist,const QString &album,const QString &fragment,bool confirmed,bool requireTimed,bool replace){
    if(!confirmed||!targetId()){m_status="请先确认联网检索所选歌曲";emit changed();return false;}
    if(busy())return false;
    const auto track=m_library->track(targetId());if(!track.id)return false;LyricsSearchRequest request;
    request.original=track;request.match=track;request.manual=true;request.replace=replace;request.saveSnapshot=m_library->lyricsSaveSnapshot(track.id);
    request.match.title=title.trimmed();request.match.artist=artist.trimmed();request.match.album=album.trimmed();request.fragment=fragment.trimmed();request.requireTimed=requireTimed;
    if(!request.saveSnapshot.valid){m_status=request.saveSnapshot.error;emit changed();return false;}
    const bool ok=m_service->start({request})!=0;emit changed();return ok;
}
bool LyricsSearchBridge::searchTargets(bool confirmed){
    if(!confirmed){m_status="请先确认联网查找所选歌曲的歌词";emit changed();return false;}
    if(busy())return false;
    auto ids=m_targets;if(ids.isEmpty()&&targetId())ids.append(targetId());
    QVector<LyricsSearchRequest> requests;
    const auto tracks=m_library->tracksByIds(ids);
    for(auto id:ids){const auto track=tracks.value(id);if(!track.id)continue;LyricsSearchRequest request;
        if(m_missingOnly){const QFileInfo file(m_library->absolute(track.lyrics));if(!track.lyrics.isEmpty()&&file.isFile()&&file.size()>0)continue;}
        request.original=track;request.match=track;request.manual=!m_missingOnly;request.replace=false;
        request.saveSnapshot=m_library->lyricsSaveSnapshot(id);if(!request.saveSnapshot.valid){m_status=request.saveSnapshot.error;emit changed();return false;}
        requests.append(request);
    }
    if(requests.isEmpty()){m_status="当前没有缺失歌词的歌曲，可查看已保存结果或手动查找其他版本";emit changed();return false;}
    const bool ok=m_service->start(requests)!=0;emit changed();return ok;
}
void LyricsSearchBridge::stop(){m_service->stop();emit changed();}
bool LyricsSearchBridge::select(int row){
    if(row<0||row>=m_snapshot.records.size()||m_snapshot.trackId!=targetId())return false;
    const auto track=m_library->track(m_snapshot.trackId);if(track.hash!=m_snapshot.trackHash)return false;
    m_selected=m_snapshot.records[row].toObject();m_guard=m_library->lyricsSaveSnapshot(track.id);
    m_selectedKey=QString::number(track.id)+':'+QString::number(m_snapshot.revision)+':'+LyricsSearchCoordinator::candidateIdentity(m_selected);
    emit changed();return true;
}
QString LyricsSearchBridge::previewText()const{
    const auto timed=m_selected["syncedLyrics"].toString();return timed.isEmpty()?m_selected["plainLyrics"].toString():timed;
}
bool LyricsSearchBridge::saveSelected(const QString &key,bool confirmed){
    if(!confirmed||key.isEmpty()||key!=m_selectedKey||m_guard.id!=targetId())return false;
    const auto latest=m_service->candidates(targetId());
    if(latest.trackHash!=m_snapshot.trackHash||latest.revision!=m_snapshot.revision)return false;
    auto bytes=previewText().toUtf8();if(m_selected["instrumental"].toBool())bytes=QString("纯音乐，请欣赏").toUtf8();
    QString error;
    if(!LyricMatchPolicy::rawLyricsAllowed(bytes,&error)||!m_library->lyricsSnapshotMatches(m_guard,&error)){
        m_status=error;emit changed();return false;
    }
    const bool ok=m_library->saveLyricsChecked(m_guard.id,bytes,m_guard);
    if(ok){m_service->cancelTrack(m_guard.id);m_service->noteManualSave();m_trackState[m_guard.id]=LyricsSearchState::Saved;rememberReview(m_guard.id,false);}
    m_status=ok?"歌词已保存":m_library->lastError();m_selectedKey.clear();emit changed();return ok;
}
bool LyricsSearchBridge::saveBytes(const QByteArray &bytes,bool confirmed){
    if(!confirmed||!targetId())return false;
    QString error;if(!LyricMatchPolicy::rawLyricsAllowed(bytes,&error)){m_status=error;emit changed();return false;}
    const auto snapshot=m_library->lyricsSaveSnapshot(targetId());
    const bool ok=m_library->saveLyricsChecked(targetId(),bytes,snapshot);
    if(ok){m_service->cancelTrack(targetId());m_service->noteManualSave();m_trackState[targetId()]=LyricsSearchState::Saved;rememberReview(targetId(),false);m_selectedKey.clear();m_selected={};}
    m_status=ok?"歌词已保存":m_library->lastError();emit changed();return ok;
}
bool LyricsSearchBridge::saveText(const QString &text,bool confirmed){return saveBytes(text.toUtf8(),confirmed);}
bool LyricsSearchBridge::importLocalLyrics(const QUrl &file,bool confirmed){
    if(!confirmed||!file.isLocalFile())return false;
    const auto input=LyricsDocumentReader::readFile(file.toLocalFile());
    if(!input.opened||!input.complete||input.tooLarge){m_status=input.error;emit changed();return false;}
    return saveBytes(input.bytes,true);
}
bool LyricsSearchBridge::markInstrumental(bool confirmed){
    if(!confirmed||!targetId())return false;
    const bool ok=m_library->markInstrumental(targetId());
    if(ok){m_service->cancelTrack(targetId());m_service->clearCandidates(targetId());m_cachedChoices.remove(targetId());m_trackState[targetId()]=LyricsSearchState::Saved;rememberReview(targetId(),false);resetForTrack();}
    m_status=ok?"已标记为纯音乐":m_library->lastError();emit changed();return ok;
}
}
