import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../theme"

Rectangle {
    id: mini;objectName:"miniPlayer"
    readonly property bool presentationActive:visible&&mini.Window.window&&mini.Window.window.visible&&mini.Window.window.visibility!==Window.Minimized
    color:Theme.footer
    signal restoreRequested()
    readonly property bool controlsVisible:miniHover.hovered||queue.visible||volumeControl.popupVisible
    property real controlsProgress:controlsVisible?1:0
    Behavior on controlsProgress {NumberAnimation{duration:settingsBridge.values.reduceMotion?0:160;easing.type:Easing.OutCubic}}
    property var lyricLine:({key:"",original:"",translation:"",start:0,end:0})
    readonly property bool showingLyrics:playbackBridge.playing&&lyricLine.original.length>0
    readonly property real lyricEnd:lyricLine.end>lyricLine.start?lyricLine.end:Math.max(lyricLine.start,Number(playbackBridge.duration))
    readonly property bool lyricOverflow:originalLine.implicitWidth>originalViewport.width||(translationViewport.visible&&translationLine.implicitWidth>translationViewport.width)
    readonly property bool marqueeActive:showingLyrics&&lyricOverflow&&presentationActive&&!controlsVisible&&!mini.Window.window.miniSwitching&&lyricEnd>lyricLine.start
    property real lyricScrollStart:0
    property real lyricTimelineProgress:0
    property real lastLyricPosition:0
    readonly property bool lyricReduceMotion:settingsBridge.values.reduceMotion
    readonly property real lyricScrollSpan:Math.max(1,lyricEnd-lyricScrollStart)
    readonly property real lyricHeadHold:Math.min(350,lyricScrollSpan*.12)
    readonly property real lyricTailHold:Math.min(250,lyricScrollSpan*.1)
    readonly property real lyricScrollProgress:Math.max(0,Math.min(1,(lyricTimelineProgress*lyricScrollSpan-lyricHeadHold)/(lyricScrollSpan-lyricHeadHold-lyricTailHold)))
    function refreshLyricLine(){
        const row=lyricsModel.currentIndex
        const valid=lyricsModel.timed&&!lyricsModel.loading&&row>=0&&lyricsModel.trackKey===playbackBridge.trackId
        const line=valid?{key:lyricsModel.trackKey+":"+row,original:lyricsModel.textAt(row).trim(),translation:lyricsModel.translationAt(row).trim(),
            start:Number(lyricsModel.timestampAt(row))+lyricsModel.offsetMs,end:row+1<lyricsModel.count?Number(lyricsModel.timestampAt(row+1))+lyricsModel.offsetMs:0}
            :{key:"",original:"",translation:"",start:0,end:0}
        if(line.key===lyricLine.key&&line.original===lyricLine.original&&line.translation===lyricLine.translation&&line.start===lyricLine.start&&line.end===lyricLine.end)return
        lyricLine=line
    }
    function syncLyricScroll(restart){
        if(!marqueeActive){lyricScrollMotion.stop();return}
        const position=Number(playbackBridge.position)
        if(restart)lyricScrollStart=Math.max(lyricLine.start,position)
        const progress=Math.max(0,Math.min(1,(position-lyricScrollStart)/lyricScrollSpan))
        // Let the scene graph animate between audio notifications. Re-anchor
        // on seeks or meaningful clock drift, not on every position update.
        if(!restart&&!lyricReduceMotion&&lyricScrollMotion.running&&Math.abs(progress-lyricTimelineProgress)*lyricScrollSpan<160)return
        lyricScrollMotion.stop();lyricTimelineProgress=progress
        if(lyricReduceMotion||progress>=1)return
        lyricScrollMotion.from=progress;lyricScrollMotion.duration=Math.max(1,lyricEnd-Math.max(lyricScrollStart,position));lyricScrollMotion.restart()
    }
    onLyricLineChanged:syncLyricScroll(true)
    onLyricEndChanged:syncLyricScroll(false)
    onMarqueeActiveChanged:syncLyricScroll(true)
    onLyricReduceMotionChanged:syncLyricScroll(false)
    Component.onCompleted:refreshLyricLine()
    NumberAnimation {id:lyricScrollMotion;objectName:"miniLyricScrollMotion";target:mini;property:"lyricTimelineProgress";to:1;easing.type:Easing.Linear}
    Connections {
        target:lyricsModel
        function onCurrentIndexChanged(){mini.refreshLyricLine()}
        function onDocumentChanged(){mini.refreshLyricLine()}
        function onOffsetChanged(){mini.refreshLyricLine()}
    }
    Connections {
        target:playbackBridge
        function onTrackChanged(){mini.refreshLyricLine()}
        function onProgressChanged(){
            const position=Number(playbackBridge.position),seeked=position<mini.lastLyricPosition-50||position>mini.lastLyricPosition+750
            mini.lastLyricPosition=position;mini.syncLyricScroll(seeked)
        }
    }
    HoverHandler {
        id:miniHover;objectName:"miniHoverHandler"
        onHoveredChanged:if(!hovered&&queue.visible)Qt.callLater(function(){if(!queueHover.hovered)queue.close()})
    }
    component MiniButton:IconButton {
        id:button;width:36;height:36;padding:6
        HintBubble {popupType:Popup.Window;visible:button.hovered;text:button.text}
    }
    component WindowDragArea:MouseArea {
        property point cursorOrigin
        property point windowOrigin
        preventStealing:true;cursorShape:Qt.SizeAllCursor
        onPressed:mouse=>{cursorOrigin=uiIntegration.pointerPosition();windowOrigin=Qt.point(mini.Window.window.x,mini.Window.window.y)}
        onPositionChanged:mouse=>{
            if(!pressed)return
            const point=uiIntegration.pointerPosition(),window=mini.Window.window
            uiIntegration.setWindowGeometry(Math.round(windowOrigin.x+point.x-cursorOrigin.x),Math.round(windowOrigin.y+point.y-cursorOrigin.y),window.width,window.height)
        }
        onDoubleClicked:mini.restoreRequested()
    }
    function closeTransientPopups(){queue.close();volumeControl.closePopup()}
    function transitionParts(){
        const entries=[{key:"cover",item:cover,kind:"art",alpha:1},
            {key:"title",item:titleLabel,kind:"text",alpha:showingLyrics?0:1-controlsProgress},{key:"artist",item:artistLabel,kind:"text",alpha:showingLyrics?0:1-controlsProgress},
            {key:"queue",item:queueButton},{key:"favorite",item:favorite},{key:"previous",item:previous},{key:"play",item:playPause},
            {key:"next",item:nextTrack},{key:"mode",item:playMode},{key:"volume",item:volumeControl}]
        const parts=[]
        for(const entry of entries){
            const item=entry.item,alpha=entry.alpha===undefined?controlsProgress*item.opacity:entry.alpha
            if(alpha<=.01)continue
            const point=item.mapToGlobal(0,0),text=entry.kind==="text"
            const w=text?Math.min(item.width,item.contentWidth+2):item.width
            parts.push({key:entry.key,kind:entry.kind||"icon",alpha:alpha,
                from:{x:point.x+(text?(item.width-w)/2:0),y:point.y,width:w,height:item.height},
                text:text?item.text:"",family:text?item.font.family:"",pixelSize:text?item.font.pixelSize:0,
                weight:text?item.font.weight:0,color:text?item.color:item.iconColor||Theme.text,
                symbol:item.symbol||"",filled:!!item.filled,padding:item.padding||0})
        }
        return parts
    }
    function openQueue(){
        // Keep the bar and its upward list together even near the screen top.
        const window=mini.Window.window
        uiIntegration.setWindowGeometry(Math.max(Screen.virtualX,Math.min(window.x,Screen.virtualX+Screen.width-window.width)),Math.max(Screen.virtualY+queue.fullHeight,Math.min(window.y,Screen.virtualY+Screen.height-window.height)),window.width,window.height)
        queue.open()
    }
    Image {
        anchors.fill:parent
        source:mini.presentationActive&&String(settingsBridge.palette.footerImage||"").length?"image://relaxer/background?file="+encodeURIComponent(String(settingsBridge.palette.footerImage))+"&blur="+Number(settingsBridge.palette.blurFooter||0):""
        sourceSize.width:360;sourceSize.height:360
        fillMode:Image.PreserveAspectCrop;asynchronous:true
    }
    WindowDragArea {
        objectName:"miniDragArea";anchors.fill:parent
    }
    Artwork {
        id:cover;objectName:"miniCover";x:8;y:(mini.height-height)/2;width:42;height:42;radius:8;resolution:96;animated:false
        file:playbackBridge.coverUrl;revision:playbackBridge.artworkRevision;seed:playbackBridge.hasTrack?playbackBridge.title:"Relaxer"
        WindowDragArea {objectName:"miniCoverDrag";anchors.fill:parent}
    }
    Item {
        id:restingInfo;objectName:"miniRestingInfo";x:cover.x+cover.width;y:cover.y;width:mini.width-x-cover.x;height:42
        opacity:1-mini.controlsProgress;visible:opacity>0;enabled:!mini.controlsVisible;clip:true
        Column {
            objectName:"miniMetadata";anchors.centerIn:parent;width:parent.width-16;spacing:3;visible:!mini.showingLyrics
            UiText {id:titleLabel;objectName:"miniTitle";width:parent.width;text:playbackBridge.hasTrack?playbackBridge.title:"Relaxer";textFormat:Text.PlainText;elide:Text.ElideRight;horizontalAlignment:Text.AlignHCenter;fontRegion:"footer";font.pixelSize:14;font.weight:Font.DemiBold;color:Theme.text}
            UiText {id:artistLabel;objectName:"miniArtist";width:parent.width;text:playbackBridge.hasTrack?playbackBridge.artist:"选一首歌，开始放松。";textFormat:Text.PlainText;elide:Text.ElideRight;horizontalAlignment:Text.AlignHCenter;fontRegion:"footer";font.pixelSize:12;color:Theme.secondary}
        }
        Column {
            objectName:"miniLyrics";anchors.centerIn:parent;width:parent.width-16;spacing:3;visible:mini.showingLyrics
            Item {
                id:originalViewport;objectName:"miniLyricViewport";width:parent.width;height:originalLine.implicitHeight;clip:true
                UiText {
                    id:originalLine;objectName:"miniLyric";text:mini.lyricLine.original;textFormat:Text.PlainText;wrapMode:Text.NoWrap
                    x:implicitWidth>parent.width?-(implicitWidth-parent.width)*mini.lyricScrollProgress:(parent.width-implicitWidth)/2
                    fontRegion:"footer";font.pixelSize:14;font.weight:Font.DemiBold;color:Theme.text
                }
            }
            Item {
                id:translationViewport;objectName:"miniTranslationViewport";width:parent.width;height:translationLine.implicitHeight;clip:true;visible:mini.lyricLine.translation.length>0
                UiText {
                    id:translationLine;objectName:"miniLyricTranslation";text:mini.lyricLine.translation;textFormat:Text.PlainText;wrapMode:Text.NoWrap
                    x:implicitWidth>parent.width?-(implicitWidth-parent.width)*mini.lyricScrollProgress:(parent.width-implicitWidth)/2
                    fontRegion:"footer";font.pixelSize:12;color:Theme.secondary
                }
            }
        }
    }
    Row {
        id:controls;objectName:"miniControls";spacing:2;height:42;y:cover.y
        x:cover.x+cover.width+6+(mini.width-(cover.x+cover.width+6)-cover.x-width)/2
        opacity:mini.controlsProgress;visible:opacity>0;enabled:mini.controlsVisible
        MiniButton {
            id:queueButton;objectName:"miniQueueButton";anchors.verticalCenter:parent.verticalCenter;text:"下一首列表"
            property bool openedAtPress:false
            contentItem:Item {Repeater{model:3;Rectangle{required property int index;x:0;y:5+index*6;width:8+index*6;height:2;radius:1;color:Theme.text}}}
            onPressed:openedAtPress=queue.visible
            onClicked:if(openedAtPress||queue.visible)queue.close();else mini.openQueue()
        }
        MiniButton {id:favorite;objectName:"miniFavorite";anchors.verticalCenter:parent.verticalCenter;symbol:playbackBridge.currentFavorite?"liked":"heart";iconColor:playbackBridge.currentFavorite?Theme.accent:Theme.text;text:playbackBridge.currentFavorite?"取消喜欢":"喜欢歌曲";enabled:playbackBridge.available&&playbackBridge.hasTrack;onClicked:playbackBridge.toggleFavorite()}
        MiniButton {id:previous;objectName:"miniPrevious";anchors.verticalCenter:parent.verticalCenter;symbol:"previous";text:"上一首";enabled:playbackBridge.available&&playbackBridge.hasTrack;onClicked:playbackBridge.previous()}
        IconButton {
            id:playPause;objectName:"miniPlayPause";width:42;height:42;circular:true;filled:true
            symbol:playbackBridge.playing?"pause":"play";text:playbackBridge.playing?"暂停":"播放"
            enabled:playbackBridge.available&&playbackBridge.hasTrack&&playbackBridge.canPlay;onClicked:playbackBridge.toggle()
        }
        MiniButton {id:nextTrack;objectName:"miniPlayingNext";anchors.verticalCenter:parent.verticalCenter;symbol:"next";text:"下一首";enabled:playbackBridge.available&&playbackBridge.hasTrack;onClicked:playbackBridge.next()}
        MiniButton {id:playMode;objectName:"miniPlayMode";anchors.verticalCenter:parent.verticalCenter;symbol:["playlist","repeat","single","shuffle"][playbackBridge.mode];text:["顺序播放","列表循环","单曲循环","随机循环"][playbackBridge.mode];enabled:playbackBridge.available;onClicked:playbackBridge.setMode((playbackBridge.mode+1)%4)}
        VolumeButton {id:volumeControl;objectName:"miniVolume";anchors.verticalCenter:parent.verticalCenter;width:36;height:36;padding:6;popupOnHover:true}
    }
    Popup {
        id:queue;objectName:"miniQueuePopup";parent:mini;popupType:Popup.Window
        readonly property int fullHeight:Math.max(1,Math.min(5,upcomingModel.count))*56+8
        property real reveal:0
        width:mini.width;height:fullHeight;padding:0;margins:0
        x:0;y:-height
        closePolicy:Popup.CloseOnEscape
        enter:Transition{NumberAnimation{target:queue;property:"reveal";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:220;easing.type:Easing.OutCubic}}
        onClosed:reveal=0
        background:Item {}
        contentItem:Item {
            Item {
                objectName:"miniQueueReveal";anchors.bottom:parent.bottom;width:parent.width;height:Math.round(queue.fullHeight*queue.reveal);clip:true
                HoverHandler {id:queueHover;objectName:"miniQueueHover";onHoveredChanged:if(!hovered&&queue.visible)queue.close()}
                Image {
                    id:queueBackdrop;objectName:"miniQueueBackdrop";anchors.fill:parent;visible:false
                    readonly property string file:String(settingsBridge.palette.footerImage).length?String(settingsBridge.palette.footerImage):String(playbackBridge.coverUrl)
                    // Empty QUrls are truthy in QML. Test the URL text so that
                    // an unset footer falls back to the cover, not placeholder art.
                    source:queue.visible&&file.length?"image://relaxer/blur?file="+encodeURIComponent(file)+"&rev="+playbackBridge.artworkRevision+"&resolution=360&blur=32":""
                    fillMode:Image.PreserveAspectCrop;asynchronous:true;sourceSize.width:360;sourceSize.height:360
                }
                GlassSurface{objectName:"miniQueueGlass";anchors.fill:parent;backdrop:queueBackdrop;sampleRect:Qt.rect(0,0,queueBackdrop.width,queueBackdrop.height);tintOpacity:Theme.dark?.78:.76;active:queue.visible;radius:12;liquid:true}
                UpcomingPanel {
                    objectName:"miniUpcomingPanel";width:parent.width;height:queue.fullHeight;y:parent.height-height
                    popupPresentation:true;expandedPlayer:true;showHeader:false;relocating:true;enabled:queue.opened
                    onActivated:queue.close()
                }
            }
        }
    }
}
