import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"
import "../theme"
Item {
    id: page
    objectName: "lyricsPage"
    property bool fullScreen: false
    property string importTrack: "0"
    property url pendingLyricsFile
    property string localNotice: ""
    signal fullscreenRequested()
    signal translationRequested()
    RowLayout {
        anchors.fill: parent; spacing: page.width < 850 ? 20 : 40
        ScrollView {
          id: artworkScroll; contentWidth: availableWidth; clip: true
          Layout.minimumWidth: 220; Layout.maximumWidth: 340; Layout.preferredWidth: Math.max(220, Math.min(340, page.width * 0.34)); Layout.fillHeight: true; Layout.minimumHeight: 0
          ColumnLayout {
            width: artworkScroll.availableWidth; spacing: 10
            Rectangle {
                property real coverSide: Math.min(artworkScroll.availableWidth, Math.max(110, page.height - 270))
                Layout.preferredWidth: coverSide; Layout.preferredHeight: coverSide; Layout.alignment: Qt.AlignHCenter
                radius: 22; color: Theme.hover; clip: true
                scale: page.fullScreen ? 1 : 0.98
                Behavior on scale { NumberAnimation { duration: settingsBridge.values.reduceMotion ? 0 : 180; easing.type: Easing.OutCubic } }
                Text { anchors.centerIn: parent; text: "♫"; font.pixelSize: 84; color: Theme.secondary; visible: art.status !== Image.Ready }
                Image {
                    id: art; objectName: "currentArtwork"; anchors.fill: parent
                    source: lyricsModel.cover; asynchronous: true; cache: false; retainWhileLoading: false
                    sourceSize.width: 640; sourceSize.height: 640; fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                    opacity: visible ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: settingsBridge.values.reduceMotion ? 0 : 180 } }
                }
            }
            Text { Layout.fillWidth: true; Layout.minimumWidth: 0; text: playbackBridge.title; textFormat: Text.PlainText; font.pixelSize: 25; font.weight: Font.Bold; color: Theme.text; elide: Text.ElideRight }
            Text { Layout.fillWidth: true; Layout.minimumWidth: 0; text: playbackBridge.artist; textFormat: Text.PlainText; font.pixelSize: 15; color: Theme.secondary; elide: Text.ElideRight }
            RowLayout {
                Layout.fillWidth: true
                ShellButton { objectName: "fullscreenButton"; Layout.fillWidth: true; text: page.fullScreen ? "↙ 退出全屏" : "↗ 全屏播放"; onClicked: page.fullscreenRequested() }
                ShellButton { text: "歌词文件"; enabled: playbackBridge.hasTrack; onClicked: { page.importTrack = playbackBridge.trackId; localLyrics.open() } }
            }
            Text { Layout.fillWidth: true; text: "歌词延时 · 正数晚出现，负数提前"; font.pixelSize: 12; color: Theme.secondary; wrapMode: Text.Wrap }
            RowLayout {
                Layout.fillWidth: true
                ToolButton { text: "−"; enabled: lyricsModel.timed && lyricsModel.offsetMs > -5000; onClicked: lyricsModel.setOffsetMs(Math.max(-5000, lyricsModel.offsetMs - 10)) }
                TextField {
                    id: offset; objectName: "lyricsOffset"; Layout.fillWidth: true; horizontalAlignment: TextInput.AlignHCenter
                    text: (lyricsModel.offsetMs / 1000).toFixed(2)
                    enabled: lyricsModel.timed
                    validator: DoubleValidator { bottom: -5; top: 5; decimals: 2; locale: "C"; notation: DoubleValidator.StandardNotation }
                    onEditingFinished: { if (acceptableInput) lyricsModel.setOffsetMs(Math.round(Number(text) * 1000)); text = (lyricsModel.offsetMs / 1000).toFixed(2) }
                    Accessible.name: "歌词延时秒，范围负五到正五"
                }
                ToolButton { text: "+"; enabled: lyricsModel.timed && lyricsModel.offsetMs < 5000; onClicked: lyricsModel.setOffsetMs(Math.min(5000, lyricsModel.offsetMs + 10)) }
                ToolButton { text: "归零"; enabled: lyricsModel.timed; onClicked: lyricsModel.setOffsetMs(0) }
            }
            ShellButton { Layout.fillWidth: true; text: "检索歌词 / 翻译与预览"; onClicked: page.translationRequested() }
            Text { Layout.fillWidth: true; text: page.localNotice; color: Theme.accent; wrapMode: Text.Wrap; textFormat: Text.PlainText }
          }
        }
        LyricsList { Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 0; Layout.minimumHeight: 0; Layout.preferredWidth: page.width * 0.64 }
    }
    FileDialog {
        id: localLyrics; title: "导入当前歌曲的本地歌词"; nameFilters: ["歌词 (*.lrc *.txt)"]
        onAccepted: {
            if (page.importTrack !== playbackBridge.trackId) { page.localNotice = "歌曲已变化，请重新选择歌词文件"; return }
            page.pendingLyricsFile = selectedFile; replaceLocal.open()
        }
    }
    ConfirmDialog {
        id: replaceLocal; parent: page.Overlay.overlay; anchors.centerIn: parent; width: Math.max(300, Math.min(440, page.width - 20)); modal: true
        title: "保存所选本地歌词？"; standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "将替换当前歌曲的本地歌词，并明确放弃该歌曲尚未保存的翻译任务或预览。"; wrapMode: Text.Wrap }
        onAccepted: {
            if (page.importTrack !== playbackBridge.trackId) { page.localNotice = "歌曲已变化，未写入"; return }
            if (translationBridge.taskKey.length) translationBridge.discard()
            page.localNotice = lyricsModel.importLocalLyrics(page.pendingLyricsFile) ? "歌词已保存" : "歌词保存失败，请检查文件"
        }
    }
}
