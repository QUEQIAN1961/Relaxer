import QtQuick
import QtQuick.Controls
import "../theme"
Item {
    id: panel
    objectName: expandedPlayer ? "nextQueueLarge" : "nextQueueCompact"
    property bool expandedPlayer: false
    property bool popupPresentation:false
    property bool showHeader:true
    property bool popupActive:true
    property bool detachedMenus:false
    property Item backdropSource
    property Item secondaryBackdrop
    property point secondaryOffset:Qt.point(0,0)
    property rect backdropRect:Qt.rect(x,y,width,height)
    property bool panelExpanded:popupPresentation
    property real dockedHeight:94
    readonly property bool dockedPreview:expandedPlayer&&!panelExpanded&&!popupPresentation
    property bool selectionMode:false
    property var selectedKeys:[]
    property string contextKey:"0"
    property int contextEpoch:0
    property int selectionAnchor:-1
    readonly property bool menuOpen:rowMenu.visible||editMenu.visible
    readonly property var selectableTrackKeys:{const revision=contextEpoch;return upcomingModel.selectableKeys()}
    readonly property bool allSelected:selectableTrackKeys.length>0&&selectedKeys.length===selectableTrackKeys.length
    // One progress value keeps toolbar, row insets and popup geometry in step.
    property real selectionProgress:selectionMode?1:0
    readonly property real editingHeight:40*selectionProgress
    property int displayedSelectionCount:0
    property bool displayedAllSelected:false
    onSelectedKeysChanged:if(selectionMode){displayedSelectionCount=selectedKeys.length;displayedAllSelected=allSelected}
    onAllSelectedChanged:if(selectionMode)displayedAllSelected=allSelected
    onSelectionModeChanged:if(selectionMode){displayedSelectionCount=selectedKeys.length;displayedAllSelected=allSelected}
    Behavior on selectionProgress {
        enabled:panel.initialized&&panel.visible&&panel.popupActive
        NumberAnimation{id:selectionMotion;duration:settingsBridge.values.reduceMotion?0:220;easing.type:Easing.OutCubic}
    }
    function beginSelection(key){selectionAnchor=-1;selectedKeys=key&&key!==playbackBridge.trackId?[key]:[];selectionMode=true;forceActiveFocus()}
    function finishSelection(){resetEditing();forceActiveFocus()}
    function selectAll(){selectedKeys=allSelected?[]:upcomingModel.selectableKeys();forceActiveFocus()}
    function dismissPanel(){resetEditing();if(popupPresentation)dismissRequested();else panelExpanded=false}
    function handleEscape(){if(menuOpen){rowMenu.dismiss();editMenu.dismiss()}else if(selectionMode)finishSelection();else dismissPanel()}
    function resetEditing(){selectionMode=false;selectedKeys=[];selectionAnchor=-1;rowMenu.dismiss();editMenu.dismiss()}
    function toggleSelection(key,index,modifiers){
        if(key===playbackBridge.trackId)return
        let next=selectedKeys.slice(),allowed=upcomingModel.selectableKeys()
        if((modifiers&Qt.ShiftModifier)&&selectionAnchor>=0){for(let i=Math.min(index,selectionAnchor);i<=Math.max(index,selectionAnchor);++i){const id=upcomingModel.idAt(i);if(allowed.indexOf(id)>=0&&next.indexOf(id)<0)next.push(id)}}
        else {const at=next.indexOf(key);if(at<0)next.push(key);else next.splice(at,1);selectionAnchor=index}
        selectedKeys=next
    }
    function deleteSelection(){if(selectedKeys.length&&upcomingModel.removeTracks(selectedKeys)){selectedKeys=[];if(!upcomingModel.count)resetEditing()}}
    onVisibleChanged:if(!visible)resetEditing()
    onPopupActiveChanged:if(initialized&&!popupActive)resetEditing()
    Keys.onPressed:event=>{if(selectionMode&&event.key===Qt.Key_Delete){deleteSelection();event.accepted=true}else if(selectionMode&&event.key===Qt.Key_A&&(event.modifiers&Qt.ControlModifier)){selectedKeys=upcomingModel.selectableKeys();event.accepted=true}else if(event.key===Qt.Key_Escape&&(panelExpanded||selectionMode)){panel.handleEscape();event.accepted=true}}
    property bool initialized: false
    property bool relocating: false
    readonly property int headerHeight:showHeader?(dockedPreview?28:34):0
    readonly property real rowHeight:popupPresentation?56:dockedPreview?Math.max(0,dockedHeight-headerHeight-4):panelExpanded?46:56
    readonly property int coverSize:popupPresentation?40:dockedPreview?Math.min(32,Math.max(0,Math.floor(rowHeight-6))):panelExpanded?36:40
    readonly property bool roomyHeader:expandedPlayer||panelExpanded
    readonly property bool inlinePreview:!expandedPlayer&&!panelExpanded
    readonly property real inlineIconLeft:12
    readonly property real inlineContentX:upcomingModel.count?(upcomingModel.firstVideo?78:70):38
    readonly property real inlineTextSpace:Math.max(0,width-inlineContentX-34-6)
    readonly property real inlineArtistWidth:0
    readonly property real fullWidth:Math.max(112,(expandedPlayer&&!panelExpanded?80:76)+titleMetrics.advanceWidth+dashMetrics.advanceWidth+artistMetrics.advanceWidth+12)
    readonly property real preferredWidth:inlinePreview?fullWidth*.7:fullWidth
    signal activated(int row)
    signal dismissRequested()
    implicitWidth: preferredWidth
    implicitHeight: !panelExpanded ? (expandedPlayer ? dockedHeight : 34) : headerHeight + editingHeight + (upcomingModel.count ? Math.min(5, upcomingModel.count) * rowHeight : 28) + (showHeader?4:8)
    width: popupPresentation?parent.width:implicitWidth
    height: popupPresentation?parent.height:implicitHeight
    clip: true
    Behavior on width { enabled: !panel.relocating; NumberAnimation { id:widthMotion;duration: settingsBridge.values.reduceMotion ? 0 : 300; easing.type: Easing.OutCubic } }
    onRelocatingChanged:if(relocating){widthMotion.complete();heightMotion.complete()}
    Behavior on height { enabled: !panel.relocating&&!selectionMotion.running; NumberAnimation { id:heightMotion;duration: settingsBridge.values.reduceMotion ? 0 : 300; easing.type: Easing.OutCubic } }
    readonly property string regionFontFamily: { const saved=settingsBridge.values.uiFont; return settingsBridge.fontFamilyFor("next") || "Microsoft YaHei UI" }
    TextMetrics { font.family: panel.regionFontFamily; id: titleMetrics; text: "歌歌歌歌歌歌歌歌"; font.pixelSize: 15 }
    TextMetrics { font.family: panel.regionFontFamily; id: artistMetrics; text: "歌歌歌歌歌"; font.pixelSize: 15 }
    TextMetrics { font.family: panel.regionFontFamily; id: dashMetrics; text: " — "; font.pixelSize: 15 }
    TextMetrics {font.family:panel.regionFontFamily;id:fullArtistMetrics;text:upcomingModel.firstArtist;font.pixelSize:15}
    function coverItem(index) {
        if (!expandedPlayer && !panelExpanded && index === 0) return compactArt
        const row = rows.itemAtIndex(index)
        return row ? row.artwork : null
    }
    function activate(index) {
        if (!upcomingModel.activate(index)) return
        if(expandedPlayer&&!popupPresentation)panelExpanded=false
        activated(index)
    }
    function resetScroll() { if (!initialized) return; queueWheel.stop(); rows.contentY = 0 }
    onPopupPresentationChanged:if(popupPresentation)panelExpanded=true
    onPanelExpandedChanged:{resetScroll();if(initialized&&!panelExpanded)resetEditing()}
    onExpandedPlayerChanged: { panelExpanded = popupPresentation; resetScroll() }
    Component.onCompleted: initialized = true
    Connections { target: upcomingModel
        function onCountChanged(){if(rows.contentY>Math.max(0,rows.contentHeight-rows.height))panel.resetScroll();if(!upcomingModel.count)panel.resetEditing()}
        function onModelReset(){const allowed=upcomingModel.selectableKeys();panel.selectedKeys=panel.selectedKeys.filter(key=>allowed.indexOf(key)>=0);panel.contextEpoch++;rowMenu.dismiss()}
    }
    RoundMenu {
        id:rowMenu;objectName:"upcomingContextMenu";popupType:panel.detachedMenus?Popup.Window:Popup.Item
        // Freeze both smooth-wheel motion and touch inertia before opening.
        onAboutToShow:{queueWheel.stop();rows.cancelFlick()}
        RoundMenuItem{objectName:"upcomingMoveUp";text:"上移";enabled:panel.contextEpoch>=0&&upcomingModel.canMove(panel.contextKey,-1);onTriggered:upcomingModel.moveTrack(panel.contextKey,-1)}
        RoundMenuItem{objectName:"upcomingMoveDown";text:"下移";enabled:panel.contextEpoch>=0&&upcomingModel.canMove(panel.contextKey,1);onTriggered:upcomingModel.moveTrack(panel.contextKey,1)}
        RoundMenuItem{objectName:"upcomingEqualizer";visible:!panel.detachedMenus;text:"歌曲均衡器…";onTriggered:equalizerBridge.requestSong(panel.contextKey)}
        RoundMenuItem{objectName:"upcomingLocate";text:"定位到来源歌单";onTriggered:if(upcomingModel.locateTrack(panel.contextKey))panel.dismissPanel()}
        RoundMenuItem{objectName:"upcomingSelectMany";text:panel.selectionMode?(panel.selectedKeys.indexOf(panel.contextKey)>=0?"取消选择":"选择此歌曲"):"多选歌曲";enabled:panel.contextKey!==playbackBridge.trackId;onTriggered:{if(!panel.selectionMode)panel.beginSelection(panel.contextKey);else panel.toggleSelection(panel.contextKey,-1,Qt.NoModifier)}}
        RoundMenuItem{objectName:"upcomingRemove";text:"移出下一首列表";destructive:true;enabled:panel.contextKey!==playbackBridge.trackId;onTriggered:upcomingModel.removeTracks([panel.contextKey])}
    }
    RoundMenu {
        id:editMenu;objectName:"upcomingEditMenu";popupType:Popup.Item;margins:panel.detachedMenus?4:8
        RoundMenuItem{objectName:"upcomingBeginSelection";text:"多选歌曲";onTriggered:panel.beginSelection("")}
        RoundMenuItem{objectName:"upcomingClear";text:"清空下一首列表";destructive:true;enabled:upcomingModel.count>0;onTriggered:upcomingModel.clearPending()}
    }
    component QueueActionGlass:GlassSurface {
        id:actionGlass
        property bool highlighted:false
        property bool pressed:false
        property bool focused:false
        // Sample only the underlying scene; capturing this panel would feed
        // its own buttons back into the blur texture.
        readonly property point panelOrigin:{
            const progress=panel.selectionProgress, bounds=panel.backdropRect
            const localX=x, localY=y, localWidth=width, localHeight=height
            return mapToItem(panel,0,0)
        }
        backdrop:panel.backdropSource
        secondaryBackdrop:panel.secondaryBackdrop;secondaryOffset:panel.secondaryOffset
        sampleRect:Qt.rect(panel.backdropRect.x+panelOrigin.x,panel.backdropRect.y+panelOrigin.y,width,height)
        active:panel.visible&&panel.popupActive&&panel.panelExpanded&&visible
        // Match the ordinary frosted surfaces; no liquid sheen or extra tint.
        radius:8;liquid:false
        effectName:"upcomingActionBackdropBlur"
        Rectangle {
            anchors.fill:parent;radius:actionGlass.radius
            color:actionGlass.pressed?Theme.pressed:actionGlass.highlighted?Theme.hover:"transparent"
            border.width:actionGlass.focused?1:0;border.color:Theme.accent
            Behavior on color{ColorAnimation{duration:settingsBridge.values.reduceMotion?0:120;easing.type:Easing.OutCubic}}
        }
    }
    component QueueAction:Button {
        id:action
        property bool destructive:false
        height:30;padding:6;hoverEnabled:true
        contentItem:UiText{fontRegion:"next";text:action.text;basePixelSize:12;color:!action.enabled?Theme.disabled:action.destructive?Theme.accent:Theme.text;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
        background:QueueActionGlass{highlighted:action.hovered&&action.enabled;pressed:action.down;focused:action.activeFocus}
    }
    MouseArea {
        objectName:"queueInputBarrier";anchors.fill:parent;enabled:panel.panelExpanded||panel.expandedPlayer
        acceptedButtons:Qt.AllButtons
        onWheel:wheel=>{wheel.accepted=true}
    }
    Rectangle {
        visible:!panel.popupPresentation&&!panel.backdropSource
        anchors.fill: parent; radius: 12
        color: panel.panelExpanded ? Qt.rgba(Theme.panel.r, Theme.panel.g, Theme.panel.b, 0.97) : "transparent"
        border.width: panel.panelExpanded ? 1 : 0
        border.color: Theme.border
    }
    GlassSurface {
        objectName:"wideQueueGlassSurface";anchors.fill:parent;radius:12
        visible:panel.panelExpanded&&!panel.popupPresentation&&!!panel.backdropSource
        active:visible;backdrop:panel.backdropSource;sampleRect:panel.backdropRect;effectName:"wideQueueBackdropBlur"
        secondaryBackdrop:panel.secondaryBackdrop;secondaryOffset:panel.secondaryOffset
    }
    Item {
        visible:panel.showHeader
        width: parent.width; height: panel.headerHeight
        Rectangle {
            x:0; y: 1
            width: parent.width - arrow.width - (editButton.visible?editButton.width:0); height: parent.height - 2; radius: 7
            color: headerMouse.containsMouse ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08) : "transparent"
        }
        UiText { fontRegion: "next";
            objectName:"nextQueueHeading";visible:!panel.inlinePreview;x:12
            width:Math.max(0,(editButton.visible?editButton.x:arrow.x)-x-8);elide:Text.ElideRight
            height: parent.height; text: panel.selectionMode?"选择歌曲":"下一首"; basePixelSize: 15; color: Theme.text; verticalAlignment: Text.AlignVCenter
        }
        Symbol {
            objectName:"nextCompactIcon";visible:panel.inlinePreview
            x:panel.inlineIconLeft;anchors.verticalCenter:parent.verticalCenter;width:18;height:18
            name:"next";color:Theme.text
        }
        Artwork {
            id: compactArt; objectName:"nextCompactCover"; visible: !panel.expandedPlayer && !panel.panelExpanded && upcomingModel.count > 0
            x:38; y:5; width:upcomingModel.firstVideo?32:24; height:24;videoCover:upcomingModel.firstVideo;  resolution:80; animated:false
            file: upcomingModel.firstCoverUrl; contentKey:upcomingModel.firstTrackKey; seed: upcomingModel.firstTitle; revision: upcomingModel.firstCoverRevision
        }
        UiText { fontRegion: "next";
            id: compactTitle; objectName: "nextCompactTitle"
            visible: !panel.expandedPlayer && !panel.panelExpanded
            x:panel.inlineContentX
            width:Math.max(0,panel.inlineTextSpace-(panel.inlineArtistWidth>0?panel.inlineArtistWidth+dashMetrics.advanceWidth:0));height:parent.height
            text: upcomingModel.count ? upcomingModel.firstTitle : "暂无待播放歌曲"
            textFormat: Text.PlainText; color: Theme.text; basePixelSize: 15; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter;horizontalAlignment:Text.AlignRight
        }
        TextMetrics { id: artistMinimum; font.family: panel.regionFontFamily; font.pixelSize: 15; text: " — …" }
        UiText { fontRegion: "next";
            objectName: "nextCompactArtist"
            x:compactTitle.x+compactTitle.width;width:panel.inlineArtistWidth+dashMetrics.advanceWidth;height:parent.height
            visible:false
            text:" — "+upcomingModel.firstArtist; textFormat: Text.PlainText; color: Theme.secondary; basePixelSize: 15; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter;horizontalAlignment:Text.AlignRight
        }
        MouseArea {
            id: headerMouse
            enabled:!panel.panelExpanded&&upcomingModel.count>0
            x:0
            width: parent.width - arrow.width - (editButton.visible?editButton.width:0); height: parent.height; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
            onClicked: if (upcomingModel.count) panel.activate(0)
        }
        IconButton {
            id:editButton;objectName:"upcomingEditButton";visible:panel.panelExpanded&&upcomingModel.count>0
            x:arrow.x-width;width:52;height:parent.height;padding:5;text:panel.selectionMode?"完成编辑":"编辑下一首列表";showTooltip:true
            contentItem:UiText{text:panel.selectionMode?"完成":"编辑";basePixelSize:12;color:panel.selectionMode?Theme.accent:Theme.text;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
            background:QueueActionGlass{anchors.fill:parent;anchors.margins:2;highlighted:editButton.hovered;pressed:editButton.down;focused:editButton.activeFocus}
            onClicked:if(panel.selectionMode)panel.finishSelection();else editMenu.popup(editButton,editButton.width-editMenu.width,editButton.height)
        }
        IconButton {
            id:arrow;objectName:"upcomingToggle";x:parent.width-width
            width:panel.dockedPreview?28:34;height:parent.height;padding:5
            text:panel.panelExpanded?"收起下一首列表":"展开下一首列表";showTooltip:true
            contentItem:UiText{fontRegion:"next";text:panel.panelExpanded?"⌄":"⌃";basePixelSize:15;color:Theme.text;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
            onClicked:if(panel.panelExpanded)panel.dismissPanel();else panel.panelExpanded=true
        }
    }
    Item {
        id:editingTools;objectName:"upcomingSelectionTools"
        visible:panel.selectionProgress>0;enabled:panel.selectionMode
        y:panel.headerHeight-6*(1-panel.selectionProgress);width:panel.width;height:40
        opacity:panel.selectionProgress
        transform:Scale{origin.x:editingTools.width/2;origin.y:0;xScale:.98+.02*panel.selectionProgress;yScale:.96+.04*panel.selectionProgress}
        QueueAction {
            id:selectAllButton;objectName:"upcomingSelectAll";x:8;anchors.verticalCenter:parent.verticalCenter;width:Math.min(94,parent.width*.3)
            text:panel.displayedAllSelected?"取消全选":"全选";enabled:panel.selectableTrackKeys.length>0;onClicked:panel.selectAll()
        }
        Item {
            x:selectAllButton.x+selectAllButton.width+6
            width:Math.max(0,removeSelected.x-x-6);height:30;anchors.verticalCenter:parent.verticalCenter
            QueueActionGlass{anchors.fill:parent}
            UiText {
                objectName:"upcomingSelectionCount";fontRegion:"next";anchors.fill:parent;anchors.margins:4
                text:"已选 "+panel.displayedSelectionCount;basePixelSize:12;color:Theme.secondary
                elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignHCenter
            }
        }
        QueueAction {
            id:removeSelected;objectName:"upcomingDeleteSelected";width:Math.min(112,parent.width*.36);x:parent.width-width-8;anchors.verticalCenter:parent.verticalCenter
            text:panel.displayedSelectionCount?"移出 ("+panel.displayedSelectionCount+")":"移出选中";destructive:true;enabled:panel.selectedKeys.length>0
            onClicked:{panel.deleteSelection();panel.forceActiveFocus()}
        }
    }
    ListView {
        id: rows; objectName: "upcomingRows"
        x: 0; y: panel.headerHeight+panel.editingHeight+(panel.showHeader?0:4); width: panel.width; height: Math.max(0, panel.height - y - 4)
        visible: panel.expandedPlayer || panel.panelExpanded
        clip: true; boundsBehavior: Flickable.StopAtBounds; interactive: panel.panelExpanded&&!rowMenu.visible
        cacheBuffer: 0; reuseItems: true; model: upcomingModel
        delegate: Item {
            id: row
            required property int index
            required property string trackKey
            required property string title
            required property string artist
            required property url coverUrl
            required property bool videoTrack
            required property string coverRevision
            property alias artwork: rowArt
            readonly property bool contextHighlighted:rowMenu.visible&&panel.contextKey===trackKey
            objectName: "upcomingDelegate"
            width: rows.width; height: panel.rowHeight
            Rectangle {
                x: 6; y: 1; width: parent.width - 12; height: parent.height - 3; radius: 7
                color: panel.selectedKeys.indexOf(row.trackKey)>=0?Theme.selected:row.contextHighlighted||(!rowMenu.visible&&rowMouse.containsMouse) ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08) : "transparent"
                border.width: row.contextHighlighted||(!rowMenu.visible&&rowMouse.containsMouse) ? 1 : 0; border.color: Theme.accent
            }
            UiText { fontRegion: "next"; x: 8; width: 20; height: parent.height; opacity:1-panel.selectionProgress;visible:opacity>0; text: row.index + 1; color: Theme.text; basePixelSize: 15; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            Artwork {
                id: rowArt; x:32+8*panel.selectionProgress; anchors.verticalCenter: parent.verticalCenter; width: panel.coverSize*(row.videoTrack?4/3:1); height: panel.coverSize;videoCover:row.videoTrack;
                file: row.coverUrl; contentKey:row.trackKey; seed: row.title; revision: row.coverRevision; resolution: 100; animated: false
            }
            UiText { fontRegion: "next";
                id: rowTitle;objectName:"upcomingTitle-"+row.index; x: rowArt.x + rowArt.width + 8; width: panel.popupPresentation?Math.max(40,(rows.width-x-30)*.58):Math.min(titleMetrics.advanceWidth,Math.max(40,(rows.width-x-30)*.58)); height: parent.height
                text: row.title; textFormat: Text.PlainText; basePixelSize: 15; color: Theme.text; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
            }
            UiText { fontRegion: "next"; id: dash;visible:row.artist.length>0; x: rowTitle.x + rowTitle.width; width:visible?dashMetrics.advanceWidth:0; height: parent.height; text:" — "; basePixelSize: 15; color: Theme.text; verticalAlignment: Text.AlignVCenter }
            UiText { fontRegion: "next";objectName:"upcomingArtist-"+row.index;
                x: dash.x + dash.width; width: Math.min(artistMetrics.advanceWidth, Math.max(0, parent.width - x - 12)); height: parent.height
                text: row.artist; textFormat: Text.PlainText; basePixelSize: 15; color: Theme.text; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
            }
            MouseArea {
                id:rowMouse;anchors.fill:parent;hoverEnabled:true;acceptedButtons:Qt.LeftButton|Qt.RightButton;cursorShape:Qt.PointingHandCursor
                onClicked:mouse=>{
                    if(mouse.button===Qt.RightButton){panel.contextKey=row.trackKey;panel.contextEpoch++;rowMenu.popup(row,mouse.x,mouse.y)}
                    else if(panel.selectionMode||(mouse.modifiers&(Qt.ControlModifier|Qt.ShiftModifier))){panel.selectionMode=true;panel.toggleSelection(row.trackKey,row.index,mouse.modifiers)}
                    else panel.activate(row.index)
                }
            }
            CheckBox {
                id:selectionBox;objectName:"upcomingSelect-"+row.trackKey;opacity:panel.selectionProgress;visible:opacity>0;enabled:panel.selectionMode&&row.trackKey!==playbackBridge.trackId
                x:6;width:28;height:28;anchors.verticalCenter:parent.verticalCenter;padding:0;hoverEnabled:true
                Accessible.name:"选择 "+row.title
                checked:panel.selectedKeys.indexOf(row.trackKey)>=0;onClicked:panel.toggleSelection(row.trackKey,row.index,Qt.NoModifier)
                contentItem:Item{}
                background:Rectangle{radius:7;color:selectionBox.hovered?Theme.hover:"transparent";border.width:selectionBox.activeFocus?1:0;border.color:Theme.accent}
                indicator:Rectangle {
                    objectName:"upcomingSelectionIndicator";x:5;y:(selectionBox.height-height)/2;width:18;height:18;radius:5
                    color:selectionBox.checked?Theme.accent:Theme.background;opacity:selectionBox.enabled?1:.45
                    border.color:selectionBox.checked?Theme.accent:Theme.secondary
                    Symbol{anchors.fill:parent;anchors.margins:3;name:"check";color:Theme.accentText;visible:selectionBox.checked}
                }
            }
        }
        UiText { fontRegion: "next"; visible: upcomingModel.count === 0; x: 12; y: 0; width: rows.width - 24; height: rows.height; text: "暂无待播放歌曲"; color: Theme.text; basePixelSize: 15; verticalAlignment: Text.AlignVCenter }
        ScrollBar.vertical: ScrollBar { width: 5; enabled:!rowMenu.visible; policy: panel.panelExpanded && upcomingModel.count > 5 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }
        SmoothWheelHandler { id: queueWheel; flickable: rows }
    }
}
