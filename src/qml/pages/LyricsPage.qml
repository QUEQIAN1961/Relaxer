import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtMultimedia
import "../components"
import "../theme"
Item {
    id:page
    objectName:"lyricsPage"
    readonly property real reflow:Theme.layoutProgress
    readonly property real shrink:Math.min(1,2*reflow)
    readonly property real arrange:Math.max(0,2*reflow-1)
    readonly property real wideColumnWidth:Math.max(0,(columns.width-32)/2)
    readonly property real wideCoverSize:Math.max(120,Math.min(360,wideColumnWidth-40,page.height-310))
    function blend(wide,narrow){return wide+(narrow-wide)*reflow}
    property url previewBackdrop
    property bool fullScreen:false
    property bool metadataTravelling:false
    readonly property bool localVideo:playbackBridge.video&&!playbackBridge.onlineActive
    readonly property bool audioVideo:playbackBridge.video&&(playbackBridge.onlineActive?settingsBridge.values.onlineVideoMode===1:settingsBridge.values.localVideoMode===1)
    readonly property bool pictureVideo:playbackBridge.video&&!audioVideo
    readonly property bool localLyrics:localVideo&&settingsBridge.values.showLocalVideoLyrics
    readonly property bool allowVideoPicture:pictureVideo
    readonly property bool startupVideoPreview:playbackBridge.video&&Window.window&&typeof Window.window.initialVideoPreviewId==="string"&&Window.window.initialVideoPreviewId===playbackBridge.trackId
    property Item metadataTitle:page.pictureVideo?videoTitle:largeTitle
    property Item metadataArtist:page.pictureVideo?videoArtist:largeArtist
    property bool artworkTravelling:false
    property real detailsOpacity:1
    property int trackDirection:1
    property Item largeArtwork:page.pictureVideo?videoCover:art
    property bool toolsVisible:false
    property bool delayOpen:false
    readonly property bool windowActive:!Window.window||(Window.window.visible&&Window.window.visibility!==Window.Minimized)
    readonly property bool presentationActive:visible&&windowActive
    function updateArtworkQuality(){playbackBridge.setHighResolutionArtwork(presentationActive&&!artworkTravelling)}
    onPresentationActiveChanged:{updateArtworkQuality();if(presentationActive)bilibili.ensureVideoParts()}
    onArtworkTravellingChanged:updateArtworkQuality()
    Component.onCompleted:{partsTrackKey=playbackBridge.trackId;updateArtworkQuality();bilibili.ensureVideoParts()}
    Component.onDestruction:playbackBridge.setHighResolutionArtwork(false)
    readonly property var videoParts:{const current=playbackBridge.trackId;return bilibili.videoParts}
    onVisibleChanged:if(!visible)partsPanel.close()
    property string partsTrackKey:""
    Connections {target:playbackBridge;function onTrackChanged(){if(page.partsTrackKey!==playbackBridge.trackId)partsPanel.close();page.partsTrackKey=playbackBridge.trackId}}
    signal collapseRequested()
    signal fullscreenRequested()
    signal translationRequested()
    Rectangle { anchors.fill:parent;color:Theme.background }
    Image {
        objectName:"playerBackdropPreview";anchors.fill:parent
        source:page.visible?page.previewBackdrop:""
        sourceSize.width:96;sourceSize.height:96;fillMode:Image.PreserveAspectCrop
        asynchronous:false;cache:true
    }
    Image {
        id:playerBackdrop
        objectName:"playerBackdrop"
        anchors.fill:parent
        property bool hasBackdrop:false
        Binding {
            target:playerBackdrop;property:"source"
            when:page.windowActive||!page.visible;restoreMode:Binding.RestoreNone
            value:!page.visible ? "" : settingsBridge.values.playerCoverBackground ? "image://relaxer/blur?file="+encodeURIComponent(playbackBridge.coverUrl)+"&seed="+encodeURIComponent(playbackBridge.hasTrack?playbackBridge.title:"Relaxer")+"&rev="+playbackBridge.artworkRevision+"&blur="+Number(settingsBridge.palette.blurExpanded||0) : String(settingsBridge.palette.expandedImage||"").length?"image://relaxer/background?file="+encodeURIComponent(String(settingsBridge.palette.expandedImage))+"&blur="+Number(settingsBridge.palette.blurExpanded||0):""
        }
        asynchronous:true;cache:false;retainWhileLoading:true
        sourceSize.width:720;sourceSize.height:720;fillMode:Image.PreserveAspectCrop
        visible:source.toString().length>0
        onStatusChanged:if(status!==Image.Loading)hasBackdrop=status===Image.Ready
        opacity:hasBackdrop?1:0
        Behavior on opacity {NumberAnimation {duration:settingsBridge.values.reduceMotion?0:180}}
    }
    Rectangle{anchors.fill:parent;color:Theme.background;opacity:settingsBridge.values.playerCoverBackground?.68:0}
    readonly property bool partsVisible:partsPanel.visible
    readonly property bool chromeHovered:wideCollapse.hovered||wideParts.hovered||localVideoSearch.hovered
    readonly property bool chromeShown:!Window.window||typeof Window.window.playerChromeVisible!=="boolean"||Window.window.playerChromeVisible
    property real chromeOpacity:chromeShown?1:0
    Behavior on chromeOpacity {NumberAnimation {duration:settingsBridge.values.reduceMotion?0:180}}
    ShellButton {
        id:wideCollapse;objectName:"playerCollapse";visible:!Theme.compact
        x:Theme.pageMargin;y:28;height:36;flat:true;symbol:"collapse";text:"收起"
        opacity:page.detailsOpacity*page.chromeOpacity;enabled:opacity>0
        onClicked:page.collapseRequested()
    }
    ShellButton {
        id:wideParts;objectName:"videoPartsEntry";visible:!Theme.compact&&bilibili.currentVideo
        x:page.width-Theme.pageMargin-width;y:28;height:36;flat:true;text:page.videoParts.length?"选集 · P"+bilibili.currentPart+"/"+page.videoParts.length:"选集"
        opacity:page.detailsOpacity*page.chromeOpacity;enabled:opacity>0&&page.videoParts.length>1
        onClicked:page.toggleParts()
    }
    ShellButton {
        id:localVideoSearch;objectName:"videoLyricsSearch";visible:page.localVideo&&page.pictureVideo&&!Theme.compact
        x:page.width-Theme.pageMargin-width;y:28;height:36;flat:true;text:"查找歌词";symbol:"search"
        opacity:page.detailsOpacity*page.chromeOpacity;enabled:opacity>0;onClicked:page.translationRequested()
    }
    readonly property real captionHeight:Window.window&&typeof Window.window.captionHeight==="number"?Window.window.captionHeight:25.6
    readonly property real panelInset:!Theme.compact&&partsPanel.visible?(partsPanel.width+12)*partsPanel.reveal:0
    function toggleParts(){if(partsPanel.visible)partsPanel.close();else if(videoParts.length>1)partsPanel.open()}
    Item {
        id:videoMetadata;objectName:"videoMetadata";visible:page.pictureVideo
        x:Theme.compact?Theme.pageMargin:Theme.pageMargin+wideCollapse.width+12;y:Theme.compact?page.captionHeight+12:28
        width:Math.max(0,page.width-x-Theme.pageMargin-(Theme.compact?0:wideParts.visible?wideParts.width+12:localVideoSearch.visible?localVideoSearch.width+12:0))
        height:Theme.compact?66:36;opacity:page.detailsOpacity
        Artwork {
            id:videoCover;objectName:"videoMetadataCover";width:height*16/9;height:parent.height;videoCover:true;coverAspect:16/9;resolution:192
            file:playbackBridge.coverUrl;seed:playbackBridge.title;revision:playbackBridge.artworkRevision
            contentKey:playbackBridge.trackId;animated:page.visible&&!page.artworkTravelling;opacity:page.artworkTravelling?0:1
        }
        UiText {
            id:videoTitle;objectName:"videoMetadataTitle";x:videoCover.width+12
            y:Theme.compact?6:(parent.height-height)/2
            width:Math.max(0,Theme.compact?parent.width-x:Math.min(videoTitleMetrics.advanceWidth+2,(parent.width-x)*.65))
            text:playbackBridge.title;textFormat:Text.PlainText;elide:Text.ElideRight;basePixelSize:Theme.compact?18:17;font.weight:Font.DemiBold
            opacity:page.metadataTravelling?0:1
        }
        UiText {
            id:videoArtist;objectName:"videoMetadataArtist";x:Theme.compact?videoTitle.x:videoTitle.x+videoTitle.width+16
            y:Theme.compact?videoTitle.y+videoTitle.height+6:(parent.height-height)/2;width:Math.max(0,parent.width-x)
            text:playbackBridge.artist;textFormat:Text.PlainText;elide:Text.ElideRight;basePixelSize:14;color:Theme.secondary
            opacity:page.metadataTravelling?0:1
        }
    }
    TextMetrics {id:videoTitleMetrics;text:videoTitle.text;font:videoTitle.font}
    LyricsList {
        id: lyrics
        visible:!page.pictureVideo&&(!page.audioVideo||page.localLyrics)
        x:Math.max(0,columns.x+lyricSide.x-previewInset)
        leadingInset:columns.x+lyricSide.x-x
        y: columns.y + lyricSide.y
        width:columns.x+lyricSide.x+lyricSide.width-x; height: lyricSide.height
        opacity: page.detailsOpacity
    }
    UiText {
        objectName:"videoAudioMessage";visible:page.audioVideo&&!page.localLyrics
        x:columns.x+lyricSide.x;y:columns.y+lyricSide.y;width:lyricSide.width;height:lyricSide.height
        text:playbackBridge.onlineActive?"云端视频音轨，请欣赏":"本地视频音轨，请欣赏"
        horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;wrapMode:Text.Wrap
        basePixelSize:22;color:Theme.secondary;opacity:page.detailsOpacity
    }
    Item{
        id: columns
        visible:!page.pictureVideo
        anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.bottom:parent.bottom
        anchors.leftMargin:Theme.pageMargin;anchors.rightMargin:Theme.pageMargin;anchors.topMargin:page.blend(76,48);anchors.bottomMargin:20
        Item {
            id:metadataSide
            width:page.wideColumnWidth+(parent.width-page.wideColumnWidth)*page.arrange;height:parent.height
            Item {
                id:metadataGroup;objectName:"expandedMetadataGroup"
                width:parent.width
                y:Math.max(0,(parent.height-height)/2)*(1-page.shrink)
                height:Math.max(art.height,largeArtist.y+largeArtist.height)
                Artwork {
                    id:art;objectName:"currentArtwork"
                    x:Math.max(0,(page.wideColumnWidth-page.wideCoverSize)/2)*(1-page.reflow);y:0
                    width:page.wideCoverSize+(88-page.wideCoverSize)*page.shrink;height:width*(playbackBridge.video?9/16:1);videoCover:playbackBridge.video;coverAspect:playbackBridge.video?16/9:1
                    file:playbackBridge.coverUrl;contentKey:playbackBridge.trackId;seed:playbackBridge.hasTrack?playbackBridge.title:"Relaxer";revision:playbackBridge.artworkRevision;
                    animated:page.visible&&!page.artworkTravelling;direction:page.trackDirection;opacity:page.artworkTravelling?0:1
                }
                UiText {
                    id:largeTitle;objectName:"expandedSongTitle";x:20+88*page.shrink;y:(art.height+22)*(1-page.arrange)+14*page.arrange
                    width:Math.max(0,parent.width-x-page.blend(20,0))
                    text:playbackBridge.hasTrack?playbackBridge.title:"还没有音乐正在播放";textFormat:Text.PlainText
                    wrapMode:Theme.compact?Text.NoWrap:Text.Wrap;maximumLineCount:Theme.compact?1:2;elide:Text.ElideRight;horizontalAlignment:Theme.compact?Text.AlignLeft:Text.AlignHCenter
                    basePixelSize:page.blend(27,20);font.weight:Font.DemiBold;color:Theme.text;opacity:page.metadataTravelling?0:page.detailsOpacity
                }
                UiText {
                    id:largeArtist;objectName:"expandedSongArtist";x:largeTitle.x;y:Math.max((art.height+22+largeTitle.height+8)*(1-page.arrange)+52*page.arrange,largeTitle.y+largeTitle.height+8);width:largeTitle.width
                    text:playbackBridge.hasTrack?playbackBridge.artist+(playbackBridge.audioPreview?" · 试听":""):"选一首喜欢的歌，慢慢听。";textFormat:Text.PlainText
                    wrapMode:Theme.compact?Text.NoWrap:Text.Wrap;maximumLineCount:Theme.compact?1:2;elide:Text.ElideRight;horizontalAlignment:Theme.compact?Text.AlignLeft:Text.AlignHCenter
                    basePixelSize:16;color:Theme.secondary;opacity:page.metadataTravelling?0:page.detailsOpacity
                }
            }
        }
        Item{
            id:lyricSide;x:(page.wideColumnWidth+32)*(1-page.arrange);y:Math.max(195*page.shrink-85*page.arrange,(metadataGroup.y+metadataGroup.height+18)*page.shrink)
            width:Math.max(0,parent.width-x);height:parent.height-y;opacity:page.detailsOpacity
            HoverHandler{onPointChanged:{page.toolsVisible=true;if(!tools.opened)idle.restart()} onHoveredChanged:if(hovered){page.toolsVisible=true;if(!tools.opened)idle.restart()}}
            ToolButton{
                id:question;objectName:"lyricToolsQuestion";visible:!page.audioVideo||page.localVideo;width:20;height:20;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.rightMargin:26;anchors.bottomMargin:6
                text:"?";font.pixelSize: 14;opacity:page.toolsVisible||tools.opened?1:0;enabled:opacity>0
                background:Rectangle{radius:10;color:Theme.hover;border.color:Theme.border}
                onHoveredChanged: { if(hovered) {page.toolsVisible=true;idle.stop()} else if(!tools.hovered) idle.restart() }
                onClicked:{if(tools.opened)tools.close();else{page.delayOpen=false;tools.open()}}
                Behavior on opacity{NumberAnimation{duration:settingsBridge.values.reduceMotion?0:140}}
            }
            Timer{id:idle;interval:3000;onTriggered:if(!question.hovered&&!tools.hovered){page.toolsVisible=false;tools.close()}}
            Popup { popupType: Popup.Item;
                id:tools;objectName:"lyricToolsMenu";property bool hovered:false
                x:Math.max(0,lyricSide.width-width-28);y:Math.max(0,lyricSide.height-height-30)
                width:page.delayOpen?320:210;padding:12
                closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
                background:Rectangle{radius:12;color:Theme.card;border.color:Theme.border}
                HoverHandler{onHoveredChanged:{tools.hovered=hovered;if(hovered)idle.stop();else if(!question.hovered)idle.restart()}}
                onClosed:{page.delayOpen=false;idle.restart()}
                contentItem:ColumnLayout{
                    spacing:8
                    ShellButton{objectName:"lyricDelayEntry";Layout.fillWidth:true;visible:!page.delayOpen;text:lyricsModel.timed?"歌词延时":"歌词延时 · 查找滚动歌词";enabled:playbackBridge.hasTrack;onClicked:{if(lyricsModel.timed){page.delayOpen=true;idle.stop()}else{tools.close();page.translationRequested()}}}
                    CheckBox{visible:!page.delayOpen;text:"显示译文";checked:settingsBridge.values.showLyricTranslation;onClicked:settingsBridge.setPreference("showLyricTranslation",checked)}
                    ShellButton{visible:!page.delayOpen;Layout.fillWidth:true;text:"查找歌词…";enabled:playbackBridge.hasTrack;onClicked:{tools.close();page.translationRequested()}}
                    ShellButton{visible:page.delayOpen;text:"‹ 返回";onClicked:page.delayOpen=false}
                    RowLayout{
                        visible:page.delayOpen
                        UiText {text:"歌词延时";color:Theme.secondary}
                        EntryField {
                            id:offset;objectName:"lyricsOffset";Layout.fillWidth:true;text:(lyricsModel.offsetMs/1000).toFixed(2)+" s";enabled:lyricsModel.timed
                            onEditingFinished:{const v=Number(text.replace("s","").trim());if(!isNaN(v))lyricsModel.setOffsetMs(Math.max(-5000,Math.min(5000,Math.round(v*1000))));text=(lyricsModel.offsetMs/1000).toFixed(2)+" s"}
                        }
                        Column{
                            IconButton{width:20;height:16;symbol:"up";onClicked:lyricsModel.setOffsetMs(Math.min(5000,lyricsModel.offsetMs+10))}
                            IconButton{width:20;height:16;symbol:"down";onClicked:lyricsModel.setOffsetMs(Math.max(-5000,lyricsModel.offsetMs-10))}
                        }
                        ShellButton{text:"归零";onClicked:lyricsModel.setOffsetMs(0)}
                    }
                }
            }
        }
    }
    Rectangle {
        objectName:"videoPlaybackSurface";visible:page.pictureVideo
        anchors.fill:parent;anchors.leftMargin:Theme.pageMargin;anchors.rightMargin:Theme.pageMargin+page.panelInset
        anchors.topMargin:videoMetadata.y+videoMetadata.height+16;anchors.bottomMargin:12
        color:page.allowVideoPicture&&settingsBridge.values.videoBlackBars?"black":"transparent";opacity:page.detailsOpacity
        Artwork {
            objectName:"startupVideoPreview";anchors.centerIn:parent;radius:0
            width:Math.min(parent.width,parent.height*16/9);height:width*9/16
            visible:page.startupVideoPreview&&page.allowVideoPicture&&videoOutput.opacity<1;file:playbackBridge.coverUrl
            videoCover:true;coverAspect:16/9;resolution:768;animated:false;seed:playbackBridge.title;contentKey:playbackBridge.trackId;revision:playbackBridge.artworkRevision
        }
        Artwork {
            objectName:"onlineAudioCover";anchors.centerIn:parent;radius:0
            retainWhenHidden:page.pictureVideo&&page.visible
            width:Math.min(parent.width,parent.height*16/9);height:width*9/16;videoCover:true;coverAspect:16/9;resolution:768;animated:false
            visible:page.pictureVideo&&videoOutput.opacity<1&&!page.startupVideoPreview
            file:playbackBridge.coverUrl;contentKey:playbackBridge.trackId;seed:playbackBridge.title;revision:playbackBridge.artworkRevision
        }
        VideoOutput {
            id:videoOutput;objectName:"activeVideoOutput";anchors.centerIn:parent
            readonly property real videoAspect:sourceRect.width>0&&sourceRect.height>0?sourceRect.width/sourceRect.height:16/9
            width:settingsBridge.values.videoBlackBars?parent.width:Math.min(parent.width,parent.height*videoAspect)
            height:settingsBridge.values.videoBlackBars?parent.height:Math.min(parent.height,parent.width/videoAspect)
            fillMode:VideoOutput.PreserveAspectFit
            visible:page.allowVideoPicture
            opacity:page.allowVideoPicture&&playbackBridge.videoFrameReady?1:0
            Behavior on opacity {NumberAnimation {duration:settingsBridge.values.reduceMotion?0:180;easing.type:Easing.OutCubic}}
            onOpacityChanged:if(opacity>=.999&&playbackBridge.videoFrameReady&&page.startupVideoPreview&&page.Window.window)page.Window.window.initialVideoPreviewId=""
            Component.onCompleted:playbackBridge.setVideoSink(videoSink)
            Component.onDestruction:playbackBridge.setVideoSink(null)
        }
    }
    VideoLyricCaption {
        objectName:"videoLyricCaption";parent:videoOutput.parent;videoItem:videoOutput
        visible:page.pictureVideo&&page.localLyrics&&text.length>0&&playbackBridge.videoFrameReady
        opacity:videoOutput.opacity*page.detailsOpacity
    }
    MouseArea {anchors.fill:parent;visible:partsPanel.visible;onClicked:partsPanel.close()}
    VideoPartsPanel {id:partsPanel;parent:page;rows:page.videoParts;currentPart:bilibili.currentPart;onPartRequested:part=>bilibili.selectPart(part);panelTop:Theme.compact?videoMetadata.y:76}
}
