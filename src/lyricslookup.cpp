#include <QApplication>
#include <QScreen>
#include <QClipboard>
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QScrollBar>
#include <QScopedValueRollback>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkProxy>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStringConverter>
#include <QTextDocumentFragment>
#include "lyricslookup.h"
#include "domain/lyric-match.h"
#include "importer.h"
#include "lyric-remote.h"
#include "lyric-text.h"
#include "lyrics.h"
#include "lyrics-document-reader.h"
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QMessageBox>
#include <QTimer>
#include <QDateTime>
#include <QFile>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QNetworkProxyFactory>
#include <QVBoxLayout>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QDir>
#include <QThreadPool>
#include <QRunnable>
#include <QThread>
#include <QCoreApplication>
#include "widgets.h"
#include "smooth-scroll.h"
#include <cmath>
#include <algorithm>
#include <QPointer>
#include <mutex>
#include <thread>
namespace relaxer {
namespace {
std::atomic<qint64> g_matchScoreCalls{0};
std::atomic<qint64> g_rankRuns{0};
std::atomic_bool &lyricComputeCancel(){static std::atomic_bool flag{false};return flag;}
QThreadPool *lyricPool(){
    static QThreadPool *pool=nullptr;
    static std::once_flag once;
    std::call_once(once,[]{
        pool=new QThreadPool;
        const int cores=QThread::idealThreadCount();
        pool->setMaxThreadCount(std::max(2,cores<1?2:cores));
        if(auto *app=QCoreApplication::instance())
            QObject::connect(app,&QCoreApplication::aboutToQuit,[]{lyricComputeCancel().store(true);});
    });
    return pool;
}
class LyricRunnable:public QRunnable {
public:
    explicit LyricRunnable(std::function<void()> fn):fn(std::move(fn)){setAutoDelete(true);}
    void run() override {if(fn)fn();}
private:
    std::function<void()> fn;
};
enum RowState { Waiting, Searching, Complete, NeedsChoice, Failed };
constexpr int StateRole = Qt::UserRole + 1;
constexpr int StatusRole = Qt::UserRole + 2;
constexpr int TranslationFilterRole = Qt::UserRole + 4;
class LookupRowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    int angle = 0;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {280,44}; }
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        p->save();p->setRenderHint(QPainter::Antialiasing);
        const bool selected=option.state & QStyle::State_Selected;
        const auto ink=option.palette.color(QPalette::Text);
        auto accent=option.palette.color(QPalette::Highlight);
        if(selected){auto fill=accent;fill.setAlpha(24);p->setPen(Qt::NoPen);p->setBrush(fill);p->drawRoundedRect(QRectF(option.rect).adjusted(2,2,-2,-2),8,8);}
        else if(option.state & QStyle::State_MouseOver){auto fill=ink;fill.setAlpha(9);p->fillRect(option.rect,fill);}
        const auto translationStatus=index.data(TranslationFilterRole).toString();
        const int state=translationStatus.isEmpty()?index.data(StateRole).toInt():NeedsChoice;
        const bool multi=parent()->property("lookupMultiSelect").toBool();const int extra=multi?24:0;
        if(multi){const QRectF box(option.rect.left()+8,option.rect.center().y()-7,14,14);p->setPen(QPen(selected?accent:ink,1));p->setBrush(selected?accent:Qt::transparent);p->drawRoundedRect(box,3,3);if(selected){p->setPen(QPen(option.palette.color(QPalette::HighlightedText),1.5));QPainterPath tick;tick.moveTo(box.topLeft()+QPointF(3,7));tick.lineTo(box.topLeft()+QPointF(6,10));tick.lineTo(box.topLeft()+QPointF(11,4));p->drawPath(tick);}}
        const QPointF center(option.rect.left()+17+extra,option.rect.center().y());
        const QRectF circle(center.x()-6,center.y()-6,12,12);
        QColor icon=option.palette.color(QPalette::PlaceholderText);
        if(state==Complete)icon=QColor("#5c9671");
        if(state==Searching)icon=accent;
        if(state==NeedsChoice)icon=QColor("#b88a35");
        p->setBrush(Qt::NoBrush);p->setPen(QPen(icon,1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        if(state==Searching){p->drawArc(circle,angle*16,270*16);}
        else if(state==Complete){
            p->drawEllipse(circle);QPainterPath check;check.moveTo(center+QPointF(-3,0));check.lineTo(center+QPointF(-.5,2.5));check.lineTo(center+QPointF(3.5,-3));p->drawPath(check);
        }else if(state==NeedsChoice){
            QPainterPath triangle;triangle.moveTo(center+QPointF(0,-7));triangle.lineTo(center+QPointF(7,6));triangle.lineTo(center+QPointF(-7,6));triangle.closeSubpath();p->drawPath(triangle);
            p->drawLine(center+QPointF(0,-2.5),center+QPointF(0,1.5));p->drawPoint(center+QPointF(0,4));
        }else if(state==Waiting){
            p->drawEllipse(circle);p->drawLine(center,center+QPointF(0,-3));p->drawLine(center,center+QPointF(3,1));
        }else{
            p->drawLine(center+QPointF(-4,-4),center+QPointF(4,4));p->drawLine(center+QPointF(-4,4),center+QPointF(4,-4));
        }
        QFont font=option.font;font.setWeight(QFont::Normal);p->setFont(font);const QFontMetrics metrics(font);
        QString status;
        switch(state){case Complete:status="已完成";break;case Searching:status="查找中";break;case NeedsChoice:status="需选择";break;case Failed:status="未完成";break;default:status="待查找";}
        if(!translationStatus.isEmpty())status=translationStatus;
        const int x=option.rect.left()+34+extra;
        const int statusWidth=metrics.horizontalAdvance(status);
        const int maxWidth=qMax(0,option.rect.right()-x-statusWidth-20);
        const QString title=metrics.elidedText(index.data().toString(),Qt::ElideRight,maxWidth);
        const int baseline=option.rect.center().y()+(metrics.ascent()-metrics.descent())/2;
        p->setPen(ink);p->drawText(x,baseline,title);
        p->setPen(option.palette.color(QPalette::PlaceholderText));
        p->drawText(x+metrics.horizontalAdvance(title)+10,baseline,status);
        p->restore();
    }
};
bool rowComplete(QListWidgetItem *item){return item && item->data(StateRole).toInt()==Complete;}
QString recordLyrics(const QJsonObject &record){
    const auto synced=record["syncedLyrics"].toString();return synced.isEmpty()?record["plainLyrics"].toString():synced;
}
}
QString LyricsLookup::normalized(const QString &text){return LyricMatchPolicy::normalized(text);}
QString LyricsLookup::cleanTitle(const QString &text){return LyricMatchPolicy::cleanTitle(text);}
namespace {
bool knownArtist(const QString &s){return LyricMatchPolicy::knownArtist(s);}
bool sameArtist(const QString &a,const QString &b){return LyricMatchPolicy::sameArtist(a,b);}
QString versionMark(const QString &s){return LyricMatchPolicy::versionMark(s);}
QString baseTitle(QString s){return LyricMatchPolicy::baseTitle(std::move(s));}
}
Track LyricsLookup::searchTrack(const Track &track){return LyricMatchPolicy::searchTrack(track);}
bool LyricsLookup::confident(const Track &track,const QJsonObject &record){return LyricMatchPolicy::confident(track,record);}
double LyricsLookup::matchScore(const Track &track,const QJsonObject &record){
    g_matchScoreCalls.fetch_add(1,std::memory_order_relaxed);
    return LyricMatchPolicy::matchScore(track,record);
}
void LyricsLookup::resetMatchScoreCount(){g_matchScoreCalls.store(0);}
qint64 LyricsLookup::matchScoreCount(){return g_matchScoreCalls.load();}
void LyricsLookup::resetLyricRankRuns(){g_rankRuns.store(0);}
qint64 LyricsLookup::lyricRankRuns(){return g_rankRuns.load();}
int LyricsLookup::lyricComputeThreadCount(){const int cores=QThread::idealThreadCount();return std::max(2,cores<1?2:cores);}
QVector<QJsonObject> LyricsLookup::rankCandidates(const Track &query,const QVector<QJsonObject> &usable){
    struct Row {QJsonObject record;double score;};
    QVector<Row> rows;rows.reserve(usable.size());
    for(const auto &record:usable)rows.push_back({record,matchScore(query,record)});
    std::stable_sort(rows.begin(),rows.end(),[](const Row &a,const Row &b){return a.score>b.score;});
    QVector<QJsonObject> out;out.reserve(rows.size());
    for(const auto &row:rows)out.push_back(row.record);
    return out;
}
QString LyricsLookup::trackContext(const Track &t) const {
    return QString::number(t.id)+QChar(1)+t.hash+QChar(1)+t.title+QChar(1)+t.artist+QChar(1)+t.album+QChar(1)+QString::number(t.duration)+QChar(1)+t.file;
}
void LyricsLookup::appendCollected(const JobPtr &j,const QJsonObject &record){
    if(j->collected.size()>=100)return;
    j->collected.append(record);
    ++j->collectionEpoch;
    j->readyValid=false;
}
QByteArray LyricsLookup::withOffset(const QByteArray &data,int ms){
    if(ms==0)return data;
    const bool utf16=data.startsWith("\xff\xfe")||data.startsWith("\xfe\xff");
    const bool bom8=data.startsWith("\xef\xbb\xbf");
    QString text;
    if(utf16){QStringDecoder decoder(data.startsWith("\xff\xfe")?QStringConverter::Utf16LE:QStringConverter::Utf16BE);text=decoder(data.mid(2));}
    else text=QString::fromLatin1(bom8?data.mid(3):data);
    if(!QRegularExpression("\\[\\d+:\\d{1,2}(?:[.:]\\d{1,3})?\\]").match(text).hasMatch())return data;
    text.remove(QRegularExpression("^\\[offset:[+-]?\\d+\\][ \\t]*\\r?\\n?",QRegularExpression::MultilineOption|QRegularExpression::CaseInsensitiveOption));
    text=QString("[offset:%1]\n").arg(ms)+text;
    if(utf16){QStringEncoder encoder(data.startsWith("\xff\xfe")?QStringConverter::Utf16LE:QStringConverter::Utf16BE);const QByteArray encoded=encoder(text);return data.left(2)+encoded;}
    return (bom8?data.left(3):QByteArray())+text.toLatin1();
}
QString LyricsLookup::matchReason(const Track &t,const QJsonObject &r){
    QStringList reasons;const auto clean=searchTrack(t);
    reasons<<((normalized(clean.title)==normalized(cleanTitle(r["trackName"].toString())))?"歌名一致":"歌名不完全一致");
    if(sameArtist(clean.artist,r["artistName"].toString()))reasons<<"歌手一致";
    else if(knownArtist(clean.artist))reasons<<"歌手不完全一致";
    const double seconds=r["duration"].toDouble(-1);if(t.duration>0&&seconds>0)reasons<<QString("时长相差 %1 秒").arg(qAbs(t.duration/1000.-seconds),0,'f',1);
    if(!clean.album.isEmpty()&&clean.album!="未知专辑"&&normalized(clean.album)!=normalized(r["albumName"].toString()))reasons<<"专辑不同";
    if(r["manualOnly"].toBool())reasons<<"备用来源需人工确认";
    if(r["instrumental"].toBool())reasons<<"来源标记为纯音乐";
    return reasons.join(" / ");
}
bool LyricsLookup::credibleLyrics(const QByteArray &data,const Track &t,QString *reason){
    if(!LyricMatchPolicy::rawLyricsAllowed(data,reason))return false;
    return LyricMatchPolicy::credibleLyrics(Lyrics::parse(data),t.duration,reason);
}
LyricsLookup::LyricsLookup(Library *library,QVector<qint64> ids,QWidget *parent,QUrl endpoint,QUrl backupEndpoint,bool pinExplicit,ai::AiRuntime *aiRuntime)
    :QDialog(parent),m_library(library),m_ids(std::move(ids)),m_sources(),m_aiRuntime(aiRuntime),m_network(this),m_pinExplicit(pinExplicit){
    installSmoothScrolling(qApp);
    if(backupEndpoint.isEmpty()&&endpoint.host()=="lrclib.net")backupEndpoint=QUrl("https://api.lyrics.ovh/v1");
    m_sources.append({"LRCLIB",endpoint,0,false});if(!backupEndpoint.isEmpty())m_sources.append({"Lyrics.ovh",backupEndpoint,1,true});
    if(endpoint.host()=="lrclib.net"){m_sources.append({"网易云",QUrl("https://music.163.com"),2,false});m_sources.append({"QQ音乐",QUrl("https://y.qq.com"),3,false});m_sources.append({"酷狗",QUrl("https://www.kugou.com"),4,false});}
    if(endpoint.host()=="127.0.0.1"||endpoint.host()=="localhost"){m_network.setProxy(QNetworkProxy::NoProxy);m_proxyReady=true;}else{m_proxyReady=false;m_proxyState=std::make_shared<std::atomic_bool>(false);QPointer<LyricsLookup> self(this);auto state=m_proxyState;std::thread([self,state]{QNetworkProxy chosen(QNetworkProxy::NoProxy);const auto found=QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(QUrl("https://lrclib.net")));for(const auto &proxy:found)if(proxy.type()==QNetworkProxy::HttpProxy||proxy.type()==QNetworkProxy::Socks5Proxy||proxy.type()==QNetworkProxy::HttpCachingProxy){chosen=proxy;break;}if(!qApp)return;QMetaObject::invokeMethod(qApp,[self,state,chosen]{if(!self||!state||state->load())return;self->m_network.setProxy(chosen);self->m_proxyReady=true;const auto jobs=self->m_jobs.values();for(const auto &job:jobs)if(self->live(job))self->pumpQueries(job);},Qt::QueuedConnection);}).detach();}
    setObjectName("lyricsLookupDialog");setWindowTitle("查找歌词");
    QSize initial(1240,780);if(auto *screen=QGuiApplication::primaryScreen())initial=initial.boundedTo(screen->availableGeometry().size()-QSize(40,60));
    setMinimumSize(860,560);resize(initial);setModal(true);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(18,18,18,18);layout->setSpacing(12);
    auto *splitter=new QSplitter(Qt::Horizontal,this);splitter->setObjectName("lyricsSplit");splitter->setChildrenCollapsible(false);
    auto *left=new QWidget(splitter);auto *leftLayout=new QVBoxLayout(left);leftLayout->setContentsMargins(0,0,8,0);leftLayout->setSpacing(10);
    auto *toolbar=new QHBoxLayout;m_start=new QPushButton("搜索歌词",left);m_start->setObjectName("lyricsStart");m_stop=new QPushButton("停止",left);m_stop->setObjectName("lyricsStop");m_stop->setEnabled(false);m_translate=new QPushButton("AI 翻译整首",left);m_translate->setObjectName("lyricsTranslate");m_aiCancelTranslation=new QPushButton("取消任务",left);m_aiCancelTranslation->setObjectName("lyricsCancelTranslation");m_aiCancelTranslation->setEnabled(false);toolbar->addWidget(m_start);toolbar->addWidget(m_stop);m_multiButton=new QPushButton("多选",left);m_multiButton->setObjectName("lyricsMultiSelect");m_multiButton->setCheckable(true);toolbar->addWidget(m_multiButton);m_allButton=new QPushButton("全选",left);m_allButton->setObjectName("lyricsSelectAll");m_allButton->hide();toolbar->addWidget(m_allButton);toolbar->addStretch();leftLayout->addLayout(toolbar);
    auto *translationTools=new QHBoxLayout;translationTools->addWidget(m_translate);translationTools->addWidget(m_aiCancelTranslation);translationTools->addStretch();leftLayout->addLayout(translationTools);
    auto *checks=new QGridLayout;
    m_replace=new QCheckBox("重新匹配并替换已有歌词",left);m_replace->setObjectName("lyricsReplace");
    m_missingOnly=new QCheckBox("仅显示无歌词歌曲",left);m_missingOnly->setObjectName("lyricsMissingOnly");
    m_missingTranslationOnly=new QCheckBox("只看缺失翻译",left);m_missingTranslationOnly->setObjectName("lyricsMissingTranslationOnly");
    m_missingTranslationOnly->setToolTip("本地判断有歌词但缺少中文译文的外文歌曲；混合、部分译文和疑似情况也显示，并标注待确认。不会发送云端请求。");
    m_refreshTranslationFilter=new QPushButton("刷新判断",left);m_refreshTranslationFilter->setObjectName("lyricsRefreshTranslationFilter");m_refreshTranslationFilter->setEnabled(false);
    m_refreshTranslationFilter->setToolTip("重新读取本地歌词，更新语言和译文判断。");
    checks->addWidget(m_missingOnly,0,0);checks->addWidget(m_missingTranslationOnly,0,1);
    checks->addWidget(m_replace,1,0);checks->addWidget(m_refreshTranslationFilter,1,1);checks->setColumnStretch(2,1);leftLayout->addLayout(checks);
    auto *cloudNotice=new QLabel("AI 翻译仅在你手动启动后发送整首歌词至 DeepSeek 云端；可能产生费用，不会自动翻译或自动重试。",left);cloudNotice->setObjectName("lyricsCloudTranslationNotice");cloudNotice->setWordWrap(true);leftLayout->addWidget(cloudNotice);
    auto *heading=new QLabel(QString("查找列表 · %1 首").arg(m_ids.size()),left);heading->setObjectName("lyricsListHeading");leftLayout->addWidget(heading);
    m_tracks=new QListWidget(left);m_tracks->setObjectName("lyricsTracks");m_tracks->setFrameShape(QFrame::NoFrame);m_tracks->setMouseTracking(true);m_tracks->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *delegate=new LookupRowDelegate(m_tracks);m_tracks->setItemDelegate(delegate);leftLayout->addWidget(m_tracks,1);
    // Freeze the opening snapshot before partitioning. Never move live rows: jobs and selections use row indices.
    for(auto id:m_ids){const auto t=m_library->track(id);if(t.lyrics.isEmpty()||!QFileInfo::exists(m_library->absolute(t.lyrics)))m_missingAtOpen.insert(id);if(m_pinExplicit)m_pinnedIds.insert(id);}
    std::stable_partition(m_ids.begin(),m_ids.end(),[this](qint64 id){return m_missingAtOpen.contains(id);});
    for(int i=0;i<m_ids.size();++i){
        const auto t=m_library->track(m_ids[i]);auto *item=new QListWidgetItem(t.title+" · "+t.artist,m_tracks);item->setData(Qt::UserRole,t.id);
        const bool has=!m_missingAtOpen.contains(t.id);
        const auto cached=m_library->lookupCache(t.id);
        if(cached.fresh&&!cached.noResult&&!cached.records.isEmpty()){m_candidates[t.id]=cached.records;m_candidateHashes[t.id]=t.hash;}
        setRowStatus(i,has?QString("已有本地歌词"):m_candidates.contains(t.id)?QString("需选择版本（%1 个） · 使用上次结果").arg(cached.records.size()):QString("等待查找"));
    }
    m_emptyHint=new QLabel("当前没有缺失歌词的歌曲，可取消勾选以查看全部歌曲。",left);m_emptyHint->setObjectName("lyricsMissingEmpty");m_emptyHint->setWordWrap(true);m_emptyHint->hide();leftLayout->addWidget(m_emptyHint);
    auto *timer=new QTimer(this);timer->setInterval(40);connect(timer,&QTimer::timeout,this,[this,delegate]{if(!m_running)return;delegate->angle=(delegate->angle+12)%360;m_tracks->setProperty("spinnerAngle",delegate->angle);QRegion region;for(int i=0;i<m_tracks->count();++i){auto *item=m_tracks->item(i);if(!item||item->isHidden()||item->data(StateRole).toInt()!=Searching)continue;region+=m_tracks->visualItemRect(item);}if(!region.isEmpty())m_tracks->viewport()->update(region);});timer->start();
    m_translationAuditDebounce=new QTimer(this);m_translationAuditDebounce->setSingleShot(true);m_translationAuditDebounce->setInterval(180);
    connect(m_translationAuditDebounce,&QTimer::timeout,this,&LyricsLookup::startTranslationCacheAudit);
    auto *right=new QWidget(splitter);auto *rightLayout=new QVBoxLayout(right);rightLayout->setContentsMargins(8,0,0,0);rightLayout->setSpacing(10);
    auto *search=new QHBoxLayout;m_searchTitle=new QLineEdit(right);m_searchTitle->setObjectName("lyricsSearchTitle");m_searchTitle->setPlaceholderText("搜索歌名");m_searchArtist=new QLineEdit(right);m_searchArtist->setObjectName("lyricsSearchArtist");m_searchArtist->setPlaceholderText("歌手（可留空）");m_manualSearch=new QPushButton("手动搜索",right);m_manualSearch->setObjectName("lyricsManualSearch");m_moreButton=new QPushButton("更多操作 ⌄",right);m_moreButton->setObjectName("lyricsMore");m_moreButton->setCheckable(true);search->addWidget(m_searchTitle,3);search->addWidget(m_searchArtist,2);search->addWidget(m_manualSearch);search->addWidget(m_moreButton);rightLayout->addLayout(search);
    m_morePanel=new QWidget(right);m_morePanel->setObjectName("lyricsMorePanel");m_morePanel->hide();auto *options=new QVBoxLayout(m_morePanel);options->setContentsMargins(0,0,0,0);options->setSpacing(8);
    auto *fragmentRow=new QHBoxLayout;m_fragment=new QLineEdit(m_morePanel);m_fragment->setObjectName("lyricsSearchFragment");m_fragment->setPlaceholderText("记得的歌词片段（用于手动搜索）");m_pasteButton=new QPushButton("粘贴歌词",m_morePanel);m_pasteButton->setObjectName("lyricsPaste");m_pasteButton->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);fragmentRow->addWidget(m_fragment,1);fragmentRow->addWidget(m_pasteButton);options->addLayout(fragmentRow);
    auto *offsetRow=new QHBoxLayout;offsetRow->setSpacing(8);m_offset=new QSpinBox(m_morePanel);m_offset->setObjectName("lyricsOffset");m_offset->setRange(-5000,5000);m_offset->setSingleStep(100);m_offset->setSuffix(" ms");m_offset->setFixedWidth(110);m_offset->setToolTip("设置歌词文件的绝对偏移；0 保留原文件偏移，正数整体延后。批量操作仅处理已经保存的滚动歌词。");
    offsetRow->addWidget(new QLabel("保存时间偏移",m_morePanel));offsetRow->addWidget(m_offset);m_saveOffset=new QPushButton("保存",m_morePanel);m_saveOffset->setObjectName("lyricsSaveOffset");m_importButton=new QPushButton("导入 .lrc",m_morePanel);m_importButton->setObjectName("lyricsImport");m_mark=new QPushButton("标记为纯音乐",m_morePanel);m_mark->setObjectName("lyricsMarkInstrumental");for(auto *button:{m_saveOffset,m_importButton,m_mark}){button->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);offsetRow->addWidget(button);}offsetRow->addStretch();options->addLayout(offsetRow);rightLayout->addWidget(m_morePanel);
    connect(m_moreButton,&QPushButton::toggled,this,[this](bool on){m_morePanel->setVisible(m_multi||on);m_moreButton->setText(on?"更多操作 ⌃":"更多操作 ⌄");});connect(m_pasteButton,&QPushButton::clicked,this,&LyricsLookup::pasteLyrics);connect(m_importButton,&QPushButton::clicked,this,&LyricsLookup::importLyrics);connect(m_mark,&QPushButton::clicked,this,&LyricsLookup::markInstrumental);connect(m_saveOffset,&QPushButton::clicked,this,&LyricsLookup::saveOffsets);
    connect(m_multiButton,&QPushButton::toggled,this,&LyricsLookup::setMultiSelect);connect(m_allButton,&QPushButton::clicked,this,[this]{int visible=0,selected=0;for(int r=0;r<m_tracks->count();++r)if(!m_tracks->item(r)->isHidden()){++visible;if(m_tracks->item(r)->isSelected())++selected;}const bool on=visible!=selected;for(int r=0;r<m_tracks->count();++r)if(!m_tracks->item(r)->isHidden())m_tracks->item(r)->setSelected(on);controls();});
    auto *filters=new QHBoxLayout;m_sourceFilter=new RoundedComboBox(right);m_sourceFilter->setObjectName("lyricsSourceFilter");m_sourceFilter->addItem("全部来源");for(const auto &s:m_sources)m_sourceFilter->addItem(s.label);m_syncedOnly=new QCheckBox("只看滚动歌词",right);m_syncedOnly->setObjectName("lyricsSyncedOnly");filters->addWidget(m_sourceFilter);filters->addWidget(m_syncedOnly);filters->addStretch();rightLayout->addLayout(filters);
    connect(m_sourceFilter,&QComboBox::currentIndexChanged,this,[this](int index){if(index>0&&m_sources[index-1].kind==1)m_syncedOnly->setChecked(false);rebuildCandidates();});connect(m_syncedOnly,&QCheckBox::toggled,this,&LyricsLookup::rebuildCandidates);
    auto *instrumentRow=new QHBoxLayout;m_instrumentHint=new QLabel(right);m_instrumentHint->setWordWrap(true);m_continue=new QPushButton("继续查找歌词",right);m_continue->setObjectName("lyricsContinueSearch");auto *confirm=new QPushButton("确认为纯音乐",right);confirm->setObjectName("lyricsConfirmInstrumental");m_instrumentHint->hide();m_continue->hide();confirm->hide();instrumentRow->addWidget(m_instrumentHint,1);instrumentRow->addWidget(confirm);instrumentRow->addWidget(m_continue);rightLayout->addLayout(instrumentRow);connect(confirm,&QPushButton::clicked,this,&LyricsLookup::markInstrumental);connect(m_continue,&QPushButton::clicked,this,&LyricsLookup::manualSearch);
    auto *panels=new QSplitter(Qt::Vertical,right);panels->setObjectName("lyricsPanels");panels->setChildrenCollapsible(false);
    m_candidatePanel=new QFrame(panels);m_candidatePanel->setObjectName("lyricsCandidatePanel");auto *candidate=new QVBoxLayout(m_candidatePanel);candidate->setContentsMargins(14,14,14,14);candidate->setSpacing(10);
    m_candidateTitle=new QLabel(m_candidatePanel);m_candidateTitle->setObjectName("lyricsCandidateTitle");m_candidateTitle->setWordWrap(true);candidate->addWidget(m_candidateTitle);
    m_versions=new QListWidget(m_candidatePanel);m_versions->setObjectName("lyricsVersions");m_versions->setFrameShape(QFrame::NoFrame);m_versions->setWordWrap(true);m_versions->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);candidate->addWidget(m_versions,1);
    m_previewPanel=new QFrame(panels);m_previewPanel->setObjectName("lyricsPreviewPanel");m_previewPanel->setMinimumHeight(150);m_previewPanel->setMaximumHeight(240);
    auto *previewLayout=new QVBoxLayout(m_previewPanel);previewLayout->setContentsMargins(14,12,14,12);previewLayout->setSpacing(8);
    auto *columns=new QHBoxLayout;columns->setSpacing(16);
    for(int column=0;column<2;++column){
        auto *columnLayout=new QVBoxLayout;columnLayout->setSpacing(6);columnLayout->setAlignment(Qt::AlignTop);
        auto *title=new QLabel(column==0?"歌词原文":"简体译文",m_previewPanel);title->setObjectName("lyricsPreviewHeading");columnLayout->addWidget(title);
        auto *edit=new QPlainTextEdit(m_previewPanel);edit->setReadOnly(true);edit->setFrameShape(QFrame::NoFrame);edit->setLineWrapMode(QPlainTextEdit::NoWrap);edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        edit->setObjectName(column==0?"lyricsPreview":"lyricsTranslationPreview");edit->setPlaceholderText(column==0?"选中版本后显示歌词原文":"暂无译文，翻译完成后在此显示");
        columnLayout->addWidget(edit,1);columns->addLayout(columnLayout,1);if(column==0)m_preview=edit;else m_translationPreview=edit;
    }
    previewLayout->addLayout(columns,1);
    connect(m_preview->verticalScrollBar(),&QScrollBar::valueChanged,m_translationPreview->verticalScrollBar(),&QScrollBar::setValue);
    connect(m_translationPreview->verticalScrollBar(),&QScrollBar::valueChanged,m_preview->verticalScrollBar(),&QScrollBar::setValue);
    panels->setStretchFactor(0,1);panels->setStretchFactor(1,0);panels->setSizes({280,195});rightLayout->addWidget(panels,1);
    auto *actions=new QHBoxLayout;m_incomplete=new QPushButton("只显示未完成",right);m_incomplete->setObjectName("lyricsIncomplete");m_incomplete->setCheckable(true);actions->addWidget(m_incomplete);actions->addStretch();m_aiSaveTranslation=new QPushButton("保存双语歌词",right);m_aiSaveTranslation->setObjectName("lyricsSaveTranslation");m_aiSaveTranslation->setEnabled(false);actions->addWidget(m_aiSaveTranslation);m_apply=new QPushButton("使用选中版本",right);m_apply->setObjectName("lyricsApply");m_apply->setEnabled(false);actions->addWidget(m_apply);auto *close=new QPushButton("保存并关闭",right);close->setObjectName("lyricsSaveClose");actions->addWidget(close);rightLayout->addLayout(actions);
    m_translationProgressPanel=new QWidget(right);m_translationProgressPanel->setObjectName("lyricsTranslationProgressPanel");m_translationProgressPanel->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Fixed);
    auto *translationLayout=new QHBoxLayout(m_translationProgressPanel);translationLayout->setContentsMargins(0,2,0,0);translationLayout->setSpacing(6);
    m_translationProgressLabel=new QLabel("翻译尚未开始",m_translationProgressPanel);m_translationProgressLabel->setObjectName("lyricsTranslationProgressLabel");m_translationProgressLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    m_translationProgress=new QProgressBar(m_translationProgressPanel);m_translationProgress->setObjectName("lyricsTranslationProgress");m_translationProgress->setRange(0,1);m_translationProgress->setValue(0);m_translationProgress->setFixedWidth(180);m_translationProgress->setFixedHeight(10);m_translationProgress->setTextVisible(false);
    translationLayout->addWidget(m_translationProgressLabel);translationLayout->addWidget(m_translationProgress);rightLayout->addWidget(m_translationProgressPanel,0,Qt::AlignRight);
    splitter->addWidget(left);splitter->addWidget(right);splitter->setStretchFactor(0,2);splitter->setStretchFactor(1,3);splitter->setSizes({360,700});layout->addWidget(splitter,1);
    m_status=new QLabel("等待开始查找",left);m_status->setObjectName("lyricsStatus");m_status->setWordWrap(true);leftLayout->addWidget(m_status);
    applyLookupStyle();
    connect(m_start,&QPushButton::clicked,this,&LyricsLookup::start);connect(m_stop,&QPushButton::clicked,this,&LyricsLookup::stop);connect(m_translate,&QPushButton::clicked,this,&LyricsLookup::translateSelected);connect(m_aiCancelTranslation,&QPushButton::clicked,this,[this]{cancelAiTranslation(QStringLiteral("本地翻译任务已取消；DeepSeek 可能已收到请求并继续处理或计费。"));});connect(m_aiSaveTranslation,&QPushButton::clicked,this,&LyricsLookup::saveAiTranslation);connect(m_manualSearch,&QPushButton::clicked,this,&LyricsLookup::manualSearch);connect(m_searchTitle,&QLineEdit::returnPressed,this,&LyricsLookup::manualSearch);connect(m_searchArtist,&QLineEdit::returnPressed,this,&LyricsLookup::manualSearch);connect(m_fragment,&QLineEdit::returnPressed,this,&LyricsLookup::manualSearch);
    connect(m_tracks,&QListWidget::itemSelectionChanged,this,[this]{const int row=m_tracks->currentRow();const qint64 selected=row>=0&&row<m_ids.size()?m_ids[row]:0;if(m_aiTaskId&&selected!=m_aiTrackId)cancelAiTranslation(QStringLiteral("已切换歌曲，旧翻译请求不会写入新歌曲。"));if(m_hasAiPreview&&selected!=m_aiTrackId)clearAiTranslationPreview();selectTrack();});connect(m_incomplete,&QPushButton::toggled,this,[this](bool on){m_incomplete->setText(on?"✓ 只显示未完成":"只显示未完成");if(on&&m_missingTranslationOnly){QSignalBlocker block(m_missingTranslationOnly);m_missingTranslationOnly->setChecked(false);}updateFilter();});
    connect(m_missingOnly,&QCheckBox::toggled,this,[this](bool on){if(on){QSignalBlocker block(m_missingTranslationOnly);m_missingTranslationOnly->setChecked(false);}updateFilter();});
    connect(m_missingTranslationOnly,&QCheckBox::toggled,this,[this](bool on){
        if(on){QSignalBlocker a(m_missingOnly),b(m_incomplete);m_missingOnly->setChecked(false);m_incomplete->setChecked(false);m_incomplete->setText("只显示未完成");}
        if(on&&!m_translationAssessments.isEmpty())requestTranslationCacheAudit();
        updateFilter();
    });
    connect(m_refreshTranslationFilter,&QPushButton::clicked,this,[this]{
        m_translationAssessments.clear();
        const auto id=m_ids.value(m_tracks->currentRow());
        if(id){m_lyricsSnapshots.insert(id,m_library->lyricsSaveSnapshot(id));m_snapshotConflict=false;}
        updateFilter();updatePreview();
    });
    connect(m_offset,qOverload<int>(&QSpinBox::valueChanged),this,[this]{updatePreview();});
    connect(m_versions,&QListWidget::currentRowChanged,this,[this](int row){const int track=m_tracks->currentRow();const auto records=m_candidates.value(m_ids.value(track));const int index=m_visibleRecords.value(row,-1);if(track<0||row<0||index<0||index>=records.size()){m_apply->setEnabled(false);updatePreview();return;}const auto r=records[index].toObject();updatePreview();m_candidateTitle->setText("候选版本 · "+matchReason(m_library->track(m_ids[track]),r));updateApplyButton();});
    connect(m_apply,&QPushButton::clicked,this,[this]{applySelected();});connect(close,&QPushButton::clicked,this,[this]{if(!m_multi&&m_apply->isEnabled()&&!applySelected())return;if(!confirmStopTranslations())return;stop();accept();});if(m_aiRuntime&&m_aiRuntime->requests())connect(m_aiRuntime->requests(),&ai::AiRequestCoordinator::completionFinished,this,[this](quint64 taskId,const QString &,const ai::ChatCompletionResult &result){completeAiTranslation(taskId,result);});m_missingOnly->setChecked(true);updateFilter();selectTrack();m_library->deferChanges(true);
}
LyricsLookup::~LyricsLookup(){if(m_proxyState)m_proxyState->store(true);if(m_translationAuditCancel)m_translationAuditCancel->store(true);if(m_translationAuditDebounce)m_translationAuditDebounce->stop();cancelAiTranslation();stop();if(m_library)m_library->deferChanges(false);}
void LyricsLookup::applyLookupStyle(){
    if(m_applyingStyle)return;
    QScopedValueRollback<bool> applying(m_applyingStyle,true);
    const auto bg=palette().color(QPalette::Base);const auto card=(bg.lightness()<128?bg.lighter(145):bg.darker(103)).name();
    const auto line=palette().color(QPalette::Mid).name(),accent=palette().color(QPalette::Highlight).name(),ink=palette().color(QPalette::Text).name();
    const auto css=QString("QFrame#lyricsCandidatePanel,QFrame#lyricsPreviewPanel{border:1px solid %1;border-radius:14px;background:%2;}QListWidget#lyricsTracks,QListWidget#lyricsVersions{background:transparent;border:none;outline:none;}QListWidget#lyricsVersions::item{padding:8px;border:1px solid transparent;border-radius:8px;}QListWidget#lyricsVersions::item:selected{background:rgba(229,69,95,20);color:%3;border-color:%4;}QPlainTextEdit#lyricsPreview,QPlainTextEdit#lyricsTranslationPreview{background:transparent;color:%3;border:none;padding:2px;selection-background-color:%4;}QLabel#lyricsListHeading,QLabel#lyricsCandidateTitle,QLabel#lyricsPreviewHeading{font-weight:600;}QLabel#lyricsTranslationProgressLabel{font-size:12px;}QProgressBar#lyricsTranslationProgress{background:%1;border:none;border-radius:4px;}QProgressBar#lyricsTranslationProgress::chunk{background:%4;border-radius:4px;}").arg(line,card,ink,accent);
    if(styleSheet()!=css)setStyleSheet(css);
}
void LyricsLookup::changeEvent(QEvent *event){
    QDialog::changeEvent(event);
    if(event->type()==QEvent::ActivationChange&&isActiveWindow())requestTranslationCacheAudit();
    if(event->type()==QEvent::PaletteChange&&m_previewPanel&&m_translationProgress)applyLookupStyle();
}
void LyricsLookup::setPreviewLyrics(const QByteArray &data){
    m_preview->clear();m_translationPreview->clear();
    m_preview->setVisible(!data.isEmpty());m_translationPreview->setVisible(!data.isEmpty());
    if(data.isEmpty())return;
    const auto lyrics=Lyrics::parse(QString::fromUtf8(data).left(6000).toUtf8());
    QStringList original,translation;bool hasTranslation=false;
    for(int i=0;i<lyrics.lines.size();++i){
        const auto &line=lyrics.lines[i];QString translated;
        QString text=line.text;
        if(lyrics.timed){
            const auto time=line.time;
            text=QString("[%1:%2.%3] ").arg(time/60000,2,10,QChar('0')).arg(time/1000%60,2,10,QChar('0')).arg(time%1000/10,2,10,QChar('0'))+text;
            QStringList parts;while(i+1<lyrics.lines.size()&&lyrics.lines[i+1].time==time)parts<<lyrics.lines[++i].text;
            translated=parts.join(" / ");
        }else if(lyrics.bilingual&&i+1<lyrics.lines.size())translated=lyrics.lines[++i].text;
        else if(lyrics.instrumental)translated="无需翻译";
        original<<text;translation<<translated;hasTranslation|=!translated.isEmpty();
    }
    m_preview->setPlainText(original.join('\n'));
    m_translationPreview->setPlainText(hasTranslation?translation.join('\n'):QString());
}
void LyricsLookup::setAiTranslationPreview(){
    QStringList original,translated;
    for(int i=0;i<m_aiPrepared.lines.size();++i){
        const auto &line=m_aiPrepared.lines.at(i);
        const auto prefix=line.timestampPrefixes.isEmpty()?QString():line.timestampPrefixes.first()+QStringLiteral(" ");
        original<<prefix+line.text;
        translated<<prefix+m_aiTranslations.value(i);
    }
    m_preview->setPlainText(original.join('\n'));
    m_translationPreview->setPlainText(translated.join('\n'));
    m_preview->setVisible(!original.isEmpty());
    m_translationPreview->setVisible(!translated.isEmpty());
}
namespace {
QByteArray comparableSource(const QByteArray &bytes) {
    QString text=QString::fromUtf8(bytes);text.replace("\r\n","\n");text.replace('\r','\n');
    if(text.startsWith(QChar(0xfeff)))text.remove(0,1);
    if(!text.contains("relaxer-bilingual"))return text.trimmed().toUtf8();
    // Metadata and empty rows have no translated partner in the AI merge format.
    static const QRegularExpression metadata(QStringLiteral("^\\s*\\[(?:ar|al|ti|by|re|ve|length|offset):"),QRegularExpression::CaseInsensitiveOption);
    QStringList rows;bool original=true;
    for(const auto &row:text.split('\n')){
        if(row.contains("relaxer-bilingual"))continue;
        if(row.trimmed().isEmpty()||metadata.match(row).hasMatch()){rows<<row;continue;}
        if(original)rows<<row;
        original=!original;
    }
    return rows.join('\n').trimmed().toUtf8();
}
}
LyricsLookup::ReplacementContent LyricsLookup::replacementContent(const Track &track,const QJsonObject &record) const {
    ReplacementContent result;
    result.bytes=simplifyTraditionalLyrics(withOffset((record["instrumental"].toBool()?QStringLiteral("纯音乐，请欣赏"):recordLyrics(record)).toUtf8(),m_offset->value()));
    if(track.lyrics.isEmpty())return result;
    const auto saved=LyricsDocumentReader::readFile(m_library->absolute(track.lyrics));
    if(saved.opened&&saved.complete&&!saved.tooLarge&&lyricsIncludeTranslation(saved.bytes)){
        const auto original=QString::fromUtf8(saved.bytes).contains("relaxer-bilingual")?comparableSource(saved.bytes):comparableSource(sourceLyricsForTranslation(saved.bytes));
        if(original==comparableSource(result.bytes)){result.bytes=saved.bytes;result.preservesTranslation=true;}
    }
    return result;
}
void LyricsLookup::ensureLyricsSnapshot(qint64 id){
    if(id>0&&!m_lyricsSnapshots.contains(id))m_lyricsSnapshots.insert(id,m_library->lyricsSaveSnapshot(id));
}
bool LyricsLookup::checkLyricsSnapshot(qint64 id){
    ensureLyricsSnapshot(id);
    if(m_library->lyricsSnapshotMatches(m_lyricsSnapshots.value(id),&m_storeError))return true;
    m_snapshotConflict=true;if(m_refreshTranslationFilter)m_refreshTranslationFilter->setEnabled(true);
    m_translationAssessments.remove(id);return false;
}
void LyricsLookup::updatePreview(){
    const int row=m_tracks->currentRow();
    if(m_hasAiPreview&&!m_multi&&row>=0&&m_ids.value(row)==m_aiTrackId&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected()&&!m_tracks->currentItem()->isHidden()){
        setAiTranslationPreview();return;
    }
    if(m_multi||row<0||!m_tracks->currentItem()||!m_tracks->currentItem()->isSelected()||m_tracks->currentItem()->isHidden()){setPreviewLyrics({});return;}
    const auto track=m_library->track(m_ids.value(row));ensureLyricsSnapshot(track.id);const auto records=m_candidates.value(track.id);
    const int index=m_visibleRecords.value(m_versions->currentRow(),-1);QByteArray data;
    if(index>=0&&index<records.size())data=replacementContent(track,records[index].toObject()).bytes;
    else if(!track.lyrics.isEmpty()){
        const auto saved=LyricsDocumentReader::readFile(m_library->absolute(track.lyrics));
        if(saved.opened&&saved.complete&&!saved.tooLarge)data=saved.bytes;
    }
    setPreviewLyrics(data);
}
QByteArray LyricsLookup::currentAiConfigurationDigest() const{
    if(!m_aiRuntime)return {};
    const auto configuration=m_aiRuntime->configuration();
    const auto task=m_aiRuntime->translationConfiguration();
    const QJsonObject snapshot{{"providerId",task.providerId},{"modelId",task.modelId},
        {"thinkingEnabled",task.thinkingEnabled},{"reasoningEffort",task.reasoningEffort},
        {"targetLanguage",configuration.targetLanguage},{"translationStylePrompt",configuration.translationStylePrompt},
        {"systemPrompt",ai::lyrics::systemPrompt()}};
    return QCryptographicHash::hash(QJsonDocument(snapshot).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);
}
bool LyricsLookup::currentAiTranslationSnapshot(QString *error) const{
    auto fail=[error](const QString &message){if(error)*error=message;return false;};
    if(!m_library||m_aiTrackId<=0)return fail(QStringLiteral("歌曲身份已失效，未保存译文"));
    const int row=m_tracks?m_tracks->currentRow():-1;
    if(row<0||m_ids.value(row)!=m_aiTrackId||m_multi)return fail(QStringLiteral("当前选择的歌曲已变化，旧译文已丢弃"));
    const auto current=m_library->track(m_aiTrackId);
    if(!current.id||current.id!=m_aiTrackSnapshot.id||current.hash!=m_aiTrackSnapshot.hash||
       current.title!=m_aiTrackSnapshot.title||current.artist!=m_aiTrackSnapshot.artist||current.lyrics!=m_aiLyricsPath)
        return fail(QStringLiteral("歌曲身份或歌词文件已变化，旧译文已丢弃"));
    const auto document=LyricsDocumentReader::readFile(m_library->absolute(current.lyrics));
    if(!document.opened||document.tooLarge||!document.complete)
        return fail(QStringLiteral("当前歌词无法完整读取，旧译文已丢弃"));
    if(QCryptographicHash::hash(document.bytes,QCryptographicHash::Sha256)!=m_aiSourceDigest)
        return fail(QStringLiteral("歌词内容已变化，旧译文已丢弃"));
    if(!m_aiRuntime||currentAiConfigurationDigest()!=m_aiConfigurationDigest)
        return fail(QStringLiteral("AI 翻译配置已变化，请重新翻译"));
    if(error)error->clear();
    return true;
}
void LyricsLookup::clearAiTranslationPreview(){
    const bool shown=m_hasAiPreview;
    m_hasAiPreview=false;m_aiMergedBytes.clear();m_aiTranslations.clear();m_aiPrepared={};
    if(m_aiSaveTranslation)m_aiSaveTranslation->setEnabled(false);
    m_aiTrackId=0;m_aiLyricsPath.clear();m_aiSourceBytes.clear();m_aiSourceDigest.clear();m_aiConfigurationDigest.clear();
    if(shown)updatePreview();
}
void LyricsLookup::resetTranslationProgress(){
    setTranslationProgress(QStringLiteral("翻译尚未开始"),{});
}
void LyricsLookup::setTranslationProgress(const QString &status,const QString &details,int value){
    if(m_translationProgressLabel){m_translationProgressLabel->setText(status);m_translationProgressLabel->setToolTip(details);}
    if(m_translationProgress){
        m_translationProgress->setRange(0,value<0?0:1);m_translationProgress->setValue(qMax(0,value));
        m_translationProgress->setAccessibleName(QStringLiteral("整首歌词翻译状态"));
        m_translationProgress->setAccessibleDescription(details);m_translationProgress->setToolTip(details);
    }
    if(!details.isEmpty()&&m_status)m_status->setText(details);
}
void LyricsLookup::cancelAiTranslation(const QString &reason){
    const auto requestId=m_aiTaskId;
    const bool hadTranslation=requestId||m_hasAiPreview;
    if(requestId){
        ++m_aiGeneration;m_aiTaskId=0;
        if(m_aiRuntime&&m_aiRuntime->requests()&&m_aiRuntime->requests()->isPending(requestId))m_aiRuntime->requests()->cancel(requestId);
        if(m_translate)m_translate->setEnabled(true);
        if(m_aiCancelTranslation)m_aiCancelTranslation->setEnabled(false);
        resetTranslationProgress();
        const int row=rowForTrack(m_aiTrackId);
        if(row>=0)setRowStatus(row,QStringLiteral("已取消"));
    }
    if(m_hasAiPreview)clearAiTranslationPreview();
    if(hadTranslation)setTranslationProgress(QStringLiteral("已取消"),reason.isEmpty()?QStringLiteral("本地翻译任务已取消；远端可能继续处理或计费。"):reason);
    if(!reason.isEmpty()&&m_status)m_status->setText(reason);
    if(hadTranslation&&m_missingTranslationOnly&&m_missingTranslationOnly->isChecked())updateFilter();
}
void LyricsLookup::cancelAiTranslationForTrack(qint64 id){
    if(id>0&&(m_aiTrackId==id)&&(m_aiTaskId||m_hasAiPreview))
        cancelAiTranslation(QStringLiteral("歌词已变更，先前的 AI 翻译结果已丢弃。"));
}
bool LyricsLookup::translationPending() const{return m_aiTaskId!=0;}
int LyricsLookup::rowForTrack(qint64 id) const{for(int row=0;row<m_ids.size();++row)if(m_ids.at(row)==id)return row;return -1;}
bool LyricsLookup::confirmStopTranslations(){
    if(!translationPending())return true;
    QMessageBox box(this);box.setObjectName("lyricsTranslateCloseConfirm");box.setIcon(QMessageBox::Warning);box.setWindowTitle("翻译任务尚未完成");
    box.setText("关闭将停止本地翻译任务。DeepSeek 可能已经收到整首歌词，并可能继续处理或计费；本地取消不能撤销供应商已处理的请求。尚未确认保存的译文不会写入歌词文件。是否关闭？");
    auto *keep=box.addButton("继续等待",QMessageBox::RejectRole);keep->setObjectName("lyricsContinueTranslate");
    auto *quit=box.addButton("停止本地任务并关闭",QMessageBox::AcceptRole);quit->setObjectName("lyricsStopTranslateQuit");
    if(auto *button=qobject_cast<QPushButton *>(keep))box.setDefaultButton(button);
    box.exec();if(box.clickedButton()!=quit)return false;
    cancelAiTranslation(QStringLiteral("本地翻译任务已取消；DeepSeek 可能已收到请求并继续处理或计费。"));return true;
}
void LyricsLookup::completeAiTranslation(quint64 taskId,const ai::ChatCompletionResult &result){
    if(!taskId||taskId!=m_aiTaskId||m_aiGeneration!=m_aiActiveGeneration)return;
    m_aiTaskId=0;if(m_translate)m_translate->setEnabled(true);if(m_aiCancelTranslation)m_aiCancelTranslation->setEnabled(false);
    const auto usage=result.usage.available?
        QStringLiteral("费用按 DeepSeek 实际计费；本次 token：输入 %1，输出 %2，总计 %3。")
            .arg(result.usage.inputTokens).arg(result.usage.outputTokens).arg(result.usage.totalTokens):
        QStringLiteral("本次 token 用量未知；费用以 DeepSeek 实际账单为准。");
    auto fail=[this,&usage](const QString &message){
        const auto detail=QStringLiteral("翻译失败：")+message+QStringLiteral("。 ")+usage;
        setRowStatus(rowForTrack(m_aiTrackId),QStringLiteral("翻译失败：")+message);
        setTranslationProgress(QStringLiteral("翻译失败"),detail);
    };
    QString snapshotError;
    if(!currentAiTranslationSnapshot(&snapshotError)){
        fail(snapshotError);clearAiTranslationPreview();return;
    }
    if(!result.ok()){
        const auto message=result.error.message.isEmpty()?QStringLiteral("AI 翻译请求失败"):result.error.message;
        fail(message);return;
    }
    const auto validation=ai::lyrics::validateResponse(result.content,m_aiPrepared);
    if(!validation.ok()){
        fail(validation.error);return;
    }
    QString mergeError;
    const auto merged=ai::lyrics::merge(m_aiSourceBytes,m_aiPrepared,validation.translations,&mergeError);
    if(merged.isEmpty()){
        fail(mergeError);return;
    }
    m_aiTranslations=validation.translations;m_aiMergedBytes=merged;m_hasAiPreview=true;
    m_aiSaveTranslation->setEnabled(true);setAiTranslationPreview();
    QString details=QStringLiteral("翻译完成，请核对双栏预览后再保存。 ");
    const auto masked=validation.translations.count(ai::lyrics::blockedLinePlaceholder());
    if(masked)details+=QStringLiteral("译文含 %1 行屏蔽占位，请核对；原文保留。 ").arg(masked);
    details+=usage;
    setRowStatus(rowForTrack(m_aiTrackId),QStringLiteral("翻译完成，等待你预览并保存"));
    setTranslationProgress(QStringLiteral("翻译完成"),details,1);
}
void LyricsLookup::saveAiTranslation(){
    if(!m_hasAiPreview)return;
    QString snapshotError;
    if(!currentAiTranslationSnapshot(&snapshotError)){
        setRowStatus(rowForTrack(m_aiTrackId),snapshotError);setTranslationProgress(QStringLiteral("未保存"),snapshotError);clearAiTranslationPreview();return;
    }
    if(!m_library->saveLyricsPreservingText(m_aiTrackId,m_aiMergedBytes)){
        const auto error=m_library->lastError().isEmpty()?QStringLiteral("歌词保存失败，原歌词仍保留"):m_library->lastError();
        setRowStatus(rowForTrack(m_aiTrackId),QStringLiteral("未保存：")+error);setTranslationProgress(QStringLiteral("未保存"),QStringLiteral("未保存：")+error);return;
    }
    const auto savedTrackId=m_aiTrackId;
    const auto savedLanguage=m_aiTargetLanguage;
    m_library->clearLookupCache(savedTrackId);remember(savedTrackId);m_translationAssessments.remove(savedTrackId);m_lyricsSnapshots.insert(savedTrackId,m_library->lyricsSaveSnapshot(savedTrackId));
    const auto message=QStringLiteral("双语歌词已保存，原文与时间戳保持不变。")+
        (savedLanguage==QStringLiteral("zh-CN")?QString():QStringLiteral(" 目标语言：")+savedLanguage);
    // An AI preview means an unsaved result. After saving, render the persisted
    // document through the normal reader rather than keeping stale preview state.
    clearAiTranslationPreview();
    setRowStatus(rowForTrack(savedTrackId),QStringLiteral("已保存 AI 双语歌词"));
    setTranslationProgress(QStringLiteral("已保存"),message,1);
    updatePreview();updateFilter();
}
void LyricsLookup::closeEvent(QCloseEvent *event){if(!confirmStopTranslations()){event->ignore();return;}QDialog::closeEvent(event);}
void LyricsLookup::reject(){if(!confirmStopTranslations())return;stop();QDialog::reject();}
bool LyricsLookup::live(const JobPtr &j) const {return m_running&&j->generation==m_generation&&m_jobs.value(j->original.id)==j;}
void LyricsLookup::cancelJob(qint64 id){
    auto j=m_jobs.take(id);if(!j)return;
    for(auto *reply:j->replies){releaseRequestSlot(reply);reply->disconnect(this);reply->abort();reply->deleteLater();}j->replies.clear();
    if(j->probe){j->probe->disconnect(this);j->probe->kill();j->probe->deleteLater();j->probe=nullptr;}
}
void LyricsLookup::releaseRequestSlot(QNetworkReply *reply){
    if(reply->property("lookupSlotReleased").toBool())return;
    reply->setProperty("lookupSlotReleased",true);
    const auto host=reply->property("lookupHost").toString();
    m_hostRequests[host]=qMax(0,m_hostRequests.value(host)-1);
    if(!m_running)return;
    const int generation=m_generation;
    QTimer::singleShot(0,this,[this,generation]{if(generation!=m_generation||!m_running)return;for(const auto &job:m_jobs.values())if(live(job))pumpQueries(job);});
}
void LyricsLookup::stop(){
    ++m_generation;m_running=false;for(const auto &j:m_recoveryQueue)setRowStatus(j->row,"已停止，等待继续");m_recoveryQueue.clear();const auto jobs=m_jobs.values();for(const auto &j:jobs){setRowStatus(j->row,"已停止，等待继续");cancelJob(j->original.id);}controls();
}
QVector<int> LyricsLookup::batchRows() const {QVector<int> rows;for(int r=0;r<m_tracks->count();++r)if(m_tracks->item(r)->isSelected()&&!m_tracks->item(r)->isHidden())rows<<r;return rows;}
void LyricsLookup::setMultiSelect(bool on){
    if(on==m_multi)return;m_multi=on;m_tracks->setProperty("lookupMultiSelect",on);m_multiButton->setText(on?"✓ 多选":"多选");m_allButton->setVisible(on);
    if(on){m_moreWasOpen=m_moreButton->isChecked();m_tracks->setSelectionMode(QAbstractItemView::MultiSelection);m_moreButton->setChecked(true);}else{const auto rows=batchRows();const int current=rows.isEmpty()?m_tracks->currentRow():rows.first();QSignalBlocker block(m_tracks);m_tracks->setSelectionMode(QAbstractItemView::SingleSelection);m_tracks->clearSelection();if(current>=0&&!m_tracks->item(current)->isHidden())m_tracks->setCurrentRow(current);m_moreButton->setChecked(m_moreWasOpen);}
    m_moreButton->setEnabled(!on);m_morePanel->setVisible(on||m_moreButton->isChecked());m_tracks->viewport()->update();selectTrack();controls();
}
void LyricsLookup::updateApplyButton(){
    const int row=m_tracks->currentRow(),index=m_visibleRecords.value(m_versions->currentRow(),-1);
    const auto id=m_ids.value(row);const auto track=m_library->track(id);const auto records=m_candidates.value(id);
    const bool chosen=!m_multi&&row>=0&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected()&&!m_tracks->currentItem()->isHidden();
    const auto record=index>=0&&index<records.size()?records[index].toObject():QJsonObject();
    m_apply->setEnabled(chosen&&track.id&&m_candidateHashes.value(id)==track.hash&&!record.isEmpty()&&
        (record["instrumental"].toBool()||!recordLyrics(record).trimmed().isEmpty()));
}
void LyricsLookup::controls(){
    const auto rows=batchRows();const bool chosen=!rows.isEmpty();const int row=m_tracks->currentRow();const bool single=!m_multi&&row>=0&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected();
    m_start->setEnabled(!m_running);m_stop->setEnabled(m_running);m_replace->setEnabled(!m_running);
    for(auto *widget:QList<QWidget *>{m_searchTitle,m_searchArtist,m_fragment,m_manualSearch,m_pasteButton,m_importButton,m_sourceFilter,m_syncedOnly})widget->setEnabled(single);
    m_candidatePanel->setEnabled(!m_multi);updateApplyButton();m_continue->setEnabled(single);if(auto *confirm=findChild<QPushButton *>("lyricsConfirmInstrumental"))confirm->setEnabled(single);
    m_mark->setEnabled(chosen);m_offset->setEnabled(chosen);m_saveOffset->setEnabled(chosen);m_allButton->setEnabled(m_multi);
    setProperty("parallelTracks",m_trackLimit);setProperty("parallelQueries",m_requestLimit);setProperty("selectedLyricsTracks",rows.size());
}
QString LyricsLookup::reuseKey(const Track &t) const {return normalized(t.title)+"|"+normalized(t.artist);}
void LyricsLookup::remember(qint64 id){const auto t=m_library->track(id);if(!t.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(t.lyrics))){auto &ids=m_reuse[reuseKey(t)];if(!ids.contains(id))ids<<id;}}
void LyricsLookup::start(){
    if(m_aiTaskId||m_hasAiPreview)cancelAiTranslation(QStringLiteral("歌词查找开始，未保存的 AI 翻译已取消。"));
    resetTranslationProgress();
    QVector<int> visible;for(int r=0;r<m_tracks->count();++r)if(!m_tracks->item(r)->isHidden())visible<<r;
    if(visible.isEmpty()){m_status->setText("当前没有缺失歌词的歌曲，可取消勾选以查看全部歌曲。");return;}
    QVector<int> selected;for(int r:visible)if(m_tracks->item(r)->isSelected())selected<<r;
    QVector<int> targets=selected;
    if(selected.isEmpty()){
        QMessageBox box(this);box.setObjectName("lyricsSearchAllConfirm");box.setIcon(QMessageBox::Question);box.setWindowTitle("确认搜索全部歌曲？");
        box.setText(QString("当前未选中任何歌曲，即将搜索当前列表中的全部 %1 首歌曲。此操作可能需要较长时间，并产生大量网络请求。是否继续？").arg(visible.size()));
        auto *cancel=box.addButton("取消",QMessageBox::RejectRole);cancel->setObjectName("lyricsSearchAllCancel");
        auto *all=box.addButton("全部搜索",QMessageBox::AcceptRole);all->setObjectName("lyricsSearchAll");
        if(auto *button=qobject_cast<QPushButton *>(cancel))box.setDefaultButton(button);
        box.exec();if(box.clickedButton()!=all)return;targets=visible;
    }
    stop();m_batchJobScope=true;m_requestedRows.clear();for(int row:targets)m_requestedRows.insert(row);m_nextRow=0;m_saved=0;m_reuse.clear();if(!m_replace->isChecked()){for(const auto &t:m_library->tracks()){if(t.lyrics.isEmpty())continue;auto &ids=m_reuse[reuseKey(t)];if(!ids.contains(t.id))ids<<t.id;}}
    clearTransientCooldowns();
    for(int row:targets)setRowStatus(row,"等待查找");
    selectTrack();m_running=true;controls();pumpTracks();
}
void LyricsLookup::pumpTracks(){
    if(!m_running)return;
    while(m_jobs.size()<m_trackLimit&&m_nextRow<m_ids.size()){
        const int row=m_nextRow++;if(m_batchJobScope&&!m_requestedRows.contains(row))continue;const int state=m_tracks->item(row)->data(StateRole).toInt();if(state==Complete||state==NeedsChoice)continue;
        const auto t=m_library->track(m_ids[row]);if(!t.id){setRowStatus(row,"歌曲已移除");continue;}
        auto j=std::make_shared<Job>();j->row=row;j->original=t;j->match=searchTrack(t);j->generation=m_generation;m_jobs[t.id]=j;lookup(j);
    }
    if(m_jobs.isEmpty()&&m_nextRow>=m_ids.size()&&!m_recoveryQueue.isEmpty()){
        auto j=m_recoveryQueue.takeFirst();j->generation=m_generation;m_jobs[j->original.id]=j;
        setRowStatus(j->row,"正在重试 · 单曲依次复查");controls();
        const int delay=property("lyricsRetryDelayMs").isValid()?qMax(0,property("lyricsRetryDelayMs").toInt()):500;
        QTimer::singleShot(delay,this,[this,j]{if(!live(j))return;if(m_library->track(j->original.id).hash!=j->original.hash){finishJob(j,"歌曲文件已变化，请重新查找");return;}clearTransientCooldowns();beginOnline(j);});
        return;
    }
    if(m_jobs.isEmpty()&&m_nextRow>=m_ids.size()){m_running=false;controls();m_status->setText(QString("查找完成，已保存 %1 首。需要选择的歌曲请点击左侧查看。").arg(m_saved));}
    else m_status->setText(QString("正在查找 · 同时 %1 首 · 每首最多 %2 个查询 · 已保存 %3 首").arg(m_jobs.size()).arg(m_requestLimit).arg(m_saved));
}
void LyricsLookup::setRowStatus(int row,const QString &text){
    if(row<0||row>=m_tracks->count())return;int state=Failed;
    if(text.startsWith("需选择"))state=NeedsChoice;
    else if(text.startsWith("正在"))state=Searching;
    else if(text.startsWith("已保存")||text.startsWith("已有")||text.startsWith("纯音乐")||text.startsWith("已是中文"))state=Complete;
    else if(text.startsWith("等待")||text.startsWith("已停止"))state=Waiting;
    // Translation is a separate task; its failure/cancellation cannot undo saved source lyrics.
    const bool translation=text.startsWith("等待翻译")||text.startsWith("正在翻译")||text=="正在保存"||text.startsWith("翻译失败")||text=="已取消";
    if(translation||state==Failed){
        const auto track=m_library->track(m_ids.value(row));
        if(!track.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(track.lyrics)))state=Complete;
        else if(m_candidateHashes.value(track.id)==track.hash&&!m_candidates.value(track.id).isEmpty())state=NeedsChoice;
    }
    if(state==Waiting&&!m_candidates.value(m_ids.value(row)).isEmpty()&&text.startsWith("已停止"))state=NeedsChoice;
    auto *item=m_tracks->item(row);item->setData(StateRole,state);item->setData(StatusRole,text);item->setToolTip(item->text()+"\n"+text);item->setData(Qt::AccessibleTextRole,item->text()+" · "+text);if((m_incomplete&&m_incomplete->isChecked())||(m_missingTranslationOnly&&m_missingTranslationOnly->isChecked()))updateFilterRow(row);
}
ai::lyrics::TranslationAssessment LyricsLookup::translationAssessment(const Track &track){
    ++m_filterMetrics.assessments;
    using Need=ai::lyrics::TranslationNeed;
    const bool instrumental=m_library->isInstrumental(track.id);
    const auto path=track.lyrics.isEmpty()?QString():m_library->absolute(track.lyrics);const QFileInfo info(path);
    const bool fileExists=!path.isEmpty()&&info.isFile();
    const auto size=fileExists?info.size():qint64(0);
    const auto modified=fileExists?info.lastModified().toMSecsSinceEpoch():qint64(0);
    const auto cached=m_translationAssessments.constFind(track.id);
    if(cached!=m_translationAssessments.cend()&&cached->path==path&&cached->size==size&&cached->modified==modified&&
       cached->hash==track.hash&&cached->title==track.title&&cached->artist==track.artist&&
       cached->instrumental==instrumental&&cached->fileExists==fileExists)return cached->value;

    AssessmentCache result;result.path=path;result.hash=track.hash;result.title=track.title;result.artist=track.artist;
    result.size=size;result.modified=modified;result.instrumental=instrumental;result.fileExists=fileExists;
    if(track.lyrics.isEmpty()||instrumental){
        result.value={Need::NotApplicable,QStringLiteral("没有歌词或已标记纯音乐")};
    }else if(!fileExists){
        result.value={Need::NotApplicable,QStringLiteral("本地歌词文件不存在")};
    }else{
        const auto file=LyricsDocumentReader::readFile(path);++m_filterMetrics.fileReads;m_filterMetrics.bytesRead+=file.bytes.size();
        result.tooLarge=file.tooLarge;result.readable=file.opened&&file.complete&&!file.tooLarge;
        if(result.readable){
            result.contentDigest=QCryptographicHash::hash(file.bytes,QCryptographicHash::Sha256);
            ++m_filterMetrics.digestComputations;m_filterMetrics.digestBytes+=file.bytes.size();
            result.value=ai::lyrics::assessTranslation(file.bytes,track.title,track.artist);
        }else result.value={Need::NotApplicable,QStringLiteral("歌词无法完整读取")};
    }
    result.revision=++m_translationAssessmentRevision;
    const auto assessment=result.value;m_translationAssessments.insert(track.id,std::move(result));return assessment;
}
void LyricsLookup::requestTranslationCacheAudit(){
    if(!m_missingTranslationOnly||!m_missingTranslationOnly->isChecked()||m_translationAssessments.isEmpty()||!m_translationAuditDebounce)return;
    m_translationAuditDebounce->start();
}
void LyricsLookup::startTranslationCacheAudit(){
    if(!m_missingTranslationOnly||!m_missingTranslationOnly->isChecked()||m_translationAssessments.isEmpty())return;
    if(m_translationAuditInFlight){m_translationAuditPending=true;return;}
    QVector<TranslationAuditSnapshot> snapshots;snapshots.reserve(m_translationAssessments.size());
    for(int row=0;row<m_ids.size();++row){
        const auto id=m_ids[row];
        const auto cached=m_translationAssessments.constFind(id);if(cached==m_translationAssessments.cend())continue;
        TranslationAuditSnapshot snapshot;snapshot.id=id;snapshot.row=row;snapshot.track=m_library->track(id);
        snapshot.path=snapshot.track.lyrics.isEmpty()?QString():m_library->absolute(snapshot.track.lyrics);
        snapshot.instrumental=m_library->isInstrumental(id);snapshot.previous=*cached;
        snapshots.push_back(std::move(snapshot));
    }
    if(snapshots.isEmpty())return;
    m_translationAuditInFlight=true;++m_filterMetrics.auditPasses;
    const auto cancel=m_translationAuditCancel;QPointer<LyricsLookup> self(this);auto *app=QCoreApplication::instance();
    lyricPool()->start(new LyricRunnable([snapshots=std::move(snapshots),cancel,self,app]() mutable {
        QVector<TranslationAuditResult> results;results.reserve(snapshots.size());FilterMetrics work;work.auditFiles=snapshots.size();
        constexpr qint64 maximumBytes=LyricsDocumentReader::MaximumBytes;
        for(const auto &snapshot:snapshots){
            if(!cancel||cancel->load())return;
            TranslationAuditResult result;result.snapshot=snapshot;auto &cache=result.current;const auto &track=snapshot.track;
            cache.path=snapshot.path;
            cache.hash=track.hash;cache.title=track.title;cache.artist=track.artist;cache.instrumental=snapshot.instrumental;
            const QFileInfo initialInfo(snapshot.path);cache.fileExists=!snapshot.path.isEmpty()&&initialInfo.isFile();
            if(cache.fileExists){cache.size=initialInfo.size();cache.modified=initialInfo.lastModified().toMSecsSinceEpoch();}
            if(track.lyrics.isEmpty()||snapshot.instrumental){
                cache.value={ai::lyrics::TranslationNeed::NotApplicable,QStringLiteral("没有歌词或已标记纯音乐")};
            }else{
                QFileInfo before(snapshot.path);cache.fileExists=before.isFile();
                if(!cache.fileExists){
                    cache.value={ai::lyrics::TranslationNeed::NotApplicable,QStringLiteral("本地歌词文件不存在")};
                }else if(before.size()>maximumBytes){
                    cache.tooLarge=true;cache.value={ai::lyrics::TranslationNeed::NotApplicable,QStringLiteral("歌词无法完整读取")};
                }else{
                    cache.size=before.size();cache.modified=before.lastModified().toMSecsSinceEpoch();result.readAttempted=true;++work.fileReads;
                    QFile file(snapshot.path);
                    if(file.open(QIODevice::ReadOnly)){
                        const auto bytes=file.read(maximumBytes+1);result.bytesRead=bytes.size();work.bytesRead+=bytes.size();
                        QFileInfo after(snapshot.path);cache.fileExists=after.isFile();cache.size=cache.fileExists?after.size():0;cache.modified=cache.fileExists?after.lastModified().toMSecsSinceEpoch():0;
                        cache.tooLarge=before.size()>maximumBytes||cache.size>maximumBytes||bytes.size()>maximumBytes;
                        cache.readable=cache.fileExists&&!cache.tooLarge&&file.error()==QFileDevice::NoError&&file.atEnd();
                        if(cache.readable){
                            cache.contentDigest=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256);result.digestComputed=true;result.digestBytes=bytes.size();
                            work.digestComputations++;work.digestBytes+=bytes.size();cache.value=ai::lyrics::assessTranslation(bytes,track.title,track.artist);
                        }else cache.value={ai::lyrics::TranslationNeed::NotApplicable,QStringLiteral("歌词无法完整读取")};
                    }else{
                        QFileInfo after(snapshot.path);cache.fileExists=after.isFile();cache.size=cache.fileExists?after.size():0;cache.modified=cache.fileExists?after.lastModified().toMSecsSinceEpoch():0;
                        cache.value=cache.fileExists?ai::lyrics::TranslationAssessment{ai::lyrics::TranslationNeed::NotApplicable,QStringLiteral("歌词无法完整读取")}:ai::lyrics::TranslationAssessment{ai::lyrics::TranslationNeed::NotApplicable,QStringLiteral("本地歌词文件不存在")};
                    }
                }
            }
            results.push_back(std::move(result));
        }
        if(!app||!cancel||cancel->load())return;
        QMetaObject::invokeMethod(app,[self,results=std::move(results),work]() mutable {if(self)self->applyTranslationCacheAudit(results,work);},Qt::QueuedConnection);
    }));
}
void LyricsLookup::applyTranslationCacheAudit(const QVector<TranslationAuditResult> &results,const FilterMetrics &work){
    m_translationAuditInFlight=false;m_filterMetrics.fileReads+=work.fileReads;m_filterMetrics.bytesRead+=work.bytesRead;
    m_filterMetrics.digestComputations+=work.digestComputations;m_filterMetrics.digestBytes+=work.digestBytes;m_filterMetrics.auditFiles+=work.auditFiles;
    for(const auto &result:results){
        const auto id=result.snapshot.id;const int row=result.snapshot.row;
        if(row<0||row>=m_ids.size()||m_ids.value(row)!=id)continue;
        const auto currentTrack=m_library->track(id);const bool currentInstrumental=m_library->isInstrumental(id);
        if(!currentTrack.id||currentTrack.hash!=result.snapshot.track.hash||currentTrack.title!=result.snapshot.track.title||
           currentTrack.artist!=result.snapshot.track.artist||currentTrack.lyrics!=result.snapshot.track.lyrics||currentInstrumental!=result.snapshot.instrumental){
            updateFilterRow(row);continue;
        }
        auto current=m_translationAssessments.find(id);
        if(current==m_translationAssessments.end()||current->revision!=result.snapshot.previous.revision)continue;
        const auto &before=result.snapshot.previous;auto refreshed=result.current;refreshed.revision=++m_translationAssessmentRevision;
        const bool changed=before.path!=refreshed.path||before.hash!=refreshed.hash||before.title!=refreshed.title||before.artist!=refreshed.artist||
            before.instrumental!=refreshed.instrumental||before.fileExists!=refreshed.fileExists||before.readable!=refreshed.readable||
            before.tooLarge!=refreshed.tooLarge||before.contentDigest!=refreshed.contentDigest||before.value.need!=refreshed.value.need||before.value.reason!=refreshed.value.reason;
        current.value()=std::move(refreshed);
        if(changed)updateFilterRow(row);
    }
    const bool rerun=m_translationAuditPending;m_translationAuditPending=false;
    if(rerun)requestTranslationCacheAudit();
}
bool LyricsLookup::filterRow(int row){
    ++m_filterMetrics.rowVisits;auto *item=m_tracks->item(row);const auto id=m_ids.value(row);
    const bool translationOnly=m_missingTranslationOnly&&m_missingTranslationOnly->isChecked();
    const bool hideMissing=m_missingOnly&&m_missingOnly->isChecked()&&!m_missingAtOpen.contains(id)&&!m_pinnedIds.contains(id);
    const bool hideDone=m_incomplete&&m_incomplete->isChecked()&&rowComplete(item);
    const auto assessment=translationOnly?translationAssessment(m_library->track(id)):ai::lyrics::TranslationAssessment{};
    const bool hideTranslation=translationOnly&&!assessment.missing();
    const auto badge=translationOnly&&assessment.missing()?(assessment.need==ai::lyrics::TranslationNeed::Review?QStringLiteral("待确认"):QStringLiteral("待翻译")):QString();
    item->setData(TranslationFilterRole,badge);
    const auto details=item->data(StatusRole).toString()+(translationOnly?QStringLiteral("\n")+(badge.isEmpty()?assessment.reason:badge+QStringLiteral("：")+assessment.reason):QString());
    item->setToolTip(item->text()+QStringLiteral("\n")+details);item->setData(Qt::AccessibleTextRole,item->text()+QStringLiteral(" · ")+details);
    const bool ownsTranslation=id==m_aiTrackId&&(m_aiTaskId||m_hasAiPreview);
    item->setHidden(!ownsTranslation&&(hideMissing||hideDone||hideTranslation));
    if(item->isHidden()&&item->isSelected())item->setSelected(false);
    return !item->isHidden();
}
void LyricsLookup::finishFilterUpdate(){
    const bool translationOnly=m_missingTranslationOnly&&m_missingTranslationOnly->isChecked();
    if(m_refreshTranslationFilter)m_refreshTranslationFilter->setEnabled(translationOnly||m_snapshotConflict);
    if(m_emptyHint){m_emptyHint->setText(translationOnly?QStringLiteral("当前没有检测到缺失翻译的歌曲。判断仅基于本地歌词；可点击刷新判断或取消筛选。"):(m_missingOnly&&m_missingOnly->isChecked()?QStringLiteral("当前没有缺失歌词的歌曲，可取消勾选以查看全部歌曲。"):QStringLiteral("当前筛选没有匹配歌曲，可取消筛选查看全部歌曲。")));m_emptyHint->setVisible(!m_filterVisibleCount&&m_tracks->count()>0);}
    if(m_tracks->currentItem()&&m_tracks->currentItem()->isHidden()){m_tracks->clearSelection();m_tracks->setCurrentRow(-1);selectTrack();}
}
void LyricsLookup::updateFilterRow(int row){
    if(!m_filterReady||m_filterUpdating||row<0||row>=m_tracks->count())return;
    QScopedValueRollback<bool> updating(m_filterUpdating,true);
    const bool wasVisible=!m_tracks->item(row)->isHidden();const bool visible=filterRow(row);
    m_filterVisibleCount+=int(visible)-int(wasVisible);finishFilterUpdate();
}
void LyricsLookup::updateFilter(){
    if(m_filterUpdating)return;
    QScopedValueRollback<bool> updating(m_filterUpdating,true);
    ++m_filterMetrics.fullPasses;m_filterVisibleCount=0;
    for(int row=0;row<m_tracks->count();++row)m_filterVisibleCount+=filterRow(row)?1:0;
    m_filterReady=true;finishFilterUpdate();
}
void LyricsLookup::finishJob(const JobPtr &j,const QString &status){
    if(!live(j))return;const int row=j->row;cancelJob(j->original.id);setRowStatus(row,status);if(m_tracks->currentRow()==row)selectTrack();const int generation=m_generation;QTimer::singleShot(0,this,[this,generation]{if(generation==m_generation)pumpTracks();});
}
bool LyricsLookup::store(const Track &t,const QByteArray &data,bool force,bool preserveText){
    m_storeError.clear();const auto now=m_library->track(t.id);
    if(!now.id||now.hash!=t.hash){m_storeError=QStringLiteral("歌曲身份已变化，请重新搜索歌词。");return false;}
    if(!force&&!now.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(now.lyrics))){m_storeError=QStringLiteral("已有本地歌词，请手动确认替换。");return false;}
    if(!checkLyricsSnapshot(t.id))return false;
    const bool ok=m_library->saveLyricsChecked(t.id,data,m_lyricsSnapshots.value(t.id),preserveText);
    if(ok){remember(t.id);m_translationAssessments.remove(t.id);m_lyricsSnapshots.insert(t.id,m_library->lyricsSaveSnapshot(t.id));}
    else {m_storeError=m_library->lastError();if(m_storeError.contains(QStringLiteral("刷新"))){m_snapshotConflict=true;m_refreshTranslationFilter->setEnabled(true);}}
    return ok;
}
void LyricsLookup::lookup(const JobPtr &j){
    ensureLyricsSnapshot(j->original.id);
    cancelAiTranslationForTrack(j->original.id);
    const auto t=j->original;const bool has=!t.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(t.lyrics));
    if(has){beginOnline(j);return;}
    if(!m_replace->isChecked()){
        const QFileInfo info(m_library->absolute(t.file));
        for(const auto &suffix:{QString(".lrc"),QString(".txt")}){
            const auto localPath=info.dir().filePath(info.completeBaseName()+suffix);
            const auto local=LyricsDocumentReader::readDocument(localPath);
            if(local.file.opened&&!local.file.tooLarge&&!local.lyrics.lines.isEmpty()&&store(t,local.file.bytes)){++m_saved;finishJob(j,"已保存本地歌词");return;}
        }
        if(knownArtist(t.artist))for(auto id:m_reuse.value(reuseKey(t))){
            if(id==t.id)continue;const auto donor=m_library->track(id);
            if(t.duration>0&&donor.duration>0&&qAbs(t.duration-donor.duration)>4000)continue;
            const auto donorDocument=LyricsDocumentReader::readFile(m_library->absolute(donor.lyrics));
            if(!donorDocument.opened||donorDocument.tooLarge)continue;
            if(store(t,donorDocument.bytes)){++m_saved;finishJob(j,"已保存同曲歌词 · 复用《"+donor.title+"》");return;}
        }
        setRowStatus(j->row,"正在检查内嵌歌词");auto *p=new QProcess(this);j->probe=p;
        connect(p,&QProcess::finished,this,[this,j,p](int,QProcess::ExitStatus){if(!live(j))return;const auto doc=QJsonDocument::fromJson(p->readAllStandardOutput()).object();auto tags=doc["format"].toObject()["tags"].toObject();for(const auto &s:doc["streams"].toArray()){const auto more=s.toObject()["tags"].toObject();for(auto i=more.begin();i!=more.end();++i)tags.insert(i.key(),i.value());}QByteArray data;for(auto i=tags.begin();i!=tags.end();++i)if(i.key().toLower().startsWith("lyrics")||i.key().toLower().startsWith("unsyncedlyrics")){data=i.value().toString().toUtf8();if(!data.isEmpty())break;}j->probe=nullptr;p->deleteLater();if(!data.isEmpty()&&store(j->original,data)){++m_saved;finishJob(j,"已保存内嵌歌词");}else beginOnline(j);});
        connect(p,&QProcess::errorOccurred,this,[this,j,p](QProcess::ProcessError e){if(e==QProcess::FailedToStart&&live(j)){j->probe=nullptr;p->deleteLater();beginOnline(j);}});
        p->start(toolPath("ffprobe"),{"-v","error","-show_entries","format_tags:stream_tags","-of","json",info.absoluteFilePath()});QTimer::singleShot(5000,p,[p]{if(p->state()!=QProcess::NotRunning)p->kill();});return;
    }beginOnline(j);
}
void LyricsLookup::beginOnline(const JobPtr &j){
    if(!live(j)||j->onlineStarted)return;j->onlineStarted=true;const auto t=j->original;
    // Every entry point is now an explicit search. Cached candidates stay available for review, but never suppress network requests.
    setRowStatus(j->row,j->searchPass?"正在重试 · 单曲依次复查":"正在排队查找");
    auto add=[this,j](QString title,QString artist,bool exact=false,QString keywords={},QString album={},int source=0,bool cover=false){if(title.isEmpty()&&keywords.isEmpty())return;if(!knownArtist(artist))artist.clear();Attempt a{title,artist,album,keywords,source,exact,!keywords.isEmpty(),cover};for(const auto &old:j->attempts)if(old.title==a.title&&old.artist==a.artist&&old.source==a.source&&old.cover==a.cover&&(m_sources[source].kind>=2||(old.album==a.album&&old.keywords==a.keywords&&old.exact==a.exact)))return;j->attempts<<a;};
    // Both buttons use the same query plan for unchanged input. Start with the
    // normalized search rather than spending a batch slot on a strict album get.
    add(j->match.title,j->match.artist);add(j->match.title,j->match.artist,true);
    const auto alias=QRegularExpression("^([A-Za-z0-9 ._-]+)\\s+[\\p{Han}\\p{Hiragana}\\p{Katakana}]+$").match(j->match.artist);if(alias.hasMatch())add(j->match.title,alias.captured(1));
    const auto lead=j->match.artist.split(QRegularExpression("\\s*(?:/|、|&|\\bfeat\\.?\\b|\\bft\\.?\\b)\\s*",QRegularExpression::CaseInsensitiveOption)).value(0);if(j->match.artist!="AC/DC"&&lead!=j->match.artist)add(j->match.title,lead);add(j->match.title,{});
    const auto base=baseTitle(j->match.title);if(base!=j->match.title){add(base,j->match.artist);add(base,{});}add(base,{},false,base);
    if(!j->match.album.isEmpty()&&j->match.album!="未知专辑")add(j->match.title,{},false,{},j->match.album);
    if(QRegularExpression("[\\p{Han}\\p{Hiragana}\\p{Katakana}]").match(j->match.title).hasMatch()&&QRegularExpression("\\s").match(j->match.title).hasMatch()){auto compact=j->match.title;compact.remove(QRegularExpression("\\s+"));add(compact,{},false,compact);}
    const auto originalMatch=searchTrack(t);
    const bool unchanged=j->match.title==originalMatch.title&&j->match.artist==originalMatch.artist;
    if(unchanged&&(t.title!=j->match.title||t.artist!=j->match.artist))add(t.title,t.artist,true);
    if(j->manual&&!m_fragment->text().trimmed().isEmpty())add(j->match.title,{},false,m_fragment->text().trimmed());
    if(m_sources.size()>1&&knownArtist(j->match.artist))add(j->match.title,j->match.artist,false,{},{},1);
    if(productionHost()){const auto baseAttempts=j->attempts;for(const auto &attempt:baseAttempts)for(int s=0;s<m_sources.size();++s)if(m_sources[s].kind>=2)add(attempt.title,attempt.artist,attempt.exact,attempt.keywords,attempt.album,s,attempt.cover);}
    if(unchanged){auto credit=originalCredit(j->match.title);if(!credit.found)credit=originalCredit(QFileInfo(j->original.file).completeBaseName());if(!credit.found)credit=originalCredit(j->original.title);if(credit.found){add(credit.title,credit.artist,true,{},{},0,true);if(productionHost())for(int s=0;s<m_sources.size();++s)if(m_sources[s].kind>=2)add(credit.title,credit.artist,true,{},{},s,true);}}
    // Try independent sources early, instead of exhausting all LRCLIB variants (and its timeouts) first.
    QVector<QVector<Attempt>> bySource(m_sources.size());for(const auto &attempt:j->attempts)bySource[attempt.source]<<attempt;
    j->attempts.clear();int rounds=0;for(const auto &list:bySource)rounds=qMax(rounds,int(list.size()));
    for(int n=0;n<rounds;++n)for(int source=0;source<bySource.size();++source)if(n<bySource[source].size()&&!m_sources[source].backup)j->attempts<<bySource[source][n];
    for(int source=0;source<bySource.size();++source)if(m_sources[source].backup)j->attempts+=bySource[source];
    pumpQueries(j);
}
void LyricsLookup::pumpQueries(const JobPtr &j){
    // Slot-release notifications can reach jobs still probing embedded lyrics.
    // Their empty attempt list means "not online yet", never "search exhausted".
    if(!live(j)||!j->onlineStarted)return;const int budget=property("lyricsSearchBudgetMs").isValid()?property("lyricsSearchBudgetMs").toInt():30000;
    if(j->clock.isValid()&&j->clock.elapsed()>=budget){j->completeResponses=false;if(j->active==0)finishOnline(j);return;}
    if(!j->manual&&!j->collected.isEmpty()&&!rankingCurrent(j))scheduleRanking(j);
    if(j->active>0&&!j->manual&&rankingCurrent(j)&&earlyStopReady(j))return;
    if(j->active==0&&!j->manual&&!j->collected.isEmpty()&&rankingCurrent(j)){
        bool strong=false,conflict=false,instrumental=false,vocal=false;QString first;
        for(const auto &item:j->readyAll){const auto &r=item.record;if(!item.confidentOriginal)continue;instrumental|=r["instrumental"].toBool();vocal|=!recordLyrics(r).isEmpty();const auto text=recordLyrics(r);if(!first.isEmpty()&&text!=first)conflict=true;if(!text.isEmpty())first=text;if(!r["syncedLyrics"].toString().isEmpty()&&item.credible)strong=true;}
        if(strong&&!conflict&&!instrumental){acceptRecords(j,j->collected);return;}
        if(instrumental&&!vocal&&j->next>=j->attempts.size()){acceptRecords(j,j->collected);return;}
    }
    if(!m_proxyReady){if(j->active==0)setRowStatus(j->row,"正在准备网络");return;}
    while(j->active<m_requestLimit&&j->next<j->attempts.size()){
        // A saturated/cooling source must not block other providers behind it in the queue.
        int ready=-1;
        for(int n=j->next;n<j->attempts.size();++n){const auto &source=m_sources[j->attempts[n].source];const auto host=source.endpoint.host();if(m_cooldowns.value(host)>QDateTime::currentMSecsSinceEpoch())continue;if(m_hostRequests.value(host)<6){ready=n;break;}}
        if(ready<0){
            bool waitingForSlot=false;for(int n=j->next;n<j->attempts.size();++n)if(m_cooldowns.value(m_sources[j->attempts[n].source].endpoint.host())<=QDateTime::currentMSecsSinceEpoch())waitingForSlot=true;
            if(!waitingForSlot){j->completeResponses=false;j->next=j->attempts.size();}
            break;
        }
        if(ready!=j->next)std::rotate(j->attempts.begin()+j->next,j->attempts.begin()+ready,j->attempts.begin()+ready+1);
        const int stage=j->next++;request(j,stage);
    }
    if(j->active==0&&j->next>=j->attempts.size()){if(!j->manual&&!j->collected.isEmpty()&&!rankingCurrent(j)){scheduleRanking(j);return;}finishOnline(j);}
}
void LyricsLookup::request(const JobPtr &j,int stage,int retry){
    if(!live(j))return;const auto a=j->attempts[stage];const auto source=m_sources[a.source];QUrl url=source.endpoint;
    if(m_hostRequests.value(source.endpoint.host())>=6){
        ++j->active;QTimer::singleShot(50,this,[this,j,stage,retry]{if(live(j)){--j->active;request(j,stage,retry);}});return;
    }
    if(!j->clock.isValid()){
        j->clock.start();
        const int budget=property("lyricsSearchBudgetMs").isValid()?property("lyricsSearchBudgetMs").toInt():30000;
        QTimer::singleShot(qMax(1,budget),this,[this,j]{if(live(j)){j->completeResponses=false;j->failures<<"搜索预算已用完";finishOnline(j);}});
    }
    QList<QPair<QByteArray,QByteArray>> extraHeaders;QByteArray remotePhase;
    if(source.kind>=2){const auto step=a.detailRequest?a.detail.step:lyric_remote::begin(source.kind,a.title,a.artist,j->original.duration);url=step.url;extraHeaders=step.headers;remotePhase=a.detailRequest?"lyric":"search";}
    else if(source.kind==1)url=QUrl::fromEncoded(source.endpoint.toEncoded(QUrl::RemoveQuery|QUrl::RemoveFragment)+"/"+QUrl::toPercentEncoding(a.artist)+"/"+QUrl::toPercentEncoding(a.title));
    else {url.setPath(source.endpoint.path()+(a.exact?QString("/get"):QString("/search")));QUrlQuery q;if(a.keywordsSearch)q.addQueryItem("q",a.keywords);else {q.addQueryItem("track_name",a.title);if(!a.artist.isEmpty())q.addQueryItem("artist_name",a.artist);}const auto album=a.exact?j->match.album:a.album;if(!album.isEmpty()&&album!="未知专辑")q.addQueryItem("album_name",album);if(a.exact&&j->original.duration>0)q.addQueryItem("duration",QString::number(j->original.duration/1000.,'f',3));url.setQuery(q);}
    setRowStatus(j->row,"正在查找 · "+source.label+" · "+(a.exact?QString("精确匹配"):a.keywordsSearch?QString("关键词搜索"):a.artist.isEmpty()?QString("仅歌名搜索"):QString("歌名和歌手")));
    QNetworkRequest req(url);req.setHeader(QNetworkRequest::UserAgentHeader,source.kind>=2?"Mozilla/5.0":"Relaxer/1.10 (local music player)");req.setRawHeader("Accept","application/json");req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);for(const auto &header:extraHeaders)req.setRawHeader(header.first,header.second);
    const int budget=property("lyricsSearchBudgetMs").isValid()?property("lyricsSearchBudgetMs").toInt():30000;const int timeout=qMax(1,qMin(property("lyricsRequestTimeoutMs").isValid()?property("lyricsRequestTimeoutMs").toInt():8000,budget-int(j->clock.elapsed())));
    auto *reply=m_network.get(req);reply->setProperty("lookupTrackId",j->original.id);reply->setProperty("lookupSearchPass",j->searchPass);reply->setProperty("lookupHost",source.endpoint.host());if(!remotePhase.isEmpty())reply->setProperty("remotePhase",remotePhase);++m_hostRequests[source.endpoint.host()];++j->active;j->replies.insert(reply);const auto started=QDateTime::currentMSecsSinceEpoch();
    QTimer::singleShot(timeout,reply,[reply]{if(!reply->isFinished()){reply->setProperty("lookupTimedOut",true);reply->abort();}});connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 n,qint64){if(n>4*1024*1024){reply->setProperty("lookupTooLarge",true);reply->abort();}});
    connect(reply,&QNetworkReply::finished,this,[this,j,reply,stage,retry,source,started,requestTimeout=timeout,a]{
        releaseRequestSlot(reply);
        if(!live(j)){reply->deleteLater();return;}if(m_library->track(j->original.id).hash!=j->original.hash){finishJob(j,"歌曲文件已变化，请重新查找");return;}j->replies.remove(reply);--j->active;
        const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto error=reply->error();const bool timeout=reply->property("lookupTimedOut").toBool(),large=reply->property("lookupTooLarge").toBool();const int retryAfter=reply->rawHeader("Retry-After").toInt();const auto body=reply->isOpen()?reply->readAll():QByteArray();reply->deleteLater();
        if(code>=500&&code<=599&&retry==0&&!large&&!timeout){++j->active;QTimer::singleShot(300,this,[this,j,stage]{if(live(j)){--j->active;const int budget=property("lyricsSearchBudgetMs").isValid()?property("lyricsSearchBudgetMs").toInt():30000;if(j->clock.elapsed()<budget)request(j,stage,1);else {j->completeResponses=false;pumpQueries(j);}}});pumpQueries(j);return;}
        if(code==404){pumpQueries(j);return;}if(code==400){j->completeResponses=false;pumpQueries(j);return;}
        if(error!=QNetworkReply::NoError||code>=500||large){j->completeResponses=false;const auto failure=large?QString("响应过大"):timeout?QString("请求超时"):code==429?QString("请求限流"):code>=500?QString("服务暂时异常"):QString("网络异常");j->failures<<source.label+"："+failure;j->failures.removeDuplicates();m_fastReplies=0;m_trackLimit=m_requestLimit=3;controls();if(code==429||error==QNetworkReply::HostNotFoundError||error==QNetworkReply::SslHandshakeFailedError||error==QNetworkReply::ConnectionRefusedError){
            const auto host=source.endpoint.host();if(code==429)m_rateLimitedHosts.insert(host);
            m_cooldowns[host]=qMax(m_cooldowns.value(host),QDateTime::currentMSecsSinceEpoch()+1000*qBound(30,retryAfter,600));
        }pumpQueries(j);return;}
        auto absorb=[&,this](QJsonArray records){for(auto v:records){auto r=v.toObject();if(a.detailRequest)for(auto field=a.detail.matched.begin();field!=a.detail.matched.end();++field)r[field.key()]=field.value();if(a.cover){r["coverMatch"]=true;r["coverTitle"]=a.title;r["coverArtist"]=a.artist;}if(recordLyrics(r).trimmed().isEmpty()&&!r["instrumental"].toBool())continue;r["source"]=source.label;const auto digest=QCryptographicHash::hash(recordLyrics(r).toUtf8(),QCryptographicHash::Sha256).toHex();const auto key=source.label+":"+QString::number(r["id"].toVariant().toLongLong())+":"+normalized(r["trackName"].toString())+":"+normalized(r["artistName"].toString())+":"+QString::number(r["duration"].toDouble())+":"+QString::fromLatin1(digest);if(j->keys.contains(key))continue;j->keys.insert(key);appendCollected(j,r);}};
        if(source.kind>=2){
            const auto follow=lyric_remote::consume(source.kind,(a.detailRequest?QByteArray("lyric"):QByteArray("search")),body,a.title,a.artist,a.detailRequest?a.detail.durationMs:j->original.duration);
            if(follow.failed){j->completeResponses=false;j->failures<<source.label+"："+follow.failure;pumpQueries(j);return;}
            if(follow.again){
                QVector<lyric_remote::Detail> details{{follow.step,follow.matched,follow.durationMs}};details+=follow.remaining;
                int position=j->next;
                for(const auto &detail:details){
                    if(j->detailUrls.contains(detail.step.url))continue;j->detailUrls.insert(detail.step.url);
                    auto next=a;next.detailRequest=true;next.detail=detail;
                    // Only insert into the unsent suffix: retry callbacks retain
                    // the original indices of every already dispatched attempt.
                    j->attempts.insert(position++,next);
                }
            }
            absorb(follow.records);pumpQueries(j);return;
        }
        QJsonParseError e;const auto doc=QJsonDocument::fromJson(body,&e);if(e.error!=QJsonParseError::NoError||(!doc.isArray()&&!doc.isObject())){j->completeResponses=false;j->failures<<source.label+"：响应格式异常";pumpQueries(j);return;}
        if(QDateTime::currentMSecsSinceEpoch()-started<600&&++m_fastReplies>=8){m_fastReplies=0;m_trackLimit=qMin(10,m_trackLimit+1);m_requestLimit=qMin(5,m_requestLimit+1);controls();QTimer::singleShot(0,this,&LyricsLookup::pumpTracks);}
        QJsonArray records;if(source.kind==1){const auto raw=doc.object()["lyrics"].toString();auto html=raw;html.replace("\r\n","\n");html.replace("\n","<br>");const auto text=QTextDocumentFragment::fromHtml(html).toPlainText();if(!text.trimmed().isEmpty()){const auto &a=j->attempts[stage];records.append(QJsonObject{{"trackName",a.title},{"artistName",a.artist},{"albumName",""},{"duration",-1},{"plainLyrics",text},{"source",source.label},{"manualOnly",true}});}}
        else if(doc.isObject())records.append(doc.object());else records=doc.array();
        for(const auto &v:records){auto r=v.toObject();if(recordLyrics(r).trimmed().isEmpty()&&!r["instrumental"].toBool())continue;r["source"]=source.label;const auto digest=QCryptographicHash::hash(recordLyrics(r).toUtf8(),QCryptographicHash::Sha256).toHex();const auto key=source.label+":"+QString::number(r["id"].toVariant().toLongLong())+":"+normalized(r["trackName"].toString())+":"+normalized(r["artistName"].toString())+":"+QString::number(r["duration"].toDouble())+":"+QString::fromLatin1(digest);if(j->keys.contains(key))continue;j->keys.insert(key);appendCollected(j,r);}
        pumpQueries(j);
    });
}
void LyricsLookup::finishOnline(const JobPtr &j){
    if(!live(j))return;if(!j->collected.isEmpty()){acceptRecords(j,j->collected);return;}
    if(j->completeResponses&&j->failures.isEmpty()&&j->next>=j->attempts.size()&&m_candidates.value(j->original.id).isEmpty())m_library->saveLookupCache(j->original.id,{},true);
    if(!j->manual&&j->searchPass==0&&!j->completeResponses&&m_candidates.value(j->original.id).isEmpty()){
        auto retry=std::make_shared<Job>();retry->original=j->original;retry->match=j->match;retry->row=j->row;retry->searchPass=1;
        m_recoveryQueue<<retry;
        finishJob(j,"等待重试 · 首轮来源异常，稍后逐首复查");return;
    }
    const auto version=versionMark(j->original.title);if(version.contains("instrumental")||version.contains("纯音乐")||version.contains("伴奏")){finishJob(j,"需选择 · 需确认纯音乐（仅曲目标签线索）");return;}
    const auto existing=m_candidates.value(j->original.id);
    if(!existing.isEmpty()&&m_candidateHashes.value(j->original.id)==j->original.hash){finishJob(j,QString("需选择版本（%1 个） · 本次未获得新结果，保留上次候选").arg(existing.size())+(j->failures.isEmpty()?QString():" · "+j->failures.join("；")));return;}
    const auto status=j->failures.isEmpty()?(j->completeResponses?QString("数据库无结果"):QString("未完成：来源冷却或查询未完成")):QString("未完成：")+j->failures.join("；");finishJob(j,(j->completeResponses?status:QString("等待重试 · ")+status)+" · 可修改歌名和歌手重试");
}
void LyricsLookup::acceptRecords(const JobPtr &j,const QJsonArray &records,bool cached){
    if(!live(j))return;
    j->wantAccept=true;
    if(cached){j->acceptCached=true;if(j->cachedPayload!=records){j->cachedPayload=records;++j->cacheEpoch;j->readyValid=false;}}
    else j->acceptCached=false;
    if(cached?!cacheRankingCurrent(j):!rankingCurrent(j)){scheduleRanking(j);return;}
    applyAccept(j);
}
void LyricsLookup::clearTransientCooldowns(){
    const auto now=QDateTime::currentMSecsSinceEpoch();
    for(auto it=m_cooldowns.begin();it!=m_cooldowns.end();){
        if(!m_rateLimitedHosts.contains(it.key())||it.value()<=now){m_rateLimitedHosts.remove(it.key());it=m_cooldowns.erase(it);}
        else ++it;
    }
}
void LyricsLookup::manualSearch(){
    if(m_multi)return;
    const int row=m_tracks->currentRow();if(row<0||row>=m_ids.size())return;if(m_searchTitle->text().trimmed().isEmpty()){m_status->setText("请输入用于搜索的歌曲名称。");return;}
    if(m_aiTaskId||m_hasAiPreview)cancelAiTranslation(QStringLiteral("歌词搜索开始，未保存的 AI 翻译已取消。"));
    stop();
    clearTransientCooldowns();
    resetTranslationProgress();m_nextRow=m_ids.size();const auto t=m_library->track(m_ids[row]);if(!t.id)return;auto j=std::make_shared<Job>();j->row=row;j->original=t;j->match=searchTrack(t);j->match.title=cleanTitle(m_searchTitle->text());j->match.artist=m_searchArtist->text().trimmed();j->manual=true;j->generation=m_generation;m_jobs[t.id]=j;m_running=true;controls();rebuildCandidates();beginOnline(j);
}
void LyricsLookup::selectTrack(){
    const int row=m_tracks->currentRow();const qint64 selected=row>=0&&row<m_ids.size()?m_ids[row]:0;if((m_multi||selected!=m_aiTrackId)&&(m_aiTaskId||m_hasAiPreview))cancelAiTranslation(QStringLiteral("歌曲选择已变化，旧翻译不会写入其他歌曲。"));const bool chosen=row>=0&&row<m_ids.size()&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected();m_searchTitle->setEnabled(chosen);m_searchArtist->setEnabled(chosen);m_fragment->setEnabled(chosen);m_manualSearch->setEnabled(chosen);m_mark->setEnabled(chosen);m_offset->setEnabled(chosen);
    if(chosen){ensureLyricsSnapshot(selected);m_translationAssessments.remove(selected);}
    if(chosen&&m_searchTitle->property("trackId").toLongLong()!=m_ids[row]){const auto t=searchTrack(m_library->track(m_ids[row]));m_searchTitle->setText(t.title);m_searchArtist->setText(knownArtist(t.artist)?t.artist:QString());m_fragment->clear();m_offset->setValue(0);m_searchTitle->setProperty("trackId",m_ids[row]);}
    const bool pure=chosen&&m_tracks->item(row)->data(StatusRole).toString().contains("需确认纯音乐");m_instrumentHint->setText("有纯音乐线索，请确认；没有歌词本身不能证明是纯音乐。");m_instrumentHint->setVisible(pure);m_continue->setVisible(pure);findChild<QPushButton *>("lyricsConfirmInstrumental")->setVisible(!m_multi&&pure);rebuildCandidates();controls();
}
void LyricsLookup::rebuildCandidates(){
    const auto previousTrack=m_versions->property("trackId").toLongLong();const int previousRecord=m_visibleRecords.value(m_versions->currentRow(),-1);
    m_visibleRecords.clear();m_versions->clear();const int row=m_tracks->currentRow();m_versions->setProperty("trackId",m_ids.value(row));if(m_hasAiPreview&&row>=0&&m_ids.value(row)==m_aiTrackId)setAiTranslationPreview();else setPreviewLyrics({});m_apply->setEnabled(false);const auto records=m_candidates.value(m_ids.value(row));const bool show=!m_multi&&row>=0&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected()&&!m_tracks->currentItem()->isHidden()&&!records.isEmpty();m_candidateTitle->setText("候选版本 · 搜索后在此选择");if(!show){updatePreview();return;}const auto t=m_library->track(m_ids[row]);m_candidateTitle->setText(t.title+" · 候选版本");
    for(int i=0;i<records.size();++i){const auto r=records[i].toObject();if(m_sourceFilter->currentIndex()>0&&r["source"].toString()!=m_sourceFilter->currentText())continue;if(m_syncedOnly->isChecked()&&r["syncedLyrics"].toString().isEmpty())continue;const double seconds=r["duration"].toDouble(-1);const auto duration=seconds>0?QString("%1:%2").arg(int(seconds)/60).arg(int(seconds)%60,2,10,QChar('0')):QString("时长未知");const auto difference=t.duration>0&&seconds>0?QString(" · 相差 %1 秒").arg(qAbs(t.duration/1000.-seconds),0,'f',1):QString();const auto title=(m_visibleRecords.isEmpty()?QString("★ 最推荐  ·  "):QString())+r["trackName"].toString();const auto details=r["source"].toString("LRCLIB")+" · "+r["artistName"].toString()+" · "+r["albumName"].toString()+" · "+duration+" · "+(r["instrumental"].toBool()?QString("纯音乐"):r["syncedLyrics"].toString().isEmpty()?QString("普通歌词"):QString("滚动歌词"))+difference;auto *item=new QListWidgetItem(title+"\n"+details,m_versions);item->setToolTip(details+"\n"+matchReason(t,r));item->setData(Qt::UserRole,m_visibleRecords.isEmpty());item->setData(Qt::UserRole+3,i);item->setSizeHint(QSize(0,76));m_visibleRecords<<i;}
    const int previous=previousTrack==t.id?m_visibleRecords.indexOf(previousRecord):-1;
    m_versions->setCurrentRow(m_visibleRecords.isEmpty()?-1:qMax(0,previous));
}
bool LyricsLookup::saveManual(const QByteArray &data,const QString &status,bool preserveText){
    const int row=m_tracks->currentRow();if(row<0||row>=m_ids.size())return false;const auto t=m_library->track(m_ids[row]);if(!t.id)return false;if(data.size()>4*1024*1024||Lyrics::parse(data).lines.isEmpty()){m_status->setText("歌词为空、格式无效或超过4 MB。");return false;}
    if(!checkLyricsSnapshot(t.id)){m_status->setText(m_storeError);setRowStatus(row,m_storeError);return false;}
    if((!t.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(t.lyrics)))||(m_aiTrackId==t.id&&(m_aiTaskId||m_hasAiPreview))){
        QMessageBox confirm(QMessageBox::Question,QStringLiteral("替换歌词"),preserveText?QStringLiteral("该版本的原文与时间轴一致，将保留本地已有译文。是否使用？"):QStringLiteral("使用这份内容替换已有歌词？如果存在译文，也会一起替换，不会自动套用到新版本。"),QMessageBox::Yes|QMessageBox::No,this);
        confirm.setObjectName("lyricsReplaceConfirm");confirm.setDefaultButton(QMessageBox::No);
        if(m_aiTaskId||m_hasAiPreview)confirm.setInformativeText(QStringLiteral("确认替换后，当前翻译任务或未保存的 AI 结果将作废。"));
        if(confirm.exec()!=QMessageBox::Yes)return false;
    }
    cancelJob(t.id);if(!store(t,withOffset(data,m_offset->value()),true,preserveText)){m_status->setText("未保存："+m_storeError);setRowStatus(row,"未保存："+m_storeError);return false;}cancelAiTranslationForTrack(t.id);++m_saved;setRowStatus(row,status);updateFilter();selectNextIncomplete(row);pumpTracks();return true;
}
void LyricsLookup::selectNextIncomplete(int after){
    if(m_multi)return;
    const int count=m_tracks->count();
    for(int offset=1;offset<=count;++offset){
        const int row=(after+offset)%count;
        if(m_tracks->item(row)->isHidden()||(!(m_missingTranslationOnly&&m_missingTranslationOnly->isChecked())&&rowComplete(m_tracks->item(row))))continue;
        m_tracks->setCurrentRow(row);
        m_tracks->scrollToItem(m_tracks->item(row),QAbstractItemView::EnsureVisible);
        selectTrack();return;
    }
    selectTrack();
}
bool LyricsLookup::applySelected(){
    if(m_multi||!m_tracks->currentItem()||!m_tracks->currentItem()->isSelected()||m_tracks->currentItem()->isHidden())return false;
    const int row=m_tracks->currentRow(),index=m_visibleRecords.value(m_versions->currentRow(),-1);if(row<0||index<0)return false;const auto t=m_library->track(m_ids[row]);const auto records=m_candidates.value(t.id);if(index>=records.size()||m_candidateHashes.value(t.id)!=t.hash){m_status->setText("歌曲文件已变化，请重新搜索歌词。");return false;}const auto r=records[index].toObject();if(r["instrumental"].toBool()){if(QMessageBox::question(this,"确认纯音乐","将此曲标记为纯音乐？",QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes)return false;return saveManual(QString("纯音乐，请欣赏").toUtf8(),"已保存纯音乐标记");}const auto plan=replacementContent(t,r);return saveManual(plan.bytes,"已保存所选版本",plan.preservesTranslation);
}
void LyricsLookup::pasteLyrics(){
    if(m_multi)return;
    if(m_tracks->currentRow()<0)return;QDialog dialog(this);dialog.setObjectName("lyricsPasteDialog");dialog.setWindowTitle("粘贴歌词");dialog.resize(580,440);auto *layout=new QVBoxLayout(&dialog);auto *text=new QPlainTextEdit(&dialog);text->setObjectName("lyricsPasteText");text->setPlaceholderText("粘贴 LRC 或普通歌词");const auto clip=QApplication::clipboard()->text();if(clip.contains('\n'))text->setPlainText(clip);layout->addWidget(text,1);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(dialog.exec()==QDialog::Accepted)saveManual(text->toPlainText().toUtf8(),"已保存粘贴歌词");
}
void LyricsLookup::importLyrics(){if(m_multi||m_tracks->currentRow()<0)return;const auto path=QFileDialog::getOpenFileName(this,"导入歌词",{},"歌词 (*.lrc *.txt)");if(path.isEmpty())return;const auto file=LyricsDocumentReader::readFile(path);if(!file.opened||file.tooLarge){m_status->setText("歌词文件无法读取或超过4 MB。");return;}saveManual(file.bytes,"已保存导入歌词");}
void LyricsLookup::saveOffsets(){
    const auto rows=batchRows();if(rows.isEmpty())return;const int offset=m_offset->value();stop();int saved=0,skipped=0,failed=0;
    for(int row:rows){const auto track=m_library->track(m_ids[row]);if(track.lyrics.isEmpty()){++skipped;continue;}const auto document=LyricsDocumentReader::readDocument(m_library->absolute(track.lyrics));if(!document.file.opened||document.file.tooLarge){++skipped;continue;}const auto &data=document.file.bytes;const auto &parsed=document.lyrics;if(!parsed.timed||parsed.instrumental){++skipped;continue;}
        ensureLyricsSnapshot(track.id);if(track.id==m_aiTrackId&&(m_aiTaskId||m_hasAiPreview)&&QMessageBox::question(this,"替换歌词",QStringLiteral("保存偏移会替换当前歌词并清除未保存的 AI 结果，是否继续？"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes){++skipped;continue;}if(store(track,withOffset(data,offset),true)){cancelAiTranslationForTrack(track.id);++saved;setRowStatus(row,"已保存时间偏移");}else{++failed;setRowStatus(row,"未保存："+m_storeError);}
    }
    m_saved+=saved;selectTrack();m_status->setText(QString("时间偏移已保存 %1 首，跳过 %2 首，失败 %3 首。").arg(saved).arg(skipped).arg(failed));controls();
}
void LyricsLookup::markInstrumental(){
    if(!m_multi){saveManual(QString("纯音乐，请欣赏").toUtf8(),"已保存纯音乐标记");return;}
    const auto rows=batchRows();if(rows.isEmpty())return;int existing=0;for(int row:rows){const auto track=m_library->track(m_ids[row]);if(!track.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(track.lyrics)))++existing;}
    if(existing&&QMessageBox::question(this,"批量标记纯音乐",QString("将所选 %1 首标记为纯音乐，其中 %2 首已有歌词将被替换。是否继续？").arg(rows.size()).arg(existing),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    stop();int saved=0,failed=0;const auto data=QString("纯音乐，请欣赏").toUtf8();
    for(int row:rows){const auto track=m_library->track(m_ids[row]);ensureLyricsSnapshot(track.id);if(store(track,data,true)){cancelAiTranslationForTrack(track.id);++saved;m_candidates.remove(track.id);m_candidateHashes.remove(track.id);m_library->clearLookupCache(track.id);setRowStatus(row,"已保存纯音乐标记");}else{++failed;setRowStatus(row,"未保存："+m_storeError);}}
    m_saved+=saved;updateFilter();selectTrack();m_status->setText(QString("已标记纯音乐 %1 首，失败 %2 首。").arg(saved).arg(failed));controls();
}
bool LyricsLookup::productionHost() const {return !m_sources.isEmpty()&&m_sources.first().endpoint.host()=="lrclib.net";}
void LyricsLookup::translateSelected(){
    if(m_aiTaskId||m_aiConsentPending){m_status->setText(QStringLiteral("当前整首翻译请求仍在进行中。"));return;}
    QScopedValueRollback<bool> preparing(m_aiConsentPending,true);
    if(m_multi){m_status->setText(QStringLiteral("整首 AI 翻译一次只处理一首歌，请先退出多选并选择一首歌曲。"));return;}
    const int row=m_tracks->currentRow();
    if(row<0||row>=m_ids.size()||!m_tracks->currentItem()||!m_tracks->currentItem()->isSelected()||m_tracks->currentItem()->isHidden()){
        m_status->setText(QStringLiteral("请先选择一首歌曲。"));return;
    }
    if(m_hasAiPreview){
        if(QMessageBox::question(this,QStringLiteral("重新发送整首翻译"),
             QStringLiteral("当前译文预览尚未保存。再次翻译会丢弃当前预览，并发送一次新的整首歌词请求。是否继续？"),
             QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        clearAiTranslationPreview();
    }
    if(!m_aiRuntime||!m_aiRuntime->requests()){
        m_status->setText(QStringLiteral("AI 服务不可用。请从主窗口打开歌词翻译。"));return;
    }
    const auto track=m_library->track(m_ids[row]);
    if(!track.id||track.lyrics.isEmpty()){
        setRowStatus(row,QStringLiteral("没有已保存的原文歌词，无法翻译"));m_status->setText(QStringLiteral("请先为这首歌保存歌词。"));return;
    }
    const auto document=LyricsDocumentReader::readDocument(m_library->absolute(track.lyrics));
    if(!document.file.opened||document.file.tooLarge||!document.file.complete){
        setRowStatus(row,QStringLiteral("歌词文件无法完整读取"));m_status->setText(QStringLiteral("歌词文件无法完整读取或超过 4 MB。"));return;
    }
    const auto prepared=ai::lyrics::prepare(document.file.bytes,track.title,track.artist);
    if(!prepared.ok()){
        setRowStatus(row,prepared.bilingual?QStringLiteral("已有双语歌词，未启动翻译"):QStringLiteral("没有可翻译的歌词"));
        m_status->setText(prepared.error);return;
    }
    const auto taskConfiguration=m_aiRuntime->translationConfiguration();
    const auto configurationError=m_aiRuntime->validateConfiguration(taskConfiguration);
    if(!configurationError.isEmpty()){m_status->setText(configurationError);return;}
    const auto *model=m_aiRuntime->model(taskConfiguration.providerId,taskConfiguration.modelId);
    if(!model){m_status->setText(QStringLiteral("所选模型不可用，请在 AI 服务设置中刷新模型列表。"));return;}
    const auto serviceConfiguration=m_aiRuntime->configuration();
    const auto targetLanguage=serviceConfiguration.targetLanguage.trimmed().isEmpty()?QStringLiteral("zh-CN"):serviceConfiguration.targetLanguage.trimmed();
    if(targetLanguage!=QStringLiteral("zh-CN")){
        m_status->setText(QStringLiteral("当前只支持简体中文目标语言，请检查 AI 翻译设置；本次未发送请求。"));return;
    }
    const auto stylePrompt=serviceConfiguration.translationStylePrompt;
    const auto messages=ai::lyrics::chatMessages(targetLanguage,stylePrompt,prepared);
    const auto inputBound=ai::lyrics::inputTokenUpperBound(messages);
    const auto contextWindow=model->capabilities.contextWindow;
    const auto outputLimit=model->capabilities.maxOutputTokens;
    if(contextWindow<=0||outputLimit<=0||inputBound>contextWindow-outputLimit){
        m_status->setText(QStringLiteral("整首歌词与输出上限无法放入所选模型的已确认上下文窗口；本次未发送请求。请手动选择兼容模型或精简歌词后再试。"));return;
    }
    const auto requestConfigurationDigest=currentAiConfigurationDigest();
    QMessageBox consent(this);consent.setObjectName("lyricsAiCloudConsent");consent.setIcon(QMessageBox::Information);
    consent.setWindowTitle(QStringLiteral("发送整首歌词到 DeepSeek"));
    consent.setText(QStringLiteral("Relaxer 将把待翻译的歌词文本、目标语言和风格提示发送至 DeepSeek 官方服务器处理。\n"
        "不会上传 MP3、封面或本地路径，也不单独发送歌名、歌手字段；已识别的署名行仅保留在本地。\n\n"
        "本次操作只发送一次 Chat Completions 请求，不会自动拆分、重试或切换模型。使用量可能产生费用；费用以供应商账单为准。\n\n"
        "取消只能停止本地任务，无法撤销 DeepSeek 已收到或处理的请求。返回译文后，只有你点击“保存双语歌词”才会写入本地。"));
    auto *decline=consent.addButton(QStringLiteral("不发送"),QMessageBox::RejectRole);decline->setObjectName("lyricsAiConsentCancel");
    auto *send=consent.addButton(QStringLiteral("发送一次整首歌词"),QMessageBox::AcceptRole);send->setObjectName("lyricsAiConsentSend");
    consent.setDefaultButton(qobject_cast<QPushButton *>(decline));consent.exec();
    if(consent.clickedButton()!=send){m_status->setText(QStringLiteral("未发送歌词。"));return;}

    // The modal consent dialog runs a nested event loop. Recheck the snapshot
    // before charging, not only when the response eventually arrives.
    const auto current=m_library->track(track.id);
    if(!m_aiRuntime||m_multi||m_tracks->currentRow()!=row||!m_tracks->currentItem()||
       !m_tracks->currentItem()->isSelected()||m_tracks->currentItem()->isHidden()||
       m_ids.value(row)!=track.id||current.hash!=track.hash||current.title!=track.title||
       current.artist!=track.artist||current.lyrics!=track.lyrics||
       currentAiConfigurationDigest()!=requestConfigurationDigest){
        m_status->setText(QStringLiteral("发送前歌曲或 AI 配置已变化；本次未发送请求。"));return;
    }
    const auto latest=LyricsDocumentReader::readFile(m_library->absolute(track.lyrics));
    if(!latest.opened||!latest.complete||latest.tooLarge||
       QCryptographicHash::hash(latest.bytes,QCryptographicHash::Sha256)!=prepared.sourceDigest){
        m_status->setText(QStringLiteral("发送前歌词已变化；本次未发送请求。"));return;
    }
    const auto dispatchError=m_aiRuntime->validateConfiguration(taskConfiguration);
    if(!dispatchError.isEmpty()){m_status->setText(dispatchError);return;}

    ai::ChatCompletionRequest request;request.modelId=taskConfiguration.modelId;request.messages=messages;
    request.thinkingEnabled=taskConfiguration.thinkingEnabled;request.reasoningEffort=taskConfiguration.reasoningEffort;
    request.maxTokens=outputLimit;
    m_aiTrackId=track.id;m_aiTrackSnapshot=track;m_aiLyricsPath=track.lyrics;m_aiSourceBytes=document.file.bytes;
    m_aiSourceDigest=prepared.sourceDigest;m_aiPrepared=prepared;
    m_aiTargetLanguage=targetLanguage;m_aiConfigurationDigest=requestConfigurationDigest;
    m_aiTranslations.clear();m_aiMergedBytes.clear();m_hasAiPreview=false;
    m_aiActiveGeneration=++m_aiGeneration;
    m_aiTaskId=m_aiRuntime->requests()->complete(taskConfiguration.providerId,request);
    if(!m_aiTaskId){m_status->setText(QStringLiteral("AI 请求未能启动。"));return;}
    m_translate->setEnabled(false);m_aiCancelTranslation->setEnabled(true);
    setTranslationProgress(QStringLiteral("正在翻译"),QStringLiteral("已发送一次整首歌词请求，正在等待 DeepSeek 响应，可取消。"),-1);
    setRowStatus(row,QStringLiteral("正在翻译整首歌词"));
}LyricsLookup::RankResult LyricsLookup::computeRank(const ScoreSnap &snap){
    RankResult result;
    result.id=snap.id;result.hash=snap.hash;result.fpO=snap.fpO;result.fpM=snap.fpM;result.generation=snap.generation;result.epoch=snap.epoch;result.cached=snap.cached;result.timedRequired=snap.timedRequired;result.payload=snap.payload;
    g_rankRuns.fetch_add(1,std::memory_order_relaxed);
    for(int left=snap.delayMs;left>0;){if(lyricComputeCancel().load()){result.dropped=true;return result;}const int step=std::min(20,left);QThread::msleep(step);left-=step;}
    for(const auto &value:snap.payload){
        if(lyricComputeCancel().load()){result.dropped=true;return result;}
        const auto r=value.toObject();RankItem item;item.record=r;
        const bool keep=!recordLyrics(r).trimmed().isEmpty()||r["instrumental"].toBool();
        const bool dropTimed=snap.timedRequired&&!Lyrics::parse(recordLyrics(r).toUtf8()).timed;
        item.confidentOriginal=confident(snap.original,r);
        const auto bytes=recordLyrics(r).toUtf8();item.credible=credibleLyrics(bytes,snap.original,&item.rejection);
        if(r["coverMatch"].toBool()){Track alias=snap.original;alias.title=r["coverTitle"].toString();alias.artist=r["coverArtist"].toString();QJsonObject copy=r;item.confidentCover=!alias.title.trimmed().isEmpty()&&confident(alias,copy);}
        result.all.push_back(item);
        if(keep&&!dropTimed){item.score=matchScore(snap.match,r);result.ranked.push_back(item);}
    }
    std::stable_sort(result.ranked.begin(),result.ranked.end(),[](const RankItem &a,const RankItem &b){return a.score>b.score;});
    return result;
}
bool LyricsLookup::rankingCurrent(const JobPtr &j) const {
    return j->readyValid&&!j->readyCached&&j->readyEpoch==j->collectionEpoch&&j->readyFpO==trackContext(j->original)&&j->readyFpM==trackContext(j->match)&&j->readyTimed==property("requireTimedLyrics").toBool();
}
bool LyricsLookup::cacheRankingCurrent(const JobPtr &j) const {
    return j->readyValid&&j->readyCached&&j->readyEpoch==j->cacheEpoch&&j->readyFpO==trackContext(j->original)&&j->readyFpM==trackContext(j->match)&&j->readyTimed==property("requireTimedLyrics").toBool();
}
bool LyricsLookup::earlyStopReady(const JobPtr &j) const {
    for(const auto &item:j->readyAll)if(!item.record["syncedLyrics"].toString().isEmpty()&&item.confidentOriginal&&item.credible)return true;
    return false;
}
void LyricsLookup::scheduleRanking(const JobPtr &j){
    if(!live(j))return;
    if(j->scoreInFlight)return;
    const bool cached=j->wantAccept&&j->acceptCached;
    const QJsonArray payload=cached?j->cachedPayload:j->collected;
    if(payload.isEmpty()&&!j->wantAccept)return;
    j->scoreInFlight=true;
    ScoreSnap snap;snap.payload=payload;snap.original=j->original;snap.match=j->match;snap.id=j->original.id;snap.hash=j->original.hash;snap.fpO=trackContext(j->original);snap.fpM=trackContext(j->match);snap.generation=j->generation;snap.epoch=cached?j->cacheEpoch:j->collectionEpoch;snap.cached=cached;snap.timedRequired=property("requireTimedLyrics").toBool();snap.delayMs=property("lyricScoreDelayMs").toInt();
    QPointer<LyricsLookup> self(this);
    lyricPool()->start(new LyricRunnable([snap,self,j]{
        const auto result=computeRank(snap);
        auto *app=QCoreApplication::instance();
        if(!self||!app)return;
        QMetaObject::invokeMethod(app,[self,j,result]{if(self)self->applyRanking(j,result);},Qt::QueuedConnection);
    }));
}
void LyricsLookup::applyRanking(const JobPtr &j,const RankResult &result){
    j->scoreInFlight=false;
    if(result.dropped||lyricComputeCancel().load())return;
    if(!live(j)||result.generation!=j->generation||result.generation!=m_generation||result.id!=j->original.id||result.hash!=j->original.hash)return;
    const bool cached=j->wantAccept&&j->acceptCached;
    const QJsonArray current=cached?j->cachedPayload:j->collected;
    const quint64 epoch=cached?j->cacheEpoch:j->collectionEpoch;
    const auto fpO=trackContext(j->original),fpM=trackContext(j->match);
    const bool timed=property("requireTimedLyrics").toBool();
    if(result.cached!=cached||result.epoch!=epoch||result.payload!=current||result.fpO!=fpO||result.fpM!=fpM||result.timedRequired!=timed){scheduleRanking(j);return;}
    j->readyRanked=result.ranked;j->readyAll=result.all;j->readyEpoch=epoch;j->readyValid=true;j->readyCached=cached;j->readyFpO=fpO;j->readyFpM=fpM;j->readyTimed=timed;
    if(!result.ranked.isEmpty()){
        QJsonArray preview;for(int n=0;n<qMin(25,result.ranked.size());++n)preview.append(result.ranked[n].record);
        m_candidates[j->original.id]=preview;m_candidateHashes[j->original.id]=j->original.hash;
        if(m_tracks->currentRow()==j->row)rebuildCandidates();
    }
    if(j->wantAccept)applyAccept(j);else pumpQueries(j);
}
void LyricsLookup::applyAccept(const JobPtr &j){
    if(!live(j))return;
    const bool cached=j->acceptCached;
    if(j->readyTimed&&j->readyRanked.isEmpty()){j->wantAccept=false;finishJob(j,"未找到滚动歌词 · 原有歌词已保留");return;}
    QJsonArray ranked;for(int i=0;i<qMin(25,j->readyRanked.size());++i)ranked.append(j->readyRanked[i].record);
    m_candidates[j->original.id]=ranked;m_candidateHashes[j->original.id]=j->original.hash;
    if(!cached&&!ranked.isEmpty())m_library->saveLookupCache(j->original.id,ranked,false);
    const QString suffix=cached?QString(" · 使用上次结果"):QString();QString rejection;bool instrumental=false;
    for(const auto &item:j->readyRanked)instrumental|=item.record["instrumental"].toBool();
    auto commit=[&](const RankItem &item)->bool{const auto now=m_library->track(j->original.id);if(!now.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(now.lyrics)))return false;const auto &r=item.record;const bool pure=r["instrumental"].toBool();const auto data=pure?QString("纯音乐，请欣赏").toUtf8():recordLyrics(r).toUtf8();if(!pure&&!item.credible){rejection=item.rejection;return false;}const auto status=(pure?QString("纯音乐 · 已保存标记"):r["syncedLyrics"].toString().isEmpty()?QString("已保存普通歌词"):QString("已保存滚动歌词"))+suffix+(r["coverMatch"].toBool()?QString(" · 已匹配原唱"):QString());if(store(j->original,data)){++m_saved;j->wantAccept=false;finishJob(j,status);return true;}j->wantAccept=false;finishJob(j,"未保存："+m_storeError+suffix);return true;};
    if(!j->manual){
        for(const auto &item:j->readyRanked){const auto &r=item.record;if(r["coverMatch"].toBool()||!item.confidentOriginal)continue;bool ambiguous=false;for(const auto &other:j->readyRanked){if(other.record["coverMatch"].toBool()||!other.confidentOriginal)continue;if(r["instrumental"].toBool()!=other.record["instrumental"].toBool()){ambiguous=true;break;}if(recordLyrics(other.record)!=recordLyrics(r)&&normalized(r["albumName"].toString())!=normalized(other.record["albumName"].toString())&&normalized(j->original.album)!=normalized(r["albumName"].toString())){ambiguous=true;break;}}if(ambiguous)break;if(commit(item))return;}
        for(const auto &item:j->readyRanked){if(!item.record["coverMatch"].toBool()||!item.confidentCover)continue;if(!item.credible){rejection=item.rejection;continue;}if(commit(item))return;}
    }
    j->wantAccept=false;QString status=QString("需选择版本（%1 个）").arg(ranked.size());if(instrumental)status+=" · 需确认纯音乐";if(!rejection.isEmpty())status+=" · "+rejection;if(!j->failures.isEmpty())status+=" · "+j->failures.join("；");finishJob(j,status+suffix);
}
} // namespace relaxer

