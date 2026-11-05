import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../theme"
Rectangle {
    id: bar
    objectName: "playerBar"
    property bool expanded: false
    readonly property bool presentationActive:visible&&(!Window.window || (Window.window.visible && Window.window.visibility !== Window.Minimized))
    property bool artworkTravelling: false
    property bool miniControlsTravelling:false
    function miniTarget(key){return ({queue:queueButton,favorite:favoriteButton,previous:previousButton,play:playButton,next:nextButton,mode:modeButton,volume:compactVolume})[key]}
    function settleMiniLayout(){transportRow.forceLayout()}
    property real backgroundCanvasHeight: height
    property real backgroundOffsetY: 0
    readonly property alias smallArtwork: art
    readonly property alias metadataTitle: title
    readonly property alias metadataArtist: artist
    readonly property alias playerCenter: center
    readonly property alias queueControl:queueButton
    readonly property real queueControlX:center.x+transportRow.x+queueButton.x
    readonly property real queueControlY:center.y+transportRow.y+queueButton.y
    readonly property real coverMargin:11
    readonly property real wideMargin:coverMargin
    readonly property real wideMetadataWidth:Math.min(304,Math.max(130,(width-288)/2))
    readonly property real wideCenterX:wideMargin+wideMetadataWidth+14
    readonly property real wideRightSpace:Math.min(356,Math.max(168,168+(width-548)*188/692))
    readonly property bool wideVolumeSlider:width>=900
    property real expansion:expanded?1:0
    Behavior on expansion {NumberAnimation{duration:settingsBridge.values.reduceMotion?0:420;easing.type:Easing.OutCubic}}
    readonly property real compactSpacing:Math.min(8,Math.max(4,(width-382)/6))
    readonly property real compactControlsLeft:width-18-(258+6*compactSpacing)
    readonly property real compactMetadataWidth:Math.max(76,compactControlsLeft-12-coverMargin)
    signal expandRequested()
    signal queueRequested(var anchor)
    readonly property real compactProgress:Theme.layoutProgress
    function blend(wide,narrow){return wide+(narrow-wide)*compactProgress}
    function showMetadata(original,elided) {
        const total=Array.from(original.replace(/\s/g,"")).length
        const shown=Array.from(elided.replace(/[\s…]/g,"")).length
        return !(shown<=2 && total-shown>3)
    }
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
        source:!bar.presentationActive ? "" : String(settingsBridge.palette.footerImage||"").length?"image://relaxer/background?file="+encodeURIComponent(String(settingsBridge.palette.footerImage))+"&blur="+Number(settingsBridge.palette.blurFooter||0):"";fillMode:Image.PreserveAspectCrop;asynchronous:true
      }
    }
    Rectangle { width: parent.width; height: 1; color: Theme.border }
    Item {
        id: metadata
        objectName: "playerMetadata"
        x:bar.coverMargin;y:bar.coverMargin;width:bar.blend(bar.wideMetadataWidth,bar.compactMetadataWidth);height:bar.height-2*bar.coverMargin
        opacity:bar.artworkTravelling||bar.expanded?0:1
        Artwork {
            id: art; objectName: "bottomCover"
            x: 0; anchors.verticalCenter: parent.verticalCenter; width:parent.height;height:width; radius: 10
            file: playbackBridge.coverUrl; seed: playbackBridge.hasTrack ? playbackBridge.title : "Relaxer"
            revision: playbackBridge.artworkRevision; resolution: 160; animated: false
        }
        Item {
            id:metadataText;clip:true
            x:art.width+12;width:Math.max(0,parent.width-x-bar.blend(12,0)); anchors.verticalCenter: parent.verticalCenter
            // Keep distinct landing rows even when the available width hides both labels.
            height:title.implicitHeight+4+artist.implicitHeight
            UiText { fontRegion: "footer";
                id: title; objectName: "currentTitle"; width: parent.width
                text: playbackBridge.hasTrack ? playbackBridge.title : "Relaxer"
                visible:!Theme.compact||bar.showMetadata(text,titleMetrics.elidedText)
                textFormat: Text.PlainText; font.pixelSize: 15; font.weight:Font.DemiBold;color: Theme.text; elide: Text.ElideRight
            }
            UiText { fontRegion: "footer";
                id: artist; objectName: "currentArtist"; y:title.height+4; width: parent.width
                text: playbackBridge.hasTrack ? playbackBridge.artist : "选一首歌，开始放松。"
                visible:!Theme.compact||bar.showMetadata(text,artistMetrics.elidedText)
                textFormat: Text.PlainText; font.pixelSize: 14; color: Theme.secondary; elide: Text.ElideRight
            }
        }
        TextMetrics {id:titleMetrics;text:title.text;font:title.font;elide:Qt.ElideRight;elideWidth:title.width}
        TextMetrics {id:artistMetrics;text:artist.text;font:artist.font;elide:Qt.ElideRight;elideWidth:artist.width}
        MouseArea {
            anchors.fill: parent; enabled: !bar.expanded && !bar.artworkTravelling
            cursorShape: Qt.PointingHandCursor
            onClicked: {metadata.forceActiveFocus();bar.expandRequested()}
        }
    }
    Item {
        id: center
        objectName: "playerCenter"
        x:bar.blend(bar.wideCenterX,Math.min(112,bar.compactControlsLeft)*(1-bar.expansion)+18*bar.expansion);y:12;width:Math.max(0,bar.blend(bar.width-bar.wideCenterX-bar.wideRightSpace,bar.width-18-(Math.min(112,bar.compactControlsLeft)*(1-bar.expansion)+18*bar.expansion)));height:74
        Row {
            id:transportRow
            visible:!bar.miniControlsTravelling
            x:Math.max(0,(parent.width-width)*(0.5+(0.5*(1-bar.expansion))*bar.compactProgress))
            height:42;spacing:bar.blend(bar.width<700?8:12,bar.compactSpacing)
            IconButton {
                id:queueButton;objectName:"compactQueueButton";anchors.verticalCenter:parent.verticalCenter
                visible:bar.compactProgress>0;opacity:bar.compactProgress;scale:bar.compactProgress;width:36*bar.compactProgress
                text:"下一首列表";showTooltip:true
                contentItem:Item {
                    Repeater {model:3;Rectangle {required property int index;x:0;y:5+index*6;width:8+index*6;height:2;radius:1;color:Theme.text}}
                }
                onClicked:bar.queueRequested(queueButton)
            }
            IconButton {
                id:favoriteButton;objectName: "currentFavoriteButton"; anchors.verticalCenter: parent.verticalCenter
                symbol: playbackBridge.currentFavorite ? "liked" : "heart"
                iconColor: playbackBridge.currentFavorite ? Theme.accent : Theme.text
                text: playbackBridge.currentFavorite ? "取消喜欢" : "喜欢歌曲"
                enabled: playbackBridge.available && playbackBridge.hasTrack
                onClicked: playbackBridge.toggleFavorite()
            }
            IconButton {
                id:previousButton;objectName: "previousButton"; anchors.verticalCenter: parent.verticalCenter; symbol: "previous"; text: "上一首"
                enabled: playbackBridge.available && playbackBridge.hasTrack; onClicked: playbackBridge.previous()
            }
            IconButton {
                id:playButton;objectName: "playPauseButton"; anchors.verticalCenter: parent.verticalCenter
                circular: true; filled: true; symbol: playbackBridge.playing ? "pause" : "play"
                text: playbackBridge.playing ? "暂停" : "播放"
                enabled: playbackBridge.available && playbackBridge.hasTrack && playbackBridge.canPlay
                onClicked: playbackBridge.toggle()
            }
            IconButton {
                id:nextButton;objectName: "nextButton"; anchors.verticalCenter: parent.verticalCenter; symbol: "next"; text: "下一首"
                enabled: playbackBridge.available && playbackBridge.hasTrack; onClicked: playbackBridge.next()
            }
            IconButton {
                id:modeButton;objectName: "playMode"; showTooltip:true; anchors.verticalCenter: parent.verticalCenter
                symbol: ["playlist", "repeat", "single", "shuffle"][playbackBridge.mode]
                text: ["顺序播放", "列表循环", "单曲循环", "随机循环"][playbackBridge.mode]
                Accessible.name: text
                enabled: playbackBridge.available; onClicked: playbackBridge.setMode((playbackBridge.mode + 1) % 4)
            }
            Item {
                id:compactVolumeSlot;anchors.verticalCenter:parent.verticalCenter
                visible:bar.compactProgress>0;width:36*bar.compactProgress;height:36
            }
        }
        RowLayout {
            x:(parent.width-width)*(1-0.5*bar.expansion);y:46;width:bar.blend(parent.width,Math.min(306,parent.width)); height: 22; spacing: 8
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
        opacity:1-bar.compactProgress;visible:opacity>0;enabled:!Theme.compact
        x: bar.width - 342; y: 12; width: 320; height: 74
        Item {
            id: volumeHost; objectName: "volumeControls"; width: 320; height: 36
            y:19*bar.expansion
            Row {
                anchors.right: parent.right; height: 36; spacing: 14
                Item {width:36;height:36}
                PlaybackSlider {
                    objectName: "volumeSlider";visible:bar.wideVolumeSlider; anchors.verticalCenter: parent.verticalCenter; width: 80
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
    function closeTransientPopups(){compactVolume.closePopup()}
    VolumeButton {
        id:compactVolume;objectName:"muteButton";width:36;height:36
        visible:!bar.miniControlsTravelling
        x:bar.blend(bar.width-(bar.wideVolumeSlider?202:108),center.x+transportRow.x+compactVolumeSlot.x)
        y:bar.blend(12+19*bar.expansion,center.y+transportRow.y+compactVolumeSlot.y)
    }
    onExpandedChanged:compactVolume.closePopup()
}
