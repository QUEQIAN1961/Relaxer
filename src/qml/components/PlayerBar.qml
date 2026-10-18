import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
Rectangle {
    id: bar
    objectName: "playerBar"
    implicitHeight: 110
    color: Theme.panel
    function timeText(ms) {
        const seconds = Math.floor(Math.max(0, ms) / 1000)
        return Math.floor(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + seconds % 60
    }
    Rectangle { width: parent.width; height: 1; color: Theme.border }
    RowLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 24
        Rectangle {
            width: 56; height: 56; radius: 16; color: Theme.selected
            Text { anchors.centerIn: parent; text: "♪"; font.pixelSize: 28; color: Theme.accent }
        }
        ColumnLayout {
            Layout.preferredWidth: 240; Layout.maximumWidth: 280
            Text {
                objectName: "currentTitle"
                Layout.fillWidth: true
                text: playbackBridge.hasTrack ? playbackBridge.title : "还没有正在播放的歌曲"
                font.pixelSize: Theme.fontSize; font.weight: Font.DemiBold; color: Theme.text; elide: Text.ElideRight
            }
            Text {
                objectName: "currentArtist"
                Layout.fillWidth: true
                text: playbackBridge.hasTrack ? playbackBridge.artist : "从 Widgets 模式选择音乐"
                font.pixelSize: 12; color: Theme.secondary; elide: Text.ElideRight
            }
        }
        ShellButton {
            objectName: "playPauseButton"
            primary: true
            enabled: playbackBridge.available && playbackBridge.hasTrack
            text: playbackBridge.playing ? "暂停" : "播放"
            onClicked: playbackBridge.toggle()
        }
        ColumnLayout {
            Layout.fillWidth: true; spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Text { objectName: "playbackState"; text: playbackBridge.playing ? "正在播放" : "已暂停 / 待播放"; color: Theme.secondary; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                Text { text: bar.timeText(playbackBridge.position) + " / " + bar.timeText(playbackBridge.duration); color: Theme.secondary; font.pixelSize: 12 }
            }
            ProgressBar {
                objectName: "playbackProgress"
                Layout.fillWidth: true
                from: 0; to: Math.max(1, playbackBridge.duration); value: playbackBridge.position
                Accessible.name: "播放进度（只读）"
                background: Rectangle { implicitHeight: 5; radius: 3; color: Theme.border }
                contentItem: Item {
                    implicitHeight: 5
                    Rectangle { width: parent.width * parent.parent.visualPosition; height: parent.height; radius: 3; color: Theme.accent }
                }
            }
        }
        Text { objectName: "volumeDisplay"; text: "音量 " + playbackBridge.volume + "%\n只读"; color: Theme.secondary; font.pixelSize: 12; horizontalAlignment: Text.AlignRight }
    }
}
