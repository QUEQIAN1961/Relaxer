#include "c4c5-ui-fixture.h"
#include <QAbstractItemModelTester>
using namespace c4c5_fixture;
namespace {
class WorkspaceReply final : public QNetworkReply {
public:
    WorkspaceReply(const QNetworkRequest &request, QObject *parent) : QNetworkReply(parent) {
        setRequest(request); setUrl(request.url()); open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this] { if (!isFinished()) { setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
            setFinished(true); emit readyRead(); emit finished(); } });
    }
    void abort() override { if (isFinished()) return; setError(OperationCanceledError, "cancelled"); setFinished(true); emit finished(); }
    qint64 bytesAvailable() const override { return 2 - m_offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *data, qint64 size) override { const auto n = qMin(size, 2 - m_offset); if (n <= 0) return -1;
        memcpy(data, "[]" + m_offset, size_t(n)); m_offset += n; return n; }
private: qint64 m_offset = 0;
};
class WorkspaceNetwork final : public QNetworkAccessManager {
public: int calls = 0;
protected: QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override {
        ++calls; return new WorkspaceReply(request, this);
    }
};
QJsonObject candidate(const Track &track) {
    return {{"id", 4501}, {"trackName", track.title}, {"artistName", track.artist}, {"albumName", track.album},
        {"duration", 60}, {"syncedLyrics", "[00:01.00]Candidate first\n[00:04.00]Candidate second\n"}, {"source", "Controlled fixture"}};
}
struct WorkspaceFixture {
    QTemporaryDir dir;
    Library library{dir.filePath("data")}; TranslationHttpMock http;
    std::unique_ptr<AiRuntime> runtime;
    std::unique_ptr<PlaybackFacade> player;
    std::unique_ptr<LyricsModel> lyrics;
    std::unique_ptr<TranslationCoordinator> translation;
    std::unique_ptr<TranslationBridge> bridge;
    Track a, b; bool ok = false;
    const QByteArray sourceA = "[00:01.00]Playing song first\n[00:04.00]Playing song second\n";
    const QByteArray sourceB = "[00:01.00]Selected song first\n[00:04.00]Selected song second\n";
    WorkspaceFixture() {
        if (!library.open() || !http.listen()) return;
        a = seed(library, "Playing", "playing"); b = seed(library, "Selected", "selected");
        if (!a.id || !b.id || !wave(library.absolute(a.file)) || !wave(library.absolute(b.file)) ||
            !library.saveLyrics(a.id, sourceA) || !library.saveLyrics(b.id, sourceB)) return;
        runtime = std::make_unique<AiRuntime>(&library, DeepSeekProvider::MockEndpoint{http.baseUrl()}, std::make_unique<FakeCredentialStore>());
        if (!configureRuntime(*runtime)) return;
        player = std::make_unique<PlaybackFacade>(&library, nullptr, true);
        lyrics = std::make_unique<LyricsModel>(&library, player.get());
        translation = std::make_unique<TranslationCoordinator>(&library, runtime.get());
        bridge = std::make_unique<TranslationBridge>(&library, player.get(), translation.get(), lyrics.get());
        player->play({a.id, b.id}, a.id); ok = true;
    }
    QByteArray bytes(qint64 id) { return LyricsDocumentReader::readFile(library.absolute(library.track(id).lyrics)).bytes; }
    void response() { http.enqueue(completion(QString::fromUtf8(translationJson({{0, "选择歌曲第一行"}, {1, "选择歌曲第二行"}})))); }
};
}
class LyricsWorkspaceParityTests : public QObject {
    Q_OBJECT
private slots:
    void cachedCandidateTargetsSelectionWithoutChangingPlayback() {
        WorkspaceFixture f; QVERIFY(f.ok); QTRY_COMPARE(f.lyrics->trackKey(), QString::number(f.a.id));
        QVERIFY(f.library.saveLookupCache(f.b.id, QJsonArray{candidate(f.b)}, false));
        WorkspaceNetwork network; LyricsSearchCoordinator service(&f.library, nullptr, QUrl("https://mock.invalid/api"), {}, &network);
        LyricsSearchBridge search(&f.library, f.player.get(), &service);
        QAbstractItemModelTester tester(&search, QAbstractItemModelTester::FailureReportingMode::QtTest);
        const auto queue = f.player->queue().items;
        QVERIFY(search.setTargetTrack(QString::number(f.b.id))); QCOMPARE(search.targetTrackKey(), QString::number(f.b.id));
        QCOMPARE(search.targetTitle(), f.b.title); QVERIFY(search.sourceText().contains("Selected song"));
        QCOMPARE(search.rowCount(), 1); QCOMPARE(network.calls, 0); QVERIFY(search.select(0)); const auto key = search.selectedKey();
        QVERIFY(!search.saveSelected(key, false)); QCOMPARE(f.bytes(f.b.id), f.sourceB);
        QSignalSpy replacement(&search,&LyricsSearchBridge::replacementRequested);
        QVERIFY(!search.saveSelected(key, true));QCOMPARE(replacement.size(),1);
        QCOMPARE(f.bytes(f.b.id),f.sourceB);QVERIFY(search.resolveReplacement(replacement.first()[0].toString(),1));
        QVERIFY(f.bytes(f.b.id).contains("Candidate first"));
        QCOMPARE(f.bytes(f.a.id), f.sourceA); QCOMPARE(f.player->current().id, f.a.id); QCOMPARE(f.player->queue().items, queue);
        QCOMPARE(f.lyrics->trackKey(), QString::number(f.a.id)); QVERIFY(!f.lyrics->preview());
        QVERIFY(search.setTargetTrack(QString::number(f.a.id))); QVERIFY(!search.saveSelected(key, true));
        QVERIFY(search.setTargetTrack("0")); QCOMPARE(search.targetTrackKey(), QString::number(f.a.id));
        service.shutdown();
    }
    void selectedTrackTranslationHasIndependentPreviewAndSave() {
        WorkspaceFixture f; QVERIFY(f.ok); QTRY_COMPARE(f.lyrics->trackKey(), QString::number(f.a.id)); const auto queue = f.player->queue().items;
        auto &bridge = *f.bridge; QVERIFY(bridge.setTargetTrack(QString::number(f.b.id))); QVERIFY(bridge.classify(1)); f.response();
        const auto task = bridge.prepareAi(); QVERIFY(!task.isEmpty()); QCOMPARE(f.http.requestCount(), 0); QVERIFY(bridge.start(task));
        QTRY_VERIFY(bridge.preview()); QCOMPARE(f.http.requestCount(), 1); QVERIFY(bridge.previewText().contains("选择歌曲第一行"));
        QVERIFY(bridge.previewText().contains("Selected song first")); QCOMPARE(bridge.translationText(), QString("选择歌曲第一行\n选择歌曲第二行"));
        QCOMPARE(f.player->current().id, f.a.id); QCOMPARE(f.player->queue().items, queue); QVERIFY(!f.lyrics->preview()); QCOMPARE(f.bytes(f.b.id), f.sourceB);
        QVERIFY(bridge.setTargetTrack(QString::number(f.a.id))); QCOMPARE(bridge.state(), QString("NoTask"));
        QVERIFY(bridge.setTargetTrack(QString::number(f.b.id))); QVERIFY(bridge.preview()); QVERIFY(bridge.save());
        QVERIFY(f.bytes(f.b.id).contains("relaxer-bilingual")); QCOMPARE(f.bytes(f.a.id), f.sourceA); QCOMPARE(f.player->current().id, f.a.id);
        QVERIFY(!f.lyrics->preview()); QVERIFY(bridge.setTargetTrack("0")); QCOMPARE(bridge.targetTrackKey(), QString::number(f.a.id));
    }
    void targetSwitchRevokesConsentAndKeepsRunningResult() {
        WorkspaceFixture f; QVERIFY(f.ok); auto &bridge = *f.bridge;
        QVERIFY(bridge.setTargetTrack(QString::number(f.b.id))); QVERIFY(bridge.classify(1)); const auto abandoned = bridge.prepareAi(); QVERIFY(!abandoned.isEmpty());
        QVERIFY(bridge.setTargetTrack(QString::number(f.a.id))); QVERIFY(!bridge.start(abandoned)); QCOMPARE(f.http.requestCount(), 0);
        QVERIFY(bridge.setTargetTrack(QString::number(f.b.id))); f.http.setHold(true); f.response(); const auto task = bridge.prepareAi(); QVERIFY(bridge.start(task));
        QTRY_COMPARE(f.http.heldCount(), 1); QVERIFY(bridge.setTargetTrack(QString::number(f.a.id))); QVERIFY(f.http.releaseHeld());
        QTRY_COMPARE(f.translation->summary(task.toULongLong()).state, TranslationState::CompletedPendingSave);
        QCOMPARE(bridge.state(), QString("NoTask")); QVERIFY(!f.lyrics->preview());
        QVERIFY(bridge.setTargetTrack(QString::number(f.b.id))); QVERIFY(bridge.preview()); QCOMPARE(f.http.requestCount(), 1);
        QVERIFY(bridge.discard()); QCOMPARE(f.bytes(f.a.id), f.sourceA); QCOMPARE(f.bytes(f.b.id), f.sourceB);
    }
    void explicitTargetSurvivesPlaybackSwitchAndFallbackFollows() {
        WorkspaceFixture f; QVERIFY(f.ok); auto &bridge = *f.bridge;
        QVERIFY(bridge.setTargetTrack(QString::number(f.b.id))); QVERIFY(bridge.classify(1)); const auto task = bridge.prepareAi(); QVERIFY(!task.isEmpty());
        f.player->play({f.a.id, f.b.id}, f.b.id); f.player->play({f.a.id, f.b.id}, f.a.id);
        QCOMPARE(bridge.targetTrackKey(), QString::number(f.b.id)); QCOMPARE(bridge.consentTask(), task);
        bridge.declineConsent(); QVERIFY(!bridge.start(task)); QCOMPARE(f.http.requestCount(), 0);
        QVERIFY(bridge.setTargetTrack("0")); f.player->play({f.a.id, f.b.id}, f.b.id);
        QCOMPARE(bridge.targetTrackKey(), QString::number(f.b.id)); QVERIFY(!bridge.setTargetTrack("invalid")); QVERIFY(!bridge.setTargetTrack("999999"));
    }
    void missingBatchAndManualSaveRemainSeparateFromPlayingSong() {
        WorkspaceFixture f; QVERIFY(f.ok); const auto c = seed(f.library, "Missing", "missing"); QVERIFY(c.id);
        WorkspaceNetwork network; LyricsSearchCoordinator service(&f.library, nullptr, QUrl("https://mock.invalid/api"), {}, &network);
        LyricsSearchBridge search(&f.library, f.player.get(), &service);
        QVERIFY(search.setTargetTracks({QString::number(f.a.id), QString::number(c.id), QString::number(c.id)}, true));
        QCOMPARE(search.targetTracks().size(), 1); QCOMPARE(search.targetTrackKey(), QString::number(c.id));
        QVERIFY(!search.searchTargets(false)); QCOMPARE(network.calls, 0); QVERIFY(search.searchTargets(true)); QTRY_VERIFY(network.calls > 0); search.stop();
        QVERIFY(!search.saveText("[00:01.00]Local text\n", false)); QVERIFY(f.library.track(c.id).lyrics.isEmpty());
        QVERIFY(search.saveText("[00:01.00]Local text\n", true)); QVERIFY(search.sourceText().contains("Local text"));
        QVERIFY(!search.markInstrumental(false));QSignalSpy replacement(&search,&LyricsSearchBridge::replacementRequested);
        QVERIFY(!search.markInstrumental(true));QCOMPARE(replacement.size(),1);QVERIFY(!f.library.isInstrumental(c.id));
        QVERIFY(search.resolveReplacement(replacement.first()[0].toString(),1));QVERIFY(f.library.isInstrumental(c.id));
        QCOMPARE(f.player->current().id, f.a.id); QCOMPARE(f.bytes(f.a.id), f.sourceA); QCOMPARE(f.http.requestCount(), 0); service.shutdown();
    }
};
int main(int argc, char **argv) { QGuiApplication app(argc, argv); LyricsWorkspaceParityTests tests; return QTest::qExec(&tests, argc, argv); }
#include "qml-lyrics-workspace-parity-tests.moc"
