import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "pages"
import "theme"
ApplicationWindow {
    id: root
    objectName: "qmlMainWindow"
    visible: true
    width: 1180; height: 760
    minimumWidth: 900; minimumHeight: 600
    title: appBridge.windowTitle + " · QML"
    color: Theme.background
    palette.window: Theme.background
    palette.base: Theme.panel
    palette.text: Theme.text
    palette.buttonText: Theme.text
    palette.button: Theme.panel
    palette.highlight: Theme.accent
    property int currentPage: appBridge.page
    property color themeAccent: Theme.accent
    readonly property QtObject designTokens: Theme
    font.family: Qt.platform.os === "windows" ? "Microsoft YaHei UI" : "sans-serif"
    font.pixelSize: Theme.fontSize
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.preferredWidth: root.width < 1050 ? 168 : 208; Layout.fillHeight: true
                color: Theme.panel
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 10
                    Text { text: "relaxer"; color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold; Layout.topMargin: 12 }
                    Text { text: "让音乐慢下来"; color: Theme.secondary; font.pixelSize: 12; Layout.bottomMargin: 28 }
                    Repeater {
                        model: ["本地音乐", "歌单", "播放队列", "设置"]
                        NavigationButton {
                            required property string modelData
                            required property int index
                            objectName: "navigation" + index
                            Layout.fillWidth: true
                            text: modelData
                            marker: ["♫", "≡", "▷", "⚙"][index]
                            selected: appBridge.page === index
                            onClicked: appBridge.navigate(index)
                        }
                    }
                    Item { Layout.fillHeight: true }
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: 74; radius: 10; color: Theme.background
                        Text { anchors.fill: parent; anchors.margins: 12; text: appBridge.version + "\nC2+C3 · 开发预览"; color: Theme.secondary; font.pixelSize: 12; lineHeight: 1.5 }
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: root.width < 1050 ? 20 : 24; spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "MUSIC · PLAYER"; color: Theme.secondary; font.pixelSize: 11; font.letterSpacing: 2 }
                    Item { Layout.fillWidth: true }
                    ShellButton { objectName: "themeToggle"; implicitHeight: 34; text: Theme.dark ? "切换浅色" : "切换深色"; onClicked: Theme.dark = !Theme.dark }
                }
                StackLayout {
                    objectName: "pageStack"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    currentIndex: appBridge.page
                    LocalPage {}
                    PlaylistsPage {}
                    QueuePage {}
                    TransitionPage { objectName: "settingsPage"; heading: "设置"; description: "这里提供基础浅色 / 深色预览，不保存主题设置。\n完整外观、快捷键、导入及 AI 设置仍由 Widgets 提供。\n歌词与 AI 界面将在后续阶段迁移。" }
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    text: libraryBridge.error.length > 0 ? libraryBridge.error : playbackBridge.error.length > 0 ? playbackBridge.error : "本地音乐与歌单 · 导入、歌词及高级设置仍可在 Widgets 中使用"
                    color: playbackBridge.error.length > 0 || libraryBridge.error.length > 0 ? Theme.accent : Theme.secondary
                    font.pixelSize: 12
                }
            }
        }
        PlayerBar { Layout.fillWidth: true }
    }
}
