#include "lyrics-replacement-service.h"
#include "domain/lyric-match.h"
#include "lyrics-document-reader.h"
#include "lyric-text.h"
#include <QCryptographicHash>
#include <QJsonDocument>

namespace relaxer {
namespace {
QByteArray taskContext(TranslationCoordinator *service, qint64 track, QVector<quint64> *ids = nullptr) {
    QJsonArray values;
    if (service) for (const auto &task : service->replacementTasks(track)) {
        values.append(QJsonArray{QString::number(task.taskId), QString::number(task.generation), int(task.state)});
        if (ids) ids->append(task.taskId);
    }
    return QJsonDocument(values).toJson(QJsonDocument::Compact);
}
bool fail(QString *error, const QString &reason) { if (error) *error = reason; return false; }
}
LyricsReplacementPlan LyricsReplacementService::prepare(Library &library, const LyricsSaveSnapshot &snapshot,
    const QByteArray &candidate, const QString &identity, TranslationCoordinator *translation) {
    LyricsReplacementPlan plan; plan.snapshot = snapshot; plan.candidate = candidate; plan.candidateIdentity = identity;
    plan.translationContext = taskContext(translation, snapshot.id, &plan.translationTasks);
    QString error;
    if (!snapshot.valid || !library.lyricsSnapshotMatches(snapshot, &error)) {
        plan.decision.reason = error.isEmpty() ? snapshot.error : error; return plan;
    }
    QByteArray existing;
    // An orphan destination is protected too, even when tracks.lyrics is empty.
    const auto source = snapshot.source.exists ? snapshot.source : snapshot.target;
    if (source.exists) {
        const auto file = LyricsDocumentReader::readFile(source.path);
        if (!file.opened || !file.complete || file.tooLarge ||
            QCryptographicHash::hash(file.bytes, QCryptographicHash::Sha256) != source.digest) {
            plan.decision.reason = QStringLiteral("歌词快照已变化，请重新选择后评估。"); return plan;
        }
        existing = file.bytes;
    }
    plan.decision = LyricsReplacementPolicy::analyze(existing, simplifyTraditionalLyrics(candidate));
    if(snapshot.source.exists && snapshot.target.exists && snapshot.source.path!=snapshot.target.path && snapshot.source.digest!=snapshot.target.digest){
        plan.decision.confirmationRequired=true;
        plan.decision.reason+=QStringLiteral(" 保存位置另有不同歌词文件；明确替换会覆盖该文件。");
    }
    if (!LyricMatchPolicy::rawLyricsAllowed(candidate, &error)) {
        plan.decision.valid = false; plan.decision.reason = error;
    }
    if (!plan.translationTasks.isEmpty()) {
        plan.decision.confirmationRequired = true;
        plan.decision.reason += QStringLiteral(" 当前歌曲有翻译任务或未保存译文；明确替换成功后才会放弃该任务或预览。");
    }
    return plan;
}
bool LyricsReplacementService::commit(Library &library, LyricsReplacementPlan &plan, LyricsReplacementChoice choice,
    qint64 currentTarget, const QString &currentIdentity, TranslationCoordinator *translation, QString *error) {
    if (choice == LyricsReplacementChoice::Cancel || choice == LyricsReplacementChoice::KeepExisting) return false;
    if (plan.consumed || !plan.decision.valid || currentTarget != plan.snapshot.id || currentIdentity != plan.candidateIdentity)
        return fail(error, QStringLiteral("歌曲或候选版本已变化，请重新选择；本次未保存。"));
    if (taskContext(translation, currentTarget) != plan.translationContext)
        return fail(error, QStringLiteral("翻译任务状态已变化，请重新评估替换；本次未保存。"));
    if (plan.decision.confirmationRequired && choice != LyricsReplacementChoice::Replace)
        return fail(error, plan.decision.reason);
    // Policy has already normalized candidate text. Preserve exact saved human
    // translations here; preserveText controls character conversion, not merging.
    if (!library.saveLyricsChecked(currentTarget, plan.decision.content, plan.snapshot, true))
        return fail(error, library.lastError());
    plan.consumed = true;
    if (translation) for (auto id : plan.translationTasks) translation->discard(id);
    if (error) error->clear();
    return true;
}
}
