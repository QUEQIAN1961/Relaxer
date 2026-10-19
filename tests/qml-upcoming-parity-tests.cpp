#include "ui-bridge/playback-bridge.h"
#include "ui-bridge/upcoming-model.h"
#include <QAbstractItemModelTester>
#include <QDataStream>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

using namespace relaxer;
namespace {
QVector<qint64> seed(Library &library, int count) {
    QFile audio(library.absolute("Music/upcoming-fixture.wav"));
    if (!audio.open(QIODevice::WriteOnly)) return {};
    QDataStream out(&audio); out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4); out << quint32(160036); out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
    out.writeRawData("data", 4); out << quint32(160000);
    const QByteArray pcm(160000, '\0'); out.writeRawData(pcm.data(), pcm.size()); audio.close();
    QVector<qint64> ids;
    library.deferChanges(true);
    for (int row = 0; row < count; ++row) {
        Track track; track.hash = QString("upcoming-%1").arg(row); track.file = "Music/upcoming-fixture.wav";
        track.title = QString("歌曲 日本語 Song %1").arg(row); track.artist = QString("Artist %1").arg(row);
        track.album = "Forecast fixture"; track.duration = 10000;
        const auto id = library.insert(track);
        if (!id) { library.deferChanges(false); return {}; }
        ids.append(id);
    }
    library.deferChanges(false);
    return ids;
}
}
class UpcomingParityTests : public QObject {
    Q_OBJECT
  private slots:
    void originalManualNextModesAndTypedRows() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        const auto ids = seed(library, 5); QCOMPARE(ids.size(), 5);
        PlaybackFacade facade(&library, nullptr, true);
        UpcomingModel model(&library, &facade);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(model.count(), 0); QCOMPARE(model.firstTrackKey(), QString("0")); QVERIFY(!model.activate(0));
        for (const auto mode : {Mode::Sequential, Mode::ListLoop, Mode::SingleLoop}) {
            facade.play(ids, ids[2]); facade.setMode(mode);
            const auto expected = facade.upcoming();
            QTRY_COMPARE(model.ids(), expected);
            QCOMPARE(facade.current().id, ids[2]); QCOMPARE(facade.queue().items, ids);
            QCOMPARE(model.firstTrackKey(), QString::number(ids[3]));
            QCOMPARE(model.firstTitle(), library.track(ids[3]).title);
            QCOMPARE(model.firstArtist(), library.track(ids[3]).artist);
            for (int row = 0; row < model.count(); ++row) {
                const auto index = model.index(row);
                QCOMPARE(model.data(index, UpcomingModel::TrackId).toLongLong(), expected[row]);
                QCOMPARE(model.data(index, UpcomingModel::TrackKey).toString(), QString::number(expected[row]));
                QCOMPARE(model.data(index, UpcomingModel::UpcomingIndex).toInt(), row);
                QCOMPARE(model.data(index, UpcomingModel::Duration).toLongLong(), qint64(10000));
                QCOMPARE(model.data(index, UpcomingModel::Album).toString(), QString("Forecast fixture"));
                QVERIFY(model.data(index, UpcomingModel::CoverUrl).canConvert<QUrl>());
            }
            QSignalSpy activated(&model, &UpcomingModel::aboutToActivate);
            QVERIFY(model.activate(1)); QCOMPARE(activated.size(), 1);
            QCOMPARE(facade.current().id, expected[1]); QCOMPARE(facade.queue().items, ids);
        }
        QVERIFY(facade.shutdown()); QTRY_COMPARE(model.count(), 0);
    }
    void shuffleUsesAuthoritativeForecastAndRejectsStaleClick() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        const auto ids = seed(library, 6); QCOMPARE(ids.size(), 6);
        PlaybackFacade facade(&library, nullptr, true); UpcomingModel model(&library, &facade);
        facade.play(ids, ids.first()); facade.setMode(Mode::Shuffle);
        QTRY_COMPARE(model.ids(), facade.upcoming());
        QSet<qint64> firstRound;
        for (int row = 0; row < 5; ++row) firstRound.insert(model.ids()[row]);
        QCOMPARE(firstRound.size(), 5); QVERIFY(!firstRound.contains(ids.first()));
        for (int step = 0; step < 8; ++step) {
            QTRY_COMPARE(model.ids(), facade.upcoming());
            const auto wanted = model.ids().first();
            QVERIFY(model.activate(0)); QCOMPARE(facade.current().id, wanted);
            QCOMPARE(facade.queue().items, ids);
        }
        facade.play(ids, ids.first()); facade.setMode(Mode::Sequential);
        QTRY_COMPARE(model.ids(), (QVector<qint64>{ids[1], ids[2], ids[3], ids[4], ids[5]}));
        facade.next();
        // The event-loop refresh has not run; this row still describes ids[1].
        QVERIFY(!model.activate(0)); QCOMPARE(facade.current().id, ids[1]);
        QTRY_COMPARE(model.firstTrackKey(), QString::number(ids[2]));
        QVERIFY(!model.activate(-1)); QVERIFY(!model.activate(model.count()));
        QVERIFY(facade.shutdown());
    }
    void realProgressAddsNoForecastMetadataReadOrReset() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        const auto catalog = seed(library, 1000); QCOMPARE(catalog.size(), 1000);
        const QVector<qint64> queue{catalog[987], catalog[2], catalog[660]};
        PlaybackFacade facade(&library, nullptr, true); UpcomingModel model(&library, &facade);
        const auto connections = QSqlDatabase::connectionNames();
        facade.play(queue, queue.first());
        QTRY_VERIFY(facade.playing()); QTRY_VERIFY(facade.position() > 300);
        QTRY_COMPARE(model.ids(), facade.upcoming()); QTest::qWait(100);
        const auto metadataReads = model.metadataReadCount(), forecasts = model.forecastReadCount();
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
        QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy progress(&facade, &PlaybackFacade::positionChanged);
        QTRY_VERIFY_WITH_TIMEOUT(facade.position() > 1400, 4000);
        QVERIFY(!progress.isEmpty()); QCOMPARE(resets.size(), 0); QCOMPARE(changes.size(), 0);
        QCOMPARE(model.metadataReadCount(), metadataReads); QCOMPARE(model.forecastReadCount(), forecasts);
        QCOMPARE(QSqlDatabase::connectionNames(), connections); QCOMPARE(facade.queue().items, queue);
        QVERIFY(facade.shutdown());
    }
    void metadataUpdatesPreserveForecastAndCurrentHeart() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        const auto ids = seed(library, 3); QCOMPARE(ids.size(), 3);
        PlaybackFacade facade(&library, nullptr, true); UpcomingModel model(&library, &facade);
        PlaybackBridge bridge(&facade, nullptr, &library);
        facade.play(ids, ids.first()); QTRY_COMPARE(model.count(), 3);
        QTRY_COMPARE(bridge.trackId(), QString::number(ids.first()));
        QVERIFY(!bridge.currentFavorite()); QSignalSpy hearts(&bridge, &PlaybackBridge::currentFavoriteChanged);
        bridge.toggleFavorite(); QVERIFY(library.track(ids.first()).favorite); QVERIFY(bridge.currentFavorite());
        QCOMPARE(hearts.size(), 1);
        QVERIFY(library.favorite(ids.first(), false)); QVERIFY(!bridge.currentFavorite()); QCOMPARE(hearts.size(), 2);
        QTRY_COMPARE(model.ids(), facade.upcoming());
        const auto expected = model.ids();
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy firstChanged(&model, &UpcomingModel::firstChanged);
        QVERIFY(library.favorite(expected.first(), true));
        QTRY_VERIFY(model.data(model.index(0), UpcomingModel::Favorite).toBool());
        QVERIFY(!changed.isEmpty()); QVERIFY(!firstChanged.isEmpty()); QCOMPARE(resets.size(), 0); QCOMPARE(model.ids(), expected);
        QImage image(32, 32, QImage::Format_RGB32); image.fill(Qt::magenta);
        const auto cover = dir.filePath("fixture-cover.png"); QVERIFY(image.save(cover));
        QVERIFY(library.updateArtwork(ids.first(), cover));
        QTRY_VERIFY(!bridge.coverUrl().isEmpty()); QVERIFY(bridge.coverUrl().isLocalFile());
        QVERIFY(QFile::exists(bridge.coverUrl().toLocalFile()));
        const auto firstRevision = bridge.artworkRevision(); QVERIFY(!firstRevision.isEmpty());
        QSignalSpy coverChanges(&bridge, &PlaybackBridge::artworkChanged);
        QTest::qWait(20); image.fill(Qt::cyan); QVERIFY(image.save(cover));
        QVERIFY(library.updateArtwork(ids.first(), cover));
        QTRY_VERIFY(bridge.artworkRevision() != firstRevision); QVERIFY(!coverChanges.isEmpty());
        QVERIFY(library.updateArtwork(expected.first(), cover));
        QTRY_VERIFY(!model.firstCoverUrl().isEmpty()); QVERIFY(model.firstCoverUrl().isLocalFile());
        QCOMPARE(model.data(model.index(0), UpcomingModel::CoverUrl).toUrl(), model.firstCoverUrl());
        const auto upcomingRevision = model.firstCoverRevision(); QVERIFY(!upcomingRevision.isEmpty());
        QTest::qWait(20); image.fill(Qt::yellow); QVERIFY(image.save(cover));
        QVERIFY(library.updateArtwork(expected.first(), cover));
        QTRY_VERIFY(model.firstCoverRevision() != upcomingRevision);
        QCOMPARE(model.data(model.index(0), UpcomingModel::CoverRevision).toString(), model.firstCoverRevision());
        QCOMPARE(resets.size(), 0); QCOMPARE(model.ids(), expected);
        QVERIFY(facade.shutdown());
    }
    void transitionNoticePrecedesTrackChangeAndLifetimesAreSafe() {
        QTemporaryDir dir; auto library = std::make_unique<Library>(dir.path()); QVERIFY(library->open());
        const auto ids = seed(*library, 4); QCOMPARE(ids.size(), 4);
        auto facade = std::make_unique<PlaybackFacade>(library.get(), nullptr, true);
        UpcomingModel model(library.get(), facade.get()); PlaybackBridge bridge(facade.get(), nullptr, library.get());
        facade->play(ids, ids.first()); QTRY_COMPARE(bridge.trackId(), QString::number(ids.first()));
        QVector<QPair<int, QString>> notices;
        connect(&bridge, &PlaybackBridge::transitionStarted, &bridge, [&](int direction) { notices.append(qMakePair(direction, bridge.trackId())); });
        bridge.next(); QCOMPARE(notices.last(), qMakePair(1, QString::number(ids.first())));
        QCOMPARE(bridge.trackId(), QString::number(ids[1])); QCOMPARE(bridge.transitionDirection(), 1);
        bridge.next(); QCOMPARE(notices.size(), 2); QCOMPARE(notices.last().first, 1);
        bridge.previous(); QCOMPARE(notices.last().first, -1); QCOMPARE(bridge.trackId(), QString::number(ids[1]));
        bridge.playUpcoming(0); QCOMPARE(notices.last().first, 0); QCOMPARE(bridge.trackId(), QString::number(ids[2]));
        QVERIFY(facade->shutdown()); QTRY_COMPARE(model.count(), 0);
        facade.reset(); QVERIFY(!bridge.available()); QVERIFY(!bridge.hasTrack());
        QVERIFY(!model.activate(0)); bridge.next(); bridge.previous(); bridge.playUpcoming(0); bridge.toggleFavorite();
        library.reset(); QCOMPARE(model.count(), 0); QVERIFY(bridge.coverUrl().isEmpty()); QVERIFY(!bridge.currentFavorite());
    }
};
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv); app.setQuitOnLastWindowClosed(false);
    UpcomingParityTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "qml-upcoming-parity-tests.moc"
