#include "lyrics-search-bridge.h"
#include "domain/lyric-match.h"
#include "lyrics-document-reader.h"
#include <QSet>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QUuid>
#include <QScopedValueRollback>
#include <QStringDecoder>
#include <QTimer>
#include <cmath>
#include "ai/whole-song-lyrics-translation.h"
namespace relaxer {
LyricsSearchBridge::LyricsSearchBridge(Library *library,PlaybackFacade *facade,LyricsSearchCoordinator *service,QObject *parent)
    :QAbstractListModel(parent),m_library(library),m_facade(facade),m_service(service){
    for(const auto &source:service->sources()) m_enabledSources.append(LyricsSearchCoordinator::sourceId(source));
    connect(facade,&PlaybackFacade::trackChanged,this,[this]{if(!m_target){m_service->stop();resetForTrack();}});
    connect(service,&LyricsSearchCoordinator::candidatesChanged,this,[this](const LyricsCandidateSnapshot &snapshot,quint64 generation){
        if(generation!=m_service->summary().generation||snapshot.revision!=m_service->candidates(snapshot.trackId).revision)return;
        if(snapshot.records.isEmpty())m_cachedChoices.remove(snapshot.trackId);else m_cachedChoices.insert(snapshot.trackId);
        emit changed();
        const auto current=m_library->track(targetId());
        if(snapshot.trackId!=current.id||snapshot.trackHash!=current.hash)return;
        const auto previous = !m_explicitSelection || m_selected.isEmpty() ? QString() : LyricsSearchCoordinator::candidateIdentity(m_selected);
        m_explicitSelection = false;
        invalidatePlan();beginResetModel();m_snapshot=snapshot;m_selected={};m_selectedKey.clear();m_guard={};endResetModel();
        // Keep an explicit choice through a ranking refresh when that record is
        // still present. New targets and removed choices default to the best rank.
        for (int row = 0; !previous.isEmpty() && row < m_snapshot.records.size(); ++row) {
            const auto record = m_snapshot.records[row].toObject();
            if (LyricsSearchCoordinator::candidateIdentity(record) == previous &&
                candidateVisible(record)) {
                select(row); return;
            }
        }
        selectRecommended(m_recommendTimedOnly);
    });
    connect(service,&LyricsSearchCoordinator::summaryChanged,this,[this](const LyricsSearchSummary &s){
        if(s.generation!=m_service->summary().generation)return;
        m_status=s.running?s.status:m_trackStatus.value(targetId(),s.status);emit changed();
    });
    connect(service,&LyricsSearchCoordinator::eventOccurred,this,[this](const LyricsSearchEvent &event){
        if(event.generation!=m_service->summary().generation)return;
        if(event.phase=="request")return;
        if(event.state==LyricsSearchState::Cancelled&&m_trackState.value(event.trackId)==LyricsSearchState::Saved)return;
        m_trackStatus[event.trackId]=event.status;m_trackState[event.trackId]=event.state;
        if(!event.sourceFailures.isEmpty())m_sourceFailures.insert(event.trackId);
        if(event.trackId==targetId()&&!event.status.isEmpty())m_status=event.status;
        if(event.state==LyricsSearchState::AwaitingChoice)rememberReview(event.trackId,true);
        else if(event.state==LyricsSearchState::Saved)rememberReview(event.trackId,false);
        emit changed();
    });
    connect(service,&LyricsSearchCoordinator::finished,this,[this](const LyricsSearchSummary &summary){
        if(summary.generation!=m_service->summary().generation||m_searchIds.isEmpty())return;
        int failures=0,empty=0,cancelled=0;QStringList details;
        for(auto id:m_searchIds){
            const auto state=m_trackState.value(id,LyricsSearchState::Cancelled);
            failures+=state==LyricsSearchState::Failed||state==LyricsSearchState::Invalidated;
            empty+=state==LyricsSearchState::NoResult;
            cancelled+=state==LyricsSearchState::Cancelled;
            if(details.size()<8)details.append(m_library->track(id).title.left(64)+" · "+m_trackStatus.value(id).left(256));
        }
        if(m_searchIds.size()>8)details.append(QString("另有 %1 首结果省略").arg(m_searchIds.size()-8));
        const bool warning=failures||!m_sourceFailures.isEmpty();
        const bool allCancelled=cancelled==m_searchIds.size();
        const auto category=allCancelled?QString("Cancellation"):warning?(summary.candidateTracks||summary.saved?QString("Warning"):QString("Error")):QString("Information");
        const auto message=allCancelled?QString("已取消歌词搜索"):warning?(summary.candidateTracks||summary.saved?QString("歌词搜索完成，部分来源或目标未完成"):QString("所选来源请求未完成，未获得候选")):empty==m_searchIds.size()?QString("所选来源均无匹配歌词"):QString("歌词搜索完成");
        emit searchFeedback(category,QString::number(summary.generation),targetTrackKey(),message,details.join('\n'));
        m_searchBatch=false;
    });
    connect(library,&Library::changed,this,[this]{if(!m_targets.isEmpty())m_targetRows=m_library->tracksByIds(m_targets);emit changed();});
}
int LyricsSearchBridge::rowCount(const QModelIndex &p)const{return p.isValid()?0:m_snapshot.records.size();}
QHash<int,QByteArray> LyricsSearchBridge::roleNames()const{return {{CandidateKey,"candidateKey"},{Title,"title"},{Artist,"artist"},{Album,"album"},{Source,"source"},{Timed,"timed"},{CandidateVisible,"candidateVisible"},{MatchDetails,"matchDetails"}};}
QVariant LyricsSearchBridge::data(const QModelIndex &index,int role)const{
    if(!index.isValid()||index.row()<0||index.row()>=m_snapshot.records.size())return {};
    const auto r=m_snapshot.records[index.row()].toObject();
    switch(role){case CandidateKey:return LyricsSearchCoordinator::candidateIdentity(r);case Title:return r["trackName"].toString();case Artist:return r["artistName"].toString();case Album:return r["albumName"].toString();case Source:return LyricsSearchCoordinator::sourceLabel(r);
    case CandidateVisible:return candidateVisible(r);
    case MatchDetails:{const double duration=r["duration"].toDouble(-1);const auto target=m_library->track(targetId());
        QString text=duration>0?QString("%1 秒").arg(duration,0,'f',1):QString("时长未知");
        if(duration>0&&target.duration>0)text+=QString(" · 相差 %1 秒").arg(qAbs(duration-target.duration/1000.),0,'f',1);
        if(r["manualOnly"].toBool())text+=" · 需人工确认";
        return text;}
    case Timed:return !r["syncedLyrics"].toString().isEmpty();default:return {};}
}
QVariantList LyricsSearchBridge::availableSources()const{
    QVariantList out;for(const auto &source:m_service->sources())out.append(QVariantMap{
        {"id",LyricsSearchCoordinator::sourceId(source)},{"label",source.label},{"enabled",m_enabledSources.contains(LyricsSearchCoordinator::sourceId(source))},
        {"hint",source.kind==1?QString("需歌名和歌手；仅普通歌词"):source.kind==0?QString("歌名/歌手/专辑/精确时长/查询词"):QString("歌名/歌手/查询词；元数据由来源提供")}});return out;
}
QVariantMap LyricsSearchBridge::defaultQuery()const{
    const auto t=LyricMatchPolicy::searchTrack(m_library->track(targetId()));
    return {{"title",t.title},{"artist",t.artist},{"album",t.album},{"durationMs",double(t.duration)}};
}
bool LyricsSearchBridge::searchInvalid(const QString &message){
    m_status=message;emit changed();emit searchFeedback("Error",QUuid::createUuid().toString(QUuid::WithoutBraces),targetTrackKey(),message,{});return false;
}
void LyricsSearchBridge::parametersEdited(){
    if(m_submitting||(m_searchBatch&&busy()))return;
    if(busy())stop();
    invalidatePlan();m_service->invalidateCandidates(targetId());
    beginResetModel();m_snapshot={};m_selected={};m_selectedKey.clear();m_guard={};endResetModel();
    m_cachedChoices.remove(targetId());m_trackState.remove(targetId());m_trackStatus.remove(targetId());
    m_status="条件已修改，请重新搜索；草稿和翻译预览已保留";emit changed();
}
bool LyricsSearchBridge::setSourceEnabled(const QString &id,bool enabled){
    if(m_submitting||busy())return false;
    bool known=false;for(const auto &source:m_service->sources())known|=LyricsSearchCoordinator::sourceId(source)==id;
    if(!known)return searchInvalid("歌词来源不存在");
    if(enabled==m_enabledSources.contains(id))return true;
    if(enabled)m_enabledSources.append(id);else m_enabledSources.removeAll(id);
    emit sourcesChanged();
    for(auto target:m_targets){m_service->invalidateCandidates(target);m_cachedChoices.remove(target);m_trackState.remove(target);m_trackStatus.remove(target);}
    parametersEdited();return true;
}
void LyricsSearchBridge::restoreSearchDefaults(){
    if(busy())return;
    m_enabledSources.clear();for(const auto &source:m_service->sources())m_enabledSources.append(LyricsSearchCoordinator::sourceId(source));
    emit sourcesChanged();
    m_requireTimed=false;m_replaceSearch=false;m_candidateSource.clear();m_recommendTimedOnly=false;
    for(auto id:m_targets){m_service->invalidateCandidates(id);m_cachedChoices.remove(id);m_trackState.remove(id);}
    parametersEdited();
}
void LyricsSearchBridge::setSearchOptions(bool requireTimed,bool replace){
    if(busy())return;m_requireTimed=requireTimed;m_replaceSearch=replace;
}
bool LyricsSearchBridge::candidateVisible(const QJsonObject &record)const{
    return (m_candidateSource.isEmpty()||record["source"].toString()==m_candidateSource)&&
        (!m_recommendTimedOnly||!record["syncedLyrics"].toString().isEmpty());
}
QString LyricsSearchBridge::recommendedKey()const{
    for(const auto &value:m_snapshot.records)if(candidateVisible(value.toObject()))return LyricsSearchCoordinator::candidateIdentity(value.toObject());return {};
}
void LyricsSearchBridge::refreshCandidateFilter(){
    invalidatePlan();
    if(rowCount())emit dataChanged(index(0),index(rowCount()-1),{CandidateVisible});
    if(m_selected.isEmpty()||!candidateVisible(m_selected))selectRecommended(m_recommendTimedOnly);
    emit changed();
}
void LyricsSearchBridge::setCandidateSource(const QString &source){
    if(source==m_candidateSource)return;m_candidateSource=source;refreshCandidateFilter();
}
bool LyricsSearchBridge::selectKey(const QString &key){
    for(int row=0;row<rowCount();++row)if(data(index(row),CandidateKey).toString()==key)return select(row);return false;
}
void LyricsSearchBridge::resetForTrack(){
    m_offsetPlan={};m_offsetSnapshot={};m_offsetView.clear();
    invalidatePlan();m_lastSavedIdentity.clear();
    m_editToken.clear();m_editGuard={};
    const auto id=targetId();
    // loadCached is a synchronous read API; it intentionally emits no signal.
    if(id)m_service->loadCached({id},m_enabledSources,m_requireTimed);
    beginResetModel();m_snapshot=id?m_service->candidates(id):LyricsCandidateSnapshot{};m_selected={};m_selectedKey.clear();m_guard={};endResetModel();
    m_status="本地缓存；手动搜索才连接歌词来源";
    m_searchContext=m_searchContexts.value(id,QStringLiteral("本地缓存预览；历史条件未知的结果会单独标记"));
    selectRecommended(m_recommendTimedOnly);
    m_showDraft=m_manualDrafts.contains(id);emit changed();
}
bool LyricsSearchBridge::search(bool confirmed){
    const auto track=m_library->track(targetId());
    const auto query=LyricMatchPolicy::searchTrack(track);
    return searchDetailed(query.title,query.artist,query.album,{},confirmed,m_requireTimed,m_replaceSearch);
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
bool LyricsSearchBridge::translationMissing(const Track &track)const{
    const auto judgment=m_library->translationJudgment(track.id);
    if(judgment==TranslationJudgment::NoTranslationNeeded||m_library->isInstrumental(track.id))return false;
    const auto path=track.lyrics.isEmpty()?QString():m_library->absolute(track.lyrics);const QFileInfo file(path);
    const auto signature=QJsonDocument(QJsonArray{path,double(file.size()),double(file.lastModified().toMSecsSinceEpoch()),
        track.title,track.artist,int(judgment)}).toJson(QJsonDocument::Compact);
    const auto cached=m_translationCache.constFind(track.id);
    if(cached!=m_translationCache.cend()&&cached->first==signature)return cached->second;
    ai::lyrics::TranslationAssessment assessment;
    if(!path.isEmpty()&&file.isFile()){
        const auto source=LyricsDocumentReader::readFile(path);
        if(source.opened&&source.complete&&!source.tooLarge)assessment=ai::lyrics::assessTranslation(source.bytes,track.title,track.artist);
    }
    const bool missing=assessment.need!=ai::lyrics::TranslationNeed::Translated&&
        (judgment==TranslationJudgment::NeedsTranslation||assessment.missing());
    m_translationCache.insert(track.id,{signature,missing});return missing;
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
            {"lyricsAvailable",saved},{"translationMissing",translationMissing(track)},{"status",label},{"searchState",state}});
    }return result;
}
bool LyricsSearchBridge::setTargetTrack(const QString &key){
    if(m_submitting)return false;
    bool valid=false;const auto id=key.toLongLong(&valid);
    if(!valid||id<0||(id&&!m_library->track(id).id))return false;
    if(m_target==id)return true;
    if(!id){m_targets.clear();m_targetRows.clear();m_translationCache.clear();m_missingOnly=false;}
    if(busy()&&!m_searchBatch)stop();
    m_target=id;resetForTrack();return true;
}
bool LyricsSearchBridge::setTargetTracks(const QStringList &keys,bool missingOnly,bool showCompleted){
    if(m_submitting)return false;
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
    if(!busy())m_service->loadCached(ids,m_enabledSources,m_requireTimed);
    for(auto id:ids)if(!m_service->candidates(id).records.isEmpty())m_cachedChoices.insert(id);
    restoreReviews(ids);
    m_targets=ids;m_targetRows=tracks;m_missingOnly=missingOnly;m_target=ids.isEmpty()?0:ids.first();resetForTrack();return true;
}
bool LyricsSearchBridge::searchQuery(const QString &title,const QString &artist,const QString &fragment,bool confirmed,bool requireTimed){
    return searchDetailed(title,artist,m_library->track(targetId()).album,fragment,confirmed,requireTimed,true);
}
bool LyricsSearchBridge::searchDetailed(const QString &title,const QString &artist,const QString &album,const QString &fragment,bool confirmed,bool requireTimed,bool replace,double durationMs){
    if(m_batchPrepared||m_batchRunning)return false;
    if(!confirmed||!targetId()){m_status="请先确认联网检索所选歌曲";emit changed();return false;}
    if(busy()&&m_searchBatch)return false;
    if(!std::isfinite(durationMs)||durationMs < -1 || durationMs>86400000 || std::floor(durationMs)!=durationMs)return searchInvalid("时长须为 0 至 86400000 的整数毫秒，0 表示未知");
    const auto track=m_library->track(targetId());if(!track.id)return false;LyricsSearchRequest request;
    request.original=track;request.match=track;request.manual=true;request.replace=replace;request.saveSnapshot=m_library->lyricsSaveSnapshot(track.id);
    request.match.title=title.trimmed();request.match.artist=artist.trimmed();request.match.album=album.trimmed();request.fragment=fragment.trimmed();request.requireTimed=requireTimed;
    request.match.duration=durationMs==-1?track.duration:qint64(durationMs);request.enabledSources=m_enabledSources;
    const auto invalid=LyricsSearchCoordinator::validationError(request,m_service->sources());if(!invalid.isEmpty())return searchInvalid(invalid);
    if(!request.saveSnapshot.valid){m_status=request.saveSnapshot.error;emit changed();return false;}
    parametersEdited();m_searchBatch=false;m_searchIds={track.id};m_sourceFailures.clear();
    m_searchContext=QString("本轮：%1 · %2 · %3 · %4 ms · 来源 %5").arg(request.match.title,request.match.artist,request.match.album).arg(request.match.duration).arg(m_enabledSources.join(", "));
    if(!request.fragment.isEmpty())m_searchContext+=" · 查询："+request.fragment;
    m_searchContexts[track.id]=m_searchContext;
    const bool ok=m_service->start({request})!=0;emit changed();return ok;
}
bool LyricsSearchBridge::searchTargets(bool confirmed){
    auto ids=m_targets;if(ids.isEmpty()&&targetId())ids.append(targetId());
    return startTargets(ids,confirmed,m_missingOnly);
}
bool LyricsSearchBridge::searchTracks(const QStringList &keys,bool confirmed){
    QVector<qint64> ids;QSet<qint64> seen;
    auto allowed=m_targets;if(allowed.isEmpty()&&targetId())allowed.append(targetId());
    for(const auto &key:keys){bool valid=false;const auto id=key.toLongLong(&valid);
        if(!valid||!allowed.contains(id))return false;
        if(!seen.contains(id)){seen.insert(id);ids.append(id);}
    }
    if(ids.isEmpty())return false;
    return startTargets(ids,confirmed,false);
}
bool LyricsSearchBridge::startTargets(const QVector<qint64> &ids,bool confirmed,bool missingOnly){
    if(m_batchPrepared||m_batchRunning)return false;
    if(!confirmed){m_status="请先确认联网查找所选歌曲的歌词";emit changed();return false;}
    if(busy())return false;
    QVector<LyricsSearchRequest> requests;
    const auto tracks=m_library->tracksByIds(ids);
    for(auto id:ids){const auto track=tracks.value(id);if(!track.id)continue;LyricsSearchRequest request;
        if(missingOnly){const QFileInfo file(m_library->absolute(track.lyrics));if(!track.lyrics.isEmpty()&&file.isFile()&&file.size()>0)continue;}
        request.original=track;request.match=LyricMatchPolicy::searchTrack(track);request.manual=!missingOnly;request.replace=m_replaceSearch;
        request.requireTimed=m_requireTimed;request.enabledSources=m_enabledSources;
        const auto invalid=LyricsSearchCoordinator::validationError(request,m_service->sources());if(!invalid.isEmpty())return searchInvalid(invalid);
        request.saveSnapshot=m_library->lyricsSaveSnapshot(id);if(!request.saveSnapshot.valid){m_status=request.saveSnapshot.error;emit changed();return false;}
        requests.append(request);
    }
    if(requests.isEmpty()){m_status="当前没有缺失歌词的歌曲，可查看已保存结果或手动查找其他版本";emit changed();return false;}
    parametersEdited();m_searchBatch=true;m_searchIds.clear();m_sourceFailures.clear();
    for(const auto &request:requests){m_searchIds.insert(request.original.id);m_trackState.remove(request.original.id);m_trackStatus.remove(request.original.id);m_cachedChoices.remove(request.original.id);}
    m_searchContext=QString("本批 %1 首各自元数据 · 来源 %2 · 滚动 %3 · 重新匹配 %4（覆盖仍需确认）").arg(requests.size()).arg(m_enabledSources.join(", ")).arg(m_requireTimed?"是":"否",m_replaceSearch?"是":"否");
    for(const auto &request:requests)m_searchContexts[request.original.id]=m_searchContext;
    const bool ok=m_service->start(requests)!=0;emit changed();return ok;
}
void LyricsSearchBridge::stop(){
    const bool running=busy();m_service->stop();m_searchBatch=false;
    if(running){m_status="已取消歌词搜索；草稿和翻译预览已保留";for(auto id:m_searchIds){m_trackState[id]=LyricsSearchState::Cancelled;m_trackStatus[id]="已取消";}
        emit searchFeedback("Cancellation",QString::number(m_service->summary().generation),targetTrackKey(),m_status,{});}
    m_searchIds.clear(); // stop() already reported this operation; queued summaries must not report it twice.
    emit changed();
}
bool LyricsSearchBridge::selectRecommended(bool requireTimed) {
    m_recommendTimedOnly = requireTimed;
    if(rowCount())emit dataChanged(index(0),index(rowCount()-1),{CandidateVisible});
    for (int row = 0; row < m_snapshot.records.size(); ++row) {
        if (!candidateVisible(m_snapshot.records[row].toObject())) continue;
        if (select(row)) { m_explicitSelection = false; return true; }
    }
    m_selected = {}; m_selectedKey.clear(); m_guard = {}; m_explicitSelection = false;
    emit changed();
    return false;
}
bool LyricsSearchBridge::select(int row){
    if(m_submitting)return false;
    if(row<0||row>=m_snapshot.records.size()||m_snapshot.trackId!=targetId()||!candidateVisible(m_snapshot.records[row].toObject()))return false;
    const auto track=m_library->track(m_snapshot.trackId);if(track.hash!=m_snapshot.trackHash)return false;
    invalidatePlan();m_showDraft=false;m_selected=m_snapshot.records[row].toObject();m_guard=m_library->lyricsSaveSnapshot(track.id);
    m_explicitSelection = true;
    m_selectedKey=QString::number(track.id)+':'+QString::number(m_snapshot.revision)+':'+LyricsSearchCoordinator::candidateIdentity(m_selected);
    emit changed();return true;
}
QString LyricsSearchBridge::previewText()const{
    if(m_showDraft)return draftText();
    const auto timed=m_selected["syncedLyrics"].toString();return timed.isEmpty()?m_selected["plainLyrics"].toString():timed;
}
QString LyricsSearchBridge::draftText()const{
    const auto bytes=m_manualDrafts.value(targetId());
    QStringDecoder decoder(bytes.startsWith("\xff\xfe")||bytes.startsWith("\xfe\xff")?QStringDecoder::Utf16:QStringDecoder::Utf8);
    return decoder(bytes);
}
bool LyricsSearchBridge::saveSelected(const QString &key,bool confirmed){
    if(m_submitting||!confirmed||key.isEmpty()||key!=m_selectedKey||m_guard.id!=targetId())return false;
    const auto latest=m_service->candidates(targetId());
    if(latest.trackHash!=m_snapshot.trackHash||latest.revision!=m_snapshot.revision){m_status="候选版本已变化，请重新选择";emit changed();emit operationFailed(QString::number(targetId()),m_status);return false;}
    const auto timed=m_selected["syncedLyrics"].toString();
    auto bytes=(timed.isEmpty()?m_selected["plainLyrics"].toString():timed).toUtf8();if(m_selected["instrumental"].toBool())bytes=QString("纯音乐，请欣赏").toUtf8();
    m_showDraft=false;
    return prepareSave(bytes,key,m_guard,true);
}
QString LyricsSearchBridge::beginLocalEdit(){
    if(m_submitting||!targetId())return {};
    m_editGuard=m_library->lyricsSaveSnapshot(targetId());
    if(!m_editGuard.valid){m_status=m_editGuard.error;emit changed();emit operationFailed(QString::number(targetId()),m_status);return {};}
    m_editToken=QUuid::createUuid().toString(QUuid::WithoutBraces);return m_editToken;
}
bool LyricsSearchBridge::saveBytes(const QByteArray &bytes,bool confirmed,const QString &editToken){
    if(m_submitting||!confirmed||!targetId())return false;
    if(!editToken.isEmpty()&&(editToken!=m_editToken||m_editGuard.id!=targetId())){m_status="导入或粘贴的目标已变化，本次未保存";emit changed();emit operationFailed(QString::number(targetId()),m_status);return false;}
    const auto identity=QString::number(targetId())+":"+QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
    if(identity==m_lastSavedIdentity&&m_library->lyricsSnapshotMatches(m_guard))return false;
    if(bytes.size()<=LyricsDocumentReader::MaximumBytes){m_manualDrafts.insert(targetId(),bytes);m_showDraft=true;}
    const auto snapshot=editToken.isEmpty()?m_library->lyricsSaveSnapshot(targetId()):m_editGuard;
    return prepareSave(bytes,identity,snapshot,false);
}
bool LyricsSearchBridge::saveText(const QString &text,bool confirmed,const QString &editToken){return saveBytes(text.toUtf8(),confirmed,editToken);}
bool LyricsSearchBridge::importLocalLyrics(const QUrl &file,bool confirmed,const QString &editToken){
    if(!confirmed||!file.isLocalFile())return false;
    const auto input=LyricsDocumentReader::readFile(file.toLocalFile());
    if(!input.opened||!input.complete||input.tooLarge){m_status=input.error;emit changed();emit operationFailed(QString::number(targetId()),m_status);return false;}
    return saveBytes(input.bytes,true,editToken);
}
bool LyricsSearchBridge::markInstrumental(bool confirmed){
    return saveBytes(QStringLiteral("纯音乐，请欣赏").toUtf8(),confirmed);
}
void LyricsSearchBridge::invalidatePlan(){m_planToken.clear();m_planSelection.clear();m_plan={};}
bool LyricsSearchBridge::prepareSave(const QByteArray &bytes,const QString &identity,const LyricsSaveSnapshot &snapshot,bool selected){
    invalidatePlan();
    m_plan=LyricsReplacementService::prepare(*m_library,snapshot,bytes,identity,m_translation);
    if(!m_plan.decision.valid){m_status=m_plan.decision.reason;emit changed();emit operationFailed(QString::number(targetId()),m_status);return false;}
    m_planSelection=selected?m_selectedKey:QString();
    m_planToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(m_plan.decision.confirmationRequired){
        m_status=m_plan.decision.reason;emit changed();emit replacementRequested(m_planToken,m_status);return false;
    }
    return commitPlan(LyricsReplacementChoice::SafeSave);
}
bool LyricsSearchBridge::commitPlan(LyricsReplacementChoice choice){
    if(m_submitting||m_planToken.isEmpty())return false;
    if(!m_planSelection.isEmpty()){
        const auto latest=m_service->candidates(targetId());
        if(m_planSelection!=m_selectedKey||latest.revision!=m_snapshot.revision||latest.trackHash!=m_snapshot.trackHash){
            m_status="候选版本已变化，请重新选择";invalidatePlan();emit changed();emit operationFailed(QString::number(targetId()),m_status);return false;
        }
    }
    const auto id=m_plan.snapshot.id;
    const auto identity=m_plan.candidateIdentity;
    bool ok=false;
    {
        QScopedValueRollback<bool> saving(m_submitting,true);
        ok=LyricsReplacementService::commit(*m_library,m_plan,choice,targetId(),identity,m_translation,&m_status);
        if(ok){
            m_lastSavedIdentity=identity;m_service->cancelTrack(id);m_service->noteManualSave();
            m_trackState[id]=LyricsSearchState::Saved;rememberReview(id,false);
            m_guard=m_library->lyricsSaveSnapshot(id);m_selectedKey.clear();m_selected={};
            m_status=m_plan.decision.preservesTranslation?QStringLiteral("歌词已保存；已有译文已保留，候选元数据已采用"):QStringLiteral("歌词已保存");
            m_trackStatus[id]=m_status;
            m_editToken.clear();m_editGuard={};
            m_manualDrafts.remove(id);m_showDraft=false;
        }
        invalidatePlan();
    }
    emit changed();if(ok)emit saved(QString::number(id));else emit operationFailed(QString::number(id),m_status);return ok;
}
bool LyricsSearchBridge::resolveReplacement(const QString &token,int choice){
    if(choice==0||choice==2){cancelReplacement(token);return false;}
    if(token.isEmpty()||token!=m_planToken){m_status="替换确认已失效，请重新选择后评估；本次未保存";emit changed();emit operationFailed(QString::number(targetId()),m_status);return false;}
    return choice==1&&commitPlan(LyricsReplacementChoice::Replace);
}
void LyricsSearchBridge::cancelReplacement(const QString &token){
    if(token.isEmpty()||token!=m_planToken)return;
    invalidatePlan();m_status="已保留现有歌词；候选和翻译预览仍可继续处理";emit changed();
}
bool LyricsSearchBridge::beginFileOffset(){
    if(m_submitting||m_batchRunning||m_batchPrepared||busy()||!targetId())return false;
    m_offsetSnapshot=m_library->lyricsSaveSnapshot(targetId());m_offsetView={{"active",true},{"valid",false}};
    const bool ok=previewFileOffset(0);if(!ok){m_offsetView.clear();m_offsetSnapshot={};emit changed();}return ok;
}
bool LyricsSearchBridge::previewFileOffset(double milliseconds){
    if(m_submitting||!m_offsetView.value("active").toBool()||m_offsetSnapshot.id!=targetId())return false;
    if(!std::isfinite(milliseconds)||std::floor(milliseconds)!=milliseconds||std::abs(milliseconds)>LyricsEditService::MaximumOffset){
        m_offsetView["valid"]=false;m_offsetView["error"]="文件偏移须为 -5000 至 5000 的整数毫秒。";
        emit changed();return false;
    }
    auto next=LyricsEditService::prepareOffset(*m_library,m_offsetSnapshot,int(milliseconds),m_translation);
    if(!next.valid()){
        m_offsetView["valid"]=false;m_offsetView["error"]=next.error();m_status=next.error();
        emit changed();emit operationFailed(QString::number(targetId()),m_status);return false;
    }
    m_offsetPlan=std::move(next);
    QStringDecoder decoder(m_offsetPlan.content().startsWith("\xff\xfe")||m_offsetPlan.content().startsWith("\xfe\xff")?QStringDecoder::Utf16:QStringDecoder::Utf8);
    auto preview=QString(decoder(m_offsetPlan.content()));if(decoder.hasError())preview=QString::fromLocal8Bit(m_offsetPlan.content());
    QStringList times;const auto parsed=Lyrics::parse(m_offsetPlan.content());
    for(int i=0;i<qMin<qsizetype>(8,parsed.lines.size());++i)times.append(QString::number(parsed.lines[i].time)+" ms · "+parsed.lines[i].text);
    m_offsetView={{"active",true},{"valid",true},{"target",QString::number(targetId())},{"title",targetTitle()},
        {"existingMs",m_offsetPlan.existingOffset()},{"requestedMs",int(milliseconds)},
        {"confirmationRequired",m_offsetPlan.confirmationRequired()},{"changed",m_offsetPlan.changed()},
        {"preview",preview},{"timeline",times.join('\n')},{"error",QString()}};
    emit changed();return true;
}
bool LyricsSearchBridge::saveFileOffset(bool discardTranslation){
    if(m_submitting||m_batchRunning||!m_offsetView.value("active").toBool()||!m_offsetView.value("valid").toBool())return false;
    bool ok=false;const bool fileChanged=m_offsetPlan.changed();
    {QScopedValueRollback<bool> saving(m_submitting,true);
        ok=LyricsEditService::commitOffset(*m_library,m_offsetPlan,targetId(),discardTranslation,m_translation,&m_status);}
    if(!ok){m_offsetView["error"]=m_status;emit changed();emit operationFailed(QString::number(targetId()),m_status);return false;}
    m_status=fileChanged?QStringLiteral("文件时间偏移已保存；播放显示延时未修改。"):QStringLiteral("文件时间偏移未改变；没有重写歌词或丢弃翻译任务。");
    m_guard=m_library->lyricsSaveSnapshot(targetId());m_offsetView.clear();m_offsetPlan={};m_offsetSnapshot={};
    emit changed();emit editCompleted("Information",QUuid::createUuid().toString(QUuid::WithoutBraces),m_status,{});return true;
}
void LyricsSearchBridge::cancelFileOffset(){if(m_submitting)return;m_offsetView.clear();m_offsetPlan={};m_offsetSnapshot={};emit changed();}
bool LyricsSearchBridge::prepareInstrumentalBatch(const QStringList &keys){
    if(m_batchRunning||m_batchPrepared||m_submitting||m_offsetView.value("active").toBool()||busy())return false;
    QVector<qint64> ids;
    for(const auto &key:keys){bool ok=false;auto id=key.toLongLong(&ok);if(!ok||id<=0)return false;ids.append(id);}
    m_batch=LyricsEditService::prepareInstrumental(*m_library,ids,m_translation);
    if(!m_batch.valid()){m_status=m_batch.error();emit changed();emit operationFailed({},m_status);return false;}
    m_batchOperation=QUuid::createUuid().toString(QUuid::WithoutBraces);m_batchPrepared=true;emit changed();return true;
}
bool LyricsSearchBridge::startInstrumentalBatch(bool replaceRisks){
    if(!m_batchPrepared||m_batchRunning)return false;
    m_batchPrepared=false;m_batchRunning=true;m_batchReplace=replaceRisks;const auto epoch=++m_batchEpoch;
    emit changed();QTimer::singleShot(0,this,[this,epoch]{processInstrumental(epoch);});return true;
}
void LyricsSearchBridge::processInstrumental(quint64 epoch){
    if(!m_batchRunning||epoch!=m_batchEpoch)return;
    {QScopedValueRollback<bool> stepping(m_batchStep,true);LyricsEditService::stepInstrumental(*m_library,m_batch,m_batchReplace,m_translation);}
    const auto result=m_batch.result();const auto index=result["total"].toInt()-result["pending"].toInt()-1;
    const auto entry=result["results"].toList().value(index).toMap();
    if(entry["state"]=="succeeded"){
        const auto id=entry["trackKey"].toString().toLongLong();m_service->cancelTrack(id);m_service->clearCandidates(id);
        m_trackState[id]=LyricsSearchState::Saved;m_trackStatus[id]="已保存纯音乐标记";rememberReview(id,false);m_service->noteManualSave();
        if(id==targetId())m_guard=m_library->lyricsSaveSnapshot(id);
    }
    emit changed();
    if(m_batchCancelRequested){m_batchCancelRequested=false;cancelInstrumentalBatch();return;}
    if(!m_batchRunning||epoch!=m_batchEpoch)return;
    if(m_batch.finished())finishInstrumental();
    else QTimer::singleShot(0,this,[this,epoch]{processInstrumental(epoch);});
}
void LyricsSearchBridge::finishInstrumental(){
    m_batchRunning=false;m_batchPrepared=false;const auto result=m_batch.result();
    m_status=QString("纯音乐处理：总数 %1，成功 %2，跳过 %3，失败 %4%5。").arg(result["total"].toInt()).arg(result["succeeded"].toInt())
        .arg(result["skipped"].toInt()).arg(result["failed"].toInt()).arg(result["cancelled"].toBool()?QString("，已取消 %1 首未开始目标").arg(result["cancelledCount"].toInt()):QString());
    QString details=result["details"].toString();if(result["omitted"].toInt())details+=QString("\n另省略 %1 条明细。").arg(result["omitted"].toInt());
    const auto category=result["failed"].toInt()?(result["succeeded"].toInt()?"Warning":"Error"):result["cancelled"].toBool()?"Cancellation":"Information";
    emit changed();emit editCompleted(category,m_batchOperation,m_status,details);
}
void LyricsSearchBridge::cancelInstrumentalBatch(){
    if(!m_batchPrepared&&!m_batchRunning)return;
    if(m_batchStep){m_batchCancelRequested=true;return;}
    ++m_batchEpoch;LyricsEditService::cancelInstrumental(m_batch);finishInstrumental();
}
}
