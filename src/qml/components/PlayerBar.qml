import QtQuick
import QtQuick.Controls
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
    property Item popupBackdrop
    property real backgroundOffsetY: 0
    readonly property alias smallArtwork: art
    readonly property alias metadataTitle: title
    readonly property alias metadataArtist: artist
    readonly property alias playerCenter: center
    readonly property alias queueControl:queueButton
    readonly property real queueControlX:transportRow.x+queueButton.x
    readonly property real queueControlY:transportRow.y+queueButton.y
    readonly property real coverMargin:11
    readonly property real wideMargin:coverMargin
    property real wideLayoutWidth:width
    property real compactLayoutWidth:width
    readonly property real wideCenterWidth:Math.min(400,Math.max(240,wideLayoutWidth*.36))
    readonly property real wideCenterX:(wideLayoutWidth-wideCenterWidth)/2
    // Include the hover surface's 5px outset when leaving space before the timeline.
    readonly property real wideMetadataWidth:Math.max(0,Math.min((height-2*coverMargin)*(playbackBridge.video?4/3:1)+12+Math.ceil(Math.max(titleNatural.implicitWidth,artistNatural.implicitWidth))+6,transportRow.x+favoriteButton.x-22-coverMargin+2))
    readonly property real wideRightSpace:Math.min(356,Math.max(168,168+(wideLayoutWidth-548)*188/692))
    readonly property real wideVolumeX:Math.max(wideLayoutWidth-202,wideCenterX+wideCenterWidth+2*coverMargin)
    readonly property real wideVolumeIconX:wideVolumeX+9
    readonly property bool wideVolumeSlider:wideLayoutWidth>=547
    readonly property real wideVolumeSliderWidth:Math.max(1,Math.min(80,wideLayoutWidth-coverMargin-36-10-(wideVolumeX+36+10)))
    property real expansion:expanded?1:0
    // The shell supplies its existing playerProgress. Do not start another
    // animation here: tweening an already animated value lags on every frame.
    readonly property real transportButtonWidth:blend(36,32)
    readonly property real transportButtonPadding:blend(7,5)
    readonly property real compactSpacing:6+4*Math.max(0,Math.min(1,(compactLayoutWidth-414)/132))
    readonly property real compactControlsLeft:compactLayoutWidth-coverMargin-(7*32+playButton.implicitWidth+7*compactSpacing)
    readonly property real compactTimelineStartWidth:Math.min(306,Math.max(0,compactLayoutWidth-coverMargin-Math.min(112,compactControlsLeft)))
    readonly property real compactTimelineEndWidth:Math.min(306,Math.max(0,compactLayoutWidth-2*coverMargin))
    readonly property real compactTimelineStartX:compactLayoutWidth-coverMargin-compactTimelineStartWidth
    readonly property real compactTimelineEndX:(compactLayoutWidth-compactTimelineEndWidth)/2
    readonly property real compactMetadataWidth:Math.max((height-2*coverMargin)*(playbackBridge.video?4/3:1),compactControlsLeft-18-coverMargin)
    signal expandRequested()
    signal narrowRequested()
    signal queueRequested(var anchor)
    readonly property real compactProgress:Theme.layoutProgress
    function blend(wide,narrow){return wide+(narrow-wide)*compactProgress}
    function showMetadata(original,elided) {
        const total=Array.from(original.replace(/\s/g,"")).length
        const shown=Array.from(elided.replace(/[\s…]/g,"")).length
        return !(shown<=2 && total-shown>3)
    }
    implicitHeight: 98*.9*.9
    color: Theme.footer
    function timeText(ms) {
        const seconds = Math.floor(Math.max(0, ms) / 1000)
        return Math.floor(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + seconds % 60
    }
    Item {
      id:footerBackdrop;anchors.fill: parent; clip: true
      Rectangle {anchors.fill:parent;color:Theme.footer}
      Image {
        objectName: "footerBackground"
        x: 0; y: settingsBridge.palette.shareFooter ? -bar.backgroundOffsetY : 0
        width: parent.width; height: settingsBridge.palette.shareFooter ? bar.backgroundCanvasHeight : parent.height
        source:!bar.visible ? "" : String(settingsBridge.palette.footerImage||"").length?"image://relaxer/background?file="+encodeURIComponent(String(settingsBridge.palette.footerImage))+"&blur="+Number(settingsBridge.palette.blurFooter||0):"";fillMode:Image.PreserveAspectCrop;asynchronous:true
      }
    }
    Binding {target:Theme;property:"footerBackdrop";value:footerBackdrop}
    Rectangle { width: parent.width; height: 1; color: Theme.border }
    Item {
        id: metadata
        objectName: "playerMetadata"
        x:bar.coverMargin;y:bar.coverMargin;width:bar.blend(bar.wideMetadataWidth,bar.compactMetadataWidth);height:bar.height-2*bar.coverMargin
        opacity:bar.artworkTravelling||bar.expanded?0:1
        HighlightSurface {
            objectName:"playerMetadataHover";anchors.fill:parent;anchors.margins:-5;radius:12
            positionRevision:metadata.x+metadata.y;backdrop:Theme.footerBackdrop;hovered:metadataMouse.enabled&&metadataMouse.containsMouse;pressed:metadataMouse.enabled&&metadataMouse.pressed
        }
        Artwork {
            id: art; objectName: "bottomCover"
            x: 0; anchors.verticalCenter: parent.verticalCenter; width:parent.height*(playbackBridge.video?4/3:1);height:parent.height;videoCover:playbackBridge.video;
            file: playbackBridge.coverUrl; contentKey:playbackBridge.trackId; seed: playbackBridge.hasTrack ? playbackBridge.title : "Relaxer"
            revision: playbackBridge.artworkRevision; resolution: 160; animated: false
        }
        Item {
            id:metadataText;clip:true
            x:art.width+12;width:Math.max(0,parent.width-x-bar.blend(2,0)); anchors.verticalCenter: parent.verticalCenter
            // Keep distinct landing rows even when the available width hides both labels.
            height:title.implicitHeight+4+artist.implicitHeight
            UiText { fontRegion: "footer";
                id: title; objectName: "currentTitle"; width: parent.width
                text: playbackBridge.hasTrack ? playbackBridge.title : "Relaxer"
                visible:!Theme.compact||bar.showMetadata(text,titleMetrics.elidedText)
                textFormat: Text.PlainText; basePixelSize: 15; font.weight:Font.DemiBold;color: Theme.text; elide: Text.ElideRight
            }
            UiText { fontRegion: "footer";
                id: artist; objectName: "currentArtist"; y:title.height+4; width:bar.blend(Math.max(0,Math.min(parent.width,bar.wideCenterX-2*bar.coverMargin-metadata.x-metadataText.x)),parent.width)
                text: playbackBridge.hasTrack ? playbackBridge.artist+(playbackBridge.audioPreview?" · 试听":"") : "选一首歌，开始放松。"
                visible:!Theme.compact||bar.showMetadata(text,artistMetrics.elidedText)
                textFormat: Text.PlainText; basePixelSize: 14; color: Theme.secondary; elide: Text.ElideRight
            }
        }
        Text {id:titleNatural;visible:false;text:title.text;font:title.font;textFormat:Text.PlainText}
        Text {id:artistNatural;visible:false;text:artist.text;font:artist.font;textFormat:Text.PlainText}
        TextMetrics {id:titleMetrics;text:title.text;font:title.font;elide:Qt.ElideRight;elideWidth:title.width}
        TextMetrics {id:artistMetrics;text:artist.text;font:artist.font;elide:Qt.ElideRight;elideWidth:artist.width}
        MouseArea {
            id:metadataMouse;anchors.fill: parent; enabled: !bar.expanded && !bar.artworkTravelling;hoverEnabled:true
            cursorShape: Qt.PointingHandCursor
            onClicked: {metadata.forceActiveFocus();bar.expandRequested()}
        }
    }
    Item {
        id: center
        objectName: "playerCenter"
        x:bar.blend(bar.wideCenterX,Math.min(112,bar.compactControlsLeft)*(1-bar.expansion)+bar.coverMargin*bar.expansion);y:(bar.height-height)/2+3;width:Math.max(0,bar.blend(bar.wideCenterWidth,bar.compactLayoutWidth-bar.coverMargin-(Math.min(112,bar.compactControlsLeft)*(1-bar.expansion)+bar.coverMargin*bar.expansion)));height:74
        Row {
            id:transportRow;objectName:"playbackControls";parent:bar
            y:center.y
            visible:!bar.miniControlsTravelling
            // Keep the moving row in footer coordinates. Moving a parent and
            // compensating in a child creates separate geometry updates.
            readonly property real compactLeft:(bar.compactLayoutWidth-bar.coverMargin-bar.transportButtonWidth-spacing-width)*(1-bar.expansion)+(bar.compactLayoutWidth-width)/2*bar.expansion
            x:bar.blend(bar.wideCenterX+(bar.wideCenterWidth-width)/2,compactLeft)
            height:42;spacing:bar.blend(bar.wideLayoutWidth<700?8:12,bar.compactSpacing)
            IconButton {
                id:queueButton;objectName:"compactQueueButton";anchors.verticalCenter:parent.verticalCenter
                visible:bar.compactProgress>0;opacity:bar.compactProgress;scale:bar.compactProgress;width:bar.transportButtonWidth*bar.compactProgress;padding:bar.transportButtonPadding
                text:"下一首列表";showTooltip:true
                contentItem:Item {
                    id:queueGlyph
                    readonly property real pixelRatio:Screen.devicePixelRatio
                    readonly property real lineHeight:Math.max(1,Math.round(1.25*pixelRatio))/pixelRatio
                    Repeater {model:[7,12,16]
                        Rectangle {
                            required property int index;required property real modelData
                            objectName:"queueLine-"+index
                            x:Math.round((queueGlyph.width-16)/2*queueGlyph.pixelRatio)/queueGlyph.pixelRatio
                            y:Math.round((queueGlyph.height/2+(index-1)*5.6-queueGlyph.lineHeight/2)*queueGlyph.pixelRatio)/queueGlyph.pixelRatio
                            width:Math.round(modelData*queueGlyph.pixelRatio)/queueGlyph.pixelRatio;height:queueGlyph.lineHeight
                            radius:height/2;antialiasing:true;color:Theme.text
                        }
                    }
                }
                onClicked:bar.queueRequested(queueButton)
            }
            IconButton {
                id:favoriteButton;width:bar.transportButtonWidth;padding:bar.transportButtonPadding;objectName: "currentFavoriteButton"; anchors.verticalCenter: parent.verticalCenter
                symbol: playbackBridge.currentFavorite ? "liked" : "heart"
                iconColor: playbackBridge.currentFavorite ? Theme.accent : Theme.text
                text: playbackBridge.currentFavorite ? "取消喜欢" : "喜欢歌曲"
                enabled: playbackBridge.available && playbackBridge.hasTrack
                onClicked: playbackBridge.toggleFavorite()
            }
            IconButton {
                id:previousButton;width:bar.transportButtonWidth;padding:bar.transportButtonPadding;objectName: "previousButton"; anchors.verticalCenter: parent.verticalCenter; symbol: "previous"; text: "上一首"
                enabled: playbackBridge.available && playbackBridge.hasTrack; onClicked: playbackBridge.previous()
            }
            IconButton {
                id:playButton;objectName: "playPauseButton"; anchors.verticalCenter: parent.verticalCenter
                implicitWidth:42*.9;implicitHeight:42*.9;padding:10*.9
                circular: true; filled: true; symbol: playbackBridge.playing ? "pause" : "play"
                text: playbackBridge.playing ? "暂停" : "播放"
                enabled: playbackBridge.available && playbackBridge.hasTrack && playbackBridge.canPlay
                onClicked: playbackBridge.toggle()
            }
            IconButton {
                id:nextButton;width:bar.transportButtonWidth;padding:bar.transportButtonPadding;objectName: "nextButton"; anchors.verticalCenter: parent.verticalCenter; symbol: "next"; text: "下一首"
                enabled: playbackBridge.available && playbackBridge.hasTrack; onClicked: playbackBridge.next()
            }
            IconButton {
                id:modeButton;width:bar.transportButtonWidth;padding:bar.transportButtonPadding;objectName: "playMode"; showTooltip:true; anchors.verticalCenter: parent.verticalCenter
                symbol: ["playlist", "repeat", "single", "shuffle"][playbackBridge.mode]
                text: ["顺序播放", "列表循环", "单曲循环", "随机循环"][playbackBridge.mode]
                Accessible.name: text
                enabled: playbackBridge.available; onClicked: playbackBridge.setMode((playbackBridge.mode + 1) % 4)
            }
            Item {
                id:compactVolumeSlot;anchors.verticalCenter:parent.verticalCenter
                visible:bar.compactProgress>0;width:bar.transportButtonWidth*bar.compactProgress;height:36
            }
        }
        Item {
            id:timeline;parent:bar
            // Resolve both endpoint sizes before interpolating. A changing
            // minimum/RowLayout must not alter the trajectory mid-animation.
            x:bar.blend(bar.wideCenterX,bar.compactTimelineStartX+(bar.compactTimelineEndX-bar.compactTimelineStartX)*bar.expansion)
            y:center.y+46
            width:bar.blend(bar.wideCenterWidth,bar.compactTimelineStartWidth+(bar.compactTimelineEndWidth-bar.compactTimelineStartWidth)*bar.expansion);height:22
            UiText { fontRegion: "footer";
                objectName: "playbackState";x:0;width:40;height:parent.height;verticalAlignment:Text.AlignVCenter
                text: bar.timeText(playbackBridge.position); color: Theme.secondary; basePixelSize: 12
            }
            PlaybackSlider {
                id: seek; objectName: "playbackProgress";x:48;width:Math.max(0,parent.width-96);height:parent.height
                property string dragTrackKey: "0"
                property real dragValue: 0
                from: 0; to: Math.max(1, playbackBridge.duration)
                showBuffer: true; bufferedRanges: playbackBridge.bufferedRanges
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
                objectName: "durationDisplay";x:parent.width-width;width:40;height:parent.height;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignRight
                text: bar.timeText(playbackBridge.duration); color: Theme.secondary; basePixelSize: 12
            }
        }
    }
    Item {
        objectName: "playerRight"
        opacity:1-bar.compactProgress;visible:opacity>0;enabled:!Theme.compact
        x: bar.wideLayoutWidth - 342; y:(bar.height-height)/2; width: 320; height: 74
        Item {
            id: volumeHost; objectName: "volumeControls"; width: 320; height: 36
            y:19*bar.expansion
            PlaybackSlider {
                objectName:"volumeSlider";visible:bar.wideVolumeSlider
                x:bar.wideVolumeX+46-(bar.wideLayoutWidth-342);anchors.verticalCenter:parent.verticalCenter;width:bar.wideVolumeSliderWidth
                from:0;to:100;stepSize:1;value:playbackBridge.volume
                onMoved:playbackBridge.setVolume(Math.round(value))
                Accessible.name:"音量"
            }
        }
    }
    IconButton {
        id:widthButton;objectName:"playbackWidthButton";width:bar.transportButtonWidth;height:36;padding:bar.transportButtonPadding
        visible:!bar.miniControlsTravelling
        x:bar.blend(bar.wideLayoutWidth,bar.compactLayoutWidth)-bar.coverMargin-width
        y:bar.blend((bar.height-74)/2+19*bar.expansion,transportRow.y+(transportRow.height-height)/2)
        symbol:Theme.compact?"expand":"contract";text:Theme.compact?"恢复宽窗口":"收窄窗口";showTooltip:true
        onClicked:{compactVolume.closePopup();bar.narrowRequested()}
    }
    function closeTransientPopups(){compactVolume.closePopup()}
    VolumeButton {
        id:compactVolume;objectName:"muteButton";width:bar.transportButtonWidth;height:36;padding:bar.transportButtonPadding
        backdropSource:bar.popupBackdrop;secondaryBackdrop:bar
        // Keep Basic's horizontal inset explicit while the two layouts blend.
        leftPadding:bar.transportButtonPadding+2;rightPadding:bar.transportButtonPadding+2
        visible:!bar.miniControlsTravelling
        x:bar.blend(bar.wideVolumeX,transportRow.x+compactVolumeSlot.x)
        y:bar.blend((bar.height-74)/2+19*bar.expansion,transportRow.y+compactVolumeSlot.y)
    }
    onExpandedChanged:compactVolume.closePopup()
}
