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
    title: appBridge.windowTitle + " · QML Foundation"
    color: Theme.background
    property int currentPage: appBridge.page
    property color themeAccent: Theme.accent
    font.family: Qt.platform.os === "windows" ? "Microsoft YaHei UI" : "sans-serif"
    font.pixelSize: Theme.fontSize
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.preferredWidth: 208; Layout.fillHeight: true
                color: Theme.panel
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 10
                    Text { text: "relaxer"; color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold; Layout.topMargin: 12 }
                    Text { text: "让音乐慢下来"; color: Theme.secondary; font.pixelSize: 12; Layout.bottomMargin: 28 }
                    Repeater {
                        model: ["本地音乐", "歌单", "正在播放", "设置"]
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
                        Text { anchors.fill: parent; anchors.margins: 12; text: appBridge.version + "\nC1 · 开发预览"; color: Theme.secondary; font.pixelSize: 12; lineHeight: 1.5 }
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: 32; spacing: 16
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "QML FOUNDATION"; color: Theme.secondary; font.pixelSize: 11; font.letterSpacing: 2 }
                    Item { Layout.fillWidth: true }
                    ShellButton { objectName: "themeToggle"; text: Theme.dark ? "切换浅色" : "切换深色"; onClicked: Theme.dark = !Theme.dark }
                }
                StackLayout {
                    objectName: "pageStack"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    currentIndex: appBridge.page
                    LocalPage {}
                    TransitionPage { objectName: "playlistsPage"; heading: "歌单"; description: "C1 尚未迁移歌单界面。\n创建、排序、标签与管理请使用默认 Widgets 模式。" }
                    ColumnLayout {
                        objectName: "playingPage"
                        Text { text: "正在播放"; font.pixelSize: Theme.headingSize; font.weight: Font.DemiBold; color: Theme.text }
                        Rectangle {
                            Layout.fillWidth: true; Layout.fillHeight: true; color: Theme.panel; radius: Theme.radius; border.color: Theme.border
                            EmptyState {
                                anchors.centerIn: parent; width: Math.min(parent.width - 64, 520)
                                heading: playbackBridge.hasTrack ? playbackBridge.title : "等待一首好歌"
                                description: (playbackBridge.hasTrack ? playbackBridge.artist + "\n" : "") + "播放状态来自既有 C++ PlaybackFacade。\n详情、队列、歌词与动画尚未迁移。"
                            }
                        }
                    }
                    TransitionPage { objectName: "settingsPage"; heading: "设置"; description: "C1 只提供基础浅色 / 深色预览，不保存主题设置。\n完整外观、快捷键、导入及 AI 设置仍由 Widgets 提供。\n本界面不提供 AI 翻译入口。" }
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    text: playbackBridge.error.length > 0 ? playbackBridge.error : "C1 基础建设预览 · 完整功能请重启默认 Widgets 模式"
                    color: playbackBridge.error.length > 0 ? Theme.accent : Theme.secondary
                    font.pixelSize: 12
                }
            }
        }
        PlayerBar { Layout.fillWidth: true }
    }
}
