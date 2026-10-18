import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
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
    property bool lyricsOpen: false
    readonly property bool fullScreenMode: visibility === Window.FullScreen
    property var savedGeometry: [0, 0, 1180, 760]
    property bool wasMaximized: false
    property int fullscreenRevision: 0
    property bool allowClose: false
    property bool awaitingImportClose: false
    property bool fullscreenControls: true
    readonly property Item popupLayer: contentItem.Overlay.overlay
    function showLyrics() { lyricsOpen = true }
    function toggleFullscreen() {
        const revision = ++fullscreenRevision
        if (fullScreenMode) {
            if (wasMaximized) showMaximized()
            else {
                showNormal()
                const bounds = savedGeometry
                Qt.callLater(function() { if (revision !== root.fullscreenRevision || root.fullScreenMode) return; root.x = bounds[0]; root.y = bounds[1]; root.width = bounds[2]; root.height = bounds[3] })
            }
        } else {
            savedGeometry = [x, y, width, height]; wasMaximized = visibility === Window.Maximized
            lyricsOpen = true; showFullScreen()
        }
        fullscreenControls = true; controlsTimer.restart()
    }
    function applySettings() { Theme.dark = settingsBridge.palette.dark; Theme.custom = settingsBridge.palette.custom ? settingsBridge.palette : ({}) }
    Component.onCompleted: applySettings()
    Connections { target: settingsBridge; function onSettingsChanged() { root.applySettings() } }
    Connections { target: appBridge; function onPageChanged() { root.lyricsOpen = false } }
    Connections {
        target: importBridge
        function onReadyToClose() { if (root.awaitingImportClose) { root.allowClose = true; Qt.callLater(root.close) } }
    }
    Shortcut { sequence: "Escape"; enabled: root.fullScreenMode && (!root.popupLayer || !root.popupLayer.visible); onActivated: root.toggleFullscreen() }
    HoverHandler { onPointChanged: { root.fullscreenControls = true; controlsTimer.restart() } }
    Timer { id: controlsTimer; interval: 4500; onTriggered: if (root.fullScreenMode) root.fullscreenControls = false }
    onClosing: close => {
        if (!allowClose && (importBridge.busy || translationBridge.unfinished)) { close.accepted = false; closeConfirm.open() }
    }
    font.family: Qt.platform.os === "windows" ? "Microsoft YaHei UI" : "sans-serif"
    font.pixelSize: Theme.fontSize
    Image {
        anchors.fill: parent
        source: root.lyricsOpen && settingsBridge.values.playerCoverBackground ? lyricsModel.cover : ""
        asynchronous: true; cache: false; retainWhileLoading: false
        sourceSize.width: 960; sourceSize.height: 960; fillMode: Image.PreserveAspectCrop
        visible: status === Image.Ready; opacity: 0.12
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0; spacing: 0
            Rectangle {
                visible: !root.fullScreenMode
                Layout.preferredWidth: root.width < 1050 ? 168 : 208; Layout.fillHeight: true
                color: Theme.panel
                ScrollView {
                  id: navigationScroll; anchors.fill: parent; anchors.margins: 16; contentWidth: availableWidth; clip: true
                  ColumnLayout {
                    width: navigationScroll.availableWidth; height: Math.max(implicitHeight, navigationScroll.availableHeight); spacing: 10
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
                            selected: !root.lyricsOpen && appBridge.page === index
                            onClicked: { root.lyricsOpen = false; appBridge.navigate(index) }
                        }
                    }
                    NavigationButton { objectName: "lyricsNavigation"; Layout.fillWidth: true; text: "歌词与封面"; marker: "♫"; selected: root.lyricsOpen; onClicked: root.showLyrics() }
                    NavigationButton { objectName: "importNavigation"; Layout.fillWidth: true; text: "导入音乐"; marker: "+"; onClicked: { root.lyricsOpen = false; appBridge.navigate(3); settingsPage.section = 1 } }
                    Item { Layout.fillHeight: true }
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: 74; radius: 10; color: Theme.background
                        Text { anchors.fill: parent; anchors.margins: 12; text: appBridge.version + "\nC4+C5 · 开发预览"; color: Theme.secondary; font.pixelSize: 12; lineHeight: 1.5 }
                    }
                  }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 0; Layout.minimumHeight: 0; Layout.margins: root.width < 1050 ? 20 : 24; spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "MUSIC · PLAYER"; color: Theme.secondary; font.pixelSize: 11; font.letterSpacing: 2 }
                    Item { Layout.fillWidth: true }
                    ShellButton { objectName: "themeToggle"; implicitHeight: 34; text: Theme.dark ? "切换浅色" : "切换深色"; onClicked: settingsBridge.setTheme(Theme.dark ? 0 : 1) }
                    ShellButton { visible: root.fullScreenMode; text: "↙ 退出全屏"; onClicked: root.toggleFullscreen() }
                }
                StackLayout {
                    objectName: "pageStack"
                    Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0; Layout.minimumWidth: 0
                    currentIndex: root.lyricsOpen ? 4 : appBridge.page
                    LocalPage {}
                    PlaylistsPage {}
                    QueuePage {}
                    SettingsPage { id: settingsPage }
                    LyricsPage { fullScreen: root.fullScreenMode; onFullscreenRequested: root.toggleFullscreen(); onTranslationRequested: { root.lyricsOpen = false; appBridge.navigate(3); settingsPage.section = 2 } }
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    text: libraryBridge.error.length > 0 ? libraryBridge.error : playbackBridge.error.length > 0 ? playbackBridge.error : "本地音乐、歌词与导入 · AI翻译仅在逐次确认后发送"
                    color: playbackBridge.error.length > 0 || libraryBridge.error.length > 0 ? Theme.accent : Theme.secondary
                    font.pixelSize: 12
                }
            }
        }
        PlayerBar {
            Layout.fillWidth: true
            expanded: root.fullScreenMode
            opacity: !root.fullScreenMode || root.fullscreenControls || activeFocus ? 1 : 0.28
            Behavior on opacity { NumberAnimation { duration: settingsBridge.values.reduceMotion ? 0 : 220 } }
        }
    }
    ConfirmDialog {
        id: closeConfirm; parent: root.popupLayer; objectName: "closeTasksConfirmation"; anchors.centerIn: parent; width: Math.min(480, root.width - 40); modal: true
        title: "结束当前任务并退出？"; standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "退出将取消尚未完成的导入和翻译，并放弃未保存的翻译预览。已经导入的曲目会保留；已发送的云端请求可能仍产生费用。"; wrapMode: Text.Wrap }
        onAccepted: {
            if (importBridge.busy) { root.awaitingImportClose = true; importBridge.beginShutdown() }
            else { root.allowClose = true; root.close() }
        }
    }
}
