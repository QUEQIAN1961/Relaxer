#include "domain/queue.h"
#include <QSet>
#include <QtTest>
#include <limits>
#include <type_traits>

using namespace relaxer;

class QueueTests : public QObject {
    Q_OBJECT
    static QVector<qint64> ids(int count = 12) {
        QVector<qint64> result;
        for (int i = 1; i <= count; ++i)
            result << i;
        return result;
    }
    static QSet<qint64> members(const QVector<qint64> &values) {
        return QSet<qint64>(values.begin(), values.end());
    }

  private slots:
    void initialStateAndModeOrdinals() {
        Queue q;
        QVERIFY(q.items.isEmpty());
        QCOMPARE(q.index, -1);
        QCOMPARE(q.current(), qint64(0));
        QCOMPARE(q.mode, Mode::ListLoop);
        QCOMPARE(int(Mode::Sequential), 0);
        QCOMPARE(int(Mode::ListLoop), 1);
        QCOMPARE(int(Mode::SingleLoop), 2);
        QCOMPARE(int(Mode::Shuffle), 3);
        static_assert(std::is_copy_constructible_v<Queue>);
        static_assert(std::is_copy_assignable_v<Queue>);
    }
    void filtersInvalidIdsAndRetainsOrder() {
        Queue q;
        const auto large = std::numeric_limits<qint64>::max();
        q.set({0, -8, 7, 2, 7, large, 2}, large);
        QCOMPARE(q.items, (QVector<qint64>{7, 2, large}));
        QCOMPARE(q.index, 2);
        QCOMPARE(q.current(), large);
        q.set({4, 8}, 999);
        QCOMPARE(q.current(), qint64(4));
        q.set({4, 8}, -1);
        QCOMPARE(q.current(), qint64(4));
    }
    void emptyQueueInEveryMode() {
        Queue q;
        for (auto mode : {Mode::Sequential, Mode::ListLoop, Mode::SingleLoop, Mode::Shuffle}) {
            q.setMode(mode);
            q.set({0, -1});
            QCOMPARE(q.index, -1);
            QCOMPARE(q.current(), qint64(0));
            QCOMPARE(q.next(), qint64(0));
            QCOMPARE(q.next(true), qint64(0));
            QCOMPARE(q.previous(), qint64(0));
            QVERIFY(q.upcoming().isEmpty());
            QVERIFY(q.upcoming(0).isEmpty());
        }
    }
    void sequentialEndAndPreviousBoundaries() {
        Queue q;
        q.set({1, 2, 3}, 1);
        q.setMode(Mode::Sequential);
        QCOMPARE(q.previous(), qint64(1));
        QCOMPARE(q.upcoming(), (QVector<qint64>{2, 3}));
        QCOMPARE(q.current(), qint64(1));
        QCOMPARE(q.next(), qint64(2));
        QCOMPARE(q.next(true), qint64(3));
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(q.next(), qint64(0));
            QCOMPARE(q.current(), qint64(3));
            QVERIFY(q.upcoming().isEmpty());
        }
        QCOMPARE(q.previous(), qint64(2));
        QCOMPARE(q.previous(), qint64(1));
        QCOMPARE(q.previous(), qint64(1));
    }
    void listLoopAndManualJumpSequence() {
        Queue q;
        q.set({1, 2, 3}, 3);
        QCOMPARE(q.upcoming(), (QVector<qint64>{1, 2, 3}));
        QCOMPARE(q.previous(), qint64(2));
        QCOMPARE(q.previous(), qint64(1));
        QCOMPARE(q.previous(), qint64(3));
        for (int i = 0; i < 30; ++i)
            QCOMPARE(q.next(i % 2 == 0), qint64(i % 3 + 1));
    }
    void singleLoopDistinguishesAutomaticAndManual() {
        Queue q;
        q.set({1, 2, 3}, 2);
        q.setMode(Mode::SingleLoop);
        QCOMPARE(q.upcoming(), (QVector<qint64>{3, 1, 2}));
        for (int i = 0; i < 5; ++i)
            QCOMPARE(q.next(false), qint64(2));
        QCOMPARE(q.next(true), qint64(3));
        QCOMPARE(q.next(), qint64(3));
        QCOMPARE(q.next(true), qint64(1));
        QCOMPARE(q.previous(), qint64(3));
    }
    void singleTrackInEveryMode() {
        Queue q;
        for (auto mode : {Mode::Sequential, Mode::ListLoop, Mode::SingleLoop, Mode::Shuffle}) {
            q.set({42}, 42);
            q.setMode(mode);
            for (int i = 0; i < 20; ++i) {
                if (mode == Mode::Sequential) {
                    QVERIFY(q.upcoming().isEmpty());
                    QCOMPARE(q.next(), qint64(0));
                    QCOMPARE(q.next(true), qint64(0));
                } else {
                    QCOMPARE(q.upcoming(), (QVector<qint64>{42}));
                    QCOMPARE(q.next(), qint64(42));
                    QCOMPARE(q.next(true), qint64(42));
                }
                QCOMPARE(q.previous(), qint64(42));
                QCOMPARE(q.current(), qint64(42));
            }
        }
    }
    void forecastLimitsAndPublicIndexBoundaries() {
        Queue q;
        q.set({1, 2, 3}, 1);
        QVERIFY(q.upcoming(0).isEmpty());
        QCOMPARE(q.upcoming(1), (QVector<qint64>{2}));
        QCOMPARE(q.upcoming(100), (QVector<qint64>{2, 3, 1}));
        QCOMPARE(q.upcoming(-99), q.upcoming());
        QCOMPARE(q.upcoming(std::numeric_limits<int>::min()), q.upcoming());
        q.index = -1;
        QCOMPARE(q.current(), qint64(0));
        q.index = int(q.items.size());
        QCOMPARE(q.current(), qint64(0));
        q.index = std::numeric_limits<int>::max();
        QCOMPARE(q.current(), qint64(0));
        q.set({1, 2, 3}, 2);
        QCOMPARE(q.index, 1);
    }
    void shuffleInitialBagExcludesCurrent() {
        Queue q;
        q.mode = Mode::Shuffle; // Existing public assignment remains supported.
        q.set(ids(), 5);
        QSet<qint64> seen{5};
        for (int i = 1; i < q.items.size(); ++i) {
            const auto id = q.next();
            QVERIFY(!seen.contains(id));
            seen.insert(id);
        }
        QCOMPARE(seen, members(ids()));
    }
    void shuffleFullRoundsAndBoundaryDoNotRepeat() {
        Queue q;
        q.set(ids(7), 1);
        q.setMode(Mode::Shuffle);
        for (int i = 1; i < q.items.size(); ++i)
            q.next();
        for (int round = 0; round < 40; ++round) {
            const auto last = q.current();
            QSet<qint64> seen;
            for (int i = 0; i < q.items.size(); ++i) {
                const auto id = q.next();
                if (i == 0)
                    QVERIFY(id != last);
                QVERIFY(!seen.contains(id));
                seen.insert(id);
            }
            QCOMPARE(seen, members(q.items));
        }
    }
    void repeatedForecastMatchesPlaybackAcrossRounds() {
        for (int size : {2, 3, 12, 31}) {
            Queue q;
            q.set(ids(size), 1);
            q.setMode(Mode::Shuffle);
            for (int round = 0; round < 48; ++round) {
                const auto current = q.current();
                const auto index = q.index;
                const auto preview = q.upcoming();
                QCOMPARE(preview.size(), size);
                for (int query = 0; query < 8; ++query) {
                    QCOMPARE(q.upcoming(), preview);
                    QCOMPARE(q.upcoming(1), preview.first(1));
                    QCOMPARE(q.upcoming(size + 10), preview);
                    QCOMPARE(q.current(), current);
                    QCOMPARE(q.index, index);
                }
                for (auto id : preview)
                    QCOMPARE(q.next(true), id);
            }
        }
    }
    void partialForecastConsumptionPreservesCachedFuture() {
        Queue q;
        q.set(ids(), 1);
        q.setMode(Mode::Shuffle);
        for (int step = 0; step < 500; ++step) {
            const auto preview = q.upcoming();
            QCOMPARE(q.upcoming(2), preview.first(2));
            QCOMPARE(q.next(step % 2 == 0), preview.first());
            const auto remaining = q.upcoming();
            QCOMPARE(remaining.first(preview.size() - 1), preview.sliced(1));
        }
    }
    void historyBacktrackingAndReplayPreserveForecast() {
        Queue q;
        q.set(ids(6), 1);
        q.setMode(Mode::Shuffle);
        QVector<qint64> heard{q.current()};
        for (int i = 0; i < 30; ++i)
            heard << q.next();
        const auto future = q.upcoming();
        for (int i = heard.size() - 2; i >= 0; --i)
            QCOMPARE(q.previous(), heard[i]);
        for (int i = 1; i < heard.size(); ++i) {
            QCOMPARE(q.upcoming(1).first(), heard[i]);
            QCOMPARE(q.next(true), heard[i]);
        }
        QCOMPARE(q.upcoming(), future);
        for (auto id : future)
            QCOMPARE(q.next(), id);
    }
    void shufflePreviousAtHistoryOriginRetainsLegacyFallback() {
        Queue q;
        q.set({1, 2, 3}, 1);
        q.setMode(Mode::Shuffle);
        QCOMPARE(q.previous(), qint64(3)); // Before history origin: existing index wrap.
        const auto preview = q.upcoming();
        for (auto id : preview)
            QCOMPARE(q.next(true), id);
    }
    void modeChangesInvalidateForecastAndHistory() {
        Queue q;
        q.set(ids(), 5);
        q.setMode(Mode::Shuffle);
        for (int i = 0; i < 30; ++i)
            q.next();
        q.upcoming();
        const auto current = q.current();
        q.setMode(Mode::Sequential);
        QCOMPARE(q.current(), current);
        QCOMPARE(q.next(), current == 12 ? qint64(0) : current + 1);
        q.set({1, 2, 3}, 3);
        q.setMode(Mode::ListLoop);
        QCOMPARE(q.upcoming(), (QVector<qint64>{1, 2, 3}));
        q.setMode(Mode::SingleLoop);
        QCOMPARE(q.next(false), qint64(3));
        q.setMode(Mode::Shuffle);
        const auto preview = q.upcoming();
        QVERIFY(preview.first() != 3);
        for (auto id : preview)
            QCOMPARE(q.next(), id);
    }
    void sameModeResetKeepsCurrentAndStartsANewBag() {
        Queue q;
        q.set(ids(5), 1);
        q.setMode(Mode::Shuffle);
        for (int i = 0; i < 20; ++i)
            q.next();
        q.upcoming();
        const auto current = q.current();
        q.setMode(Mode::Shuffle);
        QCOMPARE(q.current(), current);
        QSet<qint64> seen{current};
        for (int i = 1; i < q.items.size(); ++i) {
            const auto forecast = q.upcoming(1);
            const auto actual = q.next();
            QCOMPARE(actual, forecast.first());
            QVERIFY(!seen.contains(actual));
            seen.insert(actual);
        }
        QCOMPARE(seen, members(q.items));
    }
    void replacingQueueDropsOldForecastAndHistory() {
        Queue q;
        q.set(ids(), 1);
        q.setMode(Mode::Shuffle);
        for (int i = 0; i < 20; ++i)
            q.next();
        q.upcoming();
        q.set({101, 102, 103, 104, 101}, 103);
        QCOMPARE(q.mode, Mode::Shuffle);
        QCOMPARE(q.current(), qint64(103));
        QCOMPARE(q.items, (QVector<qint64>{101, 102, 103, 104}));
        for (int round = 0; round < 20; ++round) {
            const auto preview = q.upcoming();
            for (auto id : preview) {
                QVERIFY(id >= 101 && id <= 104);
                QCOMPARE(q.next(), id);
            }
        }
        q.set({});
        QCOMPARE(q.previous(), qint64(0));
        QVERIFY(q.upcoming().isEmpty());
    }
    void copyAndAssignmentRetainCachedBagsAndHistory() {
        Queue q;
        q.set(ids(), 1);
        q.setMode(Mode::Shuffle);
        for (int i = 0; i < 26; ++i)
            q.next();
        q.previous();
        q.previous();
        const auto preview = q.upcoming();
        Queue copied(q), assigned;
        assigned = q;
        QCOMPARE(copied.upcoming(), preview);
        QCOMPARE(assigned.upcoming(), preview);
        for (auto id : preview) {
            QCOMPARE(q.next(), id);
            QCOMPARE(copied.next(), id);
            QCOMPARE(assigned.next(), id);
        }
        copied.set({999}, 999);
        QCOMPARE(q.items, ids());
        QCOMPARE(assigned.items, ids());
    }
    void valueSnapshotRestoreUsesExistingSetContract() {
        for (auto mode : {Mode::Sequential, Mode::ListLoop, Mode::SingleLoop, Mode::Shuffle}) {
            Queue q;
            q.set(ids(5), 3);
            q.setMode(mode);
            q.next(true);
            q.upcoming();
            Queue restored;
            restored.mode = q.mode; // Playback constructor restores this setting first.
            restored.set(q.items, q.current());
            QCOMPARE(restored.items, q.items);
            QCOMPARE(restored.current(), q.current());
            QCOMPARE(restored.mode, q.mode);
            // Persisted state has never included shuffle bags/history: test only the
            // restored object's own forecast, not a new promise of identical randomness.
            const auto preview = restored.upcoming();
            for (auto id : preview)
                QCOMPARE(restored.next(true), id);
        }
    }
    void jumpByConsumingForecastMatchesSelectedEntry() {
        for (auto mode : {Mode::Sequential, Mode::ListLoop, Mode::SingleLoop, Mode::Shuffle}) {
            for (int selected = 0; selected < 6; ++selected) {
                Queue q;
                q.set(ids(7), 1);
                q.setMode(mode);
                const auto preview = q.upcoming();
                qint64 actual = 0;
                for (int i = 0; i <= selected; ++i)
                    actual = q.next(true); // Existing Playback::playUpcoming contract.
                QCOMPARE(actual, preview[selected]);
                QCOMPARE(q.current(), preview[selected]);
            }
        }
    }
};

QTEST_GUILESS_MAIN(QueueTests)
#include "queue-tests.moc"
