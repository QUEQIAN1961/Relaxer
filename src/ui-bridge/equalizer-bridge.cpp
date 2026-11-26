#include "equalizer-bridge.h"
#include "playback-facade.h"
#include "library-model.h"
#include "online-page-cache.h"
#include <QRunnable>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QUuid>
#include <cmath>
#include <algorithm>
namespace relaxer {
namespace {
using namespace audio;
PeqConfig defaults(){PeqConfig c;const double f[]={40,100,250,630,1600,4000,10000,16000};for(int i=0;i<PeqBands;++i)c.bands[i].frequency=f[i];c.bands[0].type=1;c.bands[7].type=2;return c;}
QJsonObject encode(const PeqConfig &c){QJsonArray bands;for(const auto &b:c.bands)bands.append(QJsonObject{{"frequency",b.frequency},{"gain",b.gain},{"q",b.q},{"type",b.type},{"enabled",bool(b.enabled)}});return {{"enabled",bool(c.enabled)},{"preamp",c.preamp},{"autoHeadroom",bool(c.autoHeadroom)},{"bands",bands}};}
bool number(const QJsonValue &v,double min,double max){return v.isDouble()&&std::isfinite(v.toDouble())&&v.toDouble()>=min&&v.toDouble()<=max;}
PeqConfig decode(const QJsonObject &o){auto c=defaults();if(o.value("enabled").isBool())c.enabled=o["enabled"].toBool();if(o.value("autoHeadroom").isBool())c.autoHeadroom=o["autoHeadroom"].toBool();if(number(o["preamp"],-24,12))c.preamp=o["preamp"].toDouble();
 const auto a=o["bands"].toArray();for(int i=0;i<std::min(PeqBands,int(a.size()));++i){auto &b=c.bands[i];const auto x=a[i].toObject();if(number(x["frequency"],20,20000))b.frequency=x["frequency"].toDouble();if(number(x["gain"],-18,18))b.gain=x["gain"].toDouble();if(number(x["q"],.1,18))b.q=x["q"].toDouble();if(number(x["type"],0,5)&&x["type"].toDouble()==x["type"].toInt())b.type=x["type"].toInt();if(x["enabled"].isBool())b.enabled=x["enabled"].toBool();}return c;}
const QStringList providers{"netease","qq","qishui","kugou","bilibili"};
const QStringList platformNames{"网易云音乐","QQ 音乐","汽水音乐","酷狗音乐","哔哩哔哩"};
QString providerFor(const Track &t){const auto p=t.hash.section(':',0,0);return p=="qqmusic"?QString("qq"):providers.contains(p)?p:QString();}
QString songKey(const Track &t){if(!t.id||t.hash.isEmpty())return {};return "song:"+(providerFor(t).isEmpty()?"local:"+t.hash:t.hash);}
QString remotePlaylistKey(const QString &provider,const QString &id){return providers.contains(provider)&&!id.isEmpty()?"playlist:"+provider+":"+id:QString();}
QVariantMap descriptor(const QString &key,const QString &name,const QString &kind){return {{"key",key},{"name",name},{"kind",kind}};}
bool validRuleKey(const QString &key){
 if(key.size()>1024)return false;
 const auto parts=key.split(':');if(parts.size()<2)return false;
 if(parts[0]=="platform")return parts.size()==2&&providers.contains(parts[1]);
 if(parts.size()<3||key.section(':',2).isEmpty())return false;
 if(parts[0]=="song")return parts[1]=="local"||parts[1]=="qqmusic"||providers.contains(parts[1]);
 if(parts[0]!="playlist")return false;
 if(providers.contains(parts[1]))return true;
 bool ok=false;const auto id=parts[2].toLongLong(&ok);return parts.size()==3&&parts[1]=="local"&&ok&&(id>=0||id==-1)&&QString::number(id)==parts[2];
}
}
EqualizerBridge::EqualizerBridge(Library *library,QObject *parent):QObject(parent),m_library(library){
 load(library->setting("parametricEqualizer",QJsonObject()).toObject());rebuildTargets();m_config=m_global;publishEffective();
 if(!mediaAvailable())m_status="当前音频后端未提供全局均衡器接口，请使用随本版本编译的 FFmpeg 插件";
 m_songSearchPool.setMaxThreadCount(1);m_songSearchPool.setExpiryTimeout(10000);m_songSearchTimer.setSingleShot(true);m_songSearchTimer.setInterval(180);connect(&m_songSearchTimer,&QTimer::timeout,this,&EqualizerBridge::searchSongs);
 m_rateTimer.setInterval(500);connect(&m_rateTimer,&QTimer::timeout,this,[this]{const int rate=audio::relaxer_peq_sample_rate();if(rate!=m_sampleRate){m_sampleRate=rate;emit changed();}});m_rateTimer.start();
 connect(library,&Library::changed,this,[this]{
  const auto previous=m_baseTargets;rebuildTargets();QSet<QString> remaining;
  auto cleanedSaved=m_savedDocument;auto savedRules=cleanedSaved.value("rules").toObject();bool savedCleanup=false;
  for(const auto &v:m_baseTargets)remaining.insert(v.toMap()["key"].toString());
  for(const auto &v:previous){const auto key=v.toMap()["key"].toString();
   if(key.startsWith("playlist:local:")&&!remaining.contains(key)){
    // SQLite local playlist IDs may be reused after deletion.
    m_rules.remove(key);if(savedRules.contains(key)){savedRules.remove(key);savedCleanup=true;}
    if(m_targetKey==key){m_requestedTarget={};m_targetKey="global";m_config=m_global;}
   }
  }
  if(savedCleanup){
   // Deleting a playlist cleans persisted associations without committing live drafts.
   cleanedSaved["rules"]=savedRules;m_saving=true;const bool ok=m_library->setSetting("parametricEqualizer",cleanedSaved);m_saving=false;
   if(ok)m_savedDocument=cleanedSaved;else message("已删除歌单的均衡器关联尚未清理："+m_library->lastError());
  }
  markDirty();refreshPlayback();if(m_songSearchActive&&!m_songQuery.trimmed().isEmpty())scheduleSongSearch();
 });
 connect(library,&Library::settingsUpdated,this,[this](const QStringList &keys){
  if(keys.contains("queueOrigins"))refreshPlayback();
  if(keys.contains("qml.cloudPlaylistRemarks")){rebuildTargets();emit changed();}
  if(m_saving||m_dirty||!keys.contains("parametricEqualizer"))return;
  load(m_library->setting("parametricEqualizer",QJsonObject()).toObject());m_config=configuration(m_targetKey);publishEffective();emit changed();
 });
}
EqualizerBridge::~EqualizerBridge(){m_songSearchTimer.stop();if(m_songSearchCancel)m_songSearchCancel->store(true);m_songSearchPool.clear();m_songSearchPool.waitForDone();}
void EqualizerBridge::load(const QJsonObject &data){
 // Version 1's top-level configuration remains the global fallback.
 m_global=decode(data);m_presets=data["presets"].toArray();if(m_presets.size()>20)m_presets={};m_rules={};
 const auto rules=data["rules"].toObject();for(auto it=rules.begin();it!=rules.end();++it){
  const auto row=it.value().toObject();if(!validRuleKey(it.key())||!row["settings"].isObject()||row["settings"].toObject()["bands"].toArray().size()!=PeqBands)continue;
  m_rules.insert(it.key(),QJsonObject{{"name",row["name"].toString().left(300)},{"settings",encode(decode(row["settings"].toObject()))}});
 }
 m_savedDocument=document();m_dirty=false;
}
void EqualizerBridge::attach(PlaybackFacade *facade,TrackListModel *source){
 if(m_facade){disconnect(m_facade,nullptr,this,nullptr);disconnect(m_facade->backend(),nullptr,this,nullptr);}m_facade=facade;m_source=source;
 if(facade){connect(facade->backend(),&Playback::audioSourceAboutToStart,this,&EqualizerBridge::refreshPlayback);
  connect(facade,&PlaybackFacade::trackChanged,this,&EqualizerBridge::refreshPlayback);
  connect(facade,&PlaybackFacade::onlineTrackUpdated,this,[this](qint64 id){if(m_facade&&m_facade->current().id==id)refreshPlayback();});}
 refreshPlayback();
}
void EqualizerBridge::rebuildTargets(){
 m_baseTargets={descriptor("global","全局默认","global"),descriptor("playlist:local:0","歌单 · 本地音乐","playlist"),descriptor("playlist:local:-1","歌单 · 我喜欢","playlist")};
 for(const auto &p:m_library->playlists()){
  const auto key=p.provider.isEmpty()?"playlist:local:"+QString::number(p.id):remotePlaylistKey(p.provider,p.remoteId);
  const auto remark=p.provider.isEmpty()?QString():m_library->setting("qml.cloudPlaylistRemarks").toObject().value(QString::number(p.id)).toString();
  if(!key.isEmpty())m_baseTargets.append(descriptor(key,"歌单 · "+(remark.isEmpty()?p.name:remark),"playlist"));
 }
 for(int i=0;i<providers.size();++i)m_baseTargets.append(descriptor("platform:"+providers[i],"发现平台 · "+platformNames[i],"platform"));
}
QVariantList EqualizerBridge::targets()const{
 auto rows=m_baseTargets;QSet<QString> seen;for(const auto &v:rows)seen.insert(v.toMap()["key"].toString());
 const auto append=[&](const QVariantMap &row){const auto key=row["key"].toString();if(!key.isEmpty()&&!seen.contains(key)){rows.append(row);seen.insert(key);}};
 if(!m_songKey.isEmpty())append(descriptor(m_songKey,"歌曲 · "+m_trackTitle,"song"));
 append(m_requestedTarget);
 for(auto it=m_rules.begin();it!=m_rules.end();++it)if(!seen.contains(it.key()))append(descriptor(it.key(),it.value().toObject()["name"].toString(),it.key().section(':',0,0)));
 return rows;
}
QVariantMap EqualizerBridge::describe(const QString &key)const{for(const auto &v:targets()){const auto row=v.toMap();if(row["key"].toString()==key)return row;}return descriptor(key,"已移除的适配对象",key.section(':',0,0));}
QString EqualizerBridge::playlistKey(const QString &localKey)const{
 if(localKey=="0"||localKey=="-1")return "playlist:local:"+localKey;
 for(const auto &p:m_library->playlists())if(QString::number(p.id)==localKey)return p.provider.isEmpty()?"playlist:local:"+localKey:remotePlaylistKey(p.provider,p.remoteId);
 return {};
}
QString EqualizerBridge::resolveKey(bool includeSong)const{
 if(includeSong&&!m_songKey.isEmpty()&&m_rules.contains(m_songKey))return m_songKey;
 if(!m_playlistKey.isEmpty()&&m_rules.contains(m_playlistKey))return m_playlistKey;
 const auto platform="platform:"+m_provider;if(!m_provider.isEmpty()&&m_rules.contains(platform))return platform;
 return "global";
}
QString EqualizerBridge::inheritedKey(const QString &key)const{
 if(key.startsWith("song:")){
  if(key==m_songKey)return resolveKey(false);
  const auto parent=m_requestedTarget.value("key").toString()==key?m_requestedTarget.value("parentKey").toString():QString();
  if(!parent.isEmpty()&&m_rules.contains(parent))return parent;
  auto provider=key.section(':',1,1);if(provider=="qqmusic")provider="qq";
  const auto platform="platform:"+provider;if(providers.contains(provider)&&m_rules.contains(platform))return platform;
 }else if(key.startsWith("playlist:")){
  const auto provider=key.section(':',1,1),platform="platform:"+provider;if(providers.contains(provider)&&m_rules.contains(platform))return platform;
 }
 return "global";
}
PeqConfig EqualizerBridge::configuration(const QString &key)const{
 if(key=="global")return m_global;
 const auto found=m_rules.value(key).toObject();if(!found.isEmpty())return decode(found["settings"].toObject());
 const auto inherited=inheritedKey(key);return inherited=="global"?m_global:decode(m_rules.value(inherited).toObject()["settings"].toObject());
}
QVariantMap EqualizerBridge::target()const{
 auto row=describe(m_targetKey);row["configured"]=m_targetKey=="global"||m_rules.contains(m_targetKey);row["inheritedName"]=describe(inheritedKey(m_targetKey))["name"];return row;
}
QVariantMap EqualizerBridge::active()const{auto row=describe(m_effectiveKey);row["trackTitle"]=m_trackTitle;row["enabled"]=bool(m_effective.enabled);return row;}
void EqualizerBridge::publishEffective(){
 const auto key=resolveKey();const auto next=configuration(key);
 if(!m_published||encode(next)!=encode(m_effective)){audio::relaxer_peq_publish(&next);m_effective=next;m_published=true;}
 m_effectiveKey=key;
}
void EqualizerBridge::refreshPlayback(){
 const auto track=m_facade?m_facade->current():Track{};m_songKey=songKey(track);m_trackTitle=track.title;m_provider=providerFor(track);m_playlistKey.clear();
 if(track.id&&m_facade){
  const auto origin=m_facade->queueOrigin(track.id);const auto provider=origin.value("provider").toString();
  const auto custom=origin.value("customPlaylistKey").toString();
  if(!custom.isEmpty()&&m_library->containsPlaylistEntry(custom.toLongLong(),track.id))m_playlistKey=playlistKey(custom);
  if(m_playlistKey.isEmpty()&&provider=="local"&&m_provider.isEmpty())m_playlistKey=playlistKey(origin.value("localKey","0").toString());
  else if(m_playlistKey.isEmpty()&&provider==m_provider&&origin.value("kind","playlist").toString()=="playlist"){
   const auto candidate=remotePlaylistKey(provider,origin.value("id").toString());
   // Only an actual sidebar-pinned playlist supplies playlist inheritance.
   for(const auto &v:m_baseTargets)if(v.toMap()["key"].toString()==candidate){m_playlistKey=candidate;break;}
  }
 }
 if(m_targetKey!="global"&&!m_rules.contains(m_targetKey))m_config=configuration(m_targetKey);
 publishEffective();emit changed();
}
void EqualizerBridge::message(const QString &text){m_status=text;emit statusChanged();}
QVariantMap EqualizerBridge::settings()const{return encode(m_config).toVariantMap();}
double EqualizerBridge::headroom()const{return audio::relaxer_peq_headroom(&m_config,m_sampleRate);}
QVariantList EqualizerBridge::response()const{QVariantList result;auto preview=m_config;preview.enabled=1;preview.preamp=0;preview.autoHeadroom=0;for(int i=0;i<=240;++i){const auto f=20*std::pow(1000.,i/240.);result.append(QVariantMap{{"frequency",f},{"gain",audio::relaxer_peq_response(&preview,f,m_sampleRate,0)}});}return result;}
QVariantList EqualizerBridge::presets()const{QVariantList list{QVariantMap{{"id","flat"},{"name","平直"},{"custom",false}},QVariantMap{{"id","bass"},{"name","低音增强"},{"custom",false}},QVariantMap{{"id","voice"},{"name","人声清晰"},{"custom",false}},QVariantMap{{"id","soft"},{"name","柔和高频"},{"custom",false}}};for(const auto &v:m_presets){auto row=v.toObject().toVariantMap();row.remove("settings");row["custom"]=true;list.append(row);}return list;}
bool EqualizerBridge::activateTarget(const QVariantMap &row,bool openEditor){
 const auto key=row.value("key").toString();if(key.isEmpty())return false;
 if(key!=m_targetKey){m_requestedTarget=row;m_targetKey=key;m_config=configuration(key);message({});emit changed();}
 if(openEditor)emit editorRequested();return true;
}
bool EqualizerBridge::requestTarget(const QVariantMap &row,bool openEditor){
 if(row.value("key").toString()!=m_targetKey&&hasUnsavedChanges()){
  m_pendingTarget=row;m_pendingOpenEditor=openEditor;emit targetChangeConfirmationRequested();return false;
 }
 return activateTarget(row,openEditor);
}
bool EqualizerBridge::selectTarget(const QString &key){
 for(const auto &v:targets())if(v.toMap().value("key").toString()==key)return requestTarget(v.toMap(),false);
 return false;
}
bool EqualizerBridge::openTarget(const QString &key){
 for(const auto &v:targets())if(v.toMap().value("key").toString()==key)return requestTarget(v.toMap(),true);
 return false;
}
bool EqualizerBridge::editSong(const Track &track,const QString &parentKey){
 const auto key=songKey(track);if(key.isEmpty()){message("无法识别这首歌曲，请刷新列表后重试");return false;}
 auto row=descriptor(key,"歌曲 · "+track.title,"song");row["parentKey"]=parentKey;if(providerFor(track).isEmpty())row["localId"]=QString::number(track.id);return requestTarget(row,true);
}
void EqualizerBridge::cancelTargetChange(){m_pendingTarget.clear();m_pendingOpenEditor=false;}
bool EqualizerBridge::continueTargetChange(){
 const auto row=m_pendingTarget;const bool openEditor=m_pendingOpenEditor;cancelTargetChange();if(row.isEmpty())return false;
 const auto key=row.value("key").toString();
 if(row.contains("localId")&&songKey(m_library->track(row.value("localId").toString().toLongLong()))!=key){message("歌曲已改变，请重新检索");return false;}
 const auto parent=row.value("parentKey").toString();
 if(key.startsWith("song:")&&parent.startsWith("playlist:")&&!parent.startsWith("playlist:local:")&&!row.value("savedRule").toBool()){
  bool pinned=false;for(const auto &v:m_baseTargets)if(v.toMap().value("key").toString()==parent){pinned=true;break;}if(!pinned){message("来源歌单已取消固定，请重新选择");return false;}
 }
 if(key.startsWith("playlist:")||key.startsWith("platform:")){
  bool exists=false;for(const auto &v:m_baseTargets)if(v.toMap().value("key")==row.value("key")){exists=true;break;}if(!exists){message("适配对象已移除，请重新选择");return false;}
 }
 return activateTarget(row,openEditor);
}
bool EqualizerBridge::requestSong(const QString &key){
 bool ok=false;const auto id=key.toLongLong(&ok);if(!ok||id<=0)return false;
 auto track=m_source?m_source->trackAt(m_source->rowForId(id)):Track{};
 if(track.id)return editSong(track,playlistKey(QString::number(m_source->playlist())));
 track=m_facade?m_facade->track(id):m_library->track(id);return editSong(track);
}
bool EqualizerBridge::requestOnlineSong(const QString &provider,const QVariantMap &song){
 if(!providers.contains(provider))return false;
 const auto id=song.value(provider=="qq"?"mid":"id").toString().trimmed();if(id.isEmpty()){message("该歌曲缺少稳定标识，暂时无法保存专属适配");return false;}
 Track track;track.id=1;track.title=song.value("title").toString();track.hash=(provider=="qq"?"qqmusic":provider)+":"+id;return editSong(track);
}
bool EqualizerBridge::requestPlaylist(const QString &key){const auto target=playlistKey(key);return !target.isEmpty()&&openTarget(target);}
bool EqualizerBridge::requestPlatform(const QString &provider){return providers.contains(provider)&&openTarget("platform:"+provider);}
QJsonObject EqualizerBridge::document()const{auto data=encode(m_global);data["version"]=2;data["presets"]=m_presets;data["rules"]=m_rules;return data;}
void EqualizerBridge::markDirty(){m_dirty=document()!=m_savedDocument;emit editStateChanged();}
void EqualizerBridge::setPresetDraft(const QString &text){const auto value=text.left(40);if(value==m_presetDraft)return;m_presetDraft=value;emit editStateChanged();}
bool EqualizerBridge::saveAllChanges(){return m_presetDraft.trimmed().isEmpty()?flush():savePreset(m_presetDraft);}
void EqualizerBridge::discardChanges(){
 const auto saved=m_savedDocument;load(saved);m_presetDraft.clear();m_config=configuration(m_targetKey);publishEffective();message({});emit changed();emit editStateChanged();
}
void EqualizerBridge::update(){
 if(m_targetKey=="global")m_global=m_config;
 else m_rules.insert(m_targetKey,QJsonObject{{"name",describe(m_targetKey)["name"].toString()},{"settings",encode(m_config)}});
 publishEffective();markDirty();emit changed();if(!m_status.isEmpty())message({});
}
bool EqualizerBridge::createOverride(){if(m_targetKey=="global"||m_rules.contains(m_targetKey))return false;update();return true;}
bool EqualizerBridge::removeOverride(){
 if(m_targetKey=="global"||!m_rules.contains(m_targetKey))return false;
 m_rules.remove(m_targetKey);m_config=configuration(m_targetKey);publishEffective();markDirty();message({});emit changed();return true;
}
void EqualizerBridge::rememberCloudSongs(const QString &provider,const QVariantMap &playlist,const QVariantList &songs){
 if(!providers.contains(provider))return;
 if(songs.isEmpty()&&playlist.isEmpty())m_liveCloud.remove(provider);
 else m_liveCloud[provider]={{"playlist",playlist},{"songs",songs}};
 if(m_songSearchActive&&!m_songQuery.trimmed().isEmpty())scheduleSongSearch();
}
void EqualizerBridge::setSongSearchActive(bool active){if(m_songSearchActive==active)return;m_songSearchActive=active;scheduleSongSearch();}
void EqualizerBridge::setSongQuery(const QString &query){const auto text=query.left(120);if(text==m_songQuery)return;m_songQuery=text;scheduleSongSearch();}
void EqualizerBridge::scheduleSongSearch(){
 m_songSearchTimer.stop();if(m_songSearchCancel)m_songSearchCancel->store(true);m_songSearchPool.clear();++m_songSearchRevision;
 m_songResults.clear();m_songMatches.clear();m_songSearchMessage.clear();m_songSearching=m_songSearchActive&&!m_songQuery.simplified().isEmpty();
 emit songSearchChanged();if(m_songSearching)m_songSearchTimer.start();
}
void EqualizerBridge::searchSongs(){
 if(!m_songSearchActive||m_songQuery.simplified().isEmpty())return;
 const auto terms=m_songQuery.simplified().toCaseFolded().split(' ',Qt::SkipEmptyParts);
 const auto matches=[&](const QString &text){const auto folded=text.toCaseFolded();return std::all_of(terms.cbegin(),terms.cend(),[&](const QString &term){return folded.contains(term);});};
 QVariantList initial,sources;QSet<QString> seeded;bool localMore=false;QString localError;
 const auto local=m_library->searchEqualizerSongs(m_songQuery,41,&localError);localMore=local.size()>40;
 for(int i=0;i<std::min(40,int(local.size()));++i){const auto &t=local[i];const auto key=songKey(t);if(key.isEmpty()||seeded.contains(key))continue;
  auto row=descriptor(key,"歌曲 · "+t.title,"song");row["title"]=t.title;row["artist"]=t.artist;row["album"]=t.album;row["source"]="本地音乐";row["localId"]=QString::number(t.id);row["parentKey"]="playlist:local:0";initial.append(row);seeded.insert(key);
 }
 for(auto it=m_rules.begin();it!=m_rules.end();++it){if(!it.key().startsWith("song:")||seeded.contains(it.key()))continue;
  const auto name=it.value().toObject()["name"].toString();const auto title=name.startsWith("歌曲 · ")?name.mid(QString("歌曲 · ").size()):name;if(!matches(title))continue;
  if(initial.size()>=60){localMore=true;break;}auto row=descriptor(it.key(),name,"song");row["title"]=title;row["source"]="已保存适配";row["savedRule"]=true;initial.append(row);seeded.insert(it.key());
 }
 for(const auto &p:m_library->playlists()){
  const auto index=providers.indexOf(p.provider);if(index<0||p.remoteId.isEmpty())continue;
  auto source=QVariantMap{{"provider",p.provider},{"id",p.remoteId},{"parentKey",remotePlaylistKey(p.provider,p.remoteId)},{"source",platformNames[index]+" · "+p.name},
    {"path",OnlinePageCache::pinnedSnapshotPath(m_library,p.provider,p.remoteId)},{"legacyRoot",QDir(m_library->root()).filePath("Online")}};
  const auto live=m_liveCloud.value(p.provider);const auto metadata=live.value("playlist").toMap();const auto kind=metadata.value("kind").toString();
  if(metadata.value("id").toString()==p.remoteId&&(kind.isEmpty()||kind=="playlist")&&!live.value("songs").toList().isEmpty())source["songs"]=live.value("songs");
  sources.append(source);
 }
 const auto revision=m_songSearchRevision;const auto cancelled=std::make_shared<std::atomic_bool>(false);m_songSearchCancel=cancelled;
 m_songSearchPool.start(QRunnable::create([this,initial,sources,terms,revision,cancelled,localMore,localError]{
  QVariantList results=initial;QSet<QString> seen;for(const auto &v:results)seen.insert(v.toMap().value("key").toString());bool more=localMore;int unavailable=0;
  const auto match=[&](const QString &text){const auto folded=text.toCaseFolded();return std::all_of(terms.cbegin(),terms.cend(),[&](const QString &term){return folded.contains(term);});};
  for(const auto &sourceValue:sources){if(cancelled->load())return;const auto source=sourceValue.toMap();QVariantList songs=source.value("songs").toList();
   if(!source.contains("songs")){
    QFile file(source.value("path").toString());if(!file.exists())file.setFileName(QDir(source.value("legacyRoot").toString()).filePath(QFileInfo(file.fileName()).fileName()));
    if(!file.open(QIODevice::ReadOnly)||file.size()>64*1024*1024){++unavailable;continue;}
    QByteArray bytes;while(!file.atEnd()){if(cancelled->load())return;const auto chunk=file.read(256*1024);if(chunk.isEmpty())break;bytes.append(chunk);}
    const auto snapshot=QJsonDocument::fromJson(bytes).object();const auto metadata=snapshot.value("playlist").toObject();
    if(!snapshot.value("songs").isArray()||(!metadata.value("id").toVariant().toString().isEmpty()&&metadata.value("id").toVariant().toString()!=source.value("id").toString())){++unavailable;continue;}
    songs=snapshot.value("songs").toArray().toVariantList();
   }
   const auto provider=source.value("provider").toString();
   for(const auto &v:songs){if(cancelled->load())return;const auto item=v.toMap();const auto title=item.value("title").toString(),artist=item.value("artist").toString(),album=item.value("album").toString();
    if(!match(title+' '+artist+' '+album))continue;const auto id=item.value(provider=="qq"?"mid":"id").toString().trimmed();if(id.isEmpty())continue;
    const auto key="song:"+(provider=="qq"?QString("qqmusic"):provider)+":"+id;if(seen.contains(key))continue;if(results.size()>=100){more=true;break;}
    auto row=descriptor(key,"歌曲 · "+title,"song");row["title"]=title;row["artist"]=artist;row["album"]=album;row["source"]=source.value("source");row["parentKey"]=source.value("parentKey");results.append(row);seen.insert(key);
   }
   if(results.size()>=100){more=true;break;}
  }
  if(cancelled->load())return;
  QMetaObject::invokeMethod(this,[this,results,revision,more,unavailable,localError]{
   if(revision!=m_songSearchRevision||!m_songSearchActive)return;
   m_songResults=results;m_songMatches.clear();for(const auto &v:results){const auto row=v.toMap();m_songMatches.insert(row.value("key").toString(),row);}m_songSearching=false;QStringList notices;
   if(!localError.isEmpty())notices.append("本地检索失败："+localError);
   if(results.isEmpty())notices.append("未找到匹配歌曲，可换用歌名、歌手或专辑关键词");
   if(more)notices.append("当前显示部分匹配，继续输入可缩小范围");
   if(unavailable)notices.append("部分固定云端歌单尚无可用缓存，可先进入对应歌单加载");
   m_songSearchMessage=notices.join("；");emit songSearchChanged();
  },Qt::QueuedConnection);
 }));
}
bool EqualizerBridge::selectSongResult(const QString &key){
 if(m_songSearching||!m_songMatches.contains(key))return false;const auto row=m_songMatches.value(key);
 if(row.contains("localId")){const auto track=m_library->track(row.value("localId").toString().toLongLong());if(songKey(track)!=key){scheduleSongSearch();return false;}}
 else if(!row.value("savedRule").toBool()){
  bool pinned=false;for(const auto &v:m_baseTargets)if(v.toMap().value("key")==row.value("parentKey")){pinned=true;break;}if(!pinned){scheduleSongSearch();return false;}
 }else if(!m_rules.contains(key)){scheduleSongSearch();return false;}
 return requestTarget(row,false);
}
bool EqualizerBridge::setOption(const QString &key,const QVariant &value){
 if(key=="enabled"||key=="autoHeadroom"){if(value.metaType().id()!=QMetaType::Bool)return false;if(key=="enabled")m_config.enabled=value.toBool();else m_config.autoHeadroom=value.toBool();}
 else if(key=="preamp"){bool ok;const auto n=value.toDouble(&ok);if(!ok||!std::isfinite(n)||n< -24||n>12)return false;m_config.preamp=n;}else return false;update();return true;
}
bool EqualizerBridge::setBand(int index,const QString &key,const QVariant &value){
 if(index<0||index>=audio::PeqBands)return false;auto &b=m_config.bands[index];if(key=="enabled"){if(value.metaType().id()!=QMetaType::Bool)return false;b.enabled=value.toBool();}
 else{bool ok;const auto n=value.toDouble(&ok);if(!ok||!std::isfinite(n))return false;if(key=="frequency"&&n>=20&&n<=20000)b.frequency=n;else if(key=="gain"&&n>=-18&&n<=18)b.gain=n;else if(key=="q"&&n>=.1&&n<=18)b.q=n;else if(key=="type"&&n>=0&&n<=5&&std::floor(n)==n)b.type=int(n);else return false;}update();return true;
}
bool EqualizerBridge::setBandPoint(int index,double frequency,double gain){if(index<0||index>=audio::PeqBands||!std::isfinite(frequency)||!std::isfinite(gain))return false;auto &b=m_config.bands[index];b.frequency=std::clamp(frequency,20.,20000.);if(b.type<3)b.gain=std::clamp(gain,-18.,18.);update();return true;}
void EqualizerBridge::reset(){const bool enabled=m_config.enabled;m_config=defaults();m_config.enabled=enabled;update();}
bool EqualizerBridge::usePreset(const QString &id){auto c=defaults();if(id=="bass"){c.bands[0].frequency=110;c.bands[0].gain=5;c.bands[0].q=.707;}else if(id=="voice"){c.bands[2].gain=-2;c.bands[4].gain=3;c.bands[4].q=.8;}else if(id=="soft"){c.bands[7].frequency=7000;c.bands[7].gain=-4;c.bands[7].q=.707;}else if(id!="flat"){bool found=false;for(const auto &v:m_presets)if(v.toObject()["id"].toString()==id){c=decode(v.toObject()["settings"].toObject());found=true;break;}if(!found)return false;}c.enabled=m_config.enabled;m_config=c;update();return true;}
bool EqualizerBridge::savePreset(const QString &name){
 const auto clean=name.trimmed();if(clean.isEmpty()||clean.size()>40){message("预设名称应为 1～40 个字符");return false;}
 for(const auto &v:m_presets)if(v.toObject()["name"].toString()==clean){message("已存在同名预设");return false;}
 if(m_presets.size()>=20){message("最多保存 20 个自定义预设");return false;}
 m_presets.append(QJsonObject{{"id",QUuid::createUuid().toString(QUuid::Id128)},{"name",clean},{"settings",encode(m_config)}});markDirty();
 if(!flush()){m_presets.removeLast();markDirty();emit changed();return false;}
 m_presetDraft.clear();emit editStateChanged();emit changed();return true;
}
bool EqualizerBridge::removePreset(const QString &id){for(int i=0;i<m_presets.size();++i)if(m_presets[i].toObject()["id"].toString()==id){m_presets.removeAt(i);markDirty();emit changed();return true;}return false;}
bool EqualizerBridge::flush(){
 if(!m_dirty)return true;const auto data=document();m_saving=true;const bool ok=m_library->setSetting("parametricEqualizer",data);m_saving=false;
 if(!ok){message("均衡器设置未保存："+m_library->lastError());return false;}
 m_savedDocument=data;m_dirty=false;message({});emit editStateChanged();return true;
}
}
