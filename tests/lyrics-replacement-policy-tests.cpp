#include "lyrics-replacement-policy.h"
#include "lyrics-replacement-fixture.h"
#include <QtTest>
using namespace relaxer;
using namespace replacement_fixture;
class LyricsReplacementPolicyTests final : public QObject {
    Q_OBJECT
private slots:
    void sharedFixtures(){
        for(const auto &c:cases()){
            const auto p=LyricsReplacementPolicy::analyze(c.old,c.candidate);
            QVERIFY2(p.valid,c.name);QCOMPARE(p.confirmationRequired,c.confirmation);
            QCOMPARE(p.preservesTranslation,c.preserves);QCOMPARE(p.content,c.expected);
        }
    }
    void normalizationAndEffectiveOffset(){
        auto candidate=mono();candidate.replace("00:01.00","00:00.5");candidate.replace("00:02.00","00:01.500");
        candidate.prepend("\xef\xbb\xbf[offset:500]\r\n");candidate.replace("\n","\r\n");
        const auto p=LyricsReplacementPolicy::analyze(bilingual(),candidate);
        QVERIFY(p.equivalent);QVERIFY(p.preservesTranslation);QVERIFY(!p.confirmationRequired);
        QVERIFY(p.content.contains("[offset:500]"));QVERIFY(p.content.contains(QStringLiteral("[00:00.5]世界你好").toUtf8()));
        candidate.replace("offset:500","offset:501");
        QVERIFY(LyricsReplacementPolicy::analyze(bilingual(),candidate).confirmationRequired);
    }
    void structuralAmbiguityNeedsDecision(){
        auto malformed=bilingual();malformed.replace("[00:02.00]走回家\n","");
        auto p=LyricsReplacementPolicy::analyze(malformed,mono());QVERIFY(p.confirmationRequired);QVERIFY(!p.preservesTranslation);
        auto blank=mono();blank.append("[00:03.00]\n");
        QVERIFY(LyricsReplacementPolicy::analyze(bilingual(),blank).confirmationRequired);
        auto mention=mono();mention.replace("Hello world","relaxer-bilingual is a lyric");
        p=LyricsReplacementPolicy::analyze(mention,mono());QVERIFY(!p.existingTranslation);QVERIFY(p.confirmationRequired);
    }
    void plainMarkedAndCandidateTranslation(){
        const auto old=QStringLiteral("[re:relaxer-bilingual]\nHello\n你好\nHome\n回家").toUtf8();
        auto p=LyricsReplacementPolicy::analyze(old,"[ti:New]\nHello\nHome");
        QVERIFY(p.preservesTranslation);QVERIFY(p.content.contains("[ti:New]"));QVERIFY(p.content.contains(QStringLiteral("你好").toUtf8()));
        auto next=bilingual();next.replace("世界你好","不同译文");
        p=LyricsReplacementPolicy::analyze(bilingual(),next);QVERIFY(p.preservesTranslation);
        QVERIFY(p.content.contains(QStringLiteral("世界你好").toUtf8()));QVERIFY(!p.content.contains(QStringLiteral("不同译文").toUtf8()));
    }
    void invalidContent(){QVERIFY(!LyricsReplacementPolicy::analyze({},{}).valid);QVERIFY(!LyricsReplacementPolicy::analyze({},"[ti:Metadata only]").valid);}
    void multiTimestampAndPartialTranslation(){
        const auto old=QStringLiteral("[re:relaxer-bilingual]\n[00:01][00:03]Hello\n[00:01][00:03]你好").toUtf8();
        auto p=LyricsReplacementPolicy::analyze(old,"[00:01.000][00:03.000]Hello");QVERIFY(p.preservesTranslation);QVERIFY(p.content.contains(QStringLiteral("[00:01.000][00:03.000]你好").toUtf8()));
        auto partial=bilingual();partial.replace("[re:relaxer-bilingual]\n","");partial.replace(QStringLiteral("[00:02.00]走回家\n").toUtf8(),"");
        auto candidate=bilingual();candidate.replace(QStringLiteral("世界你好").toUtf8(),QStringLiteral("新候选译文").toUtf8());
        p=LyricsReplacementPolicy::analyze(partial,candidate);QVERIFY(p.preservesTranslation);
        QVERIFY(p.content.contains(QStringLiteral("世界你好").toUtf8()));QVERIFY(p.content.contains(QStringLiteral("走回家").toUtf8()));QVERIFY(p.content.contains("[re:relaxer-bilingual]"));
        p=LyricsReplacementPolicy::analyze(old,"[00:01][00:03]Hello\n[00:01][00:03]Simultaneous original");
        QVERIFY(p.confirmationRequired);QVERIFY(!p.preservesTranslation);QVERIFY(p.content.contains("Simultaneous original"));
    }
    void handoffFilesMatchBusinessFixtures(){
        const QVector<QPair<QString,QByteArray>> fixtures{{"saved-bilingual.lrc",bilingual()},{"candidate-mono.lrc",mono()},{"expected-preserved.lrc",merged()}};
        for(const auto &fixture:fixtures){QFile file(QFINDTESTDATA("fixtures/d2-01/"+fixture.first));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),fixture.second);}
    }
};
QTEST_GUILESS_MAIN(LyricsReplacementPolicyTests)
#include "lyrics-replacement-policy-tests.moc"
