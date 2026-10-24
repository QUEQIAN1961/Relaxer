import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
Rectangle {
    id: bar
    objectName: "playerBar"
    property bool expanded: false
    property bool artworkTravelling: false
    property real backgroundCanvasHeight: height
    property real backgroundOffsetY: 0
    readonly property alias smallArtwork: art
    readonly property alias metadataTitle: title
    readonly property alias metadataArtist: artist
    readonly property alias playerCenter: center
    signal expandRequested()
    implicitHeight: 98
    color: Theme.footer
    function timeText(ms) {
        const seconds = Math.floor(Math.max(0, ms) / 1000)
        return Math.floor(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + seconds % 60
    }
    Item {
      anchors.fill: parent; clip: true
      Image {
        objectName: "footerBackground"
        x: 0; y: settingsBridge.palette.shareFooter ? -bar.backgroundOffsetY : 0
        width: parent.width; height: settingsBridge.palette.shareFooter ? bar.backgroundCanvasHeight : parent.height
        source:String(settingsBridge.palette.footerImage||"").length?"image://relaxer/background?file="+encodeURIComponent(String(settingsBridge.palette.footerImage))+"&blur="+Number(settingsBridge.palette.blurFooter||0):"";fillMode:Image.PreserveAspectCrop;asynchronous:true
      }
    }
    Rectangle { width: parent.width; height: 1; color: Theme.border }
    Item {
        id: metadata
        objectName: "playerMetadata"
        x: 38; y: 12; width: 304; height: 74
        opacity: bar.expanded || bar.artworkTravelling ? 0 : 1
        Artwork {
            id: art; objectName: "bottomCover"
            x: 0; anchors.verticalCenter: parent.verticalCenter; width: 62; height: 62; radius: 10
            file: playbackBridge.coverUrl; seed: playbackBridge.hasTrack ? playbackBridge.title : "Relaxer"
            revision: playbackBridge.artworkRevision; resolution: 160; animated: false
        }
        Column {
            x: 92; width: 194; anchors.verticalCenter: parent.verticalCenter; spacing: 4
            UiText { fontRegion: "footer";
                id: title; objectName: "currentTitle"; width: parent.width
                text: playbackBridge.hasTrack ? playbackBridge.title : "Relaxer"
                textFormat: Text.PlainText; font.pixelSize: 15; font.weight:Font.DemiBold;color: Theme.text; elide: Text.ElideRight
            }
            UiText { fontRegion: "footer";
                id: artist; objectName: "currentArtist"; width: parent.width
                text: playbackBridge.hasTrack ? playbackBridge.artist : "选一首歌，开始放松。"
                textFormat: Text.PlainText; font.pixelSize: 14; color: Theme.secondary; elide: Text.ElideRight
            }
        }
        MouseArea {
            anchors.fill: parent; enabled: !bar.expanded && !bar.artworkTravelling
            cursorShape: Qt.PointingHandCursor
            onClicked: {metadata.forceActiveFocus();bar.expandRequested()}
        }
    }
    Item {
        id: center
        objectName: "playerCenter"
        x: 356; y: 12; width: Math.max(0, bar.width - 712); height: 74
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            height: 42; spacing: 12
            IconButton {
                objectName: "currentFavoriteButton"; anchors.verticalCenter: parent.verticalCenter
                symbol: playbackBridge.currentFavorite ? "liked" : "heart"
                iconColor: playbackBridge.currentFavorite ? Theme.accent : Theme.text
                text: playbackBridge.currentFavorite ? "取消喜欢" : "喜欢歌曲"
                enabled: playbackBridge.available && playbackBridge.hasTrack
                onClicked: playbackBridge.toggleFavorite()
            }
            IconButton {
                objectName: "previousButton"; anchors.verticalCenter: parent.verticalCenter; symbol: "previous"; text: "上一首"
                enabled: playbackBridge.available && playbackBridge.hasTrack; onClicked: playbackBridge.previous()
            }
            IconButton {
                objectName: "playPauseButton"; anchors.verticalCenter: parent.verticalCenter
                circular: true; filled: true; symbol: playbackBridge.playing ? "pause" : "play"
                text: playbackBridge.playing ? "暂停" : "播放"
                enabled: playbackBridge.available && playbackBridge.hasTrack && playbackBridge.canPlay
                onClicked: playbackBridge.toggle()
            }
            IconButton {
                objectName: "nextButton"; anchors.verticalCenter: parent.verticalCenter; symbol: "next"; text: "下一首"
                enabled: playbackBridge.available && playbackBridge.hasTrack; onClicked: playbackBridge.next()
            }
            IconButton {
                objectName: "playMode"; showTooltip:true; anchors.verticalCenter: parent.verticalCenter
                symbol: ["playlist", "repeat", "single", "shuffle"][playbackBridge.mode]
                text: ["顺序播放", "列表循环", "单曲循环", "随机循环"][playbackBridge.mode]
                Accessible.name: text
                enabled: playbackBridge.available; onClicked: playbackBridge.setMode((playbackBridge.mode + 1) % 4)
            }
        }
        RowLayout {
            x: 0; y: 46; width: parent.width; height: 22; spacing: 8
            UiText { fontRegion: "footer";
                objectName: "playbackState"; Layout.preferredWidth: 40
                text: bar.timeText(playbackBridge.position); color: Theme.secondary; font.pixelSize: 12
            }
            PlaybackSlider {
                id: seek; objectName: "playbackProgress"; Layout.fillWidth: true
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
                Binding { target: seek; property: "value"; value: Math.min(playbackBridge.position,seek.to); when: !seek.pressed; restoreMode: Binding.RestoreBindingOrValue }
            }
            UiText { fontRegion: "footer";
                objectName: "durationDisplay"; Layout.preferredWidth: 40; horizontalAlignment: Text.AlignRight
                text: bar.timeText(playbackBridge.duration); color: Theme.secondary; font.pixelSize: 12
            }
        }
    }
    Item {
        objectName: "playerRight"
        x: bar.width - 342; y: 12; width: 320; height: 74
        Item {
            id: volumeHost; objectName: "volumeControls"; width: 320; height: 36
            y: bar.expanded ? 19 : 0
            Behavior on y { NumberAnimation { duration: settingsBridge.values.reduceMotion ? 0 : 300; easing.type: Easing.OutCubic } }
            Row {
                anchors.right: parent.right; height: 36; spacing: 14
                IconButton {
                    objectName: "muteButton"; anchors.verticalCenter: parent.verticalCenter
                    symbol: playbackBridge.volume === 0 ? "volumeMuted" : playbackBridge.volume < 34 ? "volumeLow" : playbackBridge.volume < 67 ? "volumeMedium" : "volumeHigh"
                    text: playbackBridge.muted ? "取消静音" : "静音"; onClicked: playbackBridge.toggleMute()
                }
                PlaybackSlider {
                    objectName: "volumeSlider"; anchors.verticalCenter: parent.verticalCenter; width: 80
                    from: 0; to: 100; stepSize: 1; value: playbackBridge.volume
                    onMoved: playbackBridge.setVolume(Math.round(value))
                    Accessible.name: "音量"
                }
                IconButton {
                    objectName: "playbackExpandButton"; anchors.verticalCenter: parent.verticalCenter
                    symbol: bar.expanded ? "contract" : "expand"; text: bar.expanded ? "收起播放" : "展开播放"
                    showTooltip: true; onClicked: {metadata.forceActiveFocus();bar.expandRequested()}
                }
            }
        }
    }
}
