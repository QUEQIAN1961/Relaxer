#include "lyric-match.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <utility>
namespace relaxer {
QString LyricMatchPolicy::normalized(const QString &text) {
    QString result;
    for(const QChar c:text.normalized(QString::NormalizationForm_KC).toLower())
        if(c.isLetterOrNumber())result+=c;
    return result;
}
QString LyricMatchPolicy::cleanTitle(const QString &text) {
    static thread_local const QRegularExpression ext("\\.(mp3|flac|wav|aac|ape|m4a|ogg|opus|wma)$",QRegularExpression::CaseInsensitiveOption);
    static thread_local const QRegularExpression leading("^\\s*(?:\\d{1,3}\\s*[.、_-]\\s+|0\\d{1,2}[.、](?=[^\\d])\\s*)");
    static thread_local const QRegularExpression tag("[\\[\\(【](?:FLAC|MP3|WAV|AAC|APE|DSD|Hi[- ]?Res|SQ|HQ|无损|高音质|音质|\\d{2,3}\\s*kbps|\\d{2}\\s*bit)[^\\]\\)】]*[\\]\\)】]",QRegularExpression::CaseInsensitiveOption);
    static thread_local const QRegularExpression tail("\\s+(?:FLAC|MP3|\\d{2,3}\\s*kbps|\\d{2}\\s*bit)\\s*$",QRegularExpression::CaseInsensitiveOption);
    static thread_local const QRegularExpression spaces("\\s+");
    QString s=text.normalized(QString::NormalizationForm_KC).trimmed();
    s.replace(ext,"");
    s.remove(leading);
    s.remove(tag);
    s.remove(tail);
    s.replace(QChar(0x2019),QChar('\''));s.replace(QChar(0x2018),QChar('\''));
    s.replace(spaces," ");
    return s.trimmed();
}
namespace lyric_match_detail {
bool knownArtist(const QString &s){return !s.trimmed().isEmpty()&&s!="未知歌手";}
QStringList artistForms(QString s) {
    static thread_local const QRegularExpression splitArtists("\\s*(?:\\bfeat\\.?\\b|\\bft\\.?\\b|\\bfeaturing\\b|\\bwith\\b|[,;&/、])\\s*",QRegularExpression::CaseInsensitiveOption);
    static thread_local const QRegularExpression alias("^([A-Za-z0-9 ._-]+)\\s+([\\p{Han}\\p{Hiragana}\\p{Katakana}]+)$");
    QStringList out{LyricMatchPolicy::normalized(s)};
    QString canonical=s.toLower();
    canonical.replace("ac/dc","acdc");
    canonical.replace(splitArtists,"|");
    QStringList pieces;
    for(const auto &p:canonical.split('|',Qt::SkipEmptyParts))pieces<<LyricMatchPolicy::normalized(p);
    std::sort(pieces.begin(),pieces.end());out<<pieces.join("|");
    const auto matched=alias.match(s.trimmed());
    if(matched.hasMatch()){out<<LyricMatchPolicy::normalized(matched.captured(1))<<LyricMatchPolicy::normalized(matched.captured(2));}
    out.removeAll(QString());out.removeDuplicates();return out;
}
bool sameArtist(const QString &a,const QString &b) {
    if(!knownArtist(a)||!knownArtist(b))return false;
    const auto x=artistForms(a),y=artistForms(b);
    for(const auto &s:x)if(y.contains(s))return true;
    return false;
}
QString versionMark(const QString &s) {
    static thread_local const QRegularExpression re("(\\b(?:live|remix|remaster(?:ed)?|acoustic|instrumental|karaoke|cover|demo|radio[ -]?edit|extended|sped[ -]?up|slowed|reverb)\\b|现场|翻唱|伴奏|纯音乐|重制|不插电)",QRegularExpression::CaseInsensitiveOption);
    static thread_local const QRegularExpression gaps("[ -]");
    QStringList parts;auto it=re.globalMatch(s);
    while(it.hasNext())parts<<it.next().captured().toLower().remove(gaps);
    parts.removeDuplicates();std::sort(parts.begin(),parts.end());return parts.join("|");
}
QString titleWithoutFeaturing(QString s) {
    static thread_local const QRegularExpression feat("\\s+(?:feat\\.?|ft\\.?|featuring)\\s+.+$",QRegularExpression::CaseInsensitiveOption);
    return s.remove(feat).trimmed();
}
QString baseTitle(QString s) {
    static thread_local const QRegularExpression bracket("[\\[\\(]([^\\]\\)]*)[\\]\\)]");
    static thread_local const QRegularExpression tail("\\s+[-–—]\\s*(?:live|.*remix|remaster(?:ed)?|acoustic|instrumental|cover|demo|radio edit|extended|sped up|slowed|现场|翻唱|伴奏).*$",QRegularExpression::CaseInsensitiveOption);
    auto it=bracket.globalMatch(s);QStringList remove;
    while(it.hasNext()){const auto m=it.next();if(!versionMark(m.captured(1)).isEmpty())remove<<m.captured();}
    for(const auto &v:remove)s.remove(v);
    s.remove(tail);
    return titleWithoutFeaturing(s).simplified();
}
double similarity(const QString &a,const QString &b) {
    const QString x=LyricMatchPolicy::normalized(a).left(300),y=LyricMatchPolicy::normalized(b).left(300);
    if(x.isEmpty()||y.isEmpty())return 0.;
    QVector<int> previous(y.size()+1),current(y.size()+1);
    for(int j=0;j<=y.size();++j)previous[j]=j;
    for(int i=1;i<=x.size();++i){current[0]=i;for(int j=1;j<=y.size();++j)current[j]=qMin(qMin(current[j-1]+1,previous[j]+1),previous[j-1]+(x[i-1]==y[j-1]?0:1));previous.swap(current);}
    return 1.-double(previous.last())/qMax(x.size(),y.size());
}
}
using lyric_match_detail::titleWithoutFeaturing;
using lyric_match_detail::similarity;
bool LyricMatchPolicy::knownArtist(const QString &s){return lyric_match_detail::knownArtist(s);}
bool LyricMatchPolicy::sameArtist(const QString &a,const QString &b){return lyric_match_detail::sameArtist(a,b);}
QString LyricMatchPolicy::versionMark(const QString &s){return lyric_match_detail::versionMark(s);}
QString LyricMatchPolicy::baseTitle(QString s){return lyric_match_detail::baseTitle(std::move(s));}
Track LyricMatchPolicy::searchTrack(const Track &track) {
    Track t=track;
    if(t.title.trimmed().isEmpty()||t.title=="未知歌曲")t.title=QFileInfo(t.file).completeBaseName();
    t.title=cleanTitle(t.title);
    t.artist=t.artist.trimmed();t.album=cleanTitle(t.album);
    if(knownArtist(t.artist)){
        for(const QString &sep:{QString(" - "),QString(" – "),QString(" — ")})
            if(t.title.startsWith(t.artist+sep,Qt::CaseInsensitive)){t.title.remove(0,t.artist.size()+sep.size());break;}
    }else{
        static thread_local const QRegularExpression dash("\\s+[-–—]\\s+");
        const auto parts=t.title.split(dash);
        if(parts.size()==2&&!parts[0].isEmpty()&&!parts[1].isEmpty()){t.artist=parts[0];t.title=parts[1];}
    }
    return t;
}
bool LyricMatchPolicy::confident(const Track &t,const QJsonObject &record) {
    if(record["manualOnly"].toBool()||!knownArtist(t.artist)||t.duration<=0)return false;
    const Track clean=searchTrack(t);
    const QString title=cleanTitle(record["trackName"].toString());
    const double seconds=record["duration"].toDouble(-1);
    return !clean.title.isEmpty()&&
        normalized(titleWithoutFeaturing(clean.title))==normalized(titleWithoutFeaturing(title))&&
        versionMark(clean.title)==versionMark(title)&&
        (clean.album.isEmpty()||clean.album=="未知专辑"||record["albumName"].toString().isEmpty()||versionMark(clean.title+" "+clean.album)==versionMark(title+" "+record["albumName"].toString()))&&sameArtist(clean.artist,record["artistName"].toString())&&
        seconds>0&&qAbs(t.duration/1000.-seconds)<=qMin(4.,qMax(2.,t.duration/100000.));
}
double LyricMatchPolicy::matchScore(const Track &track,const QJsonObject &r) {
    const Track t=searchTrack(track);
    double score=300*similarity(baseTitle(t.title),baseTitle(cleanTitle(r["trackName"].toString())));
    if(knownArtist(t.artist))score+=sameArtist(t.artist,r["artistName"].toString())?180:180*similarity(t.artist,r["artistName"].toString());
    if(!t.album.isEmpty()&&t.album!="未知专辑")score+=80*similarity(t.album,r["albumName"].toString());
    const double seconds=r["duration"].toDouble(-1);
    if(t.duration>0&&seconds>0)score+=qMax(0.,240.-qAbs(t.duration/1000.-seconds)*15.);
    if(!r["syncedLyrics"].toString().isEmpty())score+=60;
    if(versionMark(t.title)!=versionMark(r["trackName"].toString()))score-=450;
    if(confident(track,r))score+=500;
    if(r["manualOnly"].toBool())score-=180;
    return score;
}
bool LyricMatchPolicy::rawLyricsAllowed(const QByteArray &data,QString *reason){
    auto fail=[reason](const QString &why){if(reason)*reason=why;return false;};
    const auto low=data.toLower();if(low.contains("<html")||low.contains("<div")||low.contains("<br")||low.contains("</p"))return fail("内容像网页，需人工确认");
    return true;
}
bool LyricMatchPolicy::credibleLyrics(const Lyrics &lyric,qint64 duration,QString *reason){
    auto fail=[reason](const QString &why){if(reason)*reason=why;return false;};
    int lines=0;QString plain;for(const auto &line:lyric.lines){if(!line.text.trimmed().isEmpty())++lines;plain+=line.text.toLower();}if(plain.contains("<html")||plain.contains("<div")||plain.contains("<br")||plain.contains("</p"))return fail("内容像网页，需人工确认");
    if(lines<2)return fail("有效歌词少于两行，需人工确认");
    if(lyric.timed&&duration>0&&!lyric.lines.isEmpty()&&lyric.lines.last().time>duration+30000)return fail("歌词时间超过歌曲时长，需人工确认");
    if(reason)reason->clear();return true;
}
} // namespace relaxer
