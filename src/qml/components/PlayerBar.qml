import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
Rectangle {
    id: bar
    objectName: "playerBar"
    implicitHeight: 138
    color: Theme.panel
    function timeText(ms) {
        const seconds = Math.floor(Math.max(0, ms) / 1000)
        return Math.floor(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + seconds % 60
    }
    Rectangle { width: parent.width; height: 1; color: Theme.border }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 8
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Text {
                    objectName: "currentTitle"; Layout.fillWidth: true
                    text: playbackBridge.hasTrack ? playbackBridge.title : "选择一首音乐，开始放松"
                    font.pixelSize: Theme.fontSize; font.weight: Font.DemiBold; color: Theme.text; elide: Text.ElideRight
                }
                Text {
                    objectName: "currentArtist"; Layout.fillWidth: true
                    text: playbackBridge.hasTrack ? playbackBridge.artist : "从音乐库或歌单开始播放"
                    font.pixelSize: 12; color: Theme.secondary; elide: Text.ElideRight
                }
            }
            ShellButton { objectName: "previousButton"; Layout.preferredWidth: 44; text: "|‹"; Accessible.name: "上一首"; enabled: playbackBridge.available && playbackBridge.hasTrack; onClicked: playbackBridge.previous() }
            ShellButton {
                objectName: "playPauseButton"; Layout.preferredWidth: 76; primary: true
                enabled: playbackBridge.available && playbackBridge.hasTrack && playbackBridge.canPlay
                text: playbackBridge.playing ? "暂停" : "播放"
                onClicked: playbackBridge.toggle()
            }
            ShellButton { objectName: "nextButton"; Layout.preferredWidth: 44; text: "›|"; Accessible.name: "下一首"; enabled: playbackBridge.available && playbackBridge.hasTrack; onClicked: playbackBridge.next() }
            ComboBox {
                objectName: "playMode"; Layout.preferredWidth: 132
                model: ["顺序播放", "列表循环", "单曲循环", "随机播放"]
                currentIndex: playbackBridge.mode; enabled: playbackBridge.available
                onActivated: playbackBridge.setMode(currentIndex)
                Accessible.name: "播放模式"
            }
            ShellButton { Layout.preferredWidth: 68; text: "队列"; onClicked: appBridge.navigate(2) }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            Text {
                objectName: "playbackState"; Layout.preferredWidth: 110
                text: bar.timeText(playbackBridge.position) + " / " + bar.timeText(playbackBridge.duration)
                color: Theme.secondary; font.pixelSize: 12
            }
            Slider {
                id: seek
                objectName: "playbackProgress"; Layout.fillWidth: true
                property string dragTrackKey: "0"
                property real dragValue: 0
                from: 0; to: Math.max(1, playbackBridge.duration)
                enabled: playbackBridge.available && playbackBridge.seekable
                Accessible.name: "播放进度"
                onPressedChanged: {
                    if (pressed) { dragTrackKey = playbackBridge.trackId; dragValue = value }
                    else if (dragTrackKey === playbackBridge.trackId && playbackBridge.seekable) playbackBridge.seek(Math.round(dragValue))
                }
                onMoved: { if (pressed) dragValue = value; else if (playbackBridge.seekable) playbackBridge.seek(Math.round(value)) }
                Binding {
                    target: seek; property: "value"; value: playbackBridge.position
                    when: !seek.pressed; restoreMode: Binding.RestoreBindingOrValue
                }
            }
            ToolButton { objectName: "muteButton"; Layout.preferredWidth: 48; text: playbackBridge.muted ? "静音" : "♫"; Accessible.name: playbackBridge.muted ? "取消静音" : "静音"; onClicked: playbackBridge.toggleMute() }
            Slider {
                objectName: "volumeSlider"; Layout.preferredWidth: 110
                from: 0; to: 100; stepSize: 1; value: playbackBridge.volume
                onMoved: playbackBridge.setVolume(Math.round(value))
                Accessible.name: "音量"
            }
            Text { objectName: "volumeDisplay"; Layout.preferredWidth: 48; text: playbackBridge.volume + "%"; color: Theme.secondary; font.pixelSize: 12; horizontalAlignment: Text.AlignRight }
        }
    }
}
