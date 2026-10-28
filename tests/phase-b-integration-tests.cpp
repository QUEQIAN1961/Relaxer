#include "integration-fixture.h"
#include "window.h"
#include <QElapsedTimer>
#include <QThreadPool>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
// clang-format on
#endif
using namespace relaxer;
using namespace integration_fixture;
namespace {
QJsonObject resources() {
    QJsonObject value{{"sqliteConnections", QSqlDatabase::connectionNames().size()}};
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS memory{};
    memory.cb = sizeof(memory);
    DWORD handles = 0;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory)))
        value["workingSetBytes"] = double(memory.WorkingSetSize);
    if (GetProcessHandleCount(GetCurrentProcess(), &handles))
        value["handles"] = int(handles);
    FILETIME created, exited, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        ULARGE_INTEGER k{}, u{};
        k.LowPart = kernel.dwLowDateTime;
        k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user.dwLowDateTime;
        u.HighPart = user.dwHighDateTime;
        value["cpuSeconds"] = double(k.QuadPart + u.QuadPart) / 10000000.;
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    int threads = 0;
    if (Thread32First(snapshot, &entry))
        do {
            if (entry.th32OwnerProcessID == GetCurrentProcessId())
                ++threads;
        } while (Thread32Next(snapshot, &entry));
    CloseHandle(snapshot);
    value["threads"] = threads;
#endif
    return value;
}
void record(const QString &name, QJsonObject value) {
    qInfo().noquote() << name << QJsonDocument(value).toJson(QJsonDocument::Compact);
    const auto folder = qEnvironmentVariable("RELAXER_INTEGRATION_EVIDENCE");
    if (!folder.isEmpty()) {
        QDir().mkpath(folder);
        QFile file(QDir(folder).filePath(name + ".json"));
        if (file.open(QIODevice::WriteOnly))
            file.write(QJsonDocument(value).toJson());
    }
}
void keepOnClose(LyricsLookup &dialog) {
    QTimer::singleShot(0, &dialog, [&dialog] {
        auto *box = dialog.findChild<QMessageBox *>("lyricsUnsavedTranslationCloseConfirm");
        QVERIFY(box);
        box->findChild<QPushButton *>("lyricsKeepTranslationPreview")->click();
    });
}
} // namespace
class PhaseBIntegrationTests final : public QObject {
    Q_OBJECT
  private slots:
    void playbackAndSearchKeepIndependentOwnership() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        QCOMPARE(ids.size(), 3);
        TranslationHttpMock source, paid;
        QVERIFY(source.listen());
        QVERIFY(paid.listen());
        answers(source, {candidate(library.track(ids[1])), candidate(library.track(ids[2]))});
        source.setHold(true);
        AiRuntime runtime(&library, DeepSeekProvider::MockEndpoint{paid.baseUrl()},
                          std::make_unique<FakeCredentialStore>());
        QVERIFY(configureRuntime(runtime));
        Window window(&library);
        window.show();
        auto *player = window.playbackFacade();
        player->play(ids);
        QTRY_VERIFY(player->playing());
        QTRY_VERIFY(player->position() > 0);
        LyricsLookup dialog(&library, {ids[1], ids[2]}, &window, source.baseUrl(), {}, true, &runtime);
        dialog.show();
        select(dialog, ids[1]);
        auto *search = dialog.findChild<LyricsSearchCoordinator *>();
        QVERIFY(search);
        QSignalSpy candidates(search, &LyricsSearchCoordinator::candidatesChanged);
        const auto generation = search->start(requests(library, {ids[1], ids[2]}), {5000, 2000, 20, 0});
        QVERIFY(generation);
        QTRY_VERIFY(source.heldCount() > 0);
        player->next();
        QCOMPARE(player->current().id, ids[1]);
        QTRY_VERIFY(player->playing());
        source.setHold(false);
        while (source.releaseHeld()) {
        }
        QTRY_VERIFY(!search->candidates(ids[1]).records.isEmpty());
        QTRY_VERIFY(!search->candidates(ids[2]).records.isEmpty());
        QCOMPARE(search->candidates(ids[1]).trackId, ids[1]);
        QCOMPARE(search->candidates(ids[2]).trackId, ids[2]);
        QCOMPARE(search->candidates(ids[1]).records.first().toObject()["trackName"].toString(),
                 library.track(ids[1]).title);
        search->stop();
        const auto revision = search->candidates(ids[1]).revision;
        QTRY_COMPARE(LyricsSearchCoordinator::computePool()->activeThreadCount(), 0);
        QCoreApplication::processEvents();
        QCOMPARE(search->candidates(ids[1]).revision, revision);
        auto *versions = dialog.findChild<QListWidget *>("lyricsVersions");
        QTRY_VERIFY(versions->count() > 0);
        versions->setCurrentRow(0);
        auto *apply = dialog.findChild<QPushButton *>("lyricsApply");
        QTRY_VERIFY(apply->isEnabled());
        apply->click();
        QVERIFY(bytes(library.absolute(library.track(ids[1]).lyrics)).contains("Hello world"));
        QCOMPARE(player->queue().items, ids);
        QVERIFY(player->save());
        QCOMPARE(library.setting("current").toInt(), int(ids[1]));
        QCOMPARE(paid.requestCount(), 0);
        QVERIFY(dialog.close());
        QVERIFY(window.close());
        record("playback-search", {{"searchGeneration", double(generation)},
                                   {"candidateSignals", candidates.size()},
                                   {"mockSearchRequests", source.requestCount()},
                                   {"paidRequests", paid.requestCount()}});
    }
    void playbackAutomaticAdvancePreservesPaidPreview() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        const QByteArray original = "[00:01.00]Hello world";
        QVERIFY(library.saveLyrics(ids[1], original));
        TranslationHttpMock paid;
        QVERIFY(paid.listen());
        paid.setHold(true);
        paid.enqueue(completion(QString::fromUtf8(translationJson({{0, QStringLiteral("你好世界")}}))));
        AiRuntime runtime(&library, DeepSeekProvider::MockEndpoint{paid.baseUrl()},
                          std::make_unique<FakeCredentialStore>());
        QVERIFY(configureRuntime(runtime));
        Window window(&library);
        window.show();
        auto *player = window.playbackFacade();
        player->play(ids);
        QTRY_VERIFY(player->seekable());
        LyricsLookup dialog(&library, {ids[1]}, &window, QUrl("http://127.0.0.1:1"), {}, true, &runtime);
        dialog.show();
        dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);
        select(dialog, ids[1]);
        QSignalSpy translation(dialog.findChild<TranslationCoordinator *>(),
                               &TranslationCoordinator::taskChanged);
        consent(dialog);
        QTRY_COMPARE(paid.heldCount(), 1);
        QMetaObject::invokeMethod(player->backend()->media(), "mediaStatusChanged", Qt::DirectConnection,
                                  Q_ARG(QMediaPlayer::MediaStatus, QMediaPlayer::EndOfMedia));
        QCOMPARE(player->current().id, ids[1]);
        QVERIFY(paid.releaseHeld());
        auto *save = dialog.findChild<QPushButton *>("lyricsSaveTranslation");
        QTRY_VERIFY(save->isEnabled());
        const auto preview = dialog.findChild<QPlainTextEdit *>("lyricsTranslationPreview")->toPlainText();
        QTRY_VERIFY(player->seekable());
        QMetaObject::invokeMethod(player->backend()->media(), "mediaStatusChanged", Qt::DirectConnection,
                                  Q_ARG(QMediaPlayer::MediaStatus, QMediaPlayer::EndOfMedia));
        QCOMPARE(player->current().id, ids[2]);
        QVERIFY(dialog.findChildren<QMessageBox *>().isEmpty());
        QCOMPARE(dialog.findChild<QPlainTextEdit *>("lyricsTranslationPreview")->toPlainText(), preview);
        QVERIFY(save->isEnabled());
        keepOnClose(dialog);
        QVERIFY(!dialog.close());
        QVERIFY(save->isEnabled());
        QTRY_VERIFY(player->playing());
        save->click();
        QVERIFY(!save->isEnabled());
        const auto saved = bytes(library.absolute(library.track(ids[1]).lyrics));
        QVERIFY(saved.contains("relaxer-bilingual"));
        QVERIFY(saved.contains(original));
        QVERIFY(library.track(ids[0]).lyrics.isEmpty());
        QVERIFY(library.track(ids[2]).lyrics.isEmpty());
        QCOMPARE(paid.requestCount(), 1);
        QVERIFY(dialog.close());
        QVERIFY(window.close());
        record("playback-ai",
               {{"taskEvents", translation.size()}, {"paidRequests", 1}, {"owner", double(ids[1])}});
    }
    void searchAndTranslationRefreshCancelSaveAndReselect() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        const QByteArray original = "[00:01.00]Hello world\n[00:02.00]Second line";
        QVERIFY(library.saveLyrics(ids[1], original));
        TranslationHttpMock source, paid;
        QVERIFY(source.listen());
        QVERIFY(paid.listen());
        answers(source, {candidate(library.track(ids[1]))});
        source.setHold(true);
        paid.enqueue(completion(QString::fromUtf8(
            translationJson({{0, QStringLiteral("你好世界")}, {1, QStringLiteral("第二行")}}))));
        AiRuntime runtime(&library, DeepSeekProvider::MockEndpoint{paid.baseUrl()},
                          std::make_unique<FakeCredentialStore>());
        QVERIFY(configureRuntime(runtime));
        PlaybackFacade player(&library, nullptr, true);
        player.play(ids);
        LyricsLookup dialog(&library, {ids[1]}, nullptr, source.baseUrl(), {}, true, &runtime);
        dialog.show();
        dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);
        select(dialog, ids[1]);
        auto *search = dialog.findChild<LyricsSearchCoordinator *>();
        const auto generation = search->start(requests(library, {ids[1]}), {5000, 2000, 20, 0});
        QTRY_VERIFY(source.heldCount() > 0);
        consent(dialog);
        auto *save = dialog.findChild<QPushButton *>("lyricsSaveTranslation");
        QTRY_VERIFY(save->isEnabled());
        const auto preview = dialog.findChild<QPlainTextEdit *>("lyricsTranslationPreview")->toPlainText();
        dialog.findChild<QCheckBox *>("lyricsMissingTranslationOnly")->setChecked(true);
        dialog.findChild<QPushButton *>("lyricsRefreshTranslationFilter")->click();
        source.setHold(false);
        while (source.releaseHeld()) {
        }
        QTRY_VERIFY(!search->candidates(ids[1]).records.isEmpty());
        QCOMPARE(search->summary().generation, generation); // Local judgment refresh starts no search.
        search->stop();
        QCOMPARE(dialog.findChild<QPlainTextEdit *>("lyricsTranslationPreview")->toPlainText(), preview);
        QVERIFY(save->isEnabled());
        QCOMPARE(search->summary().generation, generation + 1); // stop invalidates queued old callbacks.
        QVERIFY(!search->summary().running);
        save->click();
        const auto bilingual = bytes(library.absolute(library.track(ids[1]).lyrics));
        QVERIFY(bilingual.contains("relaxer-bilingual"));
        dialog.findChild<QCheckBox *>("lyricsMissingTranslationOnly")->setChecked(false);
        select(dialog, ids[1]);
        auto *versions = dialog.findChild<QListWidget *>("lyricsVersions");
        QTRY_VERIFY(versions->count() > 0);
        versions->setCurrentRow(0);
        QTimer::singleShot(0, &dialog, [&] {
            auto *box = dialog.findChild<QMessageBox *>("lyricsReplaceConfirm");
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>("lyricsApply")->click();
        QCOMPARE(bytes(library.absolute(library.track(ids[1]).lyrics)), bilingual);
        QCOMPARE(player.queue().items, ids);
        QCOMPARE(paid.requestCount(), 1);
        QVERIFY(dialog.close());
        record("search-ai", {{"generation", double(generation)},
                             {"mockSearchRequests", source.requestCount()},
                             {"paidRequests", 1},
                             {"preservedBilingualBytes", bilingual.size()}});
    }
    void importAndPlayback_data() {
        QTest::addColumn<bool>("cancel");
        QTest::newRow("complete-duplicates-playlist") << false;
        QTest::newRow("cancel-at-worker-barrier") << true;
    }
    void importAndPlayback() {
        QFETCH(bool, cancel);
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        PlaybackFacade player(&library, nullptr, true);
        player.play(ids);
        QTRY_VERIFY(player.playing());
        QStringList inputs;
        for (int n = 0; n < 8; ++n) {
            const auto path = dir.filePath(QString("import-%1.wav").arg(n));
            QVERIFY(wave(path, 100 + n, 1000));
            inputs << path;
        }
        const auto playlist = library.createPlaylist("integration imports");
        GatedImport importer(&library);
        const auto release = qScopeGuard([&] { importer.gate->released.release(100); });
        QSignalSpy finished(&importer, &ImportCoordinator::finished);
        connect(&importer, &ImportCoordinator::trackCommitted, this,
                [&](quint64, qint64, bool) { QCOMPARE(QThread::currentThread(), library.thread()); });
        const auto task = importer.start({inputs, playlist, 1});
        QVERIFY(task);
        QTRY_VERIFY(importer.gate->entered.available() > 0);
        player.next();
        QCOMPARE(player.current().id, ids[1]);
        if (cancel)
            QVERIFY(importer.cancel(task));
        importer.gate->released.release();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 20000);
        const auto summary = qvariant_cast<ImportSummary>(finished.first().first());
        QCOMPARE(summary.taskId, task);
        QCOMPARE(summary.cancelled, cancel);
        if (cancel) {
            QCOMPARE(summary.added, 0);
            QCOMPARE(library.tracks().size(), 3);
        } else {
            QCOMPARE(summary.added, 8);
            QCOMPARE(library.tracks(playlist).size(), 8);
            QVERIFY(importer.start({inputs, playlist, 1}));
            QTRY_VERIFY(importer.gate->entered.available() > 1);
            importer.gate->released.release();
            QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 20000);
            QCOMPARE(qvariant_cast<ImportSummary>(finished.last().first()).duplicates, 8);
            QCOMPARE(library.tracks().size(), 11);
        }
        for (const auto &track : library.tracks())
            QVERIFY(QFileInfo::exists(library.absolute(track.file)));
        QCOMPARE(player.queue().items, ids);
        QVERIFY(player.save());
        importer.shutdown();
        QTRY_VERIFY(importer.isIdle());
        record(cancel ? "import-cancel" : "import-playback", {{"taskId", double(task)},
                                                              {"added", summary.added},
                                                              {"cancelled", summary.cancelled},
                                                              {"queueSize", player.queue().items.size()}});
    }
    void allServicesOverlapAtExplicitBarriers() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library, 20);
        QCOMPARE(ids.size(), 20);
        QVERIFY(library.saveLyrics(ids[2], "[00:01.00]Hello world"));
        Window window(&library);
        window.show();
        auto *player = window.playbackFacade();
        player->play(ids);
        QTRY_VERIFY(player->playing());
        QStringList inputs;
        for (int n = 0; n < 20; ++n) {
            const auto path = dir.filePath(QString("extra-%1.wav").arg(n));
            QVERIFY(wave(path, n + 200, 1000));
            inputs << path;
        }
        GatedImport importer(&library);
        const auto release = qScopeGuard([&] { importer.gate->released.release(100); });
        QSignalSpy imported(&importer, &ImportCoordinator::finished);
        QVERIFY(importer.start({inputs, 0, 2}));
        QTRY_VERIFY(importer.gate->entered.available() > 0);
        TranslationHttpMock source, backup, paid;
        QVERIFY(source.listen());
        QVERIFY(backup.listen());
        QVERIFY(paid.listen());
        source.setHold(true);
        backup.setHold(true);
        paid.setHold(true);
        answers(source, {candidate(library.track(ids[1]))});
        answers(backup, {candidate(library.track(ids[1]))});
        paid.enqueue(completion(QString::fromUtf8(translationJson({{0, QStringLiteral("你好世界")}}))));
        AiRuntime runtime(&library, DeepSeekProvider::MockEndpoint{paid.baseUrl()},
                          std::make_unique<FakeCredentialStore>());
        QVERIFY(configureRuntime(runtime));
        LyricsLookup dialog(&library, {ids[1], ids[2]}, &window, source.baseUrl(), backup.baseUrl(), true,
                            &runtime);
        dialog.show();
        dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);
        select(dialog, ids[2]);
        auto *search = dialog.findChild<LyricsSearchCoordinator *>();
        QVERIFY(search->sources().size() >= 2);
        search->start(requests(library, {ids[1]}), {8000, 3000, 20, 0});
        consent(dialog);
        QTRY_COMPARE(paid.heldCount(), 1);
        QTRY_VERIFY(source.heldCount() + backup.heldCount() > 0);
        int heartbeats = 0;
        QTimer heartbeat;
        connect(&heartbeat, &QTimer::timeout, this, [&] { ++heartbeats; });
        heartbeat.start(5);
        QTRY_VERIFY(heartbeats > 1);
        player->next();
        QCOMPARE(player->current().id, ids[1]);
        QVERIFY(paid.releaseHeld());
        auto *save = dialog.findChild<QPushButton *>("lyricsSaveTranslation");
        QTRY_VERIFY(save->isEnabled());
        source.setHold(false);
        backup.setHold(false);
        while (source.releaseHeld()) {
        }
        while (backup.releaseHeld()) {
        }
        QTRY_VERIFY(!search->candidates(ids[1]).records.isEmpty());
        search->stop();
        importer.gate->released.release();
        QTRY_COMPARE_WITH_TIMEOUT(imported.size(), 1, 30000);
        QCOMPARE(qvariant_cast<ImportSummary>(imported.first().first()).added, 20);
        keepOnClose(dialog);
        QVERIFY(!dialog.close());
        save->click();
        QVERIFY(bytes(library.absolute(library.track(ids[2]).lyrics)).contains("relaxer-bilingual"));
        QCOMPARE(paid.requestCount(), 1);
        QCOMPARE(library.tracks().size(), 40);
        QCOMPARE(player->queue().items, ids);
        QTRY_VERIFY(player->playing());
        QVERIFY(player->save());
        QVERIFY(dialog.close());
        importer.shutdown();
        QTRY_VERIFY(importer.isIdle());
        QElapsedTimer exit;
        exit.start();
        QVERIFY(window.close());
        QVERIFY(exit.elapsed() < 5000);
        record("all-services", {{"seedTracks", 20},
                                {"importedTracks", 20},
                                {"heartbeats", heartbeats},
                                {"paidRequests", paid.requestCount()},
                                {"searchRequests", source.requestCount() + backup.requestCount()},
                                {"closeMs", double(exit.elapsed())},
                                {"resources", resources()}});
    }
    void rejectingMultiSelectRestoresButton() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        QVERIFY(library.saveLyrics(ids[0], "[00:01.00]Hello world"));
        TranslationHttpMock paid;
        QVERIFY(paid.listen());
        paid.enqueue(completion(QString::fromUtf8(translationJson({{0, QStringLiteral("你好世界")}}))));
        AiRuntime runtime(&library, DeepSeekProvider::MockEndpoint{paid.baseUrl()},
                          std::make_unique<FakeCredentialStore>());
        QVERIFY(configureRuntime(runtime));
        LyricsLookup dialog(&library, ids, nullptr, QUrl("http://127.0.0.1:1"), {}, true, &runtime);
        dialog.show();
        dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);
        select(dialog, ids[0]);
        consent(dialog);
        auto *save = dialog.findChild<QPushButton *>("lyricsSaveTranslation");
        QTRY_VERIFY(save->isEnabled());
        const auto preview = dialog.findChild<QPlainTextEdit *>("lyricsTranslationPreview")->toPlainText();
        auto *button = dialog.findChild<QPushButton *>("lyricsMultiSelect");
        button->click();
        QVERIFY(!button->isChecked());
        QVERIFY(!dialog.findChild<QListWidget *>("lyricsTracks")->property("lookupMultiSelect").toBool());
        QCOMPARE(dialog.findChild<QPlainTextEdit *>("lyricsTranslationPreview")->toPlainText(), preview);
        QVERIFY(save->isEnabled());
        QCOMPARE(paid.requestCount(), 1);
        save->click();
        QVERIFY(!save->isEnabled());
        button->click();
        QVERIFY(button->isChecked());
        QVERIFY(dialog.findChild<QListWidget *>("lyricsTracks")->property("lookupMultiSelect").toBool());
        QVERIFY(dialog.close());
    }
    void boundedThousandTrackStability() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        QElapsedTimer elapsed;
        elapsed.start();
        const auto ids = seed(library, 1000, 2000);
        QCOMPARE(ids.size(), 1000);
        Window window(&library);
        window.show();
        auto *player = window.playbackFacade();
        player->play(ids);
        QTRY_VERIFY(player->seekable());
        const auto before = resources();
        int ticks = 0;
        QTimer pulse;
        connect(&pulse, &QTimer::timeout, this, [&] { ++ticks; });
        pulse.start(5);
        QElapsedTimer switches;
        switches.start();
        for (int n = 1; n <= 100; ++n) {
            player->next();
            QCOMPARE(player->current().id, ids[n]);
            QTRY_VERIFY(player->seekable());
        }
        const auto switchMs = switches.elapsed();
        QTRY_VERIFY(ticks > 0);
        for (int n = 0; n < 10; ++n) {
            LyricsLookup dialog(&library, {ids[n]}, &window, QUrl("http://127.0.0.1:1"), {}, true);
            dialog.show();
            QVERIFY(dialog.close());
        }
        for (int n = 0; n < 10; ++n) {
            window.showNowPlaying();
            QCoreApplication::processEvents();
            window.showLibrary();
            QCoreApplication::processEvents();
        }
        QVERIFY(player->save());
        QCOMPARE(library.setting("current").toInt(), int(ids[100]));
        QCOMPARE(player->queue().items, ids);
        const auto after = resources();
        QVERIFY(window.close());
        const int connections = QSqlDatabase::connectionNames().size();
        for (int n = 0; n < 100; ++n) {
            auto *facade = new PlaybackFacade(&library, nullptr, true);
            QPointer<PlaybackFacade> pointer = facade;
            delete facade;
            QVERIFY(!pointer);
        }
        QCOMPARE(QSqlDatabase::connectionNames().size(), connections);
        record("bounded-stability", {{"tracks", 1000},
                                     {"switches", 100},
                                     {"lyricsDialogs", 10},
                                     {"expandedCycles", 10},
                                     {"facadeLifecycles", 100},
                                     {"elapsedMs", double(elapsed.elapsed())},
                                     {"switchMs", double(switchMs)},
                                     {"heartbeats", ticks},
                                     {"before", before},
                                     {"after", after},
                                     {"oneHourRun", false}});
    }
};
QTEST_MAIN(PhaseBIntegrationTests)
#include "phase-b-integration-tests.moc"
