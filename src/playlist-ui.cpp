#include "playlist-ui.h"
#include <QDir>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QTextLayout>
#include <QVBoxLayout>
namespace relaxer {
DescriptionPreview::DescriptionPreview(QWidget *parent):QWidget(parent) {
    setObjectName("playlistDescriptionPreview");setMinimumWidth(0);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
}
void DescriptionPreview::setText(const QString &text) {
    if(m_text==text)return;
    m_text=text;emit layoutChanged();update();
}
QStringList DescriptionPreview::linesForWidth(int width,bool *elided) const {
    QString text=m_text.isEmpty()?(property("emptyText").isValid()?property("emptyText").toString():QString("未设置")):m_text;
    text.replace("\r\n","\n").replace('\r','\n').replace('\n',QChar::LineSeparator).replace('\t',QChar(' '));
    if(m_singleLine){text.replace(QChar::LineSeparator,QChar(' '));const auto shown=fontMetrics().elidedText(text,Qt::ElideRight,qMax(1,width));if(elided)*elided=shown!=text;return {shown};}
    QTextLayout layout(text,font());QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout.setTextOption(option);
    QStringList lines;bool overflow=false;
    layout.beginLayout();
    for(int i=0;i<3;++i){auto line=layout.createLine();if(!line.isValid())break;
        line.setLineWidth(qMax(1,width));
        if(i==2){overflow=true;break;}
        QString part=text.mid(line.textStart(),line.textLength());part.remove(QChar::LineSeparator);
        lines<<part;
    }
    layout.endLayout();
    if(overflow&&!lines.isEmpty()){
        QString tail=lines.last();const QFontMetrics fm(font());
        while(!tail.isEmpty()&&fm.horizontalAdvance(tail+QChar(0x2026))>width)
            tail.truncate(characterPrefixLength(tail,qMax(0,characterCount(tail)-1)));
        lines.last()=tail+QChar(0x2026);
    }
    if(lines.isEmpty())lines<<QString();
    if(elided)*elided=overflow;
    return lines;
}
int DescriptionPreview::contentHeight(int width) const { return linesForWidth(width).size()*fontMetrics().height(); }
QStringList DescriptionPreview::displayLines() const { return linesForWidth(width()); }
bool DescriptionPreview::isElided() const { bool result=false;linesForWidth(width(),&result);return result; }
void DescriptionPreview::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setPen(palette().color(QPalette::PlaceholderText));p.setFont(font());
    setToolTip(isElided()?m_text:QString());
    const auto lines=displayLines();int y=fontMetrics().ascent();
    for(const auto &line:lines){p.drawText(0,y,line);y+=fontMetrics().height();}
}
void DescriptionPreview::changeEvent(QEvent *e){QWidget::changeEvent(e);if(e->type()==QEvent::FontChange){emit layoutChanged();update();}}
static QPushButton *headerButton(const QString &text,const QString &name,QWidget *parent) {
    auto *b=new QPushButton(text,parent);b->setObjectName(name);b->setCursor(Qt::PointingHandCursor);b->setFixedHeight(36);return b;
}
static QPushButton *metadataLink(const QString &text,const QString &name,QWidget *parent) {
    auto *b=new QPushButton(text,parent);b->setObjectName(name);b->setProperty("metadataLink",true);
    b->setCursor(Qt::PointingHandCursor);return b;
}
PlaylistHeader::PlaylistHeader(QWidget *parent):QWidget(parent) {
    setObjectName("playlistHeader");setFixedHeight(160);setMinimumWidth(0);
    m_cover=new Cover(this);m_cover->setObjectName("playlistCover");m_cover->setFixedSize(160,160);
    for(int i=0;i<4;++i){m_rows[i]=new QWidget(this);m_rows[i]->setObjectName(QString("playlistRow%1").arg(i));}
    auto *playRow=new QHBoxLayout(m_rows[0]);playRow->setContentsMargins(0,0,0,0);playRow->setSpacing(10);
    auto *play=headerButton("播放全部","primary",m_rows[0]);play->setIcon(symbol("play",Qt::white));
    auto *shuffle=headerButton("随机播放","playlistShuffle",m_rows[0]);shuffle->setIcon(symbol("shuffle"));
    playRow->addWidget(play);playRow->addWidget(shuffle);playRow->addStretch();
    connect(play,&QPushButton::clicked,this,&PlaylistHeader::playRequested);
    connect(shuffle,&QPushButton::clicked,this,&PlaylistHeader::shuffleRequested);
    auto *optionRow=new QHBoxLayout(m_rows[1]);optionRow->setContentsMargins(0,0,0,0);optionRow->setSpacing(10);
    m_options=headerButton("歌单选项","playlistOptions",m_rows[1]);m_up=headerButton("上移","playlistUp",m_rows[1]);m_down=headerButton("下移","playlistDown",m_rows[1]);
    // The four row containers share a left edge; remove the first plain
    // button's theme padding so its label also begins on that edge.

    optionRow->addWidget(m_options);optionRow->addWidget(m_up);optionRow->addWidget(m_down);optionRow->addStretch();

    m_counts=new QLabel(m_rows[2]);m_counts->setObjectName("muted");m_counts->setTextFormat(Qt::PlainText);
    auto *tags=metadataLink("标签","playlistTagsLink",m_rows[2]);m_tagsLink=tags;tags->setAccessibleName("编辑歌单标签");
    m_tags=new QLabel(m_rows[2]);m_tags->setObjectName("muted");m_tags->setTextFormat(Qt::PlainText);m_tags->setMinimumWidth(0);
    m_tags->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);

    connect(tags,&QPushButton::clicked,this,&PlaylistHeader::tagsRequested);
    m_descriptionLink=metadataLink("简介","playlistDescriptionLink",m_rows[3]);m_descriptionLink->setAccessibleName("编辑歌单简介");
    m_description=new DescriptionPreview(m_rows[3]);
    connect(m_descriptionLink,&QPushButton::clicked,this,&PlaylistHeader::descriptionRequested);
    connect(m_description,&DescriptionPreview::layoutChanged,this,&PlaylistHeader::arrange);
    m_animation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_animation,&QVariantAnimation::valueChanged,this,[this](const QVariant &v){m_progress=v.toReal();arrange();});
}
void PlaylistHeader::setPlaylist(const Playlist &p,const QString &root) {
    m_cover->setCover(p.cover.isEmpty()?QString():QDir(root).filePath(p.cover),p.name);
    m_counts->setText(QString("%1 首歌曲  ·  %2 次播放").arg(p.count).arg(p.plays));
    m_tagText=p.tags.isEmpty()?QString("未设置"):(p.tags.size()==playlistTagOptions().size()?QString("全部"):p.tags.join("、"));
    m_tags->setToolTip(p.tags.join("、"));m_description->setText(p.description);arrange();
}
void PlaylistHeader::collapseTo(qreal target) {
    target=target>.5?1:0;if(m_target==target&&m_animation.state()==QAbstractAnimation::Running)return;
    m_target=target;m_animation.stop();
    if(qAbs(m_progress-target)<.001){m_progress=target;arrange();return;}
    m_animation.setDuration(220);m_animation.setStartValue(m_progress);m_animation.setEndValue(target);m_animation.start();
}
void PlaylistHeader::arrange() {
    if(m_arranging||!m_description)return;
    m_arranging=true;
    // Derive layout progress from the rounded height so all compact rows finish together.
    const int headerHeight=qRound(160-82*m_progress);
    const qreal p=(160-headerHeight)/82.0;
    const auto blend=[p](qreal a,qreal b){return qRound(a+(b-a)*p);};
    setFixedHeight(headerHeight);m_cover->setFixedSize(headerHeight,headerHeight);
    const int expandedLeft=180,compactLeft=98;
    for(int i=0;i<2;++i)m_rows[i]->ensurePolished();
    const int buttonsWidth=qMax(m_rows[0]->layout()->sizeHint().width(),m_rows[1]->layout()->sizeHint().width());
    const int rightStart=compactLeft+buttonsWidth+28;
    const int expandedWidth=qMax(1,width()-expandedLeft),compactWidth=qMax(1,width()-rightStart);
    m_description->setSingleLine(p>.001);
    m_descriptionLink->ensurePolished();m_description->ensurePolished();
    const int linkWidth=m_descriptionLink->sizeHint().width();
    const int expandedLastHeight=qMax(m_description->fontMetrics().height()*(m_description->property("expandedLines").toInt()==1?1:2),m_descriptionLink->sizeHint().height());
    // Compute expanded height using the original two-line preview even during a reverse transition.
    m_description->setSingleLine(false);
    const int naturalLast=qMax(m_description->contentHeight(qMax(1,expandedWidth-linkWidth-8)),m_descriptionLink->sizeHint().height());
    m_description->setSingleLine(p>.001);
    Q_UNUSED(expandedLastHeight);
    const int step=qMax(1,(160-naturalLast)/3);
    for(int i=0;i<2;++i){
        m_rows[i]->setGeometry(blend(expandedLeft,compactLeft),blend(i*step,i*42),blend(expandedWidth,buttonsWidth),36);
        m_rows[i]->layout()->activate();
    }
    const int infoX=blend(expandedLeft,rightStart),infoW=blend(expandedWidth,compactWidth);
    m_rows[2]->setGeometry(infoX,blend(2*step,0),infoW,blend(qMin(24,naturalLast),36));
    m_rows[3]->setGeometry(infoX,blend(3*step,42),infoW,blend(160-3*step,36));
    const int cw=qMin(m_counts->sizeHint().width(),qMax(1,infoW-70));
    const int tw=m_tagsLink->sizeHint().width();
    const int tagWidth=qMax(0,qMin(m_tags->fontMetrics().horizontalAdvance(m_tagText),infoW-cw-tw-22));
    const int groupWidth=cw+tw+tagWidth+22;
    const int groupX=blend(0,qMax(0,infoW-groupWidth));
    const int cy=blend(0,(36-m_counts->sizeHint().height())/2);
    m_counts->setGeometry(groupX,cy,cw,m_counts->sizeHint().height());
    m_tagsLink->setGeometry(groupX+cw+14,cy,tw,m_tagsLink->sizeHint().height());
    m_tags->setGeometry(groupX+cw+14+tw+8,cy,qMax(0,infoW-(groupX+cw+14+tw+8)),m_tags->sizeHint().height());
    m_tags->setText(m_tags->fontMetrics().elidedText(m_tagText,Qt::ElideRight,m_tags->width()));
    m_tags->setToolTip(m_tags->text()!=m_tagText?m_tagText:QString());
    const int previewWidth=qMax(1,infoW-linkWidth-8);
    const int previewHeight=m_description->contentHeight(previewWidth);
    const int bottom=m_rows[3]->height();
    const int compactDescriptionWidth=qMin(previewWidth,m_description->singleLineWidth());
    const int descriptionShift=blend(0,qMax(0,infoW-linkWidth-8-compactDescriptionWidth));
    m_description->setGeometry(descriptionShift+linkWidth+8,bottom-previewHeight,qMax(1,previewWidth-descriptionShift),previewHeight);
    m_descriptionLink->setGeometry(descriptionShift,bottom-previewHeight,linkWidth,m_descriptionLink->sizeHint().height());
    m_arranging=false;
}
void PlaylistHeader::resizeEvent(QResizeEvent *e){QWidget::resizeEvent(e);arrange();}
void PlaylistHeader::showEvent(QShowEvent *e){QWidget::showEvent(e);arrange();}
void PlaylistHeader::changeEvent(QEvent *e){QWidget::changeEvent(e);if(e->type()==QEvent::FontChange||e->type()==QEvent::StyleChange)arrange();}
PlaylistTagsDialog::PlaylistTagsDialog(const QStringList &tags,QWidget *parent):QDialog(parent) {
    setObjectName("playlistTagsDialog");setWindowTitle("歌单标签");setModal(true);resize(420,300);
    auto *v=new QVBoxLayout(this);v->setContentsMargins(22,20,22,20);v->setSpacing(16);
    auto *hint=new QLabel("至少选择一个，可同时选择全部标签。",this);hint->setObjectName("muted");v->addWidget(hint);
    auto *grid=new QGridLayout;grid->setHorizontalSpacing(28);grid->setVerticalSpacing(14);
    const auto options=playlistTagOptions();
    for(int i=0;i<options.size();++i){auto *check=new QCheckBox(options[i],this);check->setObjectName(QString("playlistTag%1").arg(i));
        check->setChecked(tags.contains(options[i]));m_checks<<check;grid->addWidget(check,i/2,i%2);
        connect(check,&QCheckBox::toggled,this,[this]{updateSelection();});}
    v->addLayout(grid);v->addStretch();m_count=new QLabel(this);m_count->setObjectName("muted");v->addWidget(m_count);
    m_buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this);
    m_buttons->button(QDialogButtonBox::Save)->setText("保存");m_buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    v->addWidget(m_buttons);connect(m_buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(m_buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    updateSelection();
}
QStringList PlaylistTagsDialog::selectedTags() const {QStringList result;for(auto *check:m_checks)if(check->isChecked())result<<check->text();return result;}
void PlaylistTagsDialog::updateSelection(){const auto count=selectedTags().size();m_count->setText(QString("已选 %1 / %2").arg(count).arg(m_checks.size()));m_buttons->button(QDialogButtonBox::Save)->setEnabled(count>0);}
PlaylistDescriptionDialog::PlaylistDescriptionDialog(const QString &text,QWidget *parent):QDialog(parent) {
    setObjectName("playlistDescriptionDialog");setWindowTitle("歌单简介");setModal(true);resize(520,350);
    auto *v=new QVBoxLayout(this);v->setContentsMargins(22,20,22,20);v->setSpacing(12);
    m_editor=new QPlainTextEdit(this);m_editor->setObjectName("playlistDescriptionEditor");m_editor->setPlaceholderText("写下歌单的故事，最多 1000 字…");
    m_editor->setPlainText(text.left(characterPrefixLength(text,1000)));v->addWidget(m_editor,1);
    m_remaining=new QLabel(this);m_remaining->setObjectName("descriptionRemaining");v->addWidget(m_remaining);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this);
    buttons->button(QDialogButtonBox::Save)->setText("保存");buttons->button(QDialogButtonBox::Cancel)->setText("取消");v->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(m_editor,&QPlainTextEdit::textChanged,this,&PlaylistDescriptionDialog::updateText);updateText();
}
void PlaylistDescriptionDialog::updateText(){
    QString text=m_editor->toPlainText();
    if(characterCount(text)>1000){const int end=characterPrefixLength(text,1000);const int position=m_editor->textCursor().position();
        QSignalBlocker block(m_editor);auto cursor=m_editor->textCursor();cursor.joinPreviousEditBlock();cursor.setPosition(end);cursor.movePosition(QTextCursor::End,QTextCursor::KeepAnchor);cursor.removeSelectedText();cursor.endEditBlock();cursor.setPosition(qMin(position,end));m_editor->setTextCursor(cursor);text=m_editor->toPlainText();}
    m_remaining->setText(QString("还可输入 %1 字 / 1000 字").arg(1000-characterCount(text)));
}
}
