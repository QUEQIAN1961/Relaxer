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
std::atomic_bool &lyricComputeCancel(){return LyricsSearchCoordinator::computeCancel();}
QThreadPool *lyricPool(){return LyricsSearchCoordinator::computePool();}
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
double LyricsLookup::matchScore(const Track &track,const QJsonObject &record){return LyricsSearchCoordinator::score(track,record);}
void LyricsLookup::resetMatchScoreCount(){LyricsSearchCoordinator::resetMatchScoreCount();}
qint64 LyricsLookup::matchScoreCount(){return LyricsSearchCoordinator::matchScoreCount();}
void LyricsLookup::resetLyricRankRuns(){LyricsSearchCoordinator::resetRankRuns();}
qint64 LyricsLookup::lyricRankRuns(){return LyricsSearchCoordinator::rankRuns();}
int LyricsLookup::lyricComputeThreadCount(){const int cores=QThread::idealThreadCount();return std::max(2,cores<1?2:cores);}
QVector<QJsonObject> LyricsLookup::rankCandidates(const Track &query,const QVector<QJsonObject> &usable){return LyricsSearchCoordinator::rankCandidates(query,usable);}


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
    :QDialog(parent),m_library(library),m_ids(std::move(ids)),m_aiRuntime(aiRuntime),m_pinExplicit(pinExplicit){
    installSmoothScrolling(qApp);
    m_search=new LyricsSearchCoordinator(m_library,this,endpoint,backupEndpoint);
    m_search->loadCached(m_ids);
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
        setRowStatus(i,has?QString("已有本地歌词"):!m_search->candidates(t.id).records.isEmpty()?QString("需选择版本（%1 个） · 使用上次结果").arg(cached.records.size()):QString("等待查找"));
    }
    m_emptyHint=new QLabel("当前没有缺失歌词的歌曲，可取消勾选以查看全部歌曲。",left);m_emptyHint->setObjectName("lyricsMissingEmpty");m_emptyHint->setWordWrap(true);m_emptyHint->hide();leftLayout->addWidget(m_emptyHint);
    auto *timer=new QTimer(this);timer->setInterval(40);connect(timer,&QTimer::timeout,this,[this,delegate]{if(!m_search->summary().running)return;delegate->angle=(delegate->angle+12)%360;m_tracks->setProperty("spinnerAngle",delegate->angle);QRegion region;for(int i=0;i<m_tracks->count();++i){auto *item=m_tracks->item(i);if(!item||item->isHidden()||item->data(StateRole).toInt()!=Searching)continue;region+=m_tracks->visualItemRect(item);}if(!region.isEmpty())m_tracks->viewport()->update(region);});timer->start();
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
    auto *filters=new QHBoxLayout;m_sourceFilter=new RoundedComboBox(right);m_sourceFilter->setObjectName("lyricsSourceFilter");m_sourceFilter->addItem("全部来源");for(const auto &s:m_search->sources())m_sourceFilter->addItem(s.label);m_syncedOnly=new QCheckBox("只看滚动歌词",right);m_syncedOnly->setObjectName("lyricsSyncedOnly");filters->addWidget(m_sourceFilter);filters->addWidget(m_syncedOnly);filters->addStretch();rightLayout->addLayout(filters);
    connect(m_sourceFilter,&QComboBox::currentIndexChanged,this,[this](int index){if(index>0&&m_search->sources()[index-1].kind==1)m_syncedOnly->setChecked(false);rebuildCandidates();});connect(m_syncedOnly,&QCheckBox::toggled,this,&LyricsLookup::rebuildCandidates);
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
    connect(m_tracks,&QListWidget::itemSelectionChanged,this,&LyricsLookup::trackSelectionChanged);connect(m_incomplete,&QPushButton::toggled,this,[this](bool on){m_incomplete->setText(on?"✓ 只显示未完成":"只显示未完成");if(on&&m_missingTranslationOnly){QSignalBlocker block(m_missingTranslationOnly);m_missingTranslationOnly->setChecked(false);}updateFilter();});
    connect(m_missingOnly,&QCheckBox::toggled,this,[this](bool on){if(on){QSignalBlocker block(m_missingTranslationOnly);m_missingTranslationOnly->setChecked(false);}updateFilter();});
    connect(m_missingTranslationOnly,&QCheckBox::toggled,this,[this](bool on){
        if(on){QSignalBlocker a(m_missingOnly),b(m_incomplete);m_missingOnly->setChecked(false);m_incomplete->setChecked(false);m_incomplete->setText("只显示未完成");}
        if(on&&!m_translationAssessments.isEmpty())requestTranslationCacheAudit();
        updateFilter();
    });
    connect(m_refreshTranslationFilter,&QPushButton::clicked,this,[this]{
        m_translationAssessments.clear();
        const auto id=trackIdAt(m_tracks->currentRow());
        if(id){m_lyricsSnapshots.insert(id,m_library->lyricsSaveSnapshot(id));m_snapshotConflict=false;}
        updateFilter();updatePreview();
    });
    connect(m_offset,qOverload<int>(&QSpinBox::valueChanged),this,[this]{updatePreview();});
    connect(m_versions,&QListWidget::currentRowChanged,this,[this](int row){const int track=m_tracks->currentRow();const auto records=m_search->candidates(trackIdAt(track)).records;const int index=m_visibleRecords.value(row,-1);if(track<0||row<0||index<0||index>=records.size()){m_apply->setEnabled(false);updatePreview();return;}const auto r=records[index].toObject();updatePreview();m_candidateTitle->setText("候选版本 · "+matchReason(m_library->track(trackIdAt(track)),r));updateApplyButton();});
    connect(m_apply,&QPushButton::clicked,this,[this]{applySelected();});connect(close,&QPushButton::clicked,this,[this]{if(!m_multi&&m_apply->isEnabled()&&!applySelected())return;if(!confirmStopTranslations())return;stop();accept();});m_translation=new TranslationCoordinator(m_library,m_aiRuntime,this);connect(m_translation,&TranslationCoordinator::taskChanged,this,[this](const TranslationSummary &value){if(value.taskId==m_translationTaskId)renderAiTranslation();});m_missingOnly->setChecked(true);updateFilter();selectTrack();m_library->deferChanges(true);
    connect(m_search,&LyricsSearchCoordinator::eventOccurred,this,[this](const LyricsSearchEvent &event){
        if(event.generation!=m_search->summary().generation)return;
        const int row=rowForTrack(event.trackId);if(row<0||event.status.isEmpty())return;
        setRowStatus(row,event.status);
        if(event.state==LyricsSearchState::Saved){
            m_translationAssessments.remove(event.trackId);
            m_lyricsSnapshots.insert(event.trackId,m_library->lyricsSaveSnapshot(event.trackId));
        }
        if(m_tracks->currentRow()==row)selectTrack();
    });
    connect(m_search,&LyricsSearchCoordinator::candidatesChanged,this,[this](const LyricsCandidateSnapshot &snapshot,quint64 generation){
        if(generation!=m_search->summary().generation||snapshot.revision!=m_search->candidates(snapshot.trackId).revision)return;
        if(trackIdAt(m_tracks->currentRow())==snapshot.trackId)rebuildCandidates();
    });
    connect(m_search,&LyricsSearchCoordinator::summaryChanged,this,[this](const LyricsSearchSummary &value){
        if(value.generation!=m_search->summary().generation)return;
        controls();m_status->setText(value.status);
    });
}
LyricsLookup::~LyricsLookup(){if(m_translationAuditCancel)m_translationAuditCancel->store(true);if(m_translationAuditDebounce)m_translationAuditDebounce->stop();cancelAiTranslation();stop();if(m_library)m_library->deferChanges(false);}
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
    const auto value=aiSummary();
    QStringList original,translated;
    for(int i=0;i<value.prepared.lines.size();++i){
        const auto &line=value.prepared.lines.at(i);
        const auto prefix=line.timestampPrefixes.isEmpty()?QString():line.timestampPrefixes.first()+QStringLiteral(" ");
        original<<prefix+line.text;
        translated<<prefix+value.translations.value(i);
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
    if(hasAiPreview()&&!m_multi&&row>=0&&trackIdAt(row)==aiTrackId()&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected()&&!m_tracks->currentItem()->isHidden()){
        setAiTranslationPreview();return;
    }
    if(m_multi||row<0||!m_tracks->currentItem()||!m_tracks->currentItem()->isSelected()||m_tracks->currentItem()->isHidden()){setPreviewLyrics({});return;}
    const auto track=m_library->track(trackIdAt(row));ensureLyricsSnapshot(track.id);const auto records=m_search->candidates(track.id).records;
    const int index=m_visibleRecords.value(m_versions->currentRow(),-1);QByteArray data;
    if(index>=0&&index<records.size())data=replacementContent(track,records[index].toObject()).bytes;
    else if(!track.lyrics.isEmpty()){
        const auto saved=LyricsDocumentReader::readFile(m_library->absolute(track.lyrics));
        if(saved.opened&&saved.complete&&!saved.tooLarge)data=saved.bytes;
    }
    setPreviewLyrics(data);
}
TranslationSummary LyricsLookup::aiSummary() const{return m_translation?m_translation->summary(m_translationTaskId):TranslationSummary{};}
quint64 LyricsLookup::aiTaskId() const{const auto value=aiSummary();return value.pending()?value.taskId:0;}
qint64 LyricsLookup::aiTrackId() const{return aiSummary().trackId;}
bool LyricsLookup::hasAiPreview() const{return aiSummary().hasPreview();}
void LyricsLookup::clearAiTranslationPreview(){
    const bool shown=hasAiPreview();
    const auto id=m_translationTaskId;m_translationTaskId=0;
    if(m_translation&&id){m_translation->discard(id);m_translation->forget(id);}
    if(m_aiSaveTranslation)m_aiSaveTranslation->setEnabled(false);
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
    const auto value=aiSummary();const bool hadTranslation=value.pending()||value.hasPreview();
    clearAiTranslationPreview();
    if(m_translate)m_translate->setEnabled(true);
    if(m_aiCancelTranslation)m_aiCancelTranslation->setEnabled(false);
    if(value.pending())setRowStatus(rowForTrack(value.trackId),QStringLiteral("已取消"));
    if(hadTranslation)setTranslationProgress(QStringLiteral("已取消"),reason.isEmpty()?QStringLiteral("本地翻译任务已取消；远端可能继续处理或计费。"):reason);
    if(!reason.isEmpty()&&m_status)m_status->setText(reason);
    if(hadTranslation&&m_missingTranslationOnly&&m_missingTranslationOnly->isChecked())updateFilter();
}
void LyricsLookup::cancelAiTranslationForTrack(qint64 id){
    if(id>0&&(aiTrackId()==id)&&(aiTaskId()||hasAiPreview()))
        cancelAiTranslation(QStringLiteral("歌词已变更，先前的 AI 翻译结果已丢弃。"));
}
bool LyricsLookup::translationPending() const{return aiTaskId()!=0;}
qint64 LyricsLookup::trackIdAt(int row) const {
    const auto *item=m_tracks->item(row);return item?item->data(Qt::UserRole).toLongLong():0;
}
int LyricsLookup::rowForTrack(qint64 id) const {
    for(int row=0;row<m_tracks->count();++row)if(trackIdAt(row)==id)return row;
    return -1;
}
void LyricsLookup::restoreTranslationSelection() {
    const auto owner=aiTrackId();const int row=rowForTrack(owner);
    if(row<0||!m_library->track(owner).id){
        cancelAiTranslation(QStringLiteral("翻译所属歌曲已失效，无法保存；未写入其他歌曲。"));return;
    }
    QSignalBlocker block(m_tracks);
    m_tracks->clearSelection();m_tracks->item(row)->setHidden(false);
    m_tracks->setCurrentRow(row,QItemSelectionModel::ClearAndSelect);
}
void LyricsLookup::trackSelectionChanged() {
    if(m_translationSwitchConfirmOpen){restoreTranslationSelection();return;}
    const auto selected=trackIdAt(m_tracks->currentRow());
    if(hasAiPreview()&&(selected!=aiTrackId()||!m_tracks->currentItem()||!m_tracks->currentItem()->isSelected())){
        // Filtering/empty selections are refreshes, not consent to discard a paid result.
        if(m_filterUpdating||!selected||!m_tracks->currentItem()->isSelected()){
            restoreTranslationSelection();selectTrack();return;
        }
        const auto owner=aiTrackId();const auto task=m_translationTaskId;
        QScopedValueRollback<bool> confirming(m_translationSwitchConfirmOpen,true);
        restoreTranslationSelection();
        QMessageBox box(this);box.setObjectName("lyricsUnsavedTranslationSwitchConfirm");
        box.setIcon(QMessageBox::Warning);box.setWindowTitle("未保存的翻译");
        box.setText("当前歌曲的 AI 翻译已经完成，但译文尚未保存。切换歌曲将丢弃本次翻译结果，是否确定切换？");
        auto *keep=box.addButton("取消切换",QMessageBox::RejectRole);keep->setObjectName("lyricsKeepTranslationTrack");
        auto *discard=box.addButton("丢弃译文并切换",QMessageBox::AcceptRole);discard->setObjectName("lyricsDiscardTranslationSwitch");
        box.setDefaultButton(keep);box.setEscapeButton(keep);box.exec();
        // A nested refresh/deletion must never discard a different task or use a stale row.
        if(box.clickedButton()==discard&&m_translationTaskId==task&&aiTrackId()==owner){
            cancelAiTranslation(QStringLiteral("已明确放弃未保存的 AI 译文。"));
            const int target=rowForTrack(selected);
            if(target>=0&&m_library->track(selected).id){QSignalBlocker block(m_tracks);m_tracks->setCurrentRow(target,QItemSelectionModel::ClearAndSelect);}
        }else if(hasAiPreview())restoreTranslationSelection();
    }else if(aiTaskId()&&selected!=aiTrackId()){
        cancelAiTranslation(QStringLiteral("已切换歌曲，旧翻译请求不会写入新歌曲。"));
    }
    selectTrack();
}
bool LyricsLookup::confirmStopTranslations(){
    if(m_translationCloseConfirmOpen||m_translationSwitchConfirmOpen)return false;
    if(!translationPending()&&!hasAiPreview())return true;
    QScopedValueRollback<bool> confirming(m_translationCloseConfirmOpen,true);
    if(hasAiPreview()){
        QMessageBox box(this);box.setObjectName("lyricsUnsavedTranslationCloseConfirm");
        box.setIcon(QMessageBox::Warning);box.setWindowTitle("未保存的翻译");
        box.setText("当前 AI 翻译已经完成，但译文尚未保存。关闭窗口将丢弃本次翻译结果，是否确定关闭？");
        auto *keep=box.addButton("取消",QMessageBox::RejectRole);keep->setObjectName("lyricsKeepTranslationPreview");
        auto *discard=box.addButton("丢弃译文并关闭",QMessageBox::AcceptRole);discard->setObjectName("lyricsDiscardTranslationQuit");
        box.setDefaultButton(qobject_cast<QPushButton *>(keep));box.setEscapeButton(keep);
        box.exec();if(box.clickedButton()!=discard)return false;
        cancelAiTranslation(QStringLiteral("已明确放弃未保存的 AI 译文。"));return true;
    }
    QMessageBox box(this);box.setObjectName("lyricsTranslateCloseConfirm");box.setIcon(QMessageBox::Warning);box.setWindowTitle("翻译任务尚未完成");
    box.setText("关闭将停止本地翻译任务。DeepSeek 可能已经收到整首歌词，并可能继续处理或计费；本地取消不能撤销供应商已处理的请求。尚未确认保存的译文不会写入歌词文件。是否关闭？");
    auto *keep=box.addButton("继续等待",QMessageBox::RejectRole);keep->setObjectName("lyricsContinueTranslate");
    auto *quit=box.addButton("停止本地任务并关闭",QMessageBox::AcceptRole);quit->setObjectName("lyricsStopTranslateQuit");
    if(auto *button=qobject_cast<QPushButton *>(keep))box.setDefaultButton(button);
    box.exec();if(box.clickedButton()!=quit)return false;
    cancelAiTranslation(QStringLiteral("本地翻译任务已取消；DeepSeek 可能已收到请求并继续处理或计费。"));return true;
}
void LyricsLookup::renderAiTranslation(){
    const auto value=aiSummary();if(!value.taskId)return;
    m_translate->setEnabled(!value.pending());m_aiCancelTranslation->setEnabled(value.pending());
    m_aiSaveTranslation->setEnabled(value.hasPreview());
    if(value.state==TranslationState::WaitingForConsent||value.state==TranslationState::Queued)return;
    if(value.hasPreview()){
        setAiTranslationPreview();setRowStatus(rowForTrack(value.trackId),QStringLiteral("翻译完成，等待你预览并保存"));
    }else if(value.state==TranslationState::Failed||value.state==TranslationState::Invalidated){
        setRowStatus(rowForTrack(value.trackId),value.details);
    }
    setTranslationProgress(value.status,value.details,value.pending()?-1:(value.hasPreview()?1:0));
}
void LyricsLookup::saveAiTranslation(){
    const auto value=aiSummary();if(!value.hasPreview())return;
    // Explicit navigation owns discard; rows never decide the destination of save.
    QString error;
    if(!m_translation->save(value.taskId,&error)){
        setRowStatus(rowForTrack(value.trackId),error);renderAiTranslation();return;
    }
    const auto savedTrackId=value.trackId;
    m_library->clearLookupCache(savedTrackId);remember(savedTrackId);m_translationAssessments.remove(savedTrackId);m_lyricsSnapshots.insert(savedTrackId,m_library->lyricsSaveSnapshot(savedTrackId));
    const auto message=QStringLiteral("双语歌词已保存，原文与时间戳保持不变。")+
        (value.targetLanguage==QStringLiteral("zh-CN")?QString():QStringLiteral(" 目标语言：")+value.targetLanguage);
    clearAiTranslationPreview();
    setRowStatus(rowForTrack(savedTrackId),QStringLiteral("已保存 AI 双语歌词"));
    setTranslationProgress(QStringLiteral("已保存"),message,1);
    updatePreview();updateFilter();
}
void LyricsLookup::closeEvent(QCloseEvent *event){if(!confirmStopTranslations()){event->ignore();return;}QDialog::closeEvent(event);}
void LyricsLookup::reject(){if(!confirmStopTranslations())return;stop();QDialog::reject();}
void LyricsLookup::done(int result){if(!confirmStopTranslations())return;stop();QDialog::done(result);}



void LyricsLookup::stop(){
    const auto cancelled=m_search->stop();
    for(const auto &event:cancelled)setRowStatus(rowForTrack(event.trackId),event.status);
    controls();
}
QVector<int> LyricsLookup::batchRows() const {QVector<int> rows;for(int r=0;r<m_tracks->count();++r)if(m_tracks->item(r)->isSelected()&&!m_tracks->item(r)->isHidden())rows<<r;return rows;}
void LyricsLookup::setMultiSelect(bool on){
    if(on==m_multi)return;if(on&&hasAiPreview()){m_status->setText("请先保存当前 AI 译文，再进入多选。");return;}m_multi=on;m_tracks->setProperty("lookupMultiSelect",on);m_multiButton->setText(on?"✓ 多选":"多选");m_allButton->setVisible(on);
    if(on){m_moreWasOpen=m_moreButton->isChecked();m_tracks->setSelectionMode(QAbstractItemView::MultiSelection);m_moreButton->setChecked(true);}else{const auto rows=batchRows();const int current=rows.isEmpty()?m_tracks->currentRow():rows.first();QSignalBlocker block(m_tracks);m_tracks->setSelectionMode(QAbstractItemView::SingleSelection);m_tracks->clearSelection();if(current>=0&&!m_tracks->item(current)->isHidden())m_tracks->setCurrentRow(current);m_moreButton->setChecked(m_moreWasOpen);}
    m_moreButton->setEnabled(!on);m_morePanel->setVisible(on||m_moreButton->isChecked());m_tracks->viewport()->update();selectTrack();controls();
}
void LyricsLookup::updateApplyButton(){
    const int row=m_tracks->currentRow(),index=m_visibleRecords.value(m_versions->currentRow(),-1);
    const auto id=trackIdAt(row);const auto track=m_library->track(id);const auto records=m_search->candidates(id).records;
    const bool chosen=!m_multi&&row>=0&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected()&&!m_tracks->currentItem()->isHidden();
    const auto record=index>=0&&index<records.size()?records[index].toObject():QJsonObject();
    m_apply->setEnabled(chosen&&track.id&&m_search->candidates(id).trackHash==track.hash&&!record.isEmpty()&&
        (record["instrumental"].toBool()||!recordLyrics(record).trimmed().isEmpty()));
}
void LyricsLookup::controls(){
    const auto rows=batchRows();const bool chosen=!rows.isEmpty();const int row=m_tracks->currentRow();const bool single=!m_multi&&row>=0&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected();
    m_start->setEnabled(!m_search->summary().running);m_stop->setEnabled(m_search->summary().running);m_replace->setEnabled(!m_search->summary().running);
    for(auto *widget:QList<QWidget *>{m_searchTitle,m_searchArtist,m_fragment,m_manualSearch,m_pasteButton,m_importButton,m_sourceFilter,m_syncedOnly})widget->setEnabled(single);
    m_candidatePanel->setEnabled(!m_multi);updateApplyButton();m_continue->setEnabled(single);if(auto *confirm=findChild<QPushButton *>("lyricsConfirmInstrumental"))confirm->setEnabled(single);
    m_mark->setEnabled(chosen);m_offset->setEnabled(chosen);m_saveOffset->setEnabled(chosen);m_allButton->setEnabled(m_multi);
    setProperty("parallelTracks",m_search->summary().trackLimit);setProperty("parallelQueries",m_search->summary().requestLimit);setProperty("selectedLyricsTracks",rows.size());
}

void LyricsLookup::remember(qint64 id){m_search->remember(id);}
void LyricsLookup::start(){
    if(aiTaskId()||hasAiPreview())cancelAiTranslation(QStringLiteral("歌词查找开始，未保存的 AI 翻译已取消。"));
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
    stop();
    QVector<LyricsSearchRequest> requests;
    for(int row:targets){
        const auto t=m_library->track(trackIdAt(row));if(!t.id)continue;
        ensureLyricsSnapshot(t.id);LyricsSearchRequest input;input.original=t;input.match=searchTrack(t);
        input.replace=m_replace->isChecked();input.requireTimed=property("requireTimedLyrics").toBool();
        input.saveSnapshot=m_lyricsSnapshots.value(t.id);requests.append(input);setRowStatus(row,"等待查找");
    }
    selectTrack();m_search->start(requests,searchOptions());controls();
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
        const auto track=m_library->track(trackIdAt(row));
        if(!track.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(track.lyrics)))state=Complete;
        else if(m_search->candidates(track.id).trackHash==track.hash&&!m_search->candidates(track.id).records.isEmpty())state=NeedsChoice;
    }
    if(state==Waiting&&!m_search->candidates(trackIdAt(row)).records.isEmpty()&&text.startsWith("已停止"))state=NeedsChoice;
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
        const auto id=trackIdAt(row);
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
        if(row<0||row>=m_ids.size()||trackIdAt(row)!=id)continue;
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
    ++m_filterMetrics.rowVisits;auto *item=m_tracks->item(row);const auto id=trackIdAt(row);
    const bool translationOnly=m_missingTranslationOnly&&m_missingTranslationOnly->isChecked();
    const bool hideMissing=m_missingOnly&&m_missingOnly->isChecked()&&!m_missingAtOpen.contains(id)&&!m_pinnedIds.contains(id);
    const bool hideDone=m_incomplete&&m_incomplete->isChecked()&&rowComplete(item);
    const auto assessment=translationOnly?translationAssessment(m_library->track(id)):ai::lyrics::TranslationAssessment{};
    const bool hideTranslation=translationOnly&&!assessment.missing();
    const auto badge=translationOnly&&assessment.missing()?(assessment.need==ai::lyrics::TranslationNeed::Review?QStringLiteral("待确认"):QStringLiteral("待翻译")):QString();
    item->setData(TranslationFilterRole,badge);
    const auto details=item->data(StatusRole).toString()+(translationOnly?QStringLiteral("\n")+(badge.isEmpty()?assessment.reason:badge+QStringLiteral("：")+assessment.reason):QString());
    item->setToolTip(item->text()+QStringLiteral("\n")+details);item->setData(Qt::AccessibleTextRole,item->text()+QStringLiteral(" · ")+details);
    const bool ownsTranslation=id==aiTrackId()&&(aiTaskId()||hasAiPreview());
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







void LyricsLookup::manualSearch(){
    if(m_multi)return;
    const int row=m_tracks->currentRow();if(row<0||row>=m_ids.size())return;
    if(m_searchTitle->text().trimmed().isEmpty()){m_status->setText("请输入用于搜索的歌曲名称。");return;}
    if(aiTaskId()||hasAiPreview())cancelAiTranslation(QStringLiteral("歌词搜索开始，未保存的 AI 翻译已取消。"));
    stop();resetTranslationProgress();const auto t=m_library->track(trackIdAt(row));if(!t.id)return;
    ensureLyricsSnapshot(t.id);LyricsSearchRequest input;input.original=t;input.match=searchTrack(t);
    input.match.title=cleanTitle(m_searchTitle->text());input.match.artist=m_searchArtist->text().trimmed();input.manual=true;
    input.fragment=m_fragment->text().trimmed();input.replace=m_replace->isChecked();
    input.requireTimed=property("requireTimedLyrics").toBool();input.saveSnapshot=m_lyricsSnapshots.value(t.id);
    m_search->start({input},searchOptions());controls();rebuildCandidates();
}
void LyricsLookup::selectTrack(){
    if(hasAiPreview()&&(trackIdAt(m_tracks->currentRow())!=aiTrackId()||!m_tracks->currentItem()||!m_tracks->currentItem()->isSelected()))restoreTranslationSelection();
    const int row=m_tracks->currentRow();const qint64 selected=trackIdAt(row);
    if((m_multi||selected!=aiTrackId())&&aiTaskId())cancelAiTranslation(QStringLiteral("歌曲选择已变化，旧翻译不会写入其他歌曲。"));
    const bool chosen=row>=0&&row<m_ids.size()&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected();m_searchTitle->setEnabled(chosen);m_searchArtist->setEnabled(chosen);m_fragment->setEnabled(chosen);m_manualSearch->setEnabled(chosen);m_mark->setEnabled(chosen);m_offset->setEnabled(chosen);
    if(chosen){ensureLyricsSnapshot(selected);m_translationAssessments.remove(selected);}
    if(chosen&&m_searchTitle->property("trackId").toLongLong()!=trackIdAt(row)){const auto t=searchTrack(m_library->track(trackIdAt(row)));m_searchTitle->setText(t.title);m_searchArtist->setText(knownArtist(t.artist)?t.artist:QString());m_fragment->clear();m_offset->setValue(0);m_searchTitle->setProperty("trackId",trackIdAt(row));}
    const bool pure=chosen&&m_tracks->item(row)->data(StatusRole).toString().contains("需确认纯音乐");m_instrumentHint->setText("有纯音乐线索，请确认；没有歌词本身不能证明是纯音乐。");m_instrumentHint->setVisible(pure);m_continue->setVisible(pure);findChild<QPushButton *>("lyricsConfirmInstrumental")->setVisible(!m_multi&&pure);rebuildCandidates();controls();
}
void LyricsLookup::rebuildCandidates(){
    const auto previousTrack=m_versions->property("trackId").toLongLong();const auto previousIdentity=m_versions->currentItem()?m_versions->currentItem()->data(Qt::UserRole+5).toString():QString();
    m_visibleRecords.clear();m_versions->clear();const int row=m_tracks->currentRow();m_versions->setProperty("trackId",trackIdAt(row));if(hasAiPreview()&&row>=0&&trackIdAt(row)==aiTrackId())setAiTranslationPreview();else setPreviewLyrics({});m_apply->setEnabled(false);const auto records=m_search->candidates(trackIdAt(row)).records;const bool show=!m_multi&&row>=0&&m_tracks->currentItem()&&m_tracks->currentItem()->isSelected()&&!m_tracks->currentItem()->isHidden()&&!records.isEmpty();m_candidateTitle->setText("候选版本 · 搜索后在此选择");if(!show){updatePreview();return;}const auto t=m_library->track(trackIdAt(row));m_candidateTitle->setText(t.title+" · 候选版本");
    for(int i=0;i<records.size();++i){const auto r=records[i].toObject();if(m_sourceFilter->currentIndex()>0&&r["source"].toString()!=m_sourceFilter->currentText())continue;if(m_syncedOnly->isChecked()&&r["syncedLyrics"].toString().isEmpty())continue;const double seconds=r["duration"].toDouble(-1);const auto duration=seconds>0?QString("%1:%2").arg(int(seconds)/60).arg(int(seconds)%60,2,10,QChar('0')):QString("时长未知");const auto difference=t.duration>0&&seconds>0?QString(" · 相差 %1 秒").arg(qAbs(t.duration/1000.-seconds),0,'f',1):QString();const auto title=(m_visibleRecords.isEmpty()?QString("★ 最推荐  ·  "):QString())+r["trackName"].toString();const auto details=r["source"].toString("LRCLIB")+" · "+r["artistName"].toString()+" · "+r["albumName"].toString()+" · "+duration+" · "+(r["instrumental"].toBool()?QString("纯音乐"):r["syncedLyrics"].toString().isEmpty()?QString("普通歌词"):QString("滚动歌词"))+difference;auto *item=new QListWidgetItem(title+"\n"+details,m_versions);item->setToolTip(details+"\n"+matchReason(t,r));item->setData(Qt::UserRole,m_visibleRecords.isEmpty());item->setData(Qt::UserRole+3,i);item->setData(Qt::UserRole+5,LyricsSearchCoordinator::candidateIdentity(r));item->setSizeHint(QSize(0,76));m_visibleRecords<<i;}
    int previous=-1;if(previousTrack==t.id)for(int n=0;n<m_versions->count();++n)if(m_versions->item(n)->data(Qt::UserRole+5).toString()==previousIdentity){previous=n;break;}
    m_versions->setCurrentRow(m_visibleRecords.isEmpty()?-1:qMax(0,previous));
}
bool LyricsLookup::saveManual(const QByteArray &data,const QString &status,bool preserveText){
    const int row=m_tracks->currentRow();if(row<0||row>=m_ids.size())return false;const auto t=m_library->track(trackIdAt(row));if(!t.id)return false;if(data.size()>4*1024*1024||Lyrics::parse(data).lines.isEmpty()){m_status->setText("歌词为空、格式无效或超过4 MB。");return false;}
    if(!checkLyricsSnapshot(t.id)){m_status->setText(m_storeError);setRowStatus(row,m_storeError);return false;}
    if((!t.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(t.lyrics)))||(aiTrackId()==t.id&&(aiTaskId()||hasAiPreview()))){
        QMessageBox confirm(QMessageBox::Question,QStringLiteral("替换歌词"),preserveText?QStringLiteral("该版本的原文与时间轴一致，将保留本地已有译文。是否使用？"):QStringLiteral("使用这份内容替换已有歌词？如果存在译文，也会一起替换，不会自动套用到新版本。"),QMessageBox::Yes|QMessageBox::No,this);
        confirm.setObjectName("lyricsReplaceConfirm");confirm.setDefaultButton(QMessageBox::No);
        if(aiTaskId()||hasAiPreview())confirm.setInformativeText(QStringLiteral("确认替换后，当前翻译任务或未保存的 AI 结果将作废。"));
        if(confirm.exec()!=QMessageBox::Yes)return false;
    }
    m_search->cancelTrack(t.id);if(!store(t,withOffset(data,m_offset->value()),true,preserveText)){m_status->setText("未保存："+m_storeError);setRowStatus(row,"未保存："+m_storeError);return false;}cancelAiTranslationForTrack(t.id);m_search->noteManualSave();setRowStatus(row,status);updateFilter();selectNextIncomplete(row);controls();return true;
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
    const int row=m_tracks->currentRow(),index=m_visibleRecords.value(m_versions->currentRow(),-1);if(row<0||index<0)return false;const auto t=m_library->track(trackIdAt(row));const auto records=m_search->candidates(t.id).records;if(index>=records.size()||m_search->candidates(t.id).trackHash!=t.hash){m_status->setText("歌曲文件已变化，请重新搜索歌词。");return false;}const auto r=records[index].toObject();if(r["instrumental"].toBool()){if(QMessageBox::question(this,"确认纯音乐","将此曲标记为纯音乐？",QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes)return false;return saveManual(QString("纯音乐，请欣赏").toUtf8(),"已保存纯音乐标记");}const auto plan=replacementContent(t,r);return saveManual(plan.bytes,"已保存所选版本",plan.preservesTranslation);
}
void LyricsLookup::pasteLyrics(){
    if(m_multi)return;
    if(m_tracks->currentRow()<0)return;QDialog dialog(this);dialog.setObjectName("lyricsPasteDialog");dialog.setWindowTitle("粘贴歌词");dialog.resize(580,440);auto *layout=new QVBoxLayout(&dialog);auto *text=new QPlainTextEdit(&dialog);text->setObjectName("lyricsPasteText");text->setPlaceholderText("粘贴 LRC 或普通歌词");const auto clip=QApplication::clipboard()->text();if(clip.contains('\n'))text->setPlainText(clip);layout->addWidget(text,1);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(dialog.exec()==QDialog::Accepted)saveManual(text->toPlainText().toUtf8(),"已保存粘贴歌词");
}
void LyricsLookup::importLyrics(){if(m_multi||m_tracks->currentRow()<0)return;const auto path=QFileDialog::getOpenFileName(this,"导入歌词",{},"歌词 (*.lrc *.txt)");if(path.isEmpty())return;const auto file=LyricsDocumentReader::readFile(path);if(!file.opened||file.tooLarge){m_status->setText("歌词文件无法读取或超过4 MB。");return;}saveManual(file.bytes,"已保存导入歌词");}
void LyricsLookup::saveOffsets(){
    const auto rows=batchRows();if(rows.isEmpty())return;const int offset=m_offset->value();stop();int saved=0,skipped=0,failed=0;
    for(int row:rows){const auto track=m_library->track(trackIdAt(row));if(track.lyrics.isEmpty()){++skipped;continue;}const auto document=LyricsDocumentReader::readDocument(m_library->absolute(track.lyrics));if(!document.file.opened||document.file.tooLarge){++skipped;continue;}const auto &data=document.file.bytes;const auto &parsed=document.lyrics;if(!parsed.timed||parsed.instrumental){++skipped;continue;}
        ensureLyricsSnapshot(track.id);if(track.id==aiTrackId()&&(aiTaskId()||hasAiPreview())&&QMessageBox::question(this,"替换歌词",QStringLiteral("保存偏移会替换当前歌词并清除未保存的 AI 结果，是否继续？"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes){++skipped;continue;}if(store(track,withOffset(data,offset),true)){cancelAiTranslationForTrack(track.id);++saved;setRowStatus(row,"已保存时间偏移");}else{++failed;setRowStatus(row,"未保存："+m_storeError);}
    }
    m_search->noteManualSave(saved);selectTrack();m_status->setText(QString("时间偏移已保存 %1 首，跳过 %2 首，失败 %3 首。").arg(saved).arg(skipped).arg(failed));controls();
}
void LyricsLookup::markInstrumental(){
    if(!m_multi){saveManual(QString("纯音乐，请欣赏").toUtf8(),"已保存纯音乐标记");return;}
    const auto rows=batchRows();if(rows.isEmpty())return;int existing=0;for(int row:rows){const auto track=m_library->track(trackIdAt(row));if(!track.lyrics.isEmpty()&&QFileInfo::exists(m_library->absolute(track.lyrics)))++existing;}
    if(existing&&QMessageBox::question(this,"批量标记纯音乐",QString("将所选 %1 首标记为纯音乐，其中 %2 首已有歌词将被替换。是否继续？").arg(rows.size()).arg(existing),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    stop();int saved=0,failed=0;const auto data=QString("纯音乐，请欣赏").toUtf8();
    for(int row:rows){const auto track=m_library->track(trackIdAt(row));ensureLyricsSnapshot(track.id);if(store(track,data,true)){cancelAiTranslationForTrack(track.id);++saved;m_search->clearCandidates(track.id);setRowStatus(row,"已保存纯音乐标记");}else{++failed;setRowStatus(row,"未保存："+m_storeError);}}
    m_search->noteManualSave(saved);updateFilter();selectTrack();m_status->setText(QString("已标记纯音乐 %1 首，失败 %2 首。").arg(saved).arg(failed));controls();
}

void LyricsLookup::translateSelected(){
    if(aiTaskId()||m_aiConsentPending){m_status->setText(QStringLiteral("当前整首翻译请求仍在进行中。"));return;}
    QScopedValueRollback<bool> preparing(m_aiConsentPending,true);
    if(m_multi){m_status->setText(QStringLiteral("整首 AI 翻译一次只处理一首歌，请先退出多选并选择一首歌曲。"));return;}
    const int row=m_tracks->currentRow();
    if(row<0||row>=m_ids.size()||!m_tracks->currentItem()||!m_tracks->currentItem()->isSelected()||m_tracks->currentItem()->isHidden()){
        m_status->setText(QStringLiteral("请先选择一首歌曲。"));return;
    }
    if(hasAiPreview()){
        if(QMessageBox::question(this,QStringLiteral("重新发送整首翻译"),
             QStringLiteral("当前译文预览尚未保存。再次翻译会丢弃当前预览，并发送一次新的整首歌词请求。是否继续？"),
             QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        clearAiTranslationPreview();
    }
    clearAiTranslationPreview();
    const auto selectedId=trackIdAt(row);
    QString preparationError;
    m_translationTaskId=m_translation->prepareAi(selectedId,&preparationError);
    if(!m_translationTaskId){
        setRowStatus(row,preparationError);m_status->setText(preparationError);return;
    }
    QMessageBox consent(this);consent.setObjectName("lyricsAiCloudConsent");consent.setIcon(QMessageBox::Information);
    consent.setWindowTitle(QStringLiteral("发送整首歌词到 DeepSeek"));
    consent.setText(QStringLiteral("Relaxer 将把待翻译的歌词文本、目标语言和风格提示发送至 DeepSeek 官方服务器处理。\n"
        "不会上传 MP3、封面或本地路径，也不单独发送歌名、歌手字段；已识别的署名行仅保留在本地。\n\n"
        "本次操作只发送一次 Chat Completions 请求，不会自动拆分、重试或切换模型。使用量可能产生费用；费用以供应商账单为准。\n\n"
        "取消只能停止本地任务，无法撤销 DeepSeek 已收到或处理的请求。返回译文后，只有你点击“保存双语歌词”才会写入本地。"));
    auto *decline=consent.addButton(QStringLiteral("不发送"),QMessageBox::RejectRole);decline->setObjectName("lyricsAiConsentCancel");
    auto *send=consent.addButton(QStringLiteral("发送一次整首歌词"),QMessageBox::AcceptRole);send->setObjectName("lyricsAiConsentSend");
    consent.setDefaultButton(qobject_cast<QPushButton *>(decline));consent.exec();
    if(consent.clickedButton()!=send){clearAiTranslationPreview();m_status->setText(QStringLiteral("未发送歌词。"));return;}
    // The nested consent event loop may change the frontend selection. The
    // service independently rechecks file, song and configuration before dispatch.
    if(m_multi||m_tracks->currentRow()!=row||!m_tracks->currentItem()||
       !m_tracks->currentItem()->isSelected()||m_tracks->currentItem()->isHidden()||trackIdAt(row)!=selectedId){
        clearAiTranslationPreview();m_status->setText(QStringLiteral("发送前歌曲或 AI 配置已变化；本次未发送请求。"));return;
    }
    QString error;
    if(!m_translation->start(m_translationTaskId,&error)){
        m_status->setText(error);clearAiTranslationPreview();return;
    }
    m_translate->setEnabled(false);m_aiCancelTranslation->setEnabled(true);
    setTranslationProgress(QStringLiteral("正在翻译"),QStringLiteral("已发送一次整首歌词请求，正在等待 DeepSeek 响应，可取消。"),-1);
    setRowStatus(row,QStringLiteral("正在翻译整首歌词"));
}







} // namespace relaxer


namespace relaxer {
LyricsSearchOptions LyricsLookup::searchOptions() const {
    LyricsSearchOptions options;
    if(property("lyricsSearchBudgetMs").isValid())options.budgetMs=property("lyricsSearchBudgetMs").toInt();
    if(property("lyricsRequestTimeoutMs").isValid())options.requestTimeoutMs=property("lyricsRequestTimeoutMs").toInt();
    if(property("lyricsRetryDelayMs").isValid())options.retryDelayMs=property("lyricsRetryDelayMs").toInt();
    options.scoreDelayMs=property("lyricScoreDelayMs").toInt();return options;
}
}
