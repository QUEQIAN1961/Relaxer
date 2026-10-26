#include "lyrics-edit-service.h"
#include "lyrics-document-reader.h"
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QStringConverter>
#include <QSet>

namespace relaxer {
namespace {
QString byteText(const QByteArray &data) {
    if (data.startsWith("\xff\xfe") || data.startsWith("\xfe\xff")) {
        QStringDecoder decoder(data.startsWith("\xff\xfe") ? QStringConverter::Utf16LE : QStringConverter::Utf16BE);
        return decoder(data.mid(2));
    }
    // Latin1 is deliberately a reversible byte view, preserving UTF-8 and legacy bytes.
    return QString::fromLatin1(data.startsWith("\xef\xbb\xbf") ? data.mid(3) : data);
}
QString validate(const QByteArray &bytes, int *existing) {
    if (bytes.startsWith("\xff\xfe") || bytes.startsWith("\xfe\xff")) {
        QStringDecoder decoder(bytes.startsWith("\xff\xfe") ? QStringConverter::Utf16LE : QStringConverter::Utf16BE,QStringConverter::Flag::Stateless);
        const QString decoded=decoder(bytes.mid(2));
        if (decoder.hasError() || (bytes.size() % 2)) return QStringLiteral("UTF-16 歌词编码不完整，本次未保存。");
    }
    const auto text = byteText(bytes);
    static const QRegularExpression offset(R"(^[ \t]*\[offset:([+-]?\d+)\][ \t]*\r?$)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression stamp(R"(\[(\d+):(\d{1,2})(?:[.:](\d{1,3}))?\])");
    static const QRegularExpression clockLike(R"(\[\d+[:.])");
    static const QRegularExpression metadata(R"(^\[[A-Za-z][A-Za-z0-9_-]*:[^\[\]]*\]$)");
    int offsets = 0, timed = 0;
    QVector<QVector<qint64>> bilingualRows;
    const bool marked = text.contains("[re:relaxer-bilingual]", Qt::CaseInsensitive);
    for (const auto &raw : text.split('\n')) {
        const auto line = raw.trimmed();
        if (line.isEmpty()) continue;
        if (line.contains("[offset:", Qt::CaseInsensitive)) {
            const auto match = offset.match(raw); bool ok = false;
            const auto value = match.captured(1).toLongLong(&ok);
            if (!match.hasMatch() || !ok || ++offsets > 1 || value < -86400000 || value > 86400000)
                return QStringLiteral("文件 offset 无效或存在多个 offset，无法安全编辑。");
            *existing = int(value); continue;
        }
        if(metadata.match(line).hasMatch())continue;
        auto matches = stamp.globalMatch(line); int end = 0; QVector<qint64> times;
        while (matches.hasNext()) {
            const auto match = matches.next(); bool ok = false;
            const auto minutes = match.captured(1).toLongLong(&ok);
            if (!ok || minutes > 1000000 || match.captured(2).toInt() >= 60 ||
                !line.mid(end,match.capturedStart()-end).trimmed().isEmpty())
                return QStringLiteral("歌词时间戳不规范或超出安全范围，本次未保存。");
            auto fraction = match.captured(3); while (fraction.size() < 3) fraction += '0';
            times.append(minutes*60000 + match.captured(2).toInt()*1000 + fraction.toInt());
            end = int(match.capturedEnd()); ++timed;
        }
        if (clockLike.match(QString(line).remove(stamp)).hasMatch())
            return QStringLiteral("歌词含有无法安全解释的时间戳，本次未保存。");
        if (marked && !line.mid(end).trimmed().isEmpty()) bilingualRows.append(times);
    }
    if (!timed || Lyrics::parse(bytes).instrumental) return QStringLiteral("仅可编辑已保存的滚动歌词；普通文本和纯音乐不处理。");
    if (marked) {
        if (bilingualRows.size() % 2) return QStringLiteral("双语歌词配对不完整，本次未保存。");
        for (int i=0; i<bilingualRows.size(); i+=2)
            if (bilingualRows[i] != bilingualRows[i+1]) return QStringLiteral("双语歌词时间戳不一致，本次未保存。");
    }
    return {};
}
bool fail(QString *error,const QString &reason) { if(error)*error=reason; return false; }
}
QByteArray LyricsEditService::withOffset(const QByteArray &data,int milliseconds) {
    if (!milliseconds) return data; // Historical Widgets semantics: zero preserves.
    auto text = byteText(data);
    if (!QRegularExpression(R"(\[\d+:\d{1,2}(?:[.:]\d{1,3})?\])").match(text).hasMatch()) return data;
    const auto newline = text.contains("\r\n") ? QString("\r\n") : QString("\n");
    text.remove(QRegularExpression(R"(^[ \t]*\[offset:[+-]?\d+\][ \t]*\r?\n?)",
                                  QRegularExpression::MultilineOption|QRegularExpression::CaseInsensitiveOption));
    text = QString("[offset:%1]").arg(milliseconds) + newline + text;
    if (data.startsWith("\xff\xfe") || data.startsWith("\xfe\xff")) {
        QStringEncoder encoder(data.startsWith("\xff\xfe") ? QStringConverter::Utf16LE : QStringConverter::Utf16BE);
        return data.left(2) + encoder(text);
    }
    return (data.startsWith("\xef\xbb\xbf") ? data.left(3) : QByteArray()) + text.toLatin1();
}
LyricsOffsetPlan LyricsEditService::prepareOffset(Library &library,const LyricsSaveSnapshot &snapshot,
                                                 int milliseconds,TranslationCoordinator *translation) {
    LyricsOffsetPlan plan; plan.m_guard.snapshot = snapshot;
    auto reject = [&](const QString &reason) { plan.m_error=reason; plan.m_guard.decision.valid=false; return plan; };
    if (milliseconds < -MaximumOffset || milliseconds > MaximumOffset) return reject(QStringLiteral("文件偏移须为 -5000 至 5000 的整数毫秒。"));
    QString error;
    if (!snapshot.valid || !snapshot.source.exists || !library.lyricsSnapshotMatches(snapshot,&error))
        return reject(error.isEmpty()?QStringLiteral("目标没有可编辑的已保存歌词，或文件已变化。"):error);
    if (snapshot.source.path != snapshot.target.path && snapshot.target.exists && snapshot.source.digest != snapshot.target.digest)
        return reject(QStringLiteral("保存位置另有不同歌词，请先在歌词工作区处理冲突。"));
    const auto file = LyricsDocumentReader::readFile(snapshot.source.path);
    if (!file.opened || !file.complete || file.tooLarge || QCryptographicHash::hash(file.bytes,QCryptographicHash::Sha256)!=snapshot.source.digest)
        return reject(QStringLiteral("歌词读取失败或快照变化，本次未保存。"));
    error = validate(file.bytes,&plan.m_existing); if (!error.isEmpty()) return reject(error);
    const auto bytes = withOffset(file.bytes,milliseconds);
    const auto identity = QString::number(snapshot.id)+":file-offset:"+QString::number(milliseconds);
    plan.m_guard = LyricsReplacementService::prepare(library,snapshot,bytes,identity,translation);
    if (!plan.m_guard.decision.valid) return reject(plan.m_guard.decision.reason);
    // This narrow exception derives exclusively from this snapshot's original bytes.
    // It changes only the global offset, preserving every original/translation row.
    plan.m_guard.decision.content = bytes;
    plan.m_guard.decision.confirmationRequired = !plan.m_guard.translationTasks.isEmpty();
    plan.m_changed = bytes != file.bytes;
    return plan;
}
bool LyricsEditService::commitOffset(Library &library,LyricsOffsetPlan &plan,qint64 currentTarget,
                                    bool discardTranslation,TranslationCoordinator *translation,QString *error) {
    if (!plan.valid() || plan.m_consumed || currentTarget!=plan.trackId())
        return fail(error,plan.error().isEmpty()?QStringLiteral("偏移预览已失效或歌曲已切换，请重新打开编辑器。"):plan.error());
    if (!plan.m_changed) {
        if (!library.lyricsSnapshotMatches(plan.m_guard.snapshot,error)) return false;
        // A no-op never discards tasks or rewrites the file.
        plan.m_consumed=true; if(error)error->clear(); return true;
    }
    if (plan.confirmationRequired() && !discardTranslation)
        return fail(error,QStringLiteral("歌曲有翻译任务或未保存译文，请明确确认后再保存偏移。"));
    const bool ok = LyricsReplacementService::commit(library,plan.m_guard,
        discardTranslation?LyricsReplacementChoice::Replace:LyricsReplacementChoice::SafeSave,
        currentTarget,plan.m_guard.candidateIdentity,translation,error);
    if(ok)plan.m_consumed=true;
    return ok;
}
QVariantMap LyricsInstrumentalBatch::overview() const {
    int safe=0,risk=0,already=0,unavailable=0;
    for(const auto &entry:m_entries) {
        if(entry.state=="ready")++safe; else if(entry.state=="risk")++risk;
        else if(entry.state=="already")++already; else if(entry.state=="unavailable")++unavailable;
    }
    QVariantList riskTargets;
    for(const auto &entry:m_entries)if(entry.state=="risk" && riskTargets.size()<20)
        riskTargets.append(QVariantMap{{"trackKey",QString::number(entry.id)},{"title",entry.title},{"artist",entry.artist},{"reason",entry.plan.decision.reason}});
    return {{"total",m_entries.size()},{"safe",safe},{"risk",risk},{"already",already},{"unavailable",unavailable},{"error",m_error},
        {"riskTargets",riskTargets},{"riskOmitted",qMax(0,risk-int(riskTargets.size()))}};
}
QVariantMap LyricsInstrumentalBatch::result() const {
    QVariantList rows;
    for(const auto &entry:m_entries) rows.append(QVariantMap{{"trackKey",QString::number(entry.id)},{"state",entry.state},{"reason",entry.reason}});
    return {{"total",m_entries.size()},{"succeeded",m_succeeded},{"skipped",m_skipped},{"failed",m_failed},
        {"cancelled",m_cancelled},{"cancelledCount",m_cancelledCount},{"pending",m_entries.size()-m_next},
        {"details",m_details.join('\n')},{"omitted",m_omitted},{"results",rows}};
}
void LyricsInstrumentalBatch::detail(const Entry &entry) {
    if(m_details.size()<8)m_details.append(entry.title.left(128)+" ["+QString::number(entry.id)+"]："+entry.reason.left(512));
    else ++m_omitted;
}
LyricsInstrumentalBatch LyricsEditService::prepareInstrumental(Library &library,const QVector<qint64> &ids,
    TranslationCoordinator *translation,const QHash<qint64,LyricsSaveSnapshot> &snapshots) {
    LyricsInstrumentalBatch batch;
    if(ids.isEmpty() || ids.size()>MaximumBatch){batch.m_error=QStringLiteral("请选择1至1000首歌曲；更多歌曲请分批处理。");return batch;}
    QSet<qint64> seen;
    for(auto id:ids) {
        if(seen.contains(id))continue;
        seen.insert(id);
        LyricsInstrumentalBatch::Entry entry;entry.id=id;entry.title=library.track(id).title;entry.artist=library.track(id).artist;
        const auto snapshot=snapshots.contains(id)?snapshots.value(id):library.lyricsSaveSnapshot(id);
        entry.plan=LyricsReplacementService::prepare(library,snapshot,QStringLiteral("纯音乐，请欣赏").toUtf8(),QString::number(id)+":instrumental",translation);
        if(!entry.plan.decision.valid){entry.state="unavailable";entry.reason=entry.plan.decision.reason;}
        else {
            const auto file=LyricsDocumentReader::readFile(snapshot.source.exists?snapshot.source.path:snapshot.target.path);
            if(file.opened && file.complete && Lyrics::parse(file.bytes).instrumental && library.isInstrumental(id))entry.state="already";
            else entry.state=entry.plan.decision.confirmationRequired?"risk":"ready";
        }
        batch.m_entries.append(std::move(entry));
    }
    return batch;
}
bool LyricsEditService::stepInstrumental(Library &library,LyricsInstrumentalBatch &batch,bool replaceRisks,TranslationCoordinator *translation) {
    if(!batch.valid() || batch.finished() || batch.m_cancelled)return false;
    auto &entry=batch.m_entries[batch.m_next++];
    if(entry.state=="already" && (!library.lyricsSnapshotMatches(entry.plan.snapshot,&entry.reason)||!library.isInstrumental(entry.id))) {
        entry.state="failed";if(entry.reason.isEmpty())entry.reason=QStringLiteral("纯音乐状态已变化，请重新评估");++batch.m_failed;batch.detail(entry);
    }
    else if(entry.state=="already") {entry.state="skipped";entry.reason=QStringLiteral("已经是纯音乐");++batch.m_skipped;}
    else if(entry.state=="risk" && !replaceRisks){entry.state="skipped";entry.reason=QStringLiteral("未授权覆盖已有歌词、译文或翻译任务");++batch.m_skipped;batch.detail(entry);}
    else if(entry.state=="unavailable"){entry.state="failed";++batch.m_failed;batch.detail(entry);}
    else if(LyricsReplacementService::commit(library,entry.plan,replaceRisks?LyricsReplacementChoice::Replace:LyricsReplacementChoice::SafeSave,
        entry.id,QString::number(entry.id)+":instrumental",translation,&entry.reason)) {entry.state="succeeded";++batch.m_succeeded;}
    else {entry.state="failed";if(entry.reason.isEmpty())entry.reason=entry.plan.decision.reason;++batch.m_failed;batch.detail(entry);}
    return true;
}
void LyricsEditService::cancelInstrumental(LyricsInstrumentalBatch &batch) {
    if(batch.finished())return;
    batch.m_cancelled=true;
    while(!batch.finished()) {auto &entry=batch.m_entries[batch.m_next++];entry.state="cancelled";entry.reason=QStringLiteral("用户取消，尚未开始保存");++batch.m_skipped;++batch.m_cancelledCount;}
}
}
