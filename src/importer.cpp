#include "importer.h"
#include "lyric-text.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QThreadPool>
#include <atomic>
#include <cmath>
namespace relaxer {
QString toolPath(const QString &name) {
#ifdef Q_OS_WIN
    QString file = name + ".exe";
#else
    QString file = name;
#endif
    QStringList prefixes{QCoreApplication::applicationDirPath() + "/bin/",
                         QCoreApplication::applicationDirPath() + "/",
                         QCoreApplication::applicationDirPath() + "/../runtime-ffmpeg/"};
    const auto configured = qEnvironmentVariable("RELAXER_FFMPEG_DIR");
    if (!configured.isEmpty())
        prefixes.prepend(QDir(configured).filePath(QString()) );
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 4; ++i) {
        prefixes << dir.filePath("runtime-ffmpeg/");
        if (!dir.cdUp())
            break;
    }
    for (const auto &prefix : prefixes)
        if (QFileInfo::exists(prefix + file))
            return QFileInfo(prefix + file).absoluteFilePath();
    return QStandardPaths::findExecutable(file);
}
QStringList Importer::extensions() {
    return {"mp3",  "flac", "wav", "m4a", "aac", "ogg", "opus", "oga", "aiff", "aif", "wma", "ape",
            "wv",   "tta",  "mpc", "mp+", "mp2", "mka", "ac3",  "amr", "caf",  "m4b", "mid", "midi",
            "kar"};
}
static QByteArray process(const QString &exe, const QStringList &args, int timeout,
                          QString *error = nullptr,std::atomic_bool *cancel = nullptr) {
    if (exe.isEmpty()) {
        if (error)
            *error = "缺少 FFmpeg / ffprobe，请重新完成首次准备。";
        return {};
    }
    QProcess p;
    p.setProgram(exe);
    p.setArguments(args);
    p.start();
    if (!p.waitForStarted(5000)) {
        if (error)
            *error = "无法启动音频读取工具";
        return {};
    }
    QElapsedTimer timer;timer.start();
    while (!p.waitForFinished(100)) {
        if((!cancel||!cancel->load())&&timer.elapsed()<timeout)continue;
        p.kill();
        p.waitForFinished(3000);
        if (error)
            *error = cancel&&cancel->load()?QStringLiteral("已取消"):QStringLiteral("读取超时");
        return {};
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        if (error)
            *error = "无法解码此音乐文件：" + QString::fromUtf8(p.readAllStandardError()).right(260);
        return {};
    }
    return p.readAllStandardOutput();
}
static bool transcodeToFlac(const QString &source, const QString &dest, std::atomic_bool *cancel, QString *error) {
    const auto exe = toolPath("ffmpeg");
    if (exe.isEmpty()) {
        if (error)
            *error = "缺少 FFmpeg，无法在导入时转换这种格式。";
        return false;
    }
    QFile::remove(dest);
    QProcess p;
    p.setProgram(exe);
    p.setArguments({"-y", "-v", "error", "-threads", "1", "-i", source, "-map", "0:a:0", "-c:a", "flac", dest});
    p.start();
    if (!p.waitForStarted(5000)) {
        QFile::remove(dest);
        if (error)
            *error = "无法启动 FFmpeg";
        return false;
    }
    while (!p.waitForFinished(200)) {
        if (cancel && cancel->load()) {
            p.kill();
            p.waitForFinished(3000);
            QFile::remove(dest);
            if (error)
                *error = "已取消";
            return false;
        }
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0 || !QFileInfo::exists(dest) ||
        QFileInfo(dest).size() <= 0) {
        const auto detail = QString::fromUtf8(p.readAllStandardError()).trimmed().right(260);
        QFile::remove(dest);
        if (error)
            *error = detail.isEmpty() ? QString("导入时转成 FLAC 失败") : "导入时转成 FLAC 失败：" + detail;
        return false;
    }
    return true;
}
static bool atomicWrite(const QString &path, const QByteArray &bytes) {
    QSaveFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size() && f.commit();
}
static ArtworkResult readArtwork(const QString &root, const QString &source, const QString &hash,
                                 const QJsonObject &json, bool allowSidecar,std::atomic_bool *cancel=nullptr) {
    ArtworkResult result;
    result.hash = hash;
    result.origin = "none";
    const QString relative = "Artwork/" + hash + "-v2.png";
    const QString destination = QDir(root).filePath(relative);
    QJsonObject picture;
    for (const auto &v : json["streams"].toArray()) {
        const auto stream = v.toObject();
        if (stream["codec_type"] != "video" || stream["disposition"].toObject()["attached_pic"].toInt() != 1)
            continue;
        if (picture.isEmpty())
            picture = stream;
        if (stream["tags"].toObject()["comment"].toString().contains("front", Qt::CaseInsensitive)) {
            picture = stream;
            break;
        }
    }
    if (!picture.isEmpty()) {
        const qint64 pixels = qint64(picture["width"].toInt()) * picture["height"].toInt();
        if (pixels <= 0 || pixels > 120000000) {
            result.error = "内嵌封面尺寸无效或过大";
        } else {
            // Map the exact attached picture, never a video frame or another image stream.
            const auto png = process(toolPath("ffmpeg"),
                                     {"-v", "error", "-i", source, "-map",
                                      "0:" + QString::number(picture["index"].toInt()), "-frames:v", "1",
                                      "-vf", "scale=800:800:force_original_aspect_ratio=decrease", "-f",
                                      "image2pipe", "-vcodec", "png", "pipe:1"},
                                     20000, &result.error,cancel);
            if (!png.isEmpty() && png.size() < 12 * 1024 * 1024 && atomicWrite(destination, png)) {
                result.cover = relative;
                result.origin = "embedded";
                result.error.clear();
                return result;
            }
            if (result.error.isEmpty())
                result.error = "无法保存歌曲内嵌封面";
        }
    }
    if (allowSidecar) {
        const QFileInfo audio(source);
        for (const auto &suffix : QStringList{".png", ".jpg", ".jpeg", ".webp"}) {
            // A shared folder cover is ambiguous in a directory containing unrelated songs.
            QImageReader reader(audio.dir().filePath(audio.completeBaseName() + suffix));
            const auto size = reader.size();
            if (!size.isValid() || qint64(size.width()) * size.height() > 120000000)
                continue;
            reader.setAutoTransform(true);
            reader.setScaledSize(size.scaled(800, 800, Qt::KeepAspectRatio));
            const auto image = reader.read();
            QSaveFile out(destination);
            if (!image.isNull() && out.open(QIODevice::WriteOnly) && image.save(&out, "PNG") &&
                out.commit()) {
                result.cover = relative;
                result.origin = "sidecar";
                result.error.clear();
                return result;
            }
        }
    }
    return result;
}
ArtworkResult Importer::repairArtwork(const QString &root, const Track &track, std::atomic_bool *cancel) {
    ArtworkResult result;
    result.id = track.id;
    result.hash = track.hash;
    result.previousCover = track.cover;
    if (cancel && cancel->load()) {
        result.error = "已取消";
        return result;
    }
    const auto source = QDir(root).filePath(track.file);
    const auto bytes = process(toolPath("ffprobe"), {"-v", "error", "-show_streams", "-of", "json", source},
                               20000, &result.error,cancel);
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    if (bytes.isEmpty() || parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (result.error.isEmpty())
            result.error = "无法读取原音乐的封面信息";
        return result;
    }
    if (cancel && cancel->load()) {
        result.error = "已取消";
        return result;
    }
    auto artwork = readArtwork(root, source, track.hash, document.object(), false,cancel);
    artwork.id = track.id;
    artwork.previousCover = track.cover;
    return artwork;
}
ImportResult Importer::importOne(const QString &root, const QString &source, std::atomic_bool *cancel) {
    ImportResult r;
    r.source = source;
    QFileInfo fi(source);
    auto stop = [&] { return cancel && cancel->load(); };
    if (stop()) {
        r.error = "已取消";
        return r;
    }
    if (!fi.isFile() || !extensions().contains(fi.suffix().toLower())) {
        r.error = "不支持此文件类型";
        return r;
    }
    const bool midi = isMidiSuffix(fi.suffix());
    QString err;
    QByteArray bytes;
    if (!midi)
        bytes = process(toolPath("ffprobe"),
                        {"-v", "error", "-show_format", "-show_streams", "-of", "json", fi.absoluteFilePath()},
                        20000, &err);
    auto json = QJsonDocument::fromJson(bytes).object();
    bool hasAudio = midi;
    QJsonObject tags = json["format"].toObject()["tags"].toObject();
    if (!bytes.isEmpty()) {
        for (const auto &v : json["streams"].toArray()) {
            auto s = v.toObject();
            if (s["codec_type"] == "audio" || isMidiSuffix(fi.suffix())) {
                hasAudio = true;
                auto extra = s["tags"].toObject();
                for (auto it = extra.begin(); it != extra.end(); ++it)
                    if (!tags.contains(it.key()))
                        tags[it.key()] = it.value();
            }
        }
    } else if (!midi) {
        const bool toolMissing = err.contains(QString("缺少 FFmpeg")) || err.contains(QString("无法启动音频读取工具"));
        if (toolMissing && (playsWithoutFfmpeg(fi.suffix()) || needsTranscodedPlayback(fi.suffix())))
            hasAudio = true;
        else {
            r.error = err.isEmpty() ? QString("文件没有可播放的音轨") : err;
            return r;
        }
    }
    if (!hasAudio) {
        r.error = bytes.isEmpty() && !err.isEmpty() ? err : "文件没有可播放的音轨";
        return r;
    }
    QJsonObject lower;
    for (auto it = tags.begin(); it != tags.end(); ++it)
        lower[it.key().toLower()] = it.value();
    r.track.title = lower["title"].toString().trimmed();
    if (r.track.title.isEmpty())
        r.track.title = fi.completeBaseName();
    r.track.artist = lower["artist"].toString().trimmed();
    if (r.track.artist.isEmpty())
        r.track.artist = lower["album_artist"].toString().trimmed();
    if (r.track.artist.isEmpty())
        r.track.artist = "未知歌手";
    r.track.album = lower["album"].toString().trimmed();
    if (r.track.album.isEmpty())
        r.track.album = "未知专辑";
    double seconds = json["format"].toObject()["duration"].toString().toDouble();
    if (std::isfinite(seconds) && seconds > 0 && seconds < 31536000)
        r.track.duration = qRound64(seconds * 1000);
    QFile in(fi.absoluteFilePath());
    if (!in.open(QIODevice::ReadOnly)) {
        r.error = "无法读取源文件";
        return r;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!in.atEnd()) {
        if (stop()) {
            r.error = "已取消";
            return r;
        }
        auto chunk = in.read(1024 * 1024);
        if (chunk.isEmpty() && in.error() != QFile::NoError) {
            r.error = "读取源文件失败";
            return r;
        }
        hash.addData(chunk);
    }
    r.track.hash = QString::fromLatin1(hash.result().toHex());
    const bool convert = needsTranscodedPlayback(fi.suffix()) && !toolPath("ffmpeg").isEmpty();
    r.track.file = "Music/" + r.track.hash + "." + (convert ? QString("flac") : fi.suffix().toLower());
    QDir data(root);
    for (const auto &d : QStringList{"Music", "Artwork", "Lyrics"})
        if (!data.mkpath(d)) {
            r.error = "无法创建资料目录";
            return r;
        }
    QString dest = data.filePath(r.track.file);
    if (QFileInfo::exists(dest) && QFileInfo(dest).size() <= 0)
        QFile::remove(dest);
    if (!QFileInfo::exists(dest)) {
        if (convert) {
            QString terror;
            if (!transcodeToFlac(fi.absoluteFilePath(), dest, cancel, &terror)) {
                r.error = terror;
                return r;
            }
        } else {
            in.seek(0);
            QSaveFile out(dest);
            if (!out.open(QIODevice::WriteOnly)) {
                r.error = "无法写入音乐目录，请检查空间与权限";
                return r;
            }
            QCryptographicHash copiedHash(QCryptographicHash::Sha256);
            while (!in.atEnd()) {
                if (stop()) {
                    r.error = "已取消";
                    return r;
                }
                auto c = in.read(1024 * 1024);
                if ((c.isEmpty() && in.error() != QFile::NoError) || out.write(c) != c.size()) {
                    r.error = "复制失败，源文件已保留";
                    return r;
                }
                copiedHash.addData(c);
            }
            if (copiedHash.result() != hash.result()) {
                r.error = "导入时源文件发生变化，请重试";
                return r;
            }
            if (!out.commit()) {
                r.error = "保存音乐失败，请检查磁盘空间";
                return r;
            }
        }
    }
    // Artwork is optional; decoding is bounded and never blocks the UI thread.
    if (!stop()) {
        const auto artwork = readArtwork(root, fi.absoluteFilePath(), r.track.hash, json, true,cancel);
        r.track.cover = artwork.cover;
        r.track.coverOrigin = artwork.origin;
    }
    QString lyric = fi.dir().filePath(fi.completeBaseName() + ".lrc");
    QFile lrc(lyric);
    QByteArray text;
    if (lrc.open(QIODevice::ReadOnly) && lrc.size() <= 4 * 1024 * 1024)
        text = lrc.readAll();
    if (text.isEmpty())
        for (auto it = lower.begin(); it != lower.end(); ++it)
            if (it.key().startsWith("lyrics") || it.key().startsWith("unsyncedlyrics")) {
                text = it.value().toString().toUtf8();
                break;
            }
    if (!text.isEmpty() && text.size() <= 4 * 1024 * 1024) {
        text = simplifyTraditionalLyrics(text);
        r.track.lyrics = "Lyrics/" + r.track.hash + ".lrc";
        if (!atomicWrite(data.filePath(r.track.lyrics), text))
            r.track.lyrics.clear();
    }
    return r;
}
void Importer::run() {
    QStringList files;
    QSet<QString> seen;
    int ignored=0;
    auto add = [&](const QFileInfo &fi) {
        if (fi.isFile() && extensions().contains(fi.suffix().toLower())) {
            auto p = fi.canonicalFilePath();
            if (!seen.contains(p)) {
                seen.insert(p);
                files << p;
            }else ++ignored;
        }else ++ignored;
    };
    for (const auto &s : m_sources) {
        if (m_cancel->load())
            break;
        QFileInfo fi(s);
        if (fi.isDir()) {
            emit progress(0, 0, "正在扫描文件夹…");
            QDirIterator it(s, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext() && !m_cancel->load()) {
                it.next();
                add(it.fileInfo());
            }
        } else
            add(fi);
    }
    emit scanned(files.size(),ignored,!m_cancel->load());
    if (files.isEmpty()) {
        emit finished(m_cancel->load());
        return;
    }
    const int cores = qMax(1, QThread::idealThreadCount());
    const int workers = qMin(files.size(), qMin(cores, m_maximumWorkers>0?qMin(m_maximumWorkers,8):8));
    std::atomic_int doneCount{0};
    std::atomic_int next{0};
    const int total = files.size();
    QThreadPool pool;
    pool.setMaxThreadCount(workers);
    for (int n = 0; n < workers; ++n) {
        pool.start([this, &files, &doneCount, &next, total] {
            while (!m_cancel->load()) {
                const int i = next.fetch_add(1);
                if (i >= total)
                    return;
                if(m_cancel->load())return;
                const auto name = QFileInfo(files[i]).fileName();
                emit progress(doneCount.load(), total, name);
                emit imported(importOne(m_root, files[i], m_cancel.get()));
                emit progress(doneCount.fetch_add(1) + 1, total, name);
            }
        });
    }
    pool.waitForDone();
    emit finished(m_cancel->load());
}
void ArtworkRepair::run() {
    for (const auto &track : m_tracks) {
        if (m_cancel->load())
            break;
        const auto result = Importer::repairArtwork(m_root, track, m_cancel.get());
        if (!m_cancel->load())
            emit repaired(result);
    }
    emit finished();
}
} // namespace relaxer
