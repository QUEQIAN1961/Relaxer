#include <QApplication>
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
#include "browser-translation.h"
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
        const int state=index.data(StateRole).toInt();
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
LyricsLookup::LyricsLookup(Library *library,QVector<qint64> ids,QWidget *parent,QUrl endpoint,QUrl backupEndpoint,bool pinExplicit)
    :QDialog(parent),m_library(library),m_ids(std::move(ids)),m_network(this),m_pinExplicit(pinExplicit){
    installSmoothScrolling(qApp);
    if(backupEndpoint.isEmpty()&&endpoint.host()=="lrclib.net")backupEndpoint=QUrl("https://api.lyrics.ovh/v1");
    m_sources.append({"LRCLIB",endpoint,0,false});if(!backupEndpoint.isEmpty())m_sources.append({"Lyrics.ovh",backupEndpoint,1,true});
    if(endpoint.host()=="lrclib.net"){m_sources.append({"网易云",QUrl("https://music.163.com"),2,false});m_sources.append({"QQ音乐",QUrl("https://y.qq.com"),3,false});m_sources.append({"酷狗",QUrl("https://www.kugou.com"),4,false});}
    if(endpoint.host()=="127.0.0.1"||endpoint.host()=="localhost"){m_network.setProxy(QNetworkProxy::NoProxy);m_proxyReady=true;}else{m_proxyReady=false;m_proxyState=std::make_shared<std::atomic_bool>(false);QPointer<LyricsLookup> self(this);auto state=m_proxyState;std::thread([self,state]{QNetworkProxy chosen(QNetworkProxy::NoProxy);const auto found=QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(QUrl("https://lrclib.net")));for(const auto &proxy:found)if(proxy.type()==QNetworkProxy::HttpProxy||proxy.type()==QNetworkProxy::Socks5Proxy||proxy.type()==QNetworkProxy::HttpCachingProxy){chosen=proxy;break;}if(!qApp)return;QMetaObject::invokeMethod(qApp,[self,state,chosen]{if(!self||!state||state->load())return;self->m_network.setProxy(chosen);self->m_proxyReady=true;const auto jobs=self->m_jobs.values();for(const auto &job:jobs)if(self->live(job))self->pumpQueries(job);},Qt::QueuedConnection);}).detach();}
    setObjectName("lyricsLookupDialog");setWindowTitle("查找歌词");resize(1100,700);setMinimumSize(860,560);setModal(true);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(18,18,18,18);layout->setSpacing(12);
    auto *splitter=new QSplitter(Qt::Horizontal,this);splitter->setObjectName("lyricsSplit");splitter->setChildrenCollapsible(false);
    auto *left=new QWidget(splitter);auto *leftLayout=new QVBoxLayout(left);leftLayout->setContentsMargins(0,0,8,0);leftLayout->setSpacing(10);
    auto *toolbar=new QHBoxLayout;m_start=new QPushButton("搜索歌词",left);m_start->setObjectName("lyricsStart");m_stop=new QPushButton("停止",left);m_stop->setObjectName("lyricsStop");m_stop->setEnabled(false);m_translate=new QPushButton("翻译歌词",left);m_translate->setObjectName("lyricsTranslate");toolbar->addWidget(m_start);toolbar->addWidget(m_stop);toolbar->addWidget(m_translate);m_multiButton=new QPushButton("多选",left);m_multiButton->setObjectName("lyricsMultiSelect");m_multiButton->setCheckable(true);toolbar->addWidget(m_multiButton);m_allButton=new QPushButton("全选",left);m_allButton->setObjectName("lyricsSelectAll");m_allButton->hide();toolbar->addWidget(m_allButton);toolbar->addStretch();leftLayout->addLayout(toolbar);
    auto *checks=new QHBoxLayout;m_replace=new QCheckBox("重新匹配并替换已有歌词",left);m_replace->setObjectName("lyricsReplace");m_missingOnly=new QCheckBox("仅显示无歌词歌曲",left);m_missingOnly->setObjectName("lyricsMissingOnly");m_autoTranslate=new QCheckBox("自动翻译",left);m_autoTranslate->setObjectName("lyricsAutoTranslate");m_autoTranslate->setChecked(m_library->setting("translateLyrics",true).toBool());checks->addWidget(m_replace);checks->addWidget(m_missingOnly);checks->addWidget(m_autoTranslate);checks->addStretch();leftLayout->addLayout(checks);
    auto *browserRow=new QHBoxLayout;m_translationProvider=new RoundedComboBox(left);m_translationProvider->setObjectName("lyricsTranslationProvider");m_translationProvider->addItem("必应（本地浏览器）","bing");m_translationProvider->addItem("百度（本地浏览器）","baidu");m_translationProvider->setCurrentIndex(m_library->setting("lyricBrowserProvider","bing").toString()=="baidu"?1:0);
    browserRow->addWidget(new QLabel("翻译来源",left));browserRow->addWidget(m_translationProvider,1);
    auto *showBrowser=new QPushButton("翻译窗口",left);showBrowser->setObjectName("lyricsShowTranslationBrowser");showBrowser->setToolTip("翻译开始后可打开独立浏览器窗口；遇到验证时在该窗口处理。");browserRow->addWidget(showBrowser);
    m_translationResume=new QPushButton("验证完成，继续",left);m_translationResume->setObjectName("lyricsTranslationResume");m_translationResume->hide();browserRow->addWidget(m_translationResume);leftLayout->addLayout(browserRow);
    m_browserTranslation=new BrowserTranslation(m_library->root(),this);
    connect(m_translationProvider,&QComboBox::currentIndexChanged,this,[this]{m_library->setSetting("lyricBrowserProvider",m_translationProvider->currentData().toString());});
    connect(showBrowser,&QPushButton::clicked,m_browserTranslation,&BrowserTranslation::showBrowser);
    connect(m_translationResume,&QPushButton::clicked,m_browserTranslation,&BrowserTranslation::resume);
    connect(m_browserTranslation,&BrowserTranslation::needsAttention,this,[this](const QString &reason){m_translationResume->show();m_status->setText(reason);auto it=m_translateTasks.find(m_browserTrack);if(it!=m_translateTasks.end())setRowStatus(it->row,"正在翻译：等待浏览器验证");});
    connect(m_browserTranslation,&BrowserTranslation::resumed,this,[this]{m_translationResume->hide();m_status->setText("正在继续取回网页译文");});
    connect(m_browserTranslation,&BrowserTranslation::completed,this,[this](quint64 token,const QString &text,const QString &error){
        const auto id=m_browserTrack;auto it=m_translateTasks.find(id);if(it==m_translateTasks.end()||quint64(it->token)!=token||it->generation!=m_translateGeneration)return;
        m_translationResume->hide();m_browserTrack=0;
        if(!error.isEmpty()||text.trimmed().isEmpty()){finishTranslation(id,false,error.isEmpty()?QString("网页未返回有效译文"):error);return;}
        it->translated[it->cursor]=text.simplified();++it->cursor;m_translationStats[id].done=it->cursor;updateTranslationProgress();
        const auto generation=m_translateGeneration;QTimer::singleShot(600,this,[this,id,token,generation]{auto next=m_translateTasks.find(id);if(next!=m_translateTasks.end()&&quint64(next->token)==token&&generation==m_translateGeneration)translateStep(id);});
    });
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
    auto *actions=new QHBoxLayout;m_incomplete=new QPushButton("只显示未完成",right);m_incomplete->setObjectName("lyricsIncomplete");m_incomplete->setCheckable(true);actions->addWidget(m_incomplete);actions->addStretch();m_apply=new QPushButton("使用选中版本",right);m_apply->setObjectName("lyricsApply");m_apply->setEnabled(false);actions->addWidget(m_apply);auto *close=new QPushButton("保存并关闭",right);close->setObjectName("lyricsSaveClose");actions->addWidget(close);rightLayout->addLayout(actions);
    m_translationProgressPanel=new QWidget(right);m_translationProgressPanel->setObjectName("lyricsTranslationProgressPanel");m_translationProgressPanel->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Fixed);
    auto *translationLayout=new QHBoxLayout(m_translationProgressPanel);translationLayout->setContentsMargins(0,2,0,0);translationLayout->setSpacing(6);
    m_translationProgressLabel=new QLabel("翻译尚未开始",m_translationProgressPanel);m_translationProgressLabel->setObjectName("lyricsTranslationProgressLabel");m_translationProgressLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    m_translationProgress=new QProgressBar(m_translationProgressPanel);m_translationProgress->setObjectName("lyricsTranslationProgress");m_translationProgress->setRange(0,1);m_translationProgress->setValue(0);m_translationProgress->setFixedWidth(180);m_translationProgress->setFixedHeight(10);m_translationProgress->setTextVisible(false);
    translationLayout->addWidget(m_translationProgressLabel);translationLayout->addWidget(m_translationProgress);rightLayout->addWidget(m_translationProgressPanel,0,Qt::AlignRight);
    splitter->addWidget(left);splitter->addWidget(right);splitter->setStretchFactor(0,2);splitter->setStretchFactor(1,3);splitter->setSizes({360,700});layout->addWidget(splitter,1);
    m_status=new QLabel("等待开始查找",left);m_status->setObjectName("lyricsStatus");m_status->setWordWrap(true);leftLayout->addWidget(m_status);
    applyLookupStyle();
    connect(m_start,&QPushButton::clicked,this,&LyricsLookup::start);connect(m_stop,&QPushButton::clicked,this,&LyricsLookup::stop);connect(m_translate,&QPushButton::clicked,this,&LyricsLookup::translateSelected);connect(m_manualSearch,&QPushButton::clicked,this,&LyricsLookup::manualSearch);connect(m_searchTitle,&QLineEdit::returnPressed,this,&LyricsLookup::manualSearch);connect(m_searchArtist,&QLineEdit::returnPressed,this,&LyricsLookup::manualSearch);connect(m_fragment,&QLineEdit::returnPressed,this,&LyricsLookup::manualSearch);
    connect(m_tracks,&QListWidget::itemSelectionChanged,this,&LyricsLookup::selectTrack);connect(m_incomplete,&QPushButton::toggled,this,[this](bool on){m_incomplete->setText(on?"✓ 只显示未完成":"只显示未完成");updateFilter();});
    connect(m_missingOnly,&QCheckBox::toggled,this,[this]{updateFilter();});connect(m_autoTranslate,&QCheckBox::toggled,this,[this](bool on){if(m_library)m_library->setSetting("translateLyrics",on);});
    connect(m_versions,&QListWidget::currentRowChanged,this,[this](int row){const int track=m_tracks->currentRow();const auto records=m_candidates.value(m_ids.value(track));const int index=m_visibleRecords.value(row,-1);if(track<0||row<0||index<0||index>=records.size()){m_apply->setEnabled(false);updatePreview();return;}const auto r=records[index].toObject();const auto text=r["instrumental"].toBool()?QString("纯音乐，请欣赏"):recordLyrics(r);updatePreview();m_candidateTitle->setText("候选版本 · "+matchReason(m_library->track(m_ids[track]),r));m_apply->setEnabled(!m_multi&&!text.isEmpty()&&!rowComplete(m_tracks->item(track)));});
    connect(m_apply,&QPushButton::clicked,this,[this]{applySelected();});connect(close,&QPushButton::clicked,this,[this]{if(!m_multi&&m_apply->isEnabled()&&!applySelected())return;if(!confirmStopTranslations())return;stop();accept();});m_missingOnly->setChecked(true);updateFilter();selectTrack();m_library->deferChanges(true);
}
LyricsLookup::~LyricsLookup(){if(m_proxyState)m_proxyState->store(true);cancelTranslations();stop();if(m_library)m_library->deferChanges(false);}
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
void LyricsLookup::updatePreview(){
    const int row=m_tracks->currentRow();
    if(m_multi||row<0||!m_tracks->currentItem()||!m_tracks->currentItem()->isSelected()||m_tracks->currentItem()->isHidden()){setPreviewLyrics({});return;}
    const auto track=m_library->track(m_ids.value(row));const auto records=m_candidates.value(track.id);
    const int index=m_visibleRecords.value(m_versions->currentRow(),-1);QByteArray data;
    if(index>=0&&index<records.size()){const auto r=records[index].toObject();data=(r["instrumental"].toBool()?QString("纯音乐，请欣赏"):recordLyrics(r)).toUtf8();}
    if(!track.lyrics.isEmpty()){
        const auto saved=LyricsDocumentReader::readFile(m_library->absolute(track.lyrics));
        if(saved.opened&&!saved.tooLarge){
            const auto &local=saved.bytes;
            if(data.isEmpty()||(lyricsIncludeTranslation(local)&&lyricTexts(sourceLyricsForTranslation(local))==lyricTexts(sourceLyricsForTranslation(data))))data=local;
        }
    }
    setPreviewLyrics(data);
}
void LyricsLookup::resetTranslationProgress(){
    if(!translationPending()){m_translationStats.clear();updateTranslationProgress();}
}
void LyricsLookup::updateTranslationProgress(){
    if(!m_translationProgress)return;
    int total=0,processed=0,translated=0,finished=0,failed=0,cancelled=0;
    for(const auto &stat:m_translationStats){total+=stat.total;processed+=stat.finished?stat.total:stat.done;translated+=stat.done;finished+=stat.finished;failed+=stat.failed;cancelled+=stat.cancelled;}
    m_translationProgress->setRange(0,qMax(1,total));m_translationProgress->setValue(processed);
    QString text="翻译尚未开始";
    if(total>0){
        text=QString("翻译 %1 / %2 首 · %3 / %4 行").arg(finished).arg(m_translationStats.size()).arg(translated).arg(total);
        if(finished==m_translationStats.size())text=failed||cancelled?QString("翻译结束 · %1 失败 · %2 已取消").arg(failed).arg(cancelled):QString("翻译完成 · %1 首 · %2 行").arg(finished).arg(total);
    }
    m_translationProgressLabel->setText(text);m_translationProgress->setAccessibleName("歌词翻译进度");m_translationProgress->setAccessibleDescription(text);
    m_translationProgress->setToolTip(text);
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
void LyricsLookup::controls(){
    const auto rows=batchRows();const bool chosen=!rows.isEmpty();const int row=m_tracks->currentRow();const bool single=!m_multi&&row>=0&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected();
    m_start->setEnabled(!m_running);m_stop->setEnabled(m_running);m_replace->setEnabled(!m_running);
    for(auto *widget:QList<QWidget *>{m_searchTitle,m_searchArtist,m_fragment,m_manualSearch,m_pasteButton,m_importButton,m_sourceFilter,m_syncedOnly})widget->setEnabled(single);
    m_candidatePanel->setEnabled(!m_multi);if(m_multi)m_apply->setEnabled(false);m_continue->setEnabled(single);if(auto *confirm=findChild<QPushButton *>("lyricsConfirmInstrumental"))confirm->setEnabled(single);
    m_mark->setEnabled(chosen);m_offset->setEnabled(chosen);m_saveOffset->setEnabled(chosen);m_allButton->setEnabled(m_multi);
    setProperty("parallelTracks",m_trackLimit);setProperty("parallelQueries",m_requestLimit);setProperty("selectedLyricsTracks",rows.size());
}
QString LyricsLookup::reuseKey(const Track &t) const {return normalized(t.title)+"|"+normalized(t.artist);}
void LyricsLookup::remember(qint64 id){const auto t=m_library->track(id);if(!t.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(t.lyrics))){auto &ids=m_reuse[reuseKey(t)];if(!ids.contains(id))ids<<id;}}
void LyricsLookup::start(){
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
    auto *item=m_tracks->item(row);item->setData(StateRole,state);item->setData(StatusRole,text);item->setToolTip(item->text()+"\n"+text);item->setData(Qt::AccessibleTextRole,item->text()+" · "+text);if(m_incomplete&&m_incomplete->isChecked())updateFilter();
}
void LyricsLookup::updateFilter(){
    bool any=false;
    for(int r=0;r<m_tracks->count();++r){
        auto *item=m_tracks->item(r);const qint64 id=m_ids.value(r);
        const bool hideMissing=m_missingOnly&&m_missingOnly->isChecked()&&!m_missingAtOpen.contains(id)&&!m_pinnedIds.contains(id);
        const bool hideDone=m_incomplete&&m_incomplete->isChecked()&&rowComplete(item);
        item->setHidden(hideMissing||hideDone);
        if(item->isHidden()&&item->isSelected())item->setSelected(false);
        if(!item->isHidden())any=true;
    }
    if(m_emptyHint)m_emptyHint->setVisible(!any&&m_tracks->count()>0);
    if(m_tracks->currentItem()&&m_tracks->currentItem()->isHidden()){m_tracks->clearSelection();m_tracks->setCurrentRow(-1);selectTrack();}
}
void LyricsLookup::finishJob(const JobPtr &j,const QString &status){
    if(!live(j))return;const int row=j->row;cancelJob(j->original.id);setRowStatus(row,status);if(m_tracks->currentRow()==row)selectTrack();const int generation=m_generation;QTimer::singleShot(0,this,[this,generation]{if(generation==m_generation)pumpTracks();});
}
bool LyricsLookup::store(const Track &t,const QByteArray &data,bool force){const auto now=m_library->track(t.id);if(!now.id||now.hash!=t.hash)return false;if(!force&&!now.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(now.lyrics)))return false;const bool ok=m_library->saveLyrics(t.id,data);if(ok)remember(t.id);return ok;}
void LyricsLookup::lookup(const JobPtr &j){
    cancelTrackTranslation(j->original.id);
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
    stop();
    clearTransientCooldowns();
    resetTranslationProgress();m_nextRow=m_ids.size();const auto t=m_library->track(m_ids[row]);if(!t.id)return;auto j=std::make_shared<Job>();j->row=row;j->original=t;j->match=searchTrack(t);j->match.title=cleanTitle(m_searchTitle->text());j->match.artist=m_searchArtist->text().trimmed();j->manual=true;j->generation=m_generation;m_jobs[t.id]=j;m_running=true;controls();rebuildCandidates();beginOnline(j);
}
void LyricsLookup::selectTrack(){
    const int row=m_tracks->currentRow();const bool chosen=row>=0&&row<m_ids.size()&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected();m_searchTitle->setEnabled(chosen);m_searchArtist->setEnabled(chosen);m_fragment->setEnabled(chosen);m_manualSearch->setEnabled(chosen);m_mark->setEnabled(chosen);m_offset->setEnabled(chosen);
    if(chosen&&m_searchTitle->property("trackId").toLongLong()!=m_ids[row]){const auto t=searchTrack(m_library->track(m_ids[row]));m_searchTitle->setText(t.title);m_searchArtist->setText(knownArtist(t.artist)?t.artist:QString());m_fragment->clear();m_offset->setValue(0);m_searchTitle->setProperty("trackId",m_ids[row]);}
    const bool pure=chosen&&m_tracks->item(row)->data(StatusRole).toString().contains("需确认纯音乐");m_instrumentHint->setText("有纯音乐线索，请确认；没有歌词本身不能证明是纯音乐。");m_instrumentHint->setVisible(pure);m_continue->setVisible(pure);findChild<QPushButton *>("lyricsConfirmInstrumental")->setVisible(!m_multi&&pure);rebuildCandidates();controls();
}
void LyricsLookup::rebuildCandidates(){
    m_visibleRecords.clear();m_versions->clear();setPreviewLyrics({});m_apply->setEnabled(false);const int row=m_tracks->currentRow();const auto records=m_candidates.value(m_ids.value(row));const bool show=!m_multi&&row>=0&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected()&&!m_tracks->currentItem()->isHidden()&&!records.isEmpty();m_candidateTitle->setText("候选版本 · 搜索后在此选择");if(!show){updatePreview();return;}const auto t=m_library->track(m_ids[row]);m_candidateTitle->setText(t.title+" · 候选版本");
    for(int i=0;i<records.size();++i){const auto r=records[i].toObject();if(m_sourceFilter->currentIndex()>0&&r["source"].toString()!=m_sourceFilter->currentText())continue;if(m_syncedOnly->isChecked()&&r["syncedLyrics"].toString().isEmpty())continue;const double seconds=r["duration"].toDouble(-1);const auto duration=seconds>0?QString("%1:%2").arg(int(seconds)/60).arg(int(seconds)%60,2,10,QChar('0')):QString("时长未知");const auto difference=t.duration>0&&seconds>0?QString(" · 相差 %1 秒").arg(qAbs(t.duration/1000.-seconds),0,'f',1):QString();const auto title=(m_visibleRecords.isEmpty()?QString("★ 最推荐  ·  "):QString())+r["trackName"].toString();const auto details=r["source"].toString("LRCLIB")+" · "+r["artistName"].toString()+" · "+r["albumName"].toString()+" · "+duration+" · "+(r["instrumental"].toBool()?QString("纯音乐"):r["syncedLyrics"].toString().isEmpty()?QString("普通歌词"):QString("滚动歌词"))+difference;auto *item=new QListWidgetItem(title+"\n"+details,m_versions);item->setToolTip(details+"\n"+matchReason(t,r));item->setData(Qt::UserRole,m_visibleRecords.isEmpty());item->setData(Qt::UserRole+3,i);item->setSizeHint(QSize(0,76));m_visibleRecords<<i;}
    m_versions->setCurrentRow(m_visibleRecords.isEmpty()?-1:0);
}
bool LyricsLookup::saveManual(const QByteArray &data,const QString &status){
    const int row=m_tracks->currentRow();if(row<0||row>=m_ids.size())return false;const auto t=m_library->track(m_ids[row]);if(!t.id)return false;if(data.size()>4*1024*1024||Lyrics::parse(data).lines.isEmpty()){m_status->setText("歌词为空、格式无效或超过4 MB。");return false;}
    if(!t.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(t.lyrics))&&QMessageBox::question(this,"替换歌词","使用这份内容替换已有歌词？",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return false;
    cancelTrackTranslation(t.id);cancelJob(t.id);if(!store(t,withOffset(data,m_offset->value()),true)){m_status->setText("未保存："+m_library->lastError());setRowStatus(row,"未保存："+m_library->lastError());return false;}++m_saved;setRowStatus(row,status);if(wantsTranslation(data)){enqueueTranslation(t.id,row,data,status);pumpTranslations();}selectNextIncomplete(row);pumpTracks();return true;
}
void LyricsLookup::selectNextIncomplete(int after){
    if(m_multi)return;
    const int count=m_tracks->count();
    for(int offset=1;offset<=count;++offset){
        const int row=(after+offset)%count;
        if(m_tracks->item(row)->isHidden()||rowComplete(m_tracks->item(row)))continue;
        m_tracks->setCurrentRow(row);
        m_tracks->scrollToItem(m_tracks->item(row),QAbstractItemView::EnsureVisible);
        selectTrack();return;
    }
    selectTrack();
}
bool LyricsLookup::applySelected(){
    if(m_multi)return false;
    const int row=m_tracks->currentRow(),index=m_visibleRecords.value(m_versions->currentRow(),-1);if(row<0||index<0)return false;const auto t=m_library->track(m_ids[row]);const auto records=m_candidates.value(t.id);if(index>=records.size()||m_candidateHashes.value(t.id)!=t.hash){m_status->setText("歌曲文件已变化，请重新搜索歌词。");return false;}const auto r=records[index].toObject();if(r["instrumental"].toBool()){if(QMessageBox::question(this,"确认纯音乐","将此曲标记为纯音乐？",QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes)return false;return saveManual(QString("纯音乐，请欣赏").toUtf8(),"已保存纯音乐标记");}return saveManual(recordLyrics(r).toUtf8(),"已保存所选版本");
}
void LyricsLookup::pasteLyrics(){
    if(m_multi)return;
    if(m_tracks->currentRow()<0)return;QDialog dialog(this);dialog.setObjectName("lyricsPasteDialog");dialog.setWindowTitle("粘贴歌词");dialog.resize(580,440);auto *layout=new QVBoxLayout(&dialog);auto *text=new QPlainTextEdit(&dialog);text->setObjectName("lyricsPasteText");text->setPlaceholderText("粘贴 LRC 或普通歌词");const auto clip=QApplication::clipboard()->text();if(clip.contains('\n'))text->setPlainText(clip);layout->addWidget(text,1);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(dialog.exec()==QDialog::Accepted)saveManual(text->toPlainText().toUtf8(),"已保存粘贴歌词");
}
void LyricsLookup::importLyrics(){if(m_multi||m_tracks->currentRow()<0)return;const auto path=QFileDialog::getOpenFileName(this,"导入歌词",{},"歌词 (*.lrc *.txt)");if(path.isEmpty())return;const auto file=LyricsDocumentReader::readFile(path);if(!file.opened||file.tooLarge){m_status->setText("歌词文件无法读取或超过4 MB。");return;}saveManual(file.bytes,"已保存导入歌词");}
void LyricsLookup::saveOffsets(){
    const auto rows=batchRows();if(rows.isEmpty())return;const int offset=m_offset->value();stop();int saved=0,skipped=0,failed=0;
    for(int row:rows){const auto track=m_library->track(m_ids[row]);if(track.lyrics.isEmpty()){++skipped;continue;}const auto document=LyricsDocumentReader::readDocument(m_library->absolute(track.lyrics));if(!document.file.opened||document.file.tooLarge){++skipped;continue;}const auto &data=document.file.bytes;const auto &parsed=document.lyrics;if(!parsed.timed||parsed.instrumental){++skipped;continue;}
        cancelTrackTranslation(track.id);if(store(track,withOffset(data,offset),true)){++saved;setRowStatus(row,"已保存时间偏移");}else{++failed;setRowStatus(row,"未保存："+m_library->lastError());}
    }
    m_saved+=saved;selectTrack();m_status->setText(QString("时间偏移已保存 %1 首，跳过 %2 首，失败 %3 首。").arg(saved).arg(skipped).arg(failed));controls();
}
void LyricsLookup::markInstrumental(){
    if(!m_multi){saveManual(QString("纯音乐，请欣赏").toUtf8(),"已保存纯音乐标记");return;}
    const auto rows=batchRows();if(rows.isEmpty())return;int existing=0;for(int row:rows){const auto track=m_library->track(m_ids[row]);if(!track.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(track.lyrics)))++existing;}
    if(existing&&QMessageBox::question(this,"批量标记纯音乐",QString("将所选 %1 首标记为纯音乐，其中 %2 首已有歌词将被替换。是否继续？").arg(rows.size()).arg(existing),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    stop();int saved=0,failed=0;const auto data=QString("纯音乐，请欣赏").toUtf8();
    for(int row:rows){const auto track=m_library->track(m_ids[row]);cancelTrackTranslation(track.id);if(store(track,data,true)){++saved;m_candidates.remove(track.id);m_candidateHashes.remove(track.id);m_library->clearLookupCache(track.id);setRowStatus(row,"已保存纯音乐标记");}else{++failed;setRowStatus(row,"未保存："+m_library->lastError());}}
    m_saved+=saved;selectTrack();m_status->setText(QString("已标记纯音乐 %1 首，失败 %2 首。").arg(saved).arg(failed));controls();
}
bool LyricsLookup::productionHost() const {return !m_sources.isEmpty()&&m_sources.first().endpoint.host()=="lrclib.net";}
bool LyricsLookup::wantsTranslation(const QByteArray &data) const {
    if(property("skipLyricTranslation").toBool())return false;
    if(!productionHost()&&!property("lyricTranslateUrl").isValid())return false;
    if(!m_library->setting("translateLyrics",true).toBool())return false;
    const auto source=sourceLyricsForTranslation(data);
    const auto text=QString::fromUtf8(source);
    if(text.contains(QString("纯音乐"))||lyricsAreChinese(text))return false;
    return !lyricTexts(source).isEmpty();
}
void LyricsLookup::cancelTrackTranslation(qint64 id){
    if(m_browserTrack==id&&m_browserTranslation){m_browserTranslation->cancel();m_browserTrack=0;m_translationResume->hide();}
    m_translateQueue.removeAll(id);
    auto it=m_translateTasks.find(id);if(it==m_translateTasks.end())return;
    if(it->active)m_translateActive=qMax(0,m_translateActive-1);
    const int token=it->token;m_translateTasks.erase(it);
    auto &stat=m_translationStats[id];stat.finished=true;stat.cancelled=true;updateTranslationProgress();
    const auto replies=m_translateReplies;
    for(auto *reply:replies)if(reply&&reply->property("translateId").toLongLong()==id&&reply->property("translateToken").toInt()==token){reply->setProperty("translateToken",-1);reply->abort();}
}
void LyricsLookup::cancelTranslations(){
    if(m_browserTranslation)m_browserTranslation->cancel();m_browserTrack=0;if(m_translationResume)m_translationResume->hide();
    ++m_translateGeneration;QVector<int> rows;
    for(const auto &task:m_translateTasks){if(task.row<0||task.row>=m_tracks->count())continue;const auto text=m_tracks->item(task.row)->data(StatusRole).toString();if(text.startsWith("等待翻译")||text.startsWith("正在翻译")||text.startsWith("正在保存"))rows<<task.row;}
    m_translateQueue.clear();m_translateTasks.clear();m_translateActive=0;
    for(auto &stat:m_translationStats)if(!stat.finished){stat.finished=true;stat.cancelled=true;}
    updateTranslationProgress();
    const auto replies=m_translateReplies;
    for(auto *reply:replies)if(reply){reply->setProperty("translateToken",-1);reply->abort();}
    m_translateReplies.clear();
    for(int row:rows)setRowStatus(row,"已取消");
}
bool LyricsLookup::translationPending() const {return m_translateActive>0||!m_translateQueue.isEmpty()||!m_translateTasks.isEmpty();}
bool LyricsLookup::confirmStopTranslations(){
    if(!translationPending())return true;
    QMessageBox box(this);box.setObjectName("lyricsTranslateCloseConfirm");box.setIcon(QMessageBox::Warning);box.setWindowTitle("翻译任务尚未完成");
    box.setText("当前仍有歌词翻译任务尚未完成。如果现在退出，未完成的翻译将立即停止，已保存的歌词及译文不会丢失。确定要退出吗？");
    auto *keep=box.addButton("继续翻译",QMessageBox::RejectRole);keep->setObjectName("lyricsContinueTranslate");
    auto *quit=box.addButton("停止翻译并退出",QMessageBox::AcceptRole);quit->setObjectName("lyricsStopTranslateQuit");
    if(auto *button=qobject_cast<QPushButton *>(keep))box.setDefaultButton(button);
    box.exec();if(box.clickedButton()!=quit)return false;cancelTranslations();return true;
}
void LyricsLookup::translateSelected(){
    resetTranslationProgress();
    const auto rows=batchRows();if(rows.isEmpty()){m_status->setText("请先选择歌曲");return;}
    for(int row:rows){
        const auto track=m_library->track(m_ids.value(row));
        if(!track.id||track.lyrics.isEmpty()||!QFileInfo::exists(m_library->absolute(track.lyrics))){setRowStatus(row,"没有原文，无法翻译");continue;}
        const auto file=LyricsDocumentReader::readFile(m_library->absolute(track.lyrics));if(!file.opened||file.tooLarge){setRowStatus(row,"没有原文，无法翻译");continue;}
        const auto source=sourceLyricsForTranslation(file.bytes);
        if(lyricsAreChinese(QString::fromUtf8(source))){if(containsTraditional(QString::fromUtf8(source)))store(track,source,true);setRowStatus(row,"已是中文歌词，无需翻译");continue;}
        if(lyricTexts(source).isEmpty()){setRowStatus(row,"没有原文，无法翻译");continue;}
        enqueueTranslation(track.id,row,source,{});
    }
    pumpTranslations();
}
void LyricsLookup::enqueueTranslation(qint64 id,int row,const QByteArray &source,const QString &statusBase){
    cancelTrackTranslation(id);
    if(!productionHost()&&!property("lyricTranslateUrl").isValid()){setRowStatus(row,"翻译失败：当前来源不支持翻译");return;}
    const auto lines=lyricTexts(sourceLyricsForTranslation(source));if(lines.isEmpty()){setRowStatus(row,"没有原文，无法翻译");return;}
    TranslateTask task;task.id=id;task.row=row;task.token=++m_translateToken;task.generation=m_translateGeneration;task.source=sourceLyricsForTranslation(source);task.statusBase=statusBase;task.lines=lines;task.browserProvider=m_translationProvider->currentData().toString();
    for(int i=0;i<lines.size();++i)task.translated<<QString();
    const auto pair=translationLanguagePair(lines.join(' ')).split('|');task.sl=pair.value(0,"en");task.tl=pair.value(1,"zh-CN");
    m_translateTasks.insert(id,task);m_translateQueue<<id;m_translationStats[id]=TranslationProgress{int(lines.size()),0,false,false,false};updateTranslationProgress();setRowStatus(row,"等待翻译");
}
void LyricsLookup::pumpTranslations(){
    while(m_translateActive<(property("lyricTranslateUrl").isValid()?2:1)&&!m_translateQueue.isEmpty()){
        const qint64 id=m_translateQueue.takeFirst();if(!m_translateTasks.contains(id))continue;
        auto &task=m_translateTasks[id];if(task.active)continue;task.active=true;++m_translateActive;translateStep(id);
    }
}
void LyricsLookup::finishTranslation(qint64 id,bool ok,const QString &error){
    auto it=m_translateTasks.find(id);if(it==m_translateTasks.end())return;
    const int row=it->row;const auto source=it->source;const auto translated=it->translated;const auto statusBase=it->statusBase;m_translateTasks.erase(it);
    m_translateActive=qMax(0,m_translateActive-1);
    if(!ok){auto &stat=m_translationStats[id];stat.finished=stat.failed=true;updateTranslationProgress();setRowStatus(row,QString("翻译失败：")+error);pumpTranslations();return;}
    setRowStatus(row,"正在保存");
    const auto track=m_library->track(id);const auto saved=attachTranslations(source,translated);
    if(!track.id||!store(track,saved,true)){auto &stat=m_translationStats[id];stat.finished=stat.failed=true;updateTranslationProgress();setRowStatus(row,"翻译失败："+(m_library->lastError().isEmpty()?QString("未能写入歌词"):m_library->lastError()));pumpTranslations();return;}
    auto &stat=m_translationStats[id];stat.done=stat.total;stat.finished=true;updateTranslationProgress();
    setRowStatus(row,(statusBase.isEmpty()?QString("已保存歌词"):statusBase)+" · 已附加简体译文");if(m_ids.value(m_tracks->currentRow())==id)updatePreview();pumpTranslations();
}
void LyricsLookup::translateStep(qint64 id){
    auto it=m_translateTasks.find(id);if(it==m_translateTasks.end())return;
    if(it->cursor>=it->lines.size()){finishTranslation(id,true,{});return;}
    if(!property("lyricTranslateUrl").isValid()){
        m_browserTrack=id;setRowStatus(it->row,QString("正在翻译：%1/%2 行 · %3").arg(it->cursor).arg(it->lines.size()).arg(it->browserProvider=="baidu"?"百度网页":"必应网页"));
        m_browserTranslation->translate(quint64(it->token),it->browserProvider,it->lines[it->cursor],it->sl);return;
    }
    const bool lineMode=it->lineMode;const int cursor=it->cursor;int end=cursor;QStringList chunk;int chars=0;
    if(lineMode){chunk<<it->lines[cursor].left(400);end=cursor+1;}
    else while(end<it->lines.size()&&chunk.size()<12&&chars+it->lines[end].size()+1<=400){chunk<<it->lines[end];chars+=it->lines[end].size()+1;++end;}
    if(chunk.isEmpty()){chunk<<it->lines[cursor].left(400);end=cursor+1;}
    const int token=it->token;const int generation=it->generation;const QString sl=it->sl,tl=it->tl;const int row=it->row;
    setRowStatus(row,QString("正在翻译：%1/%2 行").arg(cursor).arg(it->lines.size()));
    const QString queryText=chunk.join('\n');const bool seam=property("lyricTranslateUrl").isValid();
    QUrl url;QUrlQuery query;
    if(seam){url=property("lyricTranslateUrl").toUrl();query.addQueryItem("q",queryText);query.addQueryItem("sl",sl);query.addQueryItem("tl",tl);}
    
    url.setQuery(query);QNetworkRequest req(url);req.setHeader(QNetworkRequest::UserAgentHeader,"Mozilla/5.0");req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply=m_network.get(req);reply->setProperty("translateId",id);reply->setProperty("translateToken",token);m_translateReplies.insert(reply);
    QTimer::singleShot(8000,reply,[reply]{if(!reply->isFinished())reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[=]{
        m_translateReplies.remove(reply);const auto body=reply->readAll();const bool httpOk=reply->error()==QNetworkReply::NoError;const int replyToken=reply->property("translateToken").toInt();reply->deleteLater();
        if(replyToken!=token||generation!=m_translateGeneration||!m_translateTasks.contains(id)||m_translateTasks[id].token!=token)return;
        auto &task=m_translateTasks[id];
        QStringList parts;
        if(httpOk){const auto doc=QJsonDocument::fromJson(body);if(doc.isObject()){const auto object=doc.object();const auto direct=object.contains("translatedText")?object["translatedText"].toString():object["responseData"].toObject()["translatedText"].toString();parts=direct.split('\n');}else if(doc.isArray()){QString text;const auto root=doc.array();if(!root.isEmpty())for(const auto &part:root.at(0).toArray())if(!part.toArray().isEmpty())text+=part.toArray().at(0).toString();parts=text.split('\n');}}
        bool aligned=httpOk&&parts.size()==chunk.size();
        if(aligned)for(const auto &part:parts)if(part.trimmed().isEmpty())aligned=false;
        if(aligned){for(int i=0;i<parts.size()&&cursor+i<task.translated.size();++i)task.translated[cursor+i]=parts[i].trimmed();task.cursor=end;m_translationStats[id].done=end;updateTranslationProgress();task.provider=0;if(task.lineMode&&task.cursor>=task.lineUntil)task.lineMode=false;if(task.cursor>=task.lines.size())finishTranslation(id,true,{});else translateStep(id);return;}
        
        if(!task.lineMode&&chunk.size()>1){task.lineMode=true;task.lineUntil=end;task.provider=0;translateStep(id);return;}
        finishTranslation(id,false,httpOk?QString("译文与原文行数不一致"):QString("网络异常"));
    });
}
LyricsLookup::RankResult LyricsLookup::computeRank(const ScoreSnap &snap){
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
    auto commit=[&](const RankItem &item)->bool{const auto now=m_library->track(j->original.id);if(!now.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(now.lyrics)))return false;const auto &r=item.record;const bool pure=r["instrumental"].toBool();const auto data=pure?QString("纯音乐，请欣赏").toUtf8():recordLyrics(r).toUtf8();if(!pure&&!item.credible){rejection=item.rejection;return false;}const auto status=(pure?QString("纯音乐 · 已保存标记"):r["syncedLyrics"].toString().isEmpty()?QString("已保存普通歌词"):QString("已保存滚动歌词"))+suffix+(r["coverMatch"].toBool()?QString(" · 已匹配原唱"):QString());if(!pure&&wantsTranslation(data)){if(!store(j->original,data)){j->wantAccept=false;finishJob(j,"未保存："+m_library->lastError()+suffix);return true;}++m_saved;const int row=j->row;const auto id=j->original.id;j->wantAccept=false;finishJob(j,status);enqueueTranslation(id,row,data,status);pumpTranslations();return true;}if(store(j->original,data)){++m_saved;j->wantAccept=false;finishJob(j,status);return true;}j->wantAccept=false;finishJob(j,"未保存："+m_library->lastError()+suffix);return true;};
    if(!j->manual){
        for(const auto &item:j->readyRanked){const auto &r=item.record;if(r["coverMatch"].toBool()||!item.confidentOriginal)continue;bool ambiguous=false;for(const auto &other:j->readyRanked){if(other.record["coverMatch"].toBool()||!other.confidentOriginal)continue;if(r["instrumental"].toBool()!=other.record["instrumental"].toBool()){ambiguous=true;break;}if(recordLyrics(other.record)!=recordLyrics(r)&&normalized(r["albumName"].toString())!=normalized(other.record["albumName"].toString())&&normalized(j->original.album)!=normalized(r["albumName"].toString())){ambiguous=true;break;}}if(ambiguous)break;if(commit(item))return;}
        for(const auto &item:j->readyRanked){if(!item.record["coverMatch"].toBool()||!item.confidentCover)continue;if(!item.credible){rejection=item.rejection;continue;}if(commit(item))return;}
    }
    j->wantAccept=false;QString status=QString("需选择版本（%1 个）").arg(ranked.size());if(instrumental)status+=" · 需确认纯音乐";if(!rejection.isEmpty())status+=" · "+rejection;if(!j->failures.isEmpty())status+=" · "+j->failures.join("；");finishJob(j,status+suffix);
}
} // namespace relaxer

