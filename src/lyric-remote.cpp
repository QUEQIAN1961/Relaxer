#include "lyric-remote.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QRegularExpression>
#include <QUrlQuery>
namespace relaxer::lyric_remote {
namespace {
QByteArray decodeMaybe(const QString &text) {
    const auto trimmed = text.trimmed();
    if (trimmed.size() > 16 && QRegularExpression("^[A-Za-z0-9+/=\\s]+$").match(trimmed).hasMatch() &&
        !trimmed.contains('[')) {
        const auto bytes = QByteArray::fromBase64(trimmed.toLatin1());
        if (bytes.contains('[') || bytes.contains('\n'))
            return bytes;
    }
    return {};
}
QString lyricText(const QString &value) {
    const auto decoded = decodeMaybe(value);
    return decoded.isEmpty() ? value : QString::fromUtf8(decoded);
}
QJsonDocument documentFrom(const QByteArray &body) {
    auto direct = QJsonDocument::fromJson(body);
    if (!direct.isNull())
        return direct;
    const auto start = body.indexOf('{');
    const auto end = body.lastIndexOf('}');
    if (start >= 0 && end > start)
        return QJsonDocument::fromJson(body.mid(start, end - start + 1));
    return {};
}
QString mergeTimed(const QString &original, const QString &translation) {
    if (translation.trimmed().isEmpty())
        return original;
    QMap<qint64, QString> extra;
    const QRegularExpression stamp("\\[(\\d+):(\\d{1,2})(?:[.:](\\d{1,3}))?\\]([^\\[]*)");
    auto read = [&](const QString &text, bool translated) {
        auto it = stamp.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            auto frac = match.captured(3);
            while (frac.size() < 3)
                frac += '0';
            const qint64 time = match.captured(1).toLongLong() * 60000 + match.captured(2).toLongLong() * 1000 +
                                frac.toInt();
            const auto line = match.captured(4).trimmed();
            if (!line.isEmpty() && translated)
                extra[time] = line;
        }
    };
    read(translation, true);
    if (extra.isEmpty())
        return original;
    QStringList rows;
    for (const auto &raw : original.split('\n')) {
        rows << raw;
        auto it = stamp.globalMatch(raw);
        if (!it.hasNext())
            continue;
        const auto match = it.next();
        auto frac = match.captured(3);
        while (frac.size() < 3)
            frac += '0';
        const qint64 time =
            match.captured(1).toLongLong() * 60000 + match.captured(2).toLongLong() * 1000 + frac.toInt();
        if (extra.contains(time))
            rows << raw.left(raw.lastIndexOf(']') + 1) + extra.take(time);
    }
    return rows.join("\n");
}
QJsonObject record(const QString &title, const QString &artist, const QString &album, double seconds,
                   const QString &lyrics, const QString &source, bool synced) {
    QJsonObject row{{"trackName", title}, {"artistName", artist}, {"albumName", album},
                    {"duration", seconds}, {"source", source}};
    if (synced)
        row["syncedLyrics"] = lyrics;
    else
        row["plainLyrics"] = lyrics;
    return row;
}
QJsonObject matchedSong(const QJsonObject &song,int kind) {
    QString title,artist,album;double seconds=-1;
    if(kind==4){title=song["song"].toString();artist=song["singer"].toString();const auto ms=song["duration"].toVariant().toLongLong();if(ms>0)seconds=ms/1000.;}
    else {
        title=song["name"].toString(song["songname"].toString());
        auto singers=kind==2?song["artists"].toArray():song["singer"].toArray();if(singers.isEmpty())singers=song["ar"].toArray();
        QStringList names;for(const auto &value:singers){const auto name=value.toObject()["name"].toString();if(!name.isEmpty())names<<name;}artist=names.join('/');
        const auto object=song["album"].toObject();album=object["name"].toString(song["albumname"].toString());
        if(kind==2){const auto ms=song["duration"].toDouble(song["dt"].toDouble());if(ms>0)seconds=ms/1000.;}
        else {const auto interval=song["interval"].toDouble(song["duration"].toDouble());if(interval>0)seconds=interval>3000?interval/1000.:interval;}
    }
    QJsonObject out{{"duration",seconds},{"manualOnly",title.isEmpty()||artist.isEmpty()}};
    if(!title.isEmpty())out["trackName"]=title;
    if(!artist.isEmpty())out["artistName"]=artist;
    out["albumName"]=album;
    return out;
}
Step neteaseSearch(const QString &title, const QString &artist) {
    QUrl url("https://music.163.com/api/search/get/web");
    QUrlQuery query;
    query.addQueryItem("s", artist.isEmpty() ? title : title + " " + artist);
    query.addQueryItem("type", "1");
    query.addQueryItem("offset", "0");
    query.addQueryItem("limit", "8");
    query.addQueryItem("total", "true");
    url.setQuery(query);
    return {url, {{"Referer", "https://music.163.com/"}, {"User-Agent", "Mozilla/5.0"}}};
}
Step qqSearch(const QString &title, const QString &artist) {
    QUrl url("https://c.y.qq.com/soso/fcgi-bin/client_search_cp");
    QUrlQuery query;
    query.addQueryItem("w", artist.isEmpty() ? title : title + " " + artist);
    query.addQueryItem("format", "json");
    query.addQueryItem("n", "8");
    query.addQueryItem("p", "1");
    query.addQueryItem("cr", "1");
    query.addQueryItem("new_json", "1");
    url.setQuery(query);
    return {url, {{"Referer", "https://y.qq.com/"}, {"User-Agent", "Mozilla/5.0"}}};
}
Step kugouSearch(const QString &title, const QString &artist, qint64 durationMs) {
    QUrl url("https://lyrics.kugou.com/search");
    QUrlQuery query;
    query.addQueryItem("ver", "1");
    query.addQueryItem("man", "yes");
    query.addQueryItem("client", "pc");
    query.addQueryItem("keyword", artist.isEmpty() ? title : artist + " - " + title);
    query.addQueryItem("duration", QString::number(qMax<qint64>(0, durationMs)));
    query.addQueryItem("hash", "");
    url.setQuery(query);
    return {url, {{"User-Agent", "Mozilla/5.0"}}};
}
} // namespace
Step begin(int kind, const QString &title, const QString &artist, qint64 durationMs) {
    if (kind == 3)
        return qqSearch(title, artist);
    if (kind == 4)
        return kugouSearch(title, artist, durationMs);
    return neteaseSearch(title, artist);
}
Follow consume(int kind, const QByteArray &phase, const QByteArray &body, const QString &title,
               const QString &artist, qint64 durationMs) {
    Follow out;
    const auto doc = documentFrom(body);
    if (doc.isNull()) {
        out.failed = true;
        out.failure = "响应格式异常";
        return out;
    }
    const auto root = doc.object();
    if (phase == "search") {
        QJsonArray songs;
        if(kind==2){
            if(!root["result"].isUndefined()&&!root["result"].isObject()){
                out.failed=true;out.failure="来源返回验证或异常响应";return out;
            }
            songs=root["result"].toObject()["songs"].toArray();
        }else if(kind==3){
            songs=root["data"].toObject()["song"].toObject()["list"].toArray();
            if(songs.isEmpty())songs=root["data"].toObject()["song"].toArray();
        }else songs=root["candidates"].toArray();
        const int code=root["code"].toInt(0);
        if(songs.isEmpty()&&((kind==2&&code!=0&&code!=200)||(kind==3&&code!=0)||(kind==4&&root["status"].toInt(200)!=200))){
            out.failed=true;out.failure="来源暂时拒绝搜索";return out;
        }
        QSet<QUrl> seen;
        for(const auto &value:songs){
            const auto song=value.toObject();Detail detail;detail.matched=matchedSong(song,kind);
            QUrl url;QUrlQuery query;
            if(kind==2){
                const auto id=song["id"].toVariant().toLongLong();if(!id)continue;
                detail.durationMs=song["duration"].toVariant().toLongLong();if(detail.durationMs<=0)detail.durationMs=song["dt"].toVariant().toLongLong();
                url=QUrl("https://music.163.com/api/song/lyric");query.addQueryItem("id",QString::number(id));query.addQueryItem("lv","1");query.addQueryItem("tv","1");
                detail.step.headers={{"Referer","https://music.163.com/"},{"User-Agent","Mozilla/5.0"}};
            }else if(kind==3){
                const auto mid=song["mid"].toString(song["songmid"].toString());if(mid.isEmpty())continue;
                const double interval=song["interval"].toDouble(song["duration"].toDouble());detail.durationMs=interval>3000?qint64(interval):qint64(interval*1000);
                url=QUrl("https://c.y.qq.com/lyric/fcgi-bin/fcg_query_lyric_new.fcg");query.addQueryItem("songmid",mid);query.addQueryItem("format","json");query.addQueryItem("nobase64","1");query.addQueryItem("g_tk","5381");
                detail.step.headers={{"Referer","https://y.qq.com/"},{"User-Agent","Mozilla/5.0"}};
            }else{
                const auto id=song["id"].toVariant().toString(),key=song["accesskey"].toString();if(id.isEmpty()||key.isEmpty())continue;
                detail.durationMs=song["duration"].toVariant().toLongLong();
                url=QUrl("https://lyrics.kugou.com/download");query.addQueryItem("ver","1");query.addQueryItem("client","pc");query.addQueryItem("id",id);query.addQueryItem("accesskey",key);query.addQueryItem("fmt","lrc");query.addQueryItem("charset","utf8");
                detail.step.headers={{"User-Agent","Mozilla/5.0"}};
            }
            url.setQuery(query);if(seen.contains(url))continue;seen.insert(url);detail.step.url=url;
            if(!out.again){out.again=true;out.step=detail.step;out.matched=detail.matched;out.durationMs=detail.durationMs;}
            else out.remaining<<detail;
            if(seen.size()>=8)break;
        }
        return out;
    }
    if (kind == 2) {
        const auto synced = lyricText(root["lrc"].toObject()["lyric"].toString());
        const auto translated = lyricText(root["tlyric"].toObject()["lyric"].toString());
        const auto text = mergeTimed(synced, translated);
        if (text.trimmed().isEmpty())
            return out;
        out.records.append(record(title, artist, {}, durationMs > 0 ? durationMs / 1000. : -1, text, "网易云",
                                   text.contains('[')));
        return out;
    }
    if (kind == 3) {
        const auto synced = lyricText(root["lyric"].toString());
        const auto translated = lyricText(root["trans"].toString());
        const auto text = mergeTimed(synced, translated);
        if (text.trimmed().isEmpty())
            return out;
        out.records.append(record(title, artist, {}, durationMs > 0 ? durationMs / 1000. : -1, text, "QQ音乐",
                                   text.contains('[')));
        return out;
    }
    const auto text = lyricText(root["content"].toString());
    if (text.trimmed().isEmpty())
        return out;
    out.records.append(record(title, artist, {}, durationMs > 0 ? durationMs / 1000. : -1, text, "酷狗",
                               text.contains('[')));
    return out;
}
} // namespace relaxer::lyric_remote
