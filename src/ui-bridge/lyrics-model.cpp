#include "lyrics-model.h"
#include <QFileInfo>
#include <QCryptographicHash>
#include <QThread>
#include <algorithm>
namespace relaxer {
LyricsModel::LyricsModel(Library *library,PlaybackFacade *facade,QObject *parent)
    :QAbstractListModel(parent),m_library(library),m_facade(facade) {
    Q_ASSERT(library->thread()==thread()&&facade->thread()==thread());
    m_reload.setSingleShot(true);connect(&m_reload,&QTimer::timeout,this,&LyricsModel::readCurrent);
    connect(library,&Library::changed,this,&LyricsModel::schedule);
    connect(facade,&PlaybackFacade::trackChanged,this,[this]{clear();schedule();});
    connect(facade,&PlaybackFacade::positionChanged,this,[this](qint64 p,qint64){position(p);});
    connect(library,&QObject::destroyed,this,[this]{m_library=nullptr;m_reload.stop();clear();});
    connect(facade,&QObject::destroyed,this,[this]{m_facade=nullptr;m_reload.stop();clear();});
    schedule();
}
int LyricsModel::rowCount(const QModelIndex &parent) const {return parent.isValid()?0:m_rows.size();}
QHash<int,QByteArray> LyricsModel::roleNames() const {
    return {{RowKey,"rowKey"},{Timestamp,"timestamp"},{Original,"original"},{Translation,"translation"},
            {Current,"current"},{Played,"played"},{HasTranslation,"hasTranslation"}};
}
QVariant LyricsModel::data(const QModelIndex &index,int role) const {
    if(!index.isValid()||index.row()<0||index.row()>=m_rows.size())return {};
    const auto &row=m_rows[index.row()];
    switch(role){
    case RowKey:return trackKey()+':'+QString::number(row.documentRow);
    case Timestamp:return row.time;case Original:case Qt::DisplayRole:return row.original;
    case Translation:return row.translation;case Current:return index.row()==m_current;
    case Played:return m_current>=0&&index.row()<m_current;
    case HasTranslation:return !row.translation.isEmpty();default:return {};
    }
}
QString LyricsModel::status() const {
    if(m_loading)return "正在读取本地歌词";
    if(!m_error.isEmpty())return "歌词读取失败";
    if(!m_track.id)return "选择一首音乐";
    if(m_timeline.instrumental)return "纯音乐，请欣赏";
    if(m_rows.isEmpty())return "暂无歌词，可导入本地歌词或手动检索";
    if(m_preview)return "未保存的翻译预览 · 仅在明确保存后写入";
    return timed()?"本地时间轴歌词":"文本歌词 · 无有效时间轴";
}
void LyricsModel::clear() {
    beginResetModel();m_rows.clear();m_track={};m_source.clear();m_presented.clear();m_timeline={};
    m_current=-1;m_offset=0;m_preview=false;m_error.clear();m_loading=false;endResetModel();
    if(!m_cover.isEmpty()){m_cover=QUrl();emit artworkChanged();}
    emit currentIndexChanged();emit offsetChanged();emit documentChanged();
}
void LyricsModel::schedule(){
    if(!m_loading&&m_facade&&(m_track.id!=m_facade->current().id||m_force)){m_loading=true;emit documentChanged();}
    if(!m_reload.isActive())m_reload.start(0);
}
void LyricsModel::reload(){m_force=true;schedule();}
void LyricsModel::readCurrent() {
    if(!m_library||!m_facade){clear();return;}
    Q_ASSERT(QThread::currentThread()==thread());
    const auto live=m_facade->current();const auto track=m_library->track(live.id);
    if(!track.id){clear();return;}
    const bool changed=m_track.id!=track.id||m_track.hash!=track.hash;
    if(changed){m_source.clear();m_presented.clear();m_preview=false;}
    m_track=track;
    const auto path=track.cover.isEmpty()?QString():m_library->absolute(track.cover);
    const auto cover=path.isEmpty()?QUrl():QUrl::fromLocalFile(path);
    if(m_cover!=cover){m_cover=cover;emit artworkChanged();}
    const int offset=m_library->lyricOffset(track.id);
    if(m_offset!=offset){m_offset=offset;emit offsetChanged();}
    QByteArray bytes;QString error;
    if(m_library->isInstrumental(track.id))bytes=QString("纯音乐，请欣赏").toUtf8();
    else if(!track.lyrics.isEmpty()){
        const auto file=LyricsDocumentReader::readFile(m_library->absolute(track.lyrics));
        bytes=file.bytes;
        if(!file.opened||!file.complete||file.tooLarge)error=file.error.isEmpty()?QString("歌词无法完整读取或超过4 MB"):file.error;
    }
    const bool replace=changed||m_force||bytes!=m_source||m_error!=error;
    m_force=false;m_source=bytes;m_error=error;m_loading=false;
    if(replace)present(error.isEmpty()?bytes:QByteArray(),false);
    position(m_facade->position());emit documentChanged();
}
void LyricsModel::present(const QByteArray &bytes,bool preview) {
    if(bytes==m_presented&&preview==m_preview)return;
    const auto document=Lyrics::parse(bytes);QVector<Row> rows;bool pair=false;
    for(int i=0;i<document.lines.size();++i){
        const auto &line=document.lines[i];
        // Same pairing contract as existing LyricView; no new LRC parser.
        if(document.timed&&!rows.isEmpty()&&rows.last().time==line.time&&rows.last().translation.isEmpty())rows.last().translation=line.text;
        else if(!document.timed&&document.bilingual&&pair&&!rows.isEmpty()){rows.last().translation=line.text;pair=false;}
        else{rows.append({i,line.time,line.text,{}});pair=!document.timed&&document.bilingual;}
    }
    beginResetModel();m_presented=bytes;m_preview=preview;m_rows=std::move(rows);m_timeline={};
    m_timeline.timed=document.timed;m_timeline.instrumental=document.instrumental;
    for(const auto &row:m_rows)m_timeline.lines.append(LyricLine{row.time,QString()});
    m_current=-1;endResetModel();position(m_facade?m_facade->position():0);emit documentChanged();
}
void LyricsModel::position(qint64 ms) {
    const int index=timed()?m_timeline.indexAt(ms-m_offset):-1;
    if(index==m_current)return;const int previous=m_current;m_current=index;
    // Only the affected range changes state; no per-frame lyric scan/reset.
    const int lo=qMax(0,qMin(previous,index)),hi=qMax(previous,index);
    if(hi>=lo&&hi<m_rows.size())emit dataChanged(this->index(lo),this->index(hi),{Current,Played});
    emit currentIndexChanged();
}
bool LyricsModel::setOffsetMs(int value) {
    if(!m_library||!m_facade||!m_track.id||m_facade->current().id!=m_track.id||value < -5000||value>5000)return false;
    if(!m_library->setLyricOffset(m_track.id,value))return false;
    m_offset=value;emit offsetChanged();position(m_facade->position());return true;
}
bool LyricsModel::seekRow(int row) {
    if(!m_facade||!timed()||row<0||row>=m_rows.size()||m_track.id!=m_facade->current().id||!m_facade->seekable())return false;
    m_facade->seek(qBound<qint64>(0,m_rows[row].time+m_offset,m_facade->duration()));return true;
}
void LyricsModel::presentTranslation(const TranslationSummary &summary) {
    if(!m_facade||!m_library||summary.trackId!=m_facade->current().id)return;
    if(m_track.id!=summary.trackId)readCurrent();
    if(summary.trackHash!=m_track.hash)return;
    if(summary.hasPreview()){
        if(QCryptographicHash::hash(m_source,QCryptographicHash::Sha256)!=summary.sourceDigest)return;
        QString error;const auto bytes=ai::lyrics::merge(m_source,summary.prepared,summary.translations,&error);
        if(error.isEmpty())present(bytes,true);
    }else if(m_preview){readCurrent();present(m_source,false);}
}
}
