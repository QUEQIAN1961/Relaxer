#include "online-track-matcher.h"
#include <QRegularExpression>
#include <algorithm>
namespace relaxer {
namespace {
QString normalize(QString value){static const QRegularExpression punctuation("[^\\p{L}\\p{N}]");value=value.normalized(QString::NormalizationForm_KC).toCaseFolded();value.remove(punctuation);return value;}
QString version(const QString &value){static const QRegularExpression versions("\\b(live|remix|instrumental|acoustic)\\b|伴奏|现场|翻唱|纯音乐|加速|慢速",QRegularExpression::CaseInsensitiveOption);QStringList tags;auto matches=versions.globalMatch(value);while(matches.hasNext())tags.append(matches.next().captured().toCaseFolded());tags.sort();return tags.join('|');}
double similarity(const QString &a,const QString &b){
    if(a==b)return 1.;if(a.isEmpty()||b.isEmpty())return 0.;const int length=qMax(a.size(),b.size());if(qAbs(a.size()-b.size())>length*.12)return 0.;
    QVector<int> row(b.size()+1);for(int i=0;i<row.size();++i)row[i]=i;
    for(int i=1;i<=a.size();++i){int previous=row[0];row[0]=i;for(int j=1;j<=b.size();++j){const int old=row[j];row[j]=qMin(qMin(row[j]+1,row[j-1]+1),previous+(a[i-1]!=b[j-1]));previous=old;}}
    return 1.-double(row.last())/length;
}
}
qint64 matchOnlineTrack(const Track &online,const QVector<Track> &local,bool requireUnique){
    const auto title=normalize(online.title),artist=normalize(online.artist),kind=version(online.title);
    if(title.isEmpty()||artist.isEmpty())return 0;double best=0,second=0;qint64 selected=0;
    for(const auto &track:local){
        const bool timed=track.duration>0&&online.duration>0;
        if((timed&&qAbs(track.duration-online.duration)>qMax<qint64>(2000,online.duration*15/1000))||version(track.title)!=kind)continue;
        const auto titleScore=similarity(title,normalize(track.title));if(titleScore<(timed?.92:1.))continue;
        const auto artistScore=similarity(artist,normalize(track.artist));if(artistScore<(timed?.9:1.))continue;
        const double score=titleScore*.65+artistScore*.35;if(score>best){second=best;best=score;selected=track.id;}else second=qMax(second,score);
    }
    return selected&&(!requireUnique||best-second>=.025)?selected:0;
}
}
