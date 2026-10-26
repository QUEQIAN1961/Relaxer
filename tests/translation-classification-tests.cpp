#include "lyrics-translation-classification.h"
#include "translation-classification-cases.h"
#include "ai/whole-song-lyrics-translation.h"

#include <QtTest>

using namespace relaxer;
using namespace translation_cases;
using Need = ai::lyrics::TranslationNeed;
using relaxer::TranslationJudgment;

namespace {

TranslationClassificationInput inputFor(const Case &c) {
    TranslationClassificationInput input;
    input.judgment = c.judgment;
    input.instrumental = c.instrumental;
    input.lyricsConfigured = c.configured;
    input.fileExists = c.configured && c.hasFile;
    input.fileReadable = input.fileExists;
    if (input.fileReadable) {
        input.content = ai::lyrics::assessTranslation(c.body, title(), artist());
        input.verifiedBilingual =
            input.content.need == Need::Chinese &&
            LyricsTranslationClassification::verifiedMarkedBilingual(c.body, title(), artist());
    }
    return input;
}

} // namespace

class TranslationClassificationTests : public QObject {
    Q_OBJECT
  private slots:
    // T01-T16: file content decides the same answer for every frontend.
    void contentCases_data() {
        QTest::addColumn<QString>("id");
        QTest::addColumn<int>("judgment");
        QTest::addColumn<bool>("instrumental");
        QTest::addColumn<bool>("configured");
        QTest::addColumn<bool>("hasFile");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<QString>("status");
        QTest::addColumn<bool>("missing");
        QTest::addColumn<bool>("completed");
        for (const auto &c : all())
            QTest::newRow(c.id) << QString::fromUtf8(c.id) << int(c.judgment) << c.instrumental << c.configured
                                << c.hasFile << c.body << QString::fromUtf8(c.status) << c.missing << c.completed;
    }
    void contentCases() {
        QFETCH(QString, id);
        QFETCH(int, judgment);
        QFETCH(bool, instrumental);
        QFETCH(bool, configured);
        QFETCH(bool, hasFile);
        QFETCH(QByteArray, body);
        QFETCH(QString, status);
        QFETCH(bool, missing);
        QFETCH(bool, completed);
        Case c{};
        c.id = "";
        c.name = "";
        c.body = body;
        c.configured = configured;
        c.hasFile = hasFile;
        c.instrumental = instrumental;
        c.judgment = TranslationJudgment(judgment);
        const auto value = LyricsTranslationClassification::decide(inputFor(c));
        QCOMPARE(LyricsTranslationClassification::statusKey(value.status), status);
        QCOMPARE(value.needsTranslation, missing);
        QCOMPARE(value.completed, completed);
        QVERIFY(!(missing && completed));
        QVERIFY(!value.reason.isEmpty());
        QVERIFY2(!id.isEmpty(), "case id");
    }

    // Badges are business wording, not decoration: both frontends show one.
    void badgeCases_data() {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("badge");
        for (const auto &c : all())
            QTest::newRow(c.id) << QString::fromUtf8(c.id) << QString::fromUtf8(c.badge);
    }
    void badgeCases() {
        QFETCH(QString, id);
        QFETCH(QString, badge);
        for (const auto &c : all())
            if (QString::fromUtf8(c.id) == id) {
                QCOMPARE(LyricsTranslationClassification::decide(inputFor(c)).badge, badge);
                return;
            }
        QFAIL("case missing");
    }

    // S01-S10: task lifecycle is layered on top of content, never mixed in.
    void taskStates_data() {
        QTest::addColumn<int>("judgment");
        QTest::addColumn<int>("need");
        QTest::addColumn<bool>("running");
        QTest::addColumn<bool>("unsaved");
        QTest::addColumn<int>("status");
        QTest::addColumn<bool>("missing");
        QTest::addColumn<bool>("completed");
        QTest::addColumn<bool>("keepsResult");
        QTest::addColumn<QString>("badge");

        QTest::newRow("S01-idle") << int(TranslationJudgment::Unknown) << int(Need::Missing) << false << false
                                  << int(TranslationStatus::NeedsTranslation) << true << false << false
                                  << QStringLiteral("待判断");
        QTest::newRow("S02-queued") << int(TranslationJudgment::NeedsTranslation) << int(Need::Missing) << true
                                    << false << int(TranslationStatus::InProgress) << true << false << true
                                    << QStringLiteral("正在翻译");
        QTest::newRow("S03-running") << int(TranslationJudgment::NeedsTranslation) << int(Need::Missing) << true
                                     << false << int(TranslationStatus::InProgress) << true << false << true
                                     << QStringLiteral("正在翻译");
        // S04 failed / S05 cancelled: the task is terminal, so the file decides.
        QTest::newRow("S04-failed") << int(TranslationJudgment::NeedsTranslation) << int(Need::Missing) << false
                                    << false << int(TranslationStatus::NeedsTranslation) << true << false
                                    << false << QStringLiteral("需要翻译");
        QTest::newRow("S05-cancelled") << int(TranslationJudgment::NeedsTranslation) << int(Need::Missing) << false
                                       << false << int(TranslationStatus::NeedsTranslation) << true << false
                                       << false << QStringLiteral("需要翻译");
        QTest::newRow("S06-pending-save") << int(TranslationJudgment::NeedsTranslation) << int(Need::Missing)
                                          << false << true << int(TranslationStatus::PendingSave) << true << false
                                          << true << QStringLiteral("翻译未保存");
        // An unsaved result outranks a stale manual "无需翻译": never hide it.
        QTest::newRow("S06-manual-no") << int(TranslationJudgment::NoTranslationNeeded) << int(Need::Missing)
                                       << false << true << int(TranslationStatus::PendingSave) << true << false
                                       << true << QStringLiteral("翻译未保存");
        QTest::newRow("S07-saved") << int(TranslationJudgment::NeedsTranslation) << int(Need::Translated) << false
                                   << false << int(TranslationStatus::Translated) << false << true << false
                                   << QStringLiteral("翻译完成");
        QTest::newRow("S08-save-failed") << int(TranslationJudgment::NeedsTranslation) << int(Need::Missing)
                                         << false << true << int(TranslationStatus::PendingSave) << true << false
                                         << true << QStringLiteral("翻译未保存");
        QTest::newRow("S10-stale-would-be-verified") << int(TranslationJudgment::Unknown) << int(Need::Chinese)
                                                     << false << false << int(TranslationStatus::NativeChinese)
                                                     << false << false << false << QString();
    }
    void taskStates() {
        QFETCH(int, judgment);
        QFETCH(int, need);
        QFETCH(bool, running);
        QFETCH(bool, unsaved);
        QFETCH(int, status);
        QFETCH(bool, missing);
        QFETCH(bool, completed);
        QFETCH(bool, keepsResult);
        QFETCH(QString, badge);
        TranslationClassificationInput input;
        input.judgment = TranslationJudgment(judgment);
        input.lyricsConfigured = true;
        input.fileExists = true;
        input.fileReadable = true;
        input.content.need = Need(need);
        input.content.reason = QStringLiteral("内容判断");
        input.taskRunning = running;
        input.taskUnsaved = unsaved;
        const auto value = LyricsTranslationClassification::decide(input);
        QCOMPARE(int(value.status), status);
        QCOMPARE(value.needsTranslation, missing);
        QCOMPARE(value.completed, completed);
        QCOMPARE(value.unsavedResult, keepsResult);
        QCOMPARE(value.badge, badge);
    }

    // T03/T04/T14: the marker only counts when every source line keeps a partner.
    void verifiedPairs_data() {
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<bool>("verified");
        QTest::newRow("pairs") << QByteArray("[re:relaxer-bilingual]\n[00:01.00]Hello friend\n[00:01.00]朋友你好\n")
                               << true;
        QTest::newRow("pairs-credit") << QByteArray("[re:relaxer-bilingual]\n[00:00.00]作词：测试作者\n"
                                                    "[00:01.00]Hello friend\n[00:01.00]朋友你好\n")
                                      << true;
        QTest::newRow("marker-only") << QByteArray("[re:relaxer-bilingual]") << false;
        QTest::newRow("missing-partner") << QByteArray("[re:relaxer-bilingual]\n[00:01.00]Hello friend\n") << false;
        QTest::newRow("different-time") << QByteArray("[re:relaxer-bilingual]\n[00:01.00]Hello friend\n"
                                                      "[00:02.00]朋友你好\n")
                                        << false;
        QTest::newRow("extra-unpaired") << QByteArray("[re:relaxer-bilingual]\n[00:01.00]Hello friend\n"
                                                      "[00:01.00]朋友你好\n额外原文\n")
                                        << false;
        QTest::newRow("no-marker") << QByteArray("[00:01.00]Hello friend\n[00:01.00]朋友你好\n") << false;
        QTest::newRow("bom") << QByteArray("\xef\xbb\xbf[re:relaxer-bilingual]\n[00:01.00]Hello friend\n"
                                           "[00:01.00]朋友你好\n")
                             << true;
        // UTF-16 input is not decoded by the marker check (matches the historic
        // verifiedMarkedTranslation behaviour); it is recorded, not regressed.
        QTest::newRow("utc-16") << ([]() {
            QByteArray bytes("\xff\xfe", 2);
            const auto text = QStringLiteral("[re:relaxer-bilingual]\n[00:01.00]Hello friend\n[00:01.00]朋友你好\n");
            const auto units = text.utf16();
            bytes.append(reinterpret_cast<const char *>(units), int(text.size() * 2));
            return bytes;
        }())
                                << false;
    }
    void verifiedPairs() {
        QFETCH(QByteArray, data);
        QFETCH(bool, verified);
        QCOMPARE(LyricsTranslationClassification::verifiedMarkedBilingual(data, title(), artist()), verified);
    }

    // T15: the same track must re-classify once the file changes on disk.
    void externalChangeReclassifies() {
        Case mono{"T02", "", "[00:01.00]Hello my friend\n[00:02.00]This is a synthetic song\n", true, true,
                  false, TranslationJudgment::Unknown, true, false, "needs-translation", ""};
        QCOMPARE(LyricsTranslationClassification::statusKey(
                     LyricsTranslationClassification::decide(inputFor(mono)).status),
                 QStringLiteral("needs-translation"));
        mono.body = "[re:relaxer-bilingual]\n[00:01.00]Hello my friend\n[00:01.00]朋友你好\n"
                    "[00:02.00]This is a synthetic song\n[00:02.00]这是一首合成歌曲\n";
        const auto after = LyricsTranslationClassification::decide(inputFor(mono));
        QCOMPARE(after.status, TranslationStatus::Translated);
        QVERIFY(!after.needsTranslation);
    }
};

QTEST_MAIN(TranslationClassificationTests)
#include "translation-classification-tests.moc"
