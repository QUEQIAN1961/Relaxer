#include "online-track-matcher.h"
#include <QRegularExpression>
#include <algorithm>
#include "library.h"
#include <QDir>
#include <QFileInfo>
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
OnlineTrackIndex::OnlineTrackIndex(const QVector<Track> &tracks,const QString &root){
    m_rows.reserve(tracks.size());
    for(const auto &track:tracks){if(isVideoTrack(track))continue;const int i=m_rows.size();m_rows.append({track.id,track.duration,normalize(track.title),normalize(track.artist),version(track.title),root.isEmpty()?track.file:QDir(root).filePath(track.file)});m_kinds[m_rows.last().kind].append(i);m_paths.insert(track.id,m_rows.last().path);}
}
qint64 OnlineTrackIndex::match(const Track &online,bool requireUnique,bool requireExisting)const{
    if(isVideoTrack(online))return 0;
    const auto title=normalize(online.title),artist=normalize(online.artist),kind=version(online.title);
    if(title.isEmpty()||artist.isEmpty())return 0;double best=0,second=0;qint64 selected=0;
    const auto candidates=m_kinds.constFind(kind);if(candidates==m_kinds.cend())return 0;
    for(const auto index:*candidates){const auto &track=m_rows[index];
        const bool timed=track.duration>0&&online.duration>0;
        if(timed&&qAbs(track.duration-online.duration)>qMax<qint64>(2000,online.duration*15/1000))continue;
        const auto titleScore=similarity(title,track.title);if(titleScore<(timed?.92:1.))continue;
        const auto artistScore=similarity(artist,track.artist);if(artistScore<(timed?.9:1.))continue;
        if(requireExisting&&!QFileInfo(track.path).isFile())continue;
        const double score=titleScore*.65+artistScore*.35;if(score>best){second=best;best=score;selected=track.id;}else second=qMax(second,score);
    }
    return selected&&(!requireUnique||best-second>=.025)?selected:0;
}
namespace {
class SharedIndex final:public QObject {
public:
    std::shared_ptr<const OnlineTrackIndex> value;
    explicit SharedIndex(Library *library):QObject(library){setObjectName("onlineTrackMatchIndex");connect(library,&Library::changed,this,[this]{value.reset();});}
};
}
std::shared_ptr<const OnlineTrackIndex> sharedOnlineTrackIndex(Library *library){
    if(!library)return {};
    auto *store=static_cast<SharedIndex *>(library->findChild<QObject *>("onlineTrackMatchIndex",Qt::FindDirectChildrenOnly));
    if(!store)store=new SharedIndex(library);
    if(!store->value)store->value=std::make_shared<OnlineTrackIndex>(library->matchingTracks(),library->root());
    return store->value;
}
qint64 matchOnlineTrack(const Track &online,const QVector<Track> &local,bool requireUnique){return OnlineTrackIndex(local).match(online,requireUnique);}
}
