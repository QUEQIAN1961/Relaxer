#include "lyrics-search-coordinator.h"
#include "release-version.h"
#include "domain/lyric-match.h"
#include "importer.h"
#include "lyric-text.h"
#include "lyrics-document-reader.h"
#include "lyrics.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QRegularExpression>
#include <QRunnable>
#include <QTextDocumentFragment>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <algorithm>
#include <mutex>
#include <thread>
namespace relaxer {
namespace {
std::atomic<quint64> taskSerial{0};
std::atomic<qint64> g_matchScoreCalls{0};
std::atomic<qint64> g_rankRuns{0};
std::atomic_bool &lyricComputeCancel() {
    static std::atomic_bool flag{false};
    return flag;
}
QThreadPool *lyricPool() {
    static QThreadPool *pool = nullptr;
    static std::once_flag once;
    std::call_once(once, [] {
        pool = new QThreadPool;
        const int cores = QThread::idealThreadCount();
        pool->setMaxThreadCount(std::max(2, cores < 1 ? 2 : cores));
        if (auto *app = QCoreApplication::instance())
            QObject::connect(app, &QCoreApplication::aboutToQuit, [] { lyricComputeCancel().store(true); });
    });
    return pool;
}
class LyricRunnable : public QRunnable {
  public:
    explicit LyricRunnable(std::function<void()> fn) : fn(std::move(fn)) { setAutoDelete(true); }
    void run() override {
        if (fn)
            fn();
    }

  private:
    std::function<void()> fn;
};
QString normalized(const QString &s) {
    return LyricMatchPolicy::normalized(s);
}
bool knownArtist(const QString &s) {
    return LyricMatchPolicy::knownArtist(s);
}
QString baseTitle(const QString &s) {
    return LyricMatchPolicy::baseTitle(s);
}
QString versionMark(const QString &s) {
    return LyricMatchPolicy::versionMark(s);
}
Track searchTrack(const Track &t) {
    return LyricMatchPolicy::searchTrack(t);
}
bool confident(const Track &t, const QJsonObject &r) {
    return LyricMatchPolicy::confident(t, r);
}
double matchScore(const Track &t, const QJsonObject &r) {
    g_matchScoreCalls.fetch_add(1);
    return LyricMatchPolicy::matchScore(t, r);
}
QString recordLyrics(const QJsonObject &r) {
    const auto synced = r["syncedLyrics"].toString();
    return synced.isEmpty() ? r["plainLyrics"].toString() : synced;
}
bool credibleLyrics(const QByteArray &bytes, const Track &t, QString *reason = nullptr) {
    return LyricMatchPolicy::rawLyricsAllowed(bytes, reason) &&
           LyricMatchPolicy::credibleLyrics(Lyrics::parse(bytes), t.duration, reason);
}
QString collectionKey(const QJsonObject &record) {
    QJsonObject identity;
    for (const auto *field : {"source", "id", "sourceRecordId", "trackName", "artistName", "albumName",
                              "duration", "syncedLyrics", "plainLyrics", "instrumental"})
        if (record.contains(field))
            identity.insert(field, record[field]);
    return LyricsSearchCoordinator::candidateIdentity(identity);
}
} // namespace
QThreadPool *LyricsSearchCoordinator::computePool() {
    return lyricPool();
}
std::atomic_bool &LyricsSearchCoordinator::computeCancel() {
    return lyricComputeCancel();
}
void LyricsSearchCoordinator::resetMatchScoreCount() {
    g_matchScoreCalls.store(0);
}
qint64 LyricsSearchCoordinator::matchScoreCount() {
    return g_matchScoreCalls.load();
}
void LyricsSearchCoordinator::resetRankRuns() {
    g_rankRuns.store(0);
}
qint64 LyricsSearchCoordinator::rankRuns() {
    return g_rankRuns.load();
}
bool LyricsSearchCoordinator::ownerThread() const {
    return QThread::currentThread() == thread() && m_library && m_library->thread() == thread();
}
QVector<LyricsSearchSource> LyricsSearchCoordinator::defaultSources(QUrl endpoint, QUrl backup) {
    if (backup.isEmpty() && endpoint.host() == "lrclib.net")
        backup = QUrl("https://api.lyrics.ovh/v1");
    QVector<Source> out{{"LRCLIB", endpoint, 0, false}};
    if (!backup.isEmpty())
        out.append({"Lyrics.ovh", backup, 1, true});
    if (endpoint.host() == "lrclib.net") {
        out.append({"网易云", QUrl("https://music.163.com"), 2, false});
        out.append({"QQ音乐", QUrl("https://y.qq.com"), 3, false});
        out.append({"酷狗", QUrl("https://www.kugou.com"), 4, false});
    }
    return out;
}
LyricsSearchCoordinator::LyricsSearchCoordinator(Library *library, QObject *parent, QUrl endpoint,
                                                 QUrl backup, QNetworkAccessManager *transport,
                                                 QVector<Source> sources)
    : QObject(parent), m_library(library), m_network(transport ? transport : new QNetworkAccessManager(this)),
      m_sources(sources.isEmpty() ? defaultSources(endpoint, backup) : sources) {
    qRegisterMetaType<LyricsSearchEvent>();
    qRegisterMetaType<LyricsSearchSummary>();
    qRegisterMetaType<LyricsCandidateSnapshot>();
    if (transport || endpoint.host() == "127.0.0.1" || endpoint.host() == "localhost") {
        if (!transport)
            m_network->setProxy(QNetworkProxy::NoProxy);
    } else {
        m_proxyReady = false;
        m_proxyState = std::make_shared<std::atomic_bool>(false);
        QPointer<LyricsSearchCoordinator> self(this);
        auto state = m_proxyState;
        std::thread([self, state] {
            QNetworkProxy chosen(QNetworkProxy::NoProxy);
            const auto found =
                QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(QUrl("https://lrclib.net")));
            for (const auto &proxy : found)
                if (proxy.type() == QNetworkProxy::HttpProxy || proxy.type() == QNetworkProxy::Socks5Proxy ||
                    proxy.type() == QNetworkProxy::HttpCachingProxy) {
                    chosen = proxy;
                    break;
                }
            auto *app = QCoreApplication::instance();
            if (!app)
                return;
            QMetaObject::invokeMethod(
                app,
                [self, state, chosen] {
                    if (!self || state->load() || !self->m_network)
                        return;
                    self->m_network->setProxy(chosen);
                    self->m_proxyReady = true;
                    for (const auto &job : self->m_jobs.values())
                        if (self->live(job))
                            self->pumpQueries(job);
                },
                Qt::QueuedConnection);
        }).detach();
    }
    connect(library, &QObject::destroyed, this, [this] { shutdown(); });
    connect(m_network, &QObject::destroyed, this, [this] { shutdown(); });
}
LyricsSearchCoordinator::~LyricsSearchCoordinator() {
    shutdown();
}
QString LyricsSearchCoordinator::candidateIdentity(const QJsonObject &record) {
    // Full immutable source record includes provider id, artist, version and both lyric representations.
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(record).toJson(QJsonDocument::Compact),
                                                        QCryptographicHash::Sha256)
                                   .toHex());
}
void LyricsSearchCoordinator::loadCached(const QVector<qint64> &ids) {
    if (!ownerThread() || m_stopping)
        return;
    for (auto id : ids) {
        const auto t = m_library->track(id);
        const auto cache = m_library->lookupCache(id);
        if (cache.fresh && !cache.noResult && !cache.records.isEmpty())
            m_candidates[id] = {id, t.hash, ++m_candidateRevision, cache.records};
    }
}
void LyricsSearchCoordinator::clearCandidates(qint64 id) {
    if (ownerThread()) {
        m_candidates.remove(id);
        m_library->clearLookupCache(id);
    }
}
void LyricsSearchCoordinator::noteManualSave(int count) {
    if (ownerThread() && count > 0) {
        m_saved += count;
        m_summary.saved = m_saved;
    }
}
void LyricsSearchCoordinator::publishEvent(LyricsSearchEvent event, bool terminal) {
    QTimer::singleShot(0, this, [this, event = std::move(event), terminal] {
        QPointer<LyricsSearchCoordinator> guard(this);
        emit eventOccurred(event);
        if (guard && terminal)
            emit taskFinished(event);
    });
}
void LyricsSearchCoordinator::setJobStatus(const JobPtr &j, const QString &status, QString phase, int attempt,
                                           QUrl url, QString source) {
    LyricsSearchEvent event;
    event.taskId = j->taskId;
    event.trackId = j->original.id;
    event.generation = j->generation;
    event.candidateRevision = m_candidates.value(event.trackId).revision;
    event.phase = std::move(phase);
    event.status = status;
    event.state = LyricsSearchState::Searching;
    event.attempt = attempt;
    event.pass = j->searchPass;
    event.url = std::move(url);
    event.source = std::move(source);
    publishEvent(std::move(event));
}
void LyricsSearchCoordinator::publishSummary(bool terminal) {
    m_summary.generation = m_generation;
    m_summary.running = m_running;
    m_summary.saved = m_saved;
    m_summary.activeTracks = m_jobs.size();
    m_summary.activeRequests = 0;
    for (auto n : m_hostRequests)
        m_summary.activeRequests += n;
    m_summary.trackLimit = m_trackLimit;
    m_summary.requestLimit = m_requestLimit;
    m_summary.candidateTracks = m_found.size();
    m_summary.matchedTracks = m_matched.size();
    m_summary.status = m_running
                           ? QString("正在查找 · 同时 %1 首 · 每首最多 %2 个查询 · 已保存 %3 首")
                                 .arg(m_jobs.size())
                                 .arg(m_requestLimit)
                                 .arg(m_saved)
                           : QString("查找完成，已保存 %1 首。需要选择的歌曲请点击左侧查看。").arg(m_saved);
    const auto value = m_summary;
    QTimer::singleShot(0, this, [this, value, terminal] {
        QPointer<LyricsSearchCoordinator> guard(this);
        emit summaryChanged(value);
        if (guard && terminal)
            emit finished(value);
    });
}
void LyricsSearchCoordinator::setCandidates(const JobPtr &j, const QJsonArray &records) {
    if (!records.isEmpty())
        m_found.insert(j->original.id);
    auto &snapshot = m_candidates[j->original.id];
    if (snapshot.trackHash == j->original.hash && snapshot.records == records)
        return;
    snapshot = {j->original.id, j->original.hash, ++m_candidateRevision, records};
    const auto value = snapshot;
    const quint64 generation = j->generation;
    QTimer::singleShot(0, this, [this, value, generation] { emit candidatesChanged(value, generation); });
}
quint64 LyricsSearchCoordinator::start(const QVector<LyricsSearchRequest> &requests,
                                       const LyricsSearchOptions &options) {
    if (!ownerThread() || m_stopping || !m_network || m_network->thread() != thread() || requests.isEmpty())
        return 0;
    stop();
    m_options = options;
    m_options.budgetMs = qMax(1, options.budgetMs);
    m_options.requestTimeoutMs = qMax(1, options.requestTimeoutMs);
    m_queue.clear();
    QSet<qint64> seen;
    for (auto input : requests) {
        if (!input.original.id || seen.contains(input.original.id))
            continue;
        seen.insert(input.original.id);
        if (input.match.title.isEmpty())
            input.match = searchTrack(input.original);
        if (!input.saveSnapshot.id)
            input.saveSnapshot = m_library->lyricsSaveSnapshot(input.original.id);
        m_queue.append(std::move(input));
    }
    m_summary = {};
    m_summary.requested = m_queue.size();
    m_found.clear();
    m_matched.clear();
    m_saved = 0;
    m_next = 0;
    m_reuse.clear();
    if (!m_queue.isEmpty() && !m_queue.first().replace)
        for (const auto &t : m_library->tracks())
            if (!t.lyrics.isEmpty())
                m_reuse[reuseKey(t)].append(t.id);
    clearTransientCooldowns();
    m_running = !m_queue.isEmpty();
    pumpTracks();
    return m_generation;
}
void LyricsSearchCoordinator::pumpTracks() {
    if (!m_running || !ownerThread())
        return;
    while (m_jobs.size() < m_trackLimit && m_next < m_queue.size()) {
        const auto input = m_queue[m_next++];
        if (!input.original.id)
            continue;
        auto j = std::make_shared<Job>();
        j->input = input;
        j->original = input.original;
        j->match = input.match;
        j->manual = input.manual;
        j->saveSnapshot = input.saveSnapshot;
        j->generation = m_generation;
        j->taskId = ++taskSerial;
        m_jobs[j->original.id] = j;
        if (!current(j)) {
            finishJob(j, "歌曲文件已变化或已移除，请重新查找", LyricsSearchState::Invalidated);
            continue;
        }
        if (j->manual)
            beginOnline(j);
        else
            lookup(j);
    }
    if (m_jobs.isEmpty() && m_next >= m_queue.size() && !m_recoveryQueue.isEmpty()) {
        auto j = m_recoveryQueue.takeFirst();
        j->generation = m_generation;
        m_jobs[j->original.id] = j;
        setJobStatus(j, "正在重试 · 单曲依次复查");
        publishSummary();
        QTimer::singleShot(qMax(0, m_options.retryDelayMs), this, [this, j] {
            if (!live(j))
                return;
            if (!current(j)) {
                finishJob(j, "歌曲文件已变化，请重新查找", LyricsSearchState::Invalidated);
                return;
            }
            clearTransientCooldowns();
            beginOnline(j);
        });
        return;
    }
    const bool completed = m_jobs.isEmpty() && m_next >= m_queue.size();
    if (completed)
        m_running = false;
    publishSummary(completed);
}
bool LyricsSearchCoordinator::live(const JobPtr &j) const {
    return m_running && !m_stopping && m_library && m_network && !j->terminal &&
           j->generation == m_generation && m_jobs.value(j->original.id) == j;
}
bool LyricsSearchCoordinator::current(const JobPtr &j) const {
    return ownerThread() && trackContext(m_library->track(j->original.id)) == trackContext(j->original);
}
void LyricsSearchCoordinator::detachRequests(const JobPtr &j) {
    const auto replies = j->replies;
    j->replies.clear();
    j->active = 0;
    for (auto *reply : replies) {
        releaseRequestSlot(reply);
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    if (j->probe) {
        j->probe->disconnect(this);
        j->probe->kill();
        // kill() is asynchronous on Windows. Do not schedule QObject deletion
        // while the native child is still running, especially during exit.
        if (j->probe->state() != QProcess::NotRunning)
            j->probe->waitForFinished(1000);
        j->probe->deleteLater();
        j->probe = nullptr;
    }
}
void LyricsSearchCoordinator::finishJob(const JobPtr &j, const QString &status, LyricsSearchState state) {
    if (!live(j))
        return;
    j->terminal = true;
    m_jobs.remove(j->original.id);
    detachRequests(j);
    const bool recovering = state == LyricsSearchState::Queued;
    if (!recovering) {
        ++m_summary.finished;
        if (state == LyricsSearchState::Cancelled)
            ++m_summary.cancelled;
        if (state == LyricsSearchState::Failed || state == LyricsSearchState::Invalidated)
            ++m_summary.failed;
    }
    LyricsSearchEvent event;
    event.taskId = j->taskId;
    event.trackId = j->original.id;
    event.generation = j->generation;
    event.state = state;
    event.phase = recovering ? "recovery-queued" : "finished";
    event.status = status;
    event.pass = j->searchPass;
    event.candidateRevision = m_candidates.value(event.trackId).revision;
    publishEvent(event, !recovering);
    const int generation = m_generation;
    QTimer::singleShot(0, this, [this, generation] {
        if (generation == m_generation)
            pumpTracks();
    });
}
void LyricsSearchCoordinator::cancelTrack(qint64 id) {
    if (!ownerThread())
        return;
    const auto j = m_jobs.value(id);
    if (j)
        finishJob(j, {}, LyricsSearchState::Cancelled);
    for (int n = m_next; n < m_queue.size(); ++n)
        if (m_queue[n].original.id == id) {
            m_queue[n].original.id = 0;
            ++m_summary.finished;
            ++m_summary.cancelled;
            LyricsSearchEvent event;
            event.taskId = ++taskSerial;
            event.trackId = id;
            event.generation = m_generation;
            event.state = LyricsSearchState::Cancelled;
            event.phase = "cancelled";
            publishEvent(event, true);
        }
}
QVector<LyricsSearchEvent> LyricsSearchCoordinator::stop() {
    if (QThread::currentThread() != thread())
        return {};
    QVector<LyricsSearchEvent> cancelled;
    const bool wasRunning = m_running;
    // Invalidate every business identity before abort() can emit a synchronous callback.
    m_running = false;
    const auto jobs = m_jobs.values();
    m_jobs.clear();
    const auto recovery = m_recoveryQueue;
    m_recoveryQueue.clear();
    for (const auto &j : jobs + recovery) {
        if (j->terminal)
            continue;
        j->terminal = true;
        detachRequests(j);
        ++m_summary.finished;
        ++m_summary.cancelled;
        LyricsSearchEvent event;
        event.taskId = j->taskId;
        event.trackId = j->original.id;
        event.generation = j->generation;
        event.state = LyricsSearchState::Cancelled;
        event.phase = "cancelled";
        event.status = "已停止，等待继续";
        publishEvent(event, true);
        cancelled.append(event);
    }
    for (int n = m_next; n < m_queue.size(); ++n) {
        const auto id = m_queue[n].original.id;
        if (!id)
            continue;
        ++m_summary.finished;
        ++m_summary.cancelled;
        LyricsSearchEvent event;
        event.taskId = ++taskSerial;
        event.trackId = id;
        event.generation = m_generation;
        event.state = LyricsSearchState::Cancelled;
        event.phase = "cancelled";
        event.status = "已停止，等待继续";
        publishEvent(event, true);
        cancelled.append(event);
    }
    m_queue.clear();
    m_next = 0;
    if (wasRunning)
        publishSummary(true);
    ++m_generation;
    return cancelled;
}
void LyricsSearchCoordinator::shutdown() {
    if (QThread::currentThread() != thread()) {
        qWarning("LyricsSearchCoordinator shutdown requires its owner thread");
        return;
    }
    if (m_stopping)
        return;
    stop();
    m_stopping = true;
    if (m_proxyState)
        m_proxyState->store(true);
}
bool LyricsSearchCoordinator::store(const Track &t, const QByteArray &data, bool force, bool preserveText) {
    m_storeError.clear();
    const auto j = m_jobs.value(t.id);
    if (!j || !current(j)) {
        m_storeError = "歌曲身份已变化，请重新搜索歌词。";
        return false;
    }
    const auto now = m_library->track(t.id);
    if (!force && !now.lyrics.isEmpty() && QFileInfo::exists(m_library->absolute(now.lyrics))) {
        m_storeError = "已有本地歌词，请手动确认替换。";
        return false;
    }
    const bool ok = m_library->saveLyricsChecked(t.id, data, j->saveSnapshot, preserveText);
    if (ok)
        remember(t.id);
    else
        m_storeError = m_library->lastError();
    return ok;
}
double LyricsSearchCoordinator::score(const Track &t, const QJsonObject &r) {
    return matchScore(t, r);
}
QString LyricsSearchCoordinator::trackContext(const Track &t) {
    return QString::number(t.id) + QChar(1) + t.hash + QChar(1) + t.title + QChar(1) + t.artist + QChar(1) +
           t.album + QChar(1) + QString::number(t.duration) + QChar(1) + t.file;
}

void LyricsSearchCoordinator::appendCollected(const JobPtr &j, const QJsonObject &record) {
    if (j->collected.size() >= 100)
        return;
    j->collected.append(record);
    ++j->collectionEpoch;
    j->readyValid = false;
}

QString LyricsSearchCoordinator::reuseKey(const Track &t) const {
    return normalized(t.title) + "|" + normalized(t.artist);
}

void LyricsSearchCoordinator::remember(qint64 id) {
    const auto t = m_library->track(id);
    if (!t.lyrics.isEmpty() && QFileInfo::exists(m_library->absolute(t.lyrics))) {
        auto &ids = m_reuse[reuseKey(t)];
        if (!ids.contains(id))
            ids << id;
    }
}

void LyricsSearchCoordinator::lookup(const JobPtr &j) {
    const auto t = j->original;
    const bool has = !t.lyrics.isEmpty() && QFileInfo::exists(m_library->absolute(t.lyrics));
    if (has) {
        beginOnline(j);
        return;
    }
    if (!j->input.replace) {
        const QFileInfo info(m_library->absolute(t.file));
        for (const auto &suffix : {QString(".lrc"), QString(".txt")}) {
            const auto localPath = info.dir().filePath(info.completeBaseName() + suffix);
            const auto local = LyricsDocumentReader::readDocument(localPath);
            if (local.file.opened && !local.file.tooLarge && !local.lyrics.lines.isEmpty() &&
                store(t, local.file.bytes)) {
                ++m_saved;
                finishJob(j, "已保存本地歌词", LyricsSearchState::Saved);
                return;
            }
        }
        if (knownArtist(t.artist))
            for (auto id : m_reuse.value(reuseKey(t))) {
                if (id == t.id)
                    continue;
                const auto donor = m_library->track(id);
                if (t.duration > 0 && donor.duration > 0 && qAbs(t.duration - donor.duration) > 4000)
                    continue;
                const auto donorDocument = LyricsDocumentReader::readFile(m_library->absolute(donor.lyrics));
                if (!donorDocument.opened || donorDocument.tooLarge)
                    continue;
                if (store(t, donorDocument.bytes)) {
                    ++m_saved;
                    finishJob(j, "已保存同曲歌词 · 复用《" + donor.title + "》", LyricsSearchState::Saved);
                    return;
                }
            }
        setJobStatus(j, "正在检查内嵌歌词");
        auto *p = new QProcess(this);
        j->probe = p;
        connect(p, &QProcess::finished, this, [this, j, p](int, QProcess::ExitStatus) {
            if (!live(j))
                return;
            const auto doc = QJsonDocument::fromJson(p->readAllStandardOutput()).object();
            auto tags = doc["format"].toObject()["tags"].toObject();
            for (const auto &s : doc["streams"].toArray()) {
                const auto more = s.toObject()["tags"].toObject();
                for (auto i = more.begin(); i != more.end(); ++i)
                    tags.insert(i.key(), i.value());
            }
            QByteArray data;
            for (auto i = tags.begin(); i != tags.end(); ++i)
                if (i.key().toLower().startsWith("lyrics") ||
                    i.key().toLower().startsWith("unsyncedlyrics")) {
                    data = i.value().toString().toUtf8();
                    if (!data.isEmpty())
                        break;
                }
            j->probe = nullptr;
            p->deleteLater();
            if (!data.isEmpty() && store(j->original, data)) {
                ++m_saved;
                finishJob(j, "已保存内嵌歌词", LyricsSearchState::Saved);
            } else
                beginOnline(j);
        });
        connect(p, &QProcess::errorOccurred, this, [this, j, p](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart && live(j)) {
                j->probe = nullptr;
                p->deleteLater();
                beginOnline(j);
            }
        });
        p->start(toolPath("ffprobe"), {"-v", "error", "-show_entries", "format_tags:stream_tags", "-of",
                                       "json", info.absoluteFilePath()});
        QTimer::singleShot(5000, p, [p] {
            if (p->state() != QProcess::NotRunning)
                p->kill();
        });
        return;
    }
    beginOnline(j);
}

LyricsQueryPlan LyricsSearchCoordinator::queryPlan(const LyricsSearchRequest &request,
                                                   const QVector<Source> &sources,
                                                   const LyricsSearchOptions &options) {
    LyricsQueryPlan out;
    out.request = request;
    out.options = options;
    auto &input = out.request;
    if (input.match.title.isEmpty())
        input.match = searchTrack(input.original);
    const auto t = input.original;
    if (sources.isEmpty())
        return out;
    auto add = [&](QString title, QString artist, bool exact = false, QString keywords = {},
                   QString album = {}, int source = 0, bool cover = false) {
        if (title.isEmpty() && keywords.isEmpty())
            return;
        if (!knownArtist(artist))
            artist.clear();
        Attempt a{title, artist, album, keywords, source, exact, !keywords.isEmpty(), cover, false, {}};
        for (const auto &old : out.attempts)
            if (old.title == a.title && old.artist == a.artist && old.source == a.source &&
                old.cover == a.cover &&
                (sources[source].kind >= 2 ||
                 (old.album == a.album && old.keywords == a.keywords && old.exact == a.exact)))
                return;
        out.attempts << a;
    };
    // Both buttons use the same query plan for unchanged input. Start with the
    // normalized search rather than spending a batch slot on a strict album get.
    add(input.match.title, input.match.artist);
    add(input.match.title, input.match.artist, true);
    const auto alias = QRegularExpression("^([A-Za-z0-9 ._-]+)\\s+[\\p{Han}\\p{Hiragana}\\p{Katakana}]+$")
                           .match(input.match.artist);
    if (alias.hasMatch())
        add(input.match.title, alias.captured(1));
    const auto explicitLead=LyricMatchPolicy::primaryArtist(input.match.artist);
    if(!explicitLead.isEmpty())add(input.match.title,explicitLead);
    const auto lead = input.match.artist
                          .split(QRegularExpression("\\s*(?:/|、|&|\\bfeat\\.?\\b|\\bft\\.?\\b)\\s*",
                                                    QRegularExpression::CaseInsensitiveOption))
                          .value(0);
    if (input.match.artist != "AC/DC" && lead != input.match.artist)
        add(input.match.title, lead);
    add(input.match.title, {});
    const auto base = baseTitle(input.match.title);
    if (base != input.match.title) {
        add(base, input.match.artist);
        add(base, {});
    }
    add(base, {}, false, base);
    if (!input.match.album.isEmpty() && input.match.album != "未知专辑")
        add(input.match.title, {}, false, {}, input.match.album);
    if (QRegularExpression("[\\p{Han}\\p{Hiragana}\\p{Katakana}]").match(input.match.title).hasMatch() &&
        QRegularExpression("\\s").match(input.match.title).hasMatch()) {
        auto compact = input.match.title;
        compact.remove(QRegularExpression("\\s+"));
        add(compact, {}, false, compact);
    }
    const auto originalMatch = searchTrack(t);
    const bool unchanged =
        input.match.title == originalMatch.title && input.match.artist == originalMatch.artist;
    if (unchanged && (t.title != input.match.title || t.artist != input.match.artist))
        add(t.title, t.artist, true);
    if (input.manual && !input.fragment.trimmed().isEmpty())
        add(input.match.title, {}, false, input.fragment.trimmed());
    if (sources.size() > 1 && knownArtist(input.match.artist))
        add(input.match.title, input.match.artist, false, {}, {}, 1);
    if ((!sources.isEmpty() && sources.first().endpoint.host() == "lrclib.net")) {
        const auto baseAttempts = out.attempts;
        for (const auto &attempt : baseAttempts)
            for (int s = 0; s < sources.size(); ++s)
                if (sources[s].kind >= 2)
                    add(attempt.title, attempt.artist, attempt.exact, attempt.keywords, attempt.album, s,
                        attempt.cover);
    }
    if (unchanged) {
        auto credit = originalCredit(input.match.title);
        if (!credit.found)
            credit = originalCredit(QFileInfo(input.original.file).completeBaseName());
        if (!credit.found)
            credit = originalCredit(input.original.title);
        if (credit.found) {
            add(credit.title, credit.artist, true, {}, {}, 0, true);
            if ((!sources.isEmpty() && sources.first().endpoint.host() == "lrclib.net"))
                for (int s = 0; s < sources.size(); ++s)
                    if (sources[s].kind >= 2)
                        add(credit.title, credit.artist, true, {}, {}, s, true);
        }
    }
    // Try independent sources early, instead of exhausting all LRCLIB variants (and its timeouts) first.
    QVector<QVector<Attempt>> bySource(sources.size());
    for (const auto &attempt : out.attempts)
        bySource[attempt.source] << attempt;
    out.attempts.clear();
    int rounds = 0;
    for (const auto &list : bySource)
        rounds = qMax(rounds, int(list.size()));
    for (int n = 0; n < rounds; ++n)
        for (int source = 0; source < bySource.size(); ++source)
            if (n < bySource[source].size() && !sources[source].backup)
                out.attempts << bySource[source][n];
    for (int source = 0; source < bySource.size(); ++source)
        if (sources[source].backup)
            out.attempts += bySource[source];
    return out;
}

void LyricsSearchCoordinator::beginOnline(const JobPtr &j) {
    if (!live(j) || j->onlineStarted)
        return;
    j->onlineStarted = true;
    const auto t = j->original;
    // Every entry point is now an explicit search. Cached candidates stay available for review, but never
    // suppress network requests.
    setJobStatus(j, j->searchPass ? "正在重试 · 单曲依次复查" : "正在排队查找");
    j->attempts = queryPlan(j->input, m_sources, m_options).attempts;
    pumpQueries(j);
}

void LyricsSearchCoordinator::pumpQueries(const JobPtr &j) {
    // Slot-release notifications can reach jobs still probing embedded lyrics.
    // Their empty attempt list means "not online yet", never "search exhausted".
    if (!live(j) || !j->onlineStarted || j->onlineFinished)
        return;
    const int budget = m_options.budgetMs;
    if (j->clock.isValid() && j->clock.elapsed() >= budget) {
        j->completeResponses = false;
        if (j->active == 0)
            finishOnline(j);
        return;
    }
    if (!j->manual && !j->collected.isEmpty() && !rankingCurrent(j)) {
        scheduleRanking(j);
        // Await credibility for a possible timed match before replenishing this
        // job. Clearly unrelated candidates still allow immediate fallback.
        if(awaitingEarlyStopRank(j))return;
    }
    if (!j->manual && rankingCurrent(j) && earlyStopReady(j)) {
        finishOnline(j);
        return;
    }
    if (j->active == 0 && !j->manual && !j->collected.isEmpty() && rankingCurrent(j)) {
        bool strong = false, conflict = false, instrumental = false, vocal = false;
        QString first;
        for (const auto &item : j->readyAll) {
            const auto &r = item.record;
            if (!item.confidentOriginal)
                continue;
            instrumental |= r["instrumental"].toBool();
            vocal |= !recordLyrics(r).isEmpty();
            const auto text = recordLyrics(r);
            if (!first.isEmpty() && text != first)
                conflict = true;
            if (!text.isEmpty())
                first = text;
            if (!r["syncedLyrics"].toString().isEmpty() && item.credible)
                strong = true;
        }
        if (strong && !conflict && !instrumental) {
            acceptRecords(j, j->collected);
            return;
        }
        if (instrumental && !vocal && j->next >= j->attempts.size()) {
            acceptRecords(j, j->collected);
            return;
        }
    }
    if (!m_proxyReady) {
        if (j->active == 0)
            setJobStatus(j, "正在准备网络");
        return;
    }
    while(j->active<m_requestLimit&&!j->deferredRequests.isEmpty()){
        const auto pending=j->deferredRequests.takeFirst();request(j,pending.first,pending.second);
        if(!live(j)||j->onlineFinished)return;
    }
    while (j->active < m_requestLimit && j->next < j->attempts.size()) {
        // A saturated/cooling source must not block other providers behind it in the queue.
        int ready = -1;
        for (int n = j->next; n < j->attempts.size(); ++n) {
            const auto &source = m_sources[j->attempts[n].source];
            const auto host = source.endpoint.host();
            if (m_cooldowns.value(host) > QDateTime::currentMSecsSinceEpoch())
                continue;
            if (m_hostRequests.value(host) < 6) {
                ready = n;
                break;
            }
        }
        if (ready < 0) {
            bool waitingForSlot = false;
            for (int n = j->next; n < j->attempts.size(); ++n)
                if (m_cooldowns.value(m_sources[j->attempts[n].source].endpoint.host()) <=
                    QDateTime::currentMSecsSinceEpoch())
                    waitingForSlot = true;
            if (!waitingForSlot) {
                j->completeResponses = false;
                j->next = j->attempts.size();
            }
            break;
        }
        if (ready != j->next)
            std::rotate(j->attempts.begin() + j->next, j->attempts.begin() + ready,
                        j->attempts.begin() + ready + 1);
        const int stage = j->next++;
        request(j, stage);
    }
    if (j->active == 0 && j->next >= j->attempts.size()) {
        if (!j->manual && !j->collected.isEmpty() && !rankingCurrent(j)) {
            scheduleRanking(j);
            return;
        }
        finishOnline(j);
    }
}

void LyricsSearchCoordinator::request(const JobPtr &j, int stage, int retry) {
    if (!live(j) || j->onlineFinished)
        return;
    if(awaitingEarlyStopRank(j)){
        const auto pending=qMakePair(stage,retry);
        if(!j->deferredRequests.contains(pending))j->deferredRequests.append(pending);
        scheduleRanking(j);return;
    }
    if(!j->manual&&rankingCurrent(j)&&earlyStopReady(j)){finishOnline(j);return;}
    if (j->clock.isValid() && j->clock.elapsed() >= m_options.budgetMs) {
        j->completeResponses = false;
        finishOnline(j);
        return;
    }
    const auto a = j->attempts[stage];
    const auto source = m_sources[a.source];
    QUrl url = source.endpoint;
    if (m_hostRequests.value(source.endpoint.host()) >= 6) {
        ++j->active;
        QTimer::singleShot(50, this, [this, j, stage, retry] {
            if (live(j) && !j->onlineFinished) {
                --j->active;
                request(j, stage, retry);
            }
        });
        return;
    }
    if (!j->clock.isValid()) {
        j->clock.start();
        const int budget = m_options.budgetMs;
        QTimer::singleShot(qMax(1, budget), this, [this, j] {
            if (live(j) && !j->onlineFinished) {
                j->completeResponses = false;
                j->failures << "搜索预算已用完";
                finishOnline(j);
            }
        });
    }
    QList<QPair<QByteArray, QByteArray>> extraHeaders;
    QByteArray remotePhase;
    if (source.kind >= 2) {
        const auto step = a.detailRequest
                              ? a.detail.step
                              : lyric_remote::begin(source.kind, a.title, a.artist, j->original.duration);
        url = step.url;
        extraHeaders = step.headers;
        remotePhase = a.detailRequest ? "lyric" : "search";
    } else if (source.kind == 1)
        url = QUrl::fromEncoded(source.endpoint.toEncoded(QUrl::RemoveQuery | QUrl::RemoveFragment) + "/" +
                                QUrl::toPercentEncoding(a.artist) + "/" + QUrl::toPercentEncoding(a.title));
    else {
        url.setPath(source.endpoint.path() + (a.exact ? QString("/get") : QString("/search")));
        QUrlQuery q;
        if (a.keywordsSearch)
            q.addQueryItem("q", a.keywords);
        else {
            q.addQueryItem("track_name", a.title);
            if (!a.artist.isEmpty())
                q.addQueryItem("artist_name", a.artist);
        }
        const auto album = a.exact ? j->match.album : a.album;
        if (!album.isEmpty() && album != "未知专辑")
            q.addQueryItem("album_name", album);
        if (a.exact && j->original.duration > 0)
            q.addQueryItem("duration", QString::number(j->original.duration / 1000., 'f', 3));
        url.setQuery(q);
    }
    setJobStatus(j, "正在查找 · " + source.label + " · " +
                        (a.exact              ? QString("精确匹配")
                         : a.keywordsSearch   ? QString("关键词搜索")
                         : a.artist.isEmpty() ? QString("仅歌名搜索")
                                              : QString("歌名和歌手")));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  source.kind >= 2 ? "Mozilla/5.0" : release::UserAgent);
    req.setRawHeader("Accept", "application/json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    for (const auto &header : extraHeaders)
        req.setRawHeader(header.first, header.second);
    const int budget = m_options.budgetMs;
    const int timeout = qMax(1, qMin(m_options.requestTimeoutMs, budget - int(j->clock.elapsed())));
    ++m_summary.requests;
    setJobStatus(j, {}, "request", stage, url, source.label);
    auto *reply = m_network->get(req);
    reply->setProperty("lookupTrackId", j->original.id);
    reply->setProperty("lookupSearchPass", j->searchPass);
    reply->setProperty("lookupHost", source.endpoint.host());
    if (!remotePhase.isEmpty())
        reply->setProperty("remotePhase", remotePhase);
    ++m_hostRequests[source.endpoint.host()];
    ++j->active;
    j->replies.insert(reply);
    const auto started = QDateTime::currentMSecsSinceEpoch();
    QTimer::singleShot(timeout, reply, [reply] {
        if (!reply->isFinished()) {
            reply->setProperty("lookupTimedOut", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 n, qint64) {
        if (n > 4 * 1024 * 1024) {
            reply->setProperty("lookupTooLarge", true);
            reply->abort();
        }
    });
    connect(
        reply, &QNetworkReply::finished, this,
        [this, j, reply, stage, retry, source, started, requestTimeout = timeout, a] {
            if (reply->property("lookupConsumed").toBool())
                return;
            reply->setProperty("lookupConsumed", true);
            releaseRequestSlot(reply);
            if (!live(j) || j->onlineFinished) {
                reply->deleteLater();
                return;
            }
            if (!current(j)) {
                finishJob(j, "歌曲文件已变化，请重新查找", LyricsSearchState::Invalidated);
                return;
            }
            j->replies.remove(reply);
            --j->active;
            const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto error = reply->error();
            const bool timeout = reply->property("lookupTimedOut").toBool(),
                       large = reply->property("lookupTooLarge").toBool() ||
                               reply->bytesAvailable() > 4 * 1024 * 1024;
            const int retryAfter = reply->rawHeader("Retry-After").toInt();
            const auto body = reply->isOpen() ? reply->read(4 * 1024 * 1024 + 1) : QByteArray();
            reply->deleteLater();
            if (code >= 500 && code <= 599 && retry == 0 && !large && !timeout) {
                ++j->active;
                QTimer::singleShot(300, this, [this, j, stage] {
                    if (live(j) && !j->onlineFinished) {
                        --j->active;
                        const int budget = m_options.budgetMs;
                        if (j->clock.elapsed() < budget)
                            request(j, stage, 1);
                        else {
                            j->completeResponses = false;
                            pumpQueries(j);
                        }
                    }
                });
                pumpQueries(j);
                return;
            }
            if (code == 404) {
                pumpQueries(j);
                return;
            }
            if (code == 400) {
                j->completeResponses = false;
                pumpQueries(j);
                return;
            }
            if (error != QNetworkReply::NoError || code >= 500 || large) {
                j->completeResponses = false;
                const auto failure = large         ? QString("响应过大")
                                     : timeout     ? QString("请求超时")
                                     : code == 429 ? QString("请求限流")
                                     : code >= 500 ? QString("服务暂时异常")
                                                   : QString("网络异常");
                j->failures << source.label + "：" + failure;
                j->failures.removeDuplicates();
                m_fastReplies = 0;
                m_trackLimit = m_requestLimit = 3;
                publishSummary();
                if (code == 429 || error == QNetworkReply::HostNotFoundError ||
                    error == QNetworkReply::SslHandshakeFailedError ||
                    error == QNetworkReply::ConnectionRefusedError) {
                    const auto host = source.endpoint.host();
                    if (code == 429)
                        m_rateLimitedHosts.insert(host);
                    m_cooldowns[host] = qMax(m_cooldowns.value(host), QDateTime::currentMSecsSinceEpoch() +
                                                                          1000 * qBound(30, retryAfter, 600));
                }
                pumpQueries(j);
                return;
            }
            auto absorb = [&, this](QJsonArray records) {
                for (auto v : records) {
                    auto r = v.toObject();
                    if (a.detailRequest) {
                        for (auto field = a.detail.matched.begin(); field != a.detail.matched.end(); ++field)
                            r[field.key()] = field.value();
                        const QUrlQuery detailQuery(a.detail.step.url);
                        r["sourceRecordId"] = detailQuery.queryItemValue(source.kind == 3 ? "songmid" : "id");
                    }
                    if (a.cover) {
                        r["coverMatch"] = true;
                        r["coverTitle"] = a.title;
                        r["coverArtist"] = a.artist;
                    }
                    if (recordLyrics(r).trimmed().isEmpty() && !r["instrumental"].toBool())
                        continue;
                    r["source"] = source.label;
                    const auto key = collectionKey(r);
                    if (j->keys.contains(key))
                        continue;
                    j->keys.insert(key);
                    appendCollected(j, r);
                }
            };
            if (source.kind >= 2) {
                const auto follow = lyric_remote::consume(
                    source.kind, (a.detailRequest ? QByteArray("lyric") : QByteArray("search")), body,
                    a.title, a.artist, a.detailRequest ? a.detail.durationMs : j->original.duration);
                if (follow.failed) {
                    j->completeResponses = false;
                    j->failures << source.label + "：" + follow.failure;
                    pumpQueries(j);
                    return;
                }
                if (follow.again) {
                    QVector<lyric_remote::Detail> details{{follow.step, follow.matched, follow.durationMs}};
                    details += follow.remaining;
                    int position = j->next;
                    for (const auto &detail : details) {
                        if (j->detailUrls.contains(detail.step.url))
                            continue;
                        j->detailUrls.insert(detail.step.url);
                        auto next = a;
                        next.detailRequest = true;
                        next.detail = detail;
                        // Only insert into the unsent suffix: retry callbacks retain
                        // the original indices of every already dispatched attempt.
                        j->attempts.insert(position++, next);
                    }
                }
                absorb(follow.records);
                pumpQueries(j);
                return;
            }
            QJsonParseError e;
            const auto doc = QJsonDocument::fromJson(body, &e);
            if (e.error != QJsonParseError::NoError || (!doc.isArray() && !doc.isObject())) {
                j->completeResponses = false;
                j->failures << source.label + "：响应格式异常";
                pumpQueries(j);
                return;
            }
            if (QDateTime::currentMSecsSinceEpoch() - started < 600 && ++m_fastReplies >= 8) {
                m_fastReplies = 0;
                m_trackLimit = qMin(10, m_trackLimit + 1);
                m_requestLimit = qMin(5, m_requestLimit + 1);
                publishSummary();
                QTimer::singleShot(0, this, &LyricsSearchCoordinator::pumpTracks);
            }
            QJsonArray records;
            if (source.kind == 1) {
                const auto raw = doc.object()["lyrics"].toString();
                auto html = raw;
                html.replace("\r\n", "\n");
                html.replace("\n", "<br>");
                const auto text = QTextDocumentFragment::fromHtml(html).toPlainText();
                if (!text.trimmed().isEmpty()) {
                    const auto &a = j->attempts[stage];
                    records.append(QJsonObject{{"trackName", a.title},
                                               {"artistName", a.artist},
                                               {"albumName", ""},
                                               {"duration", -1},
                                               {"plainLyrics", text},
                                               {"source", source.label},
                                               {"manualOnly", true}});
                }
            } else if (doc.isObject())
                records.append(doc.object());
            else
                records = doc.array();
            for (const auto &v : records) {
                auto r = v.toObject();
                if (recordLyrics(r).trimmed().isEmpty() && !r["instrumental"].toBool())
                    continue;
                r["source"] = source.label;
                const auto key = collectionKey(r);
                if (j->keys.contains(key))
                    continue;
                j->keys.insert(key);
                appendCollected(j, r);
            }
            pumpQueries(j);
        });
}

void LyricsSearchCoordinator::finishOnline(const JobPtr &j) {
    if (!live(j) || j->onlineFinished)
        return;
    j->onlineFinished = true;
    detachRequests(j);
    if (!j->collected.isEmpty()) {
        acceptRecords(j, j->collected);
        return;
    }
    if (j->completeResponses && j->failures.isEmpty() && j->next >= j->attempts.size() &&
        m_candidates.value(j->original.id).records.isEmpty())
        m_library->saveLookupCache(j->original.id, {}, true);
    if (!j->manual && j->searchPass == 0 && !j->completeResponses &&
        m_candidates.value(j->original.id).records.isEmpty()) {
        auto retry = std::make_shared<Job>();
        retry->input = j->input;
        retry->saveSnapshot = j->saveSnapshot;
        retry->original = j->original;
        retry->match = j->match;
        retry->searchPass = 1;
        retry->taskId = j->taskId;
        retry->generation = j->generation;
        m_recoveryQueue << retry;
        finishJob(j, "等待重试 · 首轮来源异常，稍后逐首复查", LyricsSearchState::Queued);
        return;
    }
    const auto version = versionMark(j->original.title);
    if (version.contains("instrumental") || version.contains("纯音乐") || version.contains("伴奏")) {
        finishJob(j, "需选择 · 需确认纯音乐（仅曲目标签线索）");
        return;
    }
    const auto existing = m_candidates.value(j->original.id).records;
    if (!existing.isEmpty() && m_candidates.value(j->original.id).trackHash == j->original.hash) {
        finishJob(j, QString("需选择版本（%1 个） · 本次未获得新结果，保留上次候选").arg(existing.size()) +
                         (j->failures.isEmpty() ? QString() : " · " + j->failures.join("；")));
        return;
    }
    const auto status =
        j->failures.isEmpty()
            ? (j->completeResponses ? QString("数据库无结果") : QString("未完成：来源冷却或查询未完成"))
            : QString("未完成：") + j->failures.join("；");
    finishJob(j,
              (j->completeResponses ? status : QString("等待重试 · ") + status) + " · 可修改歌名和歌手重试",
              j->completeResponses ? LyricsSearchState::NoResult : LyricsSearchState::Failed);
}

void LyricsSearchCoordinator::acceptRecords(const JobPtr &j, const QJsonArray &records, bool cached) {
    if (!live(j))
        return;
    j->wantAccept = true;
    if (cached) {
        j->acceptCached = true;
        if (j->cachedPayload != records) {
            j->cachedPayload = records;
            ++j->cacheEpoch;
            j->readyValid = false;
        }
    } else
        j->acceptCached = false;
    if (cached ? !cacheRankingCurrent(j) : !rankingCurrent(j)) {
        scheduleRanking(j);
        return;
    }
    applyAccept(j);
}

void LyricsSearchCoordinator::clearTransientCooldowns() {
    const auto now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_cooldowns.begin(); it != m_cooldowns.end();) {
        if (!m_rateLimitedHosts.contains(it.key()) || it.value() <= now) {
            m_rateLimitedHosts.remove(it.key());
            it = m_cooldowns.erase(it);
        } else
            ++it;
    }
}

bool LyricsSearchCoordinator::productionHost() const {
    return !m_sources.isEmpty() && m_sources.first().endpoint.host() == "lrclib.net";
}

LyricsSearchCoordinator::RankResult LyricsSearchCoordinator::computeRank(const ScoreSnap &snap) {
    RankResult result;
    result.id = snap.id;
    result.hash = snap.hash;
    result.fpO = snap.fpO;
    result.fpM = snap.fpM;
    result.generation = snap.generation;
    result.epoch = snap.epoch;
    result.cached = snap.cached;
    result.timedRequired = snap.timedRequired;
    result.payload = snap.payload;
    g_rankRuns.fetch_add(1, std::memory_order_relaxed);
    for (int left = snap.delayMs; left > 0;) {
        if (lyricComputeCancel().load()) {
            result.dropped = true;
            return result;
        }
        const int step = std::min(20, left);
        QThread::msleep(step);
        left -= step;
    }
    for (const auto &value : snap.payload) {
        if (lyricComputeCancel().load()) {
            result.dropped = true;
            return result;
        }
        const auto r = value.toObject();
        RankItem item;
        item.record = r;
        const bool keep = !recordLyrics(r).trimmed().isEmpty() || r["instrumental"].toBool();
        const bool dropTimed = snap.timedRequired && !Lyrics::parse(recordLyrics(r).toUtf8()).timed;
        item.confidentOriginal = confident(snap.original, r);
        const auto bytes = recordLyrics(r).toUtf8();
        item.credible = credibleLyrics(bytes, snap.original, &item.rejection);
        if (r["coverMatch"].toBool()) {
            Track alias = snap.original;
            alias.title = r["coverTitle"].toString();
            alias.artist = r["coverArtist"].toString();
            QJsonObject copy = r;
            item.confidentCover = !alias.title.trimmed().isEmpty() && confident(alias, copy);
        }
        result.all.push_back(item);
        if (keep && !dropTimed) {
            item.score = matchScore(snap.match, r);
            result.ranked.push_back(item);
        }
    }
    std::stable_sort(result.ranked.begin(), result.ranked.end(),
                     [](const RankItem &a, const RankItem &b) { return a.score > b.score; });
    return result;
}

bool LyricsSearchCoordinator::rankingCurrent(const JobPtr &j) const {
    return j->readyValid && !j->readyCached && j->readyEpoch == j->collectionEpoch &&
           j->readyFpO == trackContext(j->original) && j->readyFpM == trackContext(j->match) &&
           j->readyTimed == j->input.requireTimed;
}

bool LyricsSearchCoordinator::cacheRankingCurrent(const JobPtr &j) const {
    return j->readyValid && j->readyCached && j->readyEpoch == j->cacheEpoch &&
           j->readyFpO == trackContext(j->original) && j->readyFpM == trackContext(j->match) &&
           j->readyTimed == j->input.requireTimed;
}

bool LyricsSearchCoordinator::earlyStopReady(const JobPtr &j) const {
    if(!rankingCurrent(j))return false;
    bool strong=false;QString first;
    for(const auto &item:j->readyAll){
        if(!item.confidentOriginal)continue;
        if(item.record["instrumental"].toBool())return false;
        const auto text=recordLyrics(item.record);
        if(!first.isEmpty()&&!text.isEmpty()&&text!=first)return false;
        if(!text.isEmpty())first=text;
        strong|=!item.record["syncedLyrics"].toString().isEmpty()&&item.credible;
    }
    return strong;
}
bool LyricsSearchCoordinator::awaitingEarlyStopRank(const JobPtr &j) const {
    if(j->manual||rankingCurrent(j))return false;
    // Only a possible timed automatic match merits a per-song ranking barrier.
    // Obviously unrelated metadata must not delay relaxed fallback queries.
    // This check never accepts or stops a job: async credibility and freshness
    // validation still decide whether earlyStopReady can finish it.
    for(const auto &value:j->collected){const auto r=value.toObject();
        if(!r["instrumental"].toBool()&&!r["syncedLyrics"].toString().isEmpty()&&confident(j->original,r))return true;
    }
    return false;
}

void LyricsSearchCoordinator::scheduleRanking(const JobPtr &j) {
    if (!live(j))
        return;
    if (j->scoreInFlight)
        return;
    const bool cached = j->wantAccept && j->acceptCached;
    const QJsonArray payload = cached ? j->cachedPayload : j->collected;
    if (payload.isEmpty() && !j->wantAccept)
        return;
    j->scoreInFlight = true;
    ScoreSnap snap;
    snap.payload = payload;
    snap.original = j->original;
    snap.match = j->match;
    snap.id = j->original.id;
    snap.hash = j->original.hash;
    snap.fpO = trackContext(j->original);
    snap.fpM = trackContext(j->match);
    snap.generation = j->generation;
    snap.epoch = cached ? j->cacheEpoch : j->collectionEpoch;
    snap.cached = cached;
    snap.timedRequired = j->input.requireTimed;
    snap.delayMs = m_options.scoreDelayMs;
    QPointer<LyricsSearchCoordinator> self(this);
    lyricPool()->start(new LyricRunnable([snap, self, j] {
        const auto result = computeRank(snap);
        auto *app = QCoreApplication::instance();
        if (!self || !app)
            return;
        QMetaObject::invokeMethod(
            app,
            [self, j, result] {
                if (self)
                    self->applyRanking(j, result);
            },
            Qt::QueuedConnection);
    }));
}

void LyricsSearchCoordinator::applyRanking(const JobPtr &j, const RankResult &result) {
    j->scoreInFlight = false;
    if (result.dropped || lyricComputeCancel().load())
        return;
    if (!live(j) || result.generation != j->generation || result.generation != m_generation ||
        result.id != j->original.id || result.hash != j->original.hash)
        return;
    const bool cached = j->wantAccept && j->acceptCached;
    const QJsonArray currentPayload = cached ? j->cachedPayload : j->collected;
    const quint64 epoch = cached ? j->cacheEpoch : j->collectionEpoch;
    const auto fpO = trackContext(j->original), fpM = trackContext(j->match);
    const bool timed = j->input.requireTimed;
    if (result.cached != cached || result.epoch != epoch || result.payload != currentPayload ||
        result.fpO != fpO || result.fpM != fpM || result.timedRequired != timed) {
        scheduleRanking(j);
        return;
    }
    j->readyRanked = result.ranked;
    j->readyAll = result.all;
    j->readyEpoch = epoch;
    j->readyValid = true;
    j->readyCached = cached;
    j->readyFpO = fpO;
    j->readyFpM = fpM;
    j->readyTimed = timed;
    if (!current(j)) {
        finishJob(j, "歌曲文件已变化，请重新查找", LyricsSearchState::Invalidated);
        return;
    }
    for (const auto &item : result.all)
        if (item.confidentOriginal && item.credible)
            m_matched.insert(j->original.id);
    if (!result.ranked.isEmpty()) {
        QJsonArray preview;
        for (int n = 0; n < qMin(25, result.ranked.size()); ++n)
            preview.append(result.ranked[n].record);
        setCandidates(j, preview);
    }
    if (j->wantAccept)
        applyAccept(j);
    else
        pumpQueries(j);
}

void LyricsSearchCoordinator::applyAccept(const JobPtr &j) {
    if (!live(j))
        return;
    const bool cached = j->acceptCached;
    if (j->readyTimed && j->readyRanked.isEmpty()) {
        j->wantAccept = false;
        finishJob(j, "未找到滚动歌词 · 原有歌词已保留");
        return;
    }
    QJsonArray ranked;
    for (int i = 0; i < qMin(25, j->readyRanked.size()); ++i)
        ranked.append(j->readyRanked[i].record);
    setCandidates(j, ranked);
    if (!cached && !ranked.isEmpty())
        m_library->saveLookupCache(j->original.id, ranked, false);
    const QString suffix = cached ? QString(" · 使用上次结果") : QString();
    QString rejection;
    bool instrumental = false;
    for (const auto &item : j->readyRanked)
        instrumental |= item.record["instrumental"].toBool();
    auto commit = [&](const RankItem &item) -> bool {
        const auto now = m_library->track(j->original.id);
        if (!now.lyrics.isEmpty() && QFileInfo::exists(m_library->absolute(now.lyrics)))
            return false;
        const auto &r = item.record;
        const bool pure = r["instrumental"].toBool();
        const auto data = pure ? QString("纯音乐，请欣赏").toUtf8() : recordLyrics(r).toUtf8();
        if (!pure && !item.credible) {
            rejection = item.rejection;
            return false;
        }
        const auto status = (pure                                     ? QString("纯音乐 · 已保存标记")
                             : r["syncedLyrics"].toString().isEmpty() ? QString("已保存普通歌词")
                                                                      : QString("已保存滚动歌词")) +
                            suffix + (r["coverMatch"].toBool() ? QString(" · 已匹配原唱") : QString());
        if (store(j->original, data)) {
            ++m_summary.automaticAccepted;
            ++m_saved;
            j->wantAccept = false;
            finishJob(j, status, LyricsSearchState::Saved);
            return true;
        }
        j->wantAccept = false;
        finishJob(j, "未保存：" + m_storeError + suffix, LyricsSearchState::Failed);
        return true;
    };
    if (!j->manual) {
        for (const auto &item : j->readyRanked) {
            const auto &r = item.record;
            if (r["coverMatch"].toBool() || !item.confidentOriginal)
                continue;
            bool ambiguous = false;
            for (const auto &other : j->readyRanked) {
                if (other.record["coverMatch"].toBool() || !other.confidentOriginal)
                    continue;
                if (r["instrumental"].toBool() != other.record["instrumental"].toBool()) {
                    ambiguous = true;
                    break;
                }
                if (recordLyrics(other.record) != recordLyrics(r) &&
                    normalized(r["albumName"].toString()) !=
                        normalized(other.record["albumName"].toString()) &&
                    normalized(j->original.album) != normalized(r["albumName"].toString())) {
                    ambiguous = true;
                    break;
                }
            }
            if (ambiguous)
                break;
            if (commit(item))
                return;
        }
        for (const auto &item : j->readyRanked) {
            if (!item.record["coverMatch"].toBool() || !item.confidentCover)
                continue;
            if (!item.credible) {
                rejection = item.rejection;
                continue;
            }
            if (commit(item))
                return;
        }
    }
    j->wantAccept = false;
    QString status = QString("需选择版本（%1 个）").arg(ranked.size());
    if (instrumental)
        status += " · 需确认纯音乐";
    if (!rejection.isEmpty())
        status += " · " + rejection;
    if (!j->failures.isEmpty())
        status += " · " + j->failures.join("；");
    finishJob(j, status + suffix);
}

void LyricsSearchCoordinator::releaseRequestSlot(QNetworkReply *reply) {
    if (reply->property("lookupSlotReleased").toBool())
        return;
    reply->setProperty("lookupSlotReleased", true);
    const auto host = reply->property("lookupHost").toString();
    m_hostRequests[host] = qMax(0, m_hostRequests.value(host) - 1);
    if (!m_running)
        return;
    const int generation = m_generation;
    QTimer::singleShot(0, this, [this, generation] {
        if (generation != m_generation || !m_running)
            return;
        for (const auto &job : m_jobs.values())
            if (live(job))
                pumpQueries(job);
    });
}

QVector<QJsonObject> LyricsSearchCoordinator::rankCandidates(const Track &query,
                                                             const QVector<QJsonObject> &usable) {
    struct Row {
        QJsonObject record;
        double score;
    };
    QVector<Row> rows;
    rows.reserve(usable.size());
    for (const auto &record : usable)
        rows.push_back({record, matchScore(query, record)});
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.score > b.score; });
    QVector<QJsonObject> out;
    out.reserve(rows.size());
    for (const auto &row : rows)
        out.push_back(row.record);
    return out;
}

} // namespace relaxer
