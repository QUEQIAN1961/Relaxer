#ifdef RELAXER_SHORTCUT_BASELINE
#include "shortcuts.h"
using ShortcutRules = relaxer::PlayerShortcuts;
#else
#include "shortcut-rules.h"
using ShortcutRules = relaxer::ShortcutRules;
#endif

#include <QJsonArray>
#include <QJsonDocument>
#include <QKeySequence>
#include <QtTest>

namespace {
void setBinding(QJsonObject &config, const QString &scope, const QString &action, const QString &value) {
    auto bindings = config[scope].toObject();
    bindings[action] = value;
    config[scope] = bindings;
}

QKeySequence sequence(const QString &text) {
    return QKeySequence::fromString(text, QKeySequence::PortableText);
}
}

class ShortcutRulesTests : public QObject {
    Q_OBJECT
private slots:
    void defaultsAndPortableRoundTrip() {
        const QJsonObject expected{
            {"app", QJsonObject{{"toggle", "Space"}, {"previous", "Ctrl+Left"}, {"next", "Ctrl+Right"},
                                {"volumeUp", "Ctrl+Up"}, {"volumeDown", "Ctrl+Down"}}},
            {"global", QJsonObject{{"toggle", "Ctrl+Alt+Space"}, {"previous", "Ctrl+Alt+Left"},
                                   {"next", "Ctrl+Alt+Right"}, {"volumeUp", "Ctrl+Alt+Up"},
                                   {"volumeDown", "Ctrl+Alt+Down"}}}, {"globalEnabled", false}};
        QCOMPARE(ShortcutRules::defaults(), expected);
        QCOMPARE(ShortcutRules::actionKeys(), (QStringList{"toggle", "previous", "next", "volumeUp", "volumeDown"}));
        QCOMPARE(ShortcutRules::actionNames().size(), 5);
        const auto bytes = QJsonDocument(expected).toJson(QJsonDocument::Compact);
        QCOMPARE(QJsonDocument::fromJson(bytes).object(), expected);
        QVERIFY(ShortcutRules::validate(QJsonDocument::fromJson(bytes).object()));
        for (const auto &scope : {QString("app"), QString("global")})
            for (const auto &key : ShortcutRules::actionKeys()) {
                const auto text = expected[scope].toObject()[key].toString();
                QCOMPARE(sequence(text).toString(QKeySequence::PortableText), text);
            }
    }
    void validationCompatibility() {
        auto config = ShortcutRules::defaults();
        QString error;
        QVERIFY(ShortcutRules::validate(config, &error));
        setBinding(config, "app", "next", "Space");
        QVERIFY(!ShortcutRules::validate(config, &error));
        QVERIFY(!error.isEmpty());
        config = ShortcutRules::defaults();
        setBinding(config, "app", "next", "Ctrl+O");
        QVERIFY(!ShortcutRules::validate(config));
        setBinding(config, "app", "next", "Ctrl+K, Ctrl+C");
        QVERIFY(!ShortcutRules::validate(config));
        setBinding(config, "app", "next", "");
        QVERIFY(ShortcutRules::validate(config));
        auto app = config["app"].toObject();
        app.remove("next");
        config["app"] = app;
        QVERIFY(!ShortcutRules::validate(config));
        config = ShortcutRules::defaults();
        setBinding(config, "global", "next", "Meta+N");
        QVERIFY(!ShortcutRules::validate(config));
        setBinding(config, "global", "next", "F12");
        QVERIFY(!ShortcutRules::validate(config));
        setBinding(config, "global", "next", "N");
        QVERIFY(!ShortcutRules::validate(config));
        config = ShortcutRules::defaults();
        setBinding(config, "app", "toggle", "Ctrl+Alt+Space");
        QVERIFY(ShortcutRules::validate(config));
        config["globalEnabled"] = true;
        QVERIFY(!ShortcutRules::validate(config));
        config = ShortcutRules::defaults();
        setBinding(config, "app", "toggle", "Ctrl+Alt+K");
        setBinding(config, "app", "next", "Alt+Ctrl+K");
        QVERIFY(!ShortcutRules::validate(config));
    }
    void nativeMappings() {
        const auto up = ShortcutRules::nativeKey(sequence("Ctrl+Alt+Up"));
        QCOMPARE(up.modifiers, 3u);
        QCOMPARE(up.key, 0x26u);
        const auto f24 = ShortcutRules::nativeKey(sequence("Ctrl+Alt+Shift+F24"));
        QCOMPARE(f24.modifiers, 7u);
        QCOMPARE(f24.key, 0x87u);
        QCOMPARE(ShortcutRules::nativeKey(sequence("Ctrl+Num+1")).key, 0u);
        QCOMPARE(ShortcutRules::nativeKey(sequence("Ctrl+K, Ctrl+C")).key, 0u);
        QCOMPARE(ShortcutRules::nativeKey(QKeySequence()).key, 0u);
        QCOMPARE(ShortcutRules::nativeKey(QKeySequence(Qt::Key_MediaNext)).key, 0xb0u);
        QCOMPARE(ShortcutRules::nativeKey(QKeySequence(Qt::Key_Enter)).key, 0x0du);
    }
    void emitComparableBaseline() {
        QJsonObject snapshot{{"defaults", ShortcutRules::defaults()},
                             {"actionKeys", QJsonArray::fromStringList(ShortcutRules::actionKeys())},
                             {"actionNames", QJsonArray::fromStringList(ShortcutRules::actionNames())}};
        QJsonArray mappings;
        const QStringList inputs{"", "Ctrl+Alt+Up", "Ctrl+Alt+Shift+F24", "Ctrl+Num+1", "Meta+N",
                                 "F1", "F12", "F24", "Ctrl+K, Ctrl+C", "Space", "Ctrl+Left",
                                 "Alt+Ctrl+K", "Ctrl+Alt+K", "Ctrl+=", "Media Next", "Volume Up"};
        for (const auto &input : inputs) {
            const auto parsed = sequence(input);
            const auto native = ShortcutRules::nativeKey(parsed);
            mappings.append(QJsonObject{{"input", input}, {"portable", parsed.toString(QKeySequence::PortableText)},
                                        {"count", parsed.count()}, {"modifiers", int(native.modifiers)},
                                        {"key", int(native.key)}});
        }
        snapshot["mappings"] = mappings;
        QJsonArray validation;
        const QStringList values{"", "Space", "Ctrl+O", "Ctrl+K, Ctrl+C", "Alt+Ctrl+K", "Meta+N",
                                 "F12", "N", "F24", "Ctrl+Num+1", "Media Next", "invalid key"};
        for (const auto &scope : {QString("app"), QString("global")})
            for (const auto &value : values) {
                auto config = ShortcutRules::defaults();
                setBinding(config, scope, "next", value);
                QString error;
                const bool valid = ShortcutRules::validate(config, &error);
                validation.append(QJsonObject{{"scope", scope}, {"value", value}, {"valid", valid}, {"error", error}});
            }
        snapshot["validation"] = validation;
        qInfo().noquote() << "SHORTCUT_SNAPSHOT" << QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
    }
};

QTEST_GUILESS_MAIN(ShortcutRulesTests)
#include "shortcut-rules-tests.moc"
