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
#include "ai/whole-song-lyrics-translation.h"
namespace relaxer {
LyricsSearchBridge::LyricsSearchBridge(Library *library,PlaybackFacade *facade,LyricsSearchCoordinator *service,QObject *parent)
    :QAbstractListModel(parent),m_library(library),m_facade(facade),m_service(service){
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
                (!m_recommendTimedOnly || !record["syncedLyrics"].toString().isEmpty())) {
                select(row); return;
            }
        }
        selectRecommended(m_recommendTimedOnly);
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
    invalidatePlan();m_lastSavedIdentity.clear();
    m_editToken.clear();m_editGuard={};
    const auto id=targetId();
    // loadCached is a synchronous read API; it intentionally emits no signal.
    if(id)m_service->loadCached({id});
    beginResetModel();m_snapshot=id?m_service->candidates(id):LyricsCandidateSnapshot{};m_selected={};m_selectedKey.clear();m_guard={};endResetModel();
    m_status="本地缓存；手动搜索才连接歌词来源";
    selectRecommended(m_recommendTimedOnly);
    m_showDraft=m_manualDrafts.contains(id);emit changed();
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
    if(!confirmed){m_status="请先确认联网查找所选歌曲的歌词";emit changed();return false;}
    if(busy())return false;
    QVector<LyricsSearchRequest> requests;
    const auto tracks=m_library->tracksByIds(ids);
    for(auto id:ids){const auto track=tracks.value(id);if(!track.id)continue;LyricsSearchRequest request;
        if(missingOnly){const QFileInfo file(m_library->absolute(track.lyrics));if(!track.lyrics.isEmpty()&&file.isFile()&&file.size()>0)continue;}
        request.original=track;request.match=track;request.manual=!missingOnly;request.replace=false;
        request.saveSnapshot=m_library->lyricsSaveSnapshot(id);if(!request.saveSnapshot.valid){m_status=request.saveSnapshot.error;emit changed();return false;}
        requests.append(request);
    }
    if(requests.isEmpty()){m_status="当前没有缺失歌词的歌曲，可查看已保存结果或手动查找其他版本";emit changed();return false;}
    const bool ok=m_service->start(requests)!=0;emit changed();return ok;
}
void LyricsSearchBridge::stop(){m_service->stop();emit changed();}
bool LyricsSearchBridge::selectRecommended(bool requireTimed) {
    m_recommendTimedOnly = requireTimed;
    for (int row = 0; row < m_snapshot.records.size(); ++row) {
        if (requireTimed && m_snapshot.records[row].toObject()["syncedLyrics"].toString().isEmpty()) continue;
        if (select(row)) { m_explicitSelection = false; return true; }
    }
    m_selected = {}; m_selectedKey.clear(); m_guard = {}; m_explicitSelection = false;
    emit changed();
    return false;
}
bool LyricsSearchBridge::select(int row){
    if(m_submitting)return false;
    if(row<0||row>=m_snapshot.records.size()||m_snapshot.trackId!=targetId())return false;
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
    if(latest.trackHash!=m_snapshot.trackHash||latest.revision!=m_snapshot.revision){m_status="候选版本已变化，请重新选择";emit changed();return false;}
    const auto timed=m_selected["syncedLyrics"].toString();
    auto bytes=(timed.isEmpty()?m_selected["plainLyrics"].toString():timed).toUtf8();if(m_selected["instrumental"].toBool())bytes=QString("纯音乐，请欣赏").toUtf8();
    m_showDraft=false;
    return prepareSave(bytes,key,m_guard,true);
}
QString LyricsSearchBridge::beginLocalEdit(){
    if(m_submitting||!targetId())return {};
    m_editGuard=m_library->lyricsSaveSnapshot(targetId());
    if(!m_editGuard.valid){m_status=m_editGuard.error;emit changed();return {};}
    m_editToken=QUuid::createUuid().toString(QUuid::WithoutBraces);return m_editToken;
}
bool LyricsSearchBridge::saveBytes(const QByteArray &bytes,bool confirmed,const QString &editToken){
    if(m_submitting||!confirmed||!targetId())return false;
    if(!editToken.isEmpty()&&(editToken!=m_editToken||m_editGuard.id!=targetId())){m_status="导入或粘贴的目标已变化，本次未保存";emit changed();return false;}
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
    if(!input.opened||!input.complete||input.tooLarge){m_status=input.error;emit changed();return false;}
    return saveBytes(input.bytes,true,editToken);
}
bool LyricsSearchBridge::markInstrumental(bool confirmed){
    return saveBytes(QStringLiteral("纯音乐，请欣赏").toUtf8(),confirmed);
}
void LyricsSearchBridge::invalidatePlan(){m_planToken.clear();m_planSelection.clear();m_plan={};}
bool LyricsSearchBridge::prepareSave(const QByteArray &bytes,const QString &identity,const LyricsSaveSnapshot &snapshot,bool selected){
    invalidatePlan();
    m_plan=LyricsReplacementService::prepare(*m_library,snapshot,bytes,identity,m_translation);
    if(!m_plan.decision.valid){m_status=m_plan.decision.reason;emit changed();return false;}
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
            m_status="候选版本已变化，请重新选择";invalidatePlan();emit changed();return false;
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
            m_editToken.clear();m_editGuard={};
            m_manualDrafts.remove(id);m_showDraft=false;
        }
        invalidatePlan();
    }
    emit changed();if(ok)emit saved(QString::number(id));return ok;
}
bool LyricsSearchBridge::resolveReplacement(const QString &token,int choice){
    if(token.isEmpty()||token!=m_planToken){m_status="替换确认已失效，请重新选择后评估；本次未保存";emit changed();return false;}
    if(choice==0||choice==2){cancelReplacement(token);return false;}
    return choice==1&&commitPlan(LyricsReplacementChoice::Replace);
}
void LyricsSearchBridge::cancelReplacement(const QString &token){
    if(token.isEmpty()||token!=m_planToken)return;
    invalidatePlan();m_status="已保留现有歌词；候选和翻译预览仍可继续处理";emit changed();
}
}
