import QtQuick
import QtQuick.Controls
import "../theme"
Item {
    id: panel
    objectName: expandedPlayer ? "nextQueueLarge" : "nextQueueCompact"
    property bool expandedPlayer: false
    property bool popupPresentation:false
    property bool showHeader:true
    property Item backdropSource
    property Item secondaryBackdrop
    property point secondaryOffset:Qt.point(0,0)
    property rect backdropRect:Qt.rect(x,y,width,height)
    property bool panelExpanded:popupPresentation
    property real dockedHeight:94
    readonly property bool dockedPreview:expandedPlayer&&!panelExpanded&&!popupPresentation
    property bool initialized: false
    property bool relocating: false
    readonly property int headerHeight:showHeader?(dockedPreview?28:34):0
    readonly property real rowHeight:popupPresentation?56:dockedPreview?Math.max(0,dockedHeight-headerHeight-4):panelExpanded?46:56
    readonly property int coverSize:popupPresentation?40:dockedPreview?Math.min(32,Math.max(0,Math.floor(rowHeight-6))):panelExpanded?36:40
    readonly property bool roomyHeader:expandedPlayer||panelExpanded
    readonly property bool inlinePreview:!expandedPlayer&&!panelExpanded
    readonly property real inlineContentX:upcomingModel.count?70:38
    readonly property real inlineTextSpace:Math.max(0,width-inlineContentX-34-6)
    readonly property real inlineArtistWidth:upcomingModel.firstArtist.length&&inlineTextSpace>=96?Math.min(fullArtistMetrics.advanceWidth,inlineTextSpace*.42):0
    readonly property real preferredWidth:Math.max(112,(expandedPlayer&&!panelExpanded?80:76)+titleMetrics.advanceWidth+dashMetrics.advanceWidth+artistMetrics.advanceWidth+12)
    signal activated(int row)
    signal dismissRequested()
    implicitWidth: preferredWidth
    implicitHeight: !panelExpanded ? (expandedPlayer ? dockedHeight : 34) : headerHeight + (upcomingModel.count ? Math.min(5, upcomingModel.count) * rowHeight : 28) + (showHeader?4:8)
    width: popupPresentation?parent.width:implicitWidth
    height: popupPresentation?parent.height:implicitHeight
    clip: true
    Behavior on height { enabled: !panel.relocating; NumberAnimation { duration: settingsBridge.values.reduceMotion ? 0 : 300; easing.type: Easing.OutCubic } }
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
    onPanelExpandedChanged: resetScroll()
    onExpandedPlayerChanged: { panelExpanded = popupPresentation; resetScroll() }
    Component.onCompleted: initialized = true
    Connections { target: upcomingModel; function onCountChanged() { if (rows.contentY > Math.max(0, rows.contentHeight - rows.height)) panel.resetScroll() } }
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
            width: parent.width - arrow.width; height: parent.height - 2; radius: 7
            color: headerMouse.containsMouse ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08) : "transparent"
        }
        UiText { fontRegion: "next";
            objectName:"nextQueueHeading";visible:!panel.inlinePreview;x:12
            height: parent.height; text: "下一首"; font.pixelSize: 15; color: Theme.text; verticalAlignment: Text.AlignVCenter
        }
        Symbol {
            objectName:"nextCompactIcon";visible:panel.inlinePreview
            x:12;anchors.verticalCenter:parent.verticalCenter;width:18;height:18
            name:"next";color:Theme.text
        }
        Artwork {
            id: compactArt; visible: !panel.expandedPlayer && !panel.panelExpanded && upcomingModel.count > 0
            x:38; y:5; width:24; height:24; radius:5; resolution:80; animated:false
            file: upcomingModel.firstCoverUrl; seed: upcomingModel.firstTitle; revision: upcomingModel.firstCoverRevision
        }
        UiText { fontRegion: "next";
            id: compactTitle; objectName: "nextCompactTitle"
            visible: !panel.expandedPlayer && !panel.panelExpanded
            x:panel.inlineContentX
            width:Math.max(0,panel.inlineTextSpace-(panel.inlineArtistWidth>0?panel.inlineArtistWidth+dashMetrics.advanceWidth:0));height:parent.height
            text: upcomingModel.count ? upcomingModel.firstTitle : "暂无待播放歌曲"
            textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 15; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter;horizontalAlignment:Text.AlignRight
        }
        TextMetrics { id: artistMinimum; font.family: panel.regionFontFamily; font.pixelSize: 15; text: " — …" }
        UiText { fontRegion: "next";
            objectName: "nextCompactArtist"
            x:compactTitle.x+compactTitle.width;width:panel.inlineArtistWidth+dashMetrics.advanceWidth;height:parent.height
            visible: compactTitle.visible && upcomingModel.count > 0 && upcomingModel.firstArtist.length > 0 && width >= artistMinimum.advanceWidth
            text:" — "+upcomingModel.firstArtist; textFormat: Text.PlainText; color: Theme.secondary; font.pixelSize: 15; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter;horizontalAlignment:Text.AlignRight
        }
        MouseArea {
            id: headerMouse
            x:0
            width: parent.width - arrow.width; height: parent.height; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
            onClicked: if (upcomingModel.count) panel.activate(0)
        }
        Item {
            id: arrow
            x:parent.width-width
            width:panel.dockedPreview?28:34
            height: parent.height
            Rectangle { anchors.fill: parent; anchors.margins: 1; radius: 7; color: arrowMouse.containsMouse ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08) : "transparent" }
            UiText { fontRegion: "next"; anchors.centerIn: parent; text: panel.panelExpanded ? "⌄" : "⌃"; color: Theme.text; font.pixelSize: 15 }
            MouseArea {id:arrowMouse;objectName:"upcomingToggle"; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: {if(panel.popupPresentation)panel.dismissRequested();else panel.panelExpanded=!panel.panelExpanded} }
        }
    }
    ListView {
        id: rows; objectName: "upcomingRows"
        x: 0; y: panel.headerHeight+(panel.showHeader?0:4); width: panel.width; height: Math.max(0, panel.height - y - 4)
        visible: panel.expandedPlayer || panel.panelExpanded
        clip: true; boundsBehavior: Flickable.StopAtBounds; interactive: panel.panelExpanded
        cacheBuffer: 0; reuseItems: true; model: upcomingModel
        delegate: Item {
            id: row
            required property int index
            required property string trackKey
            required property string title
            required property string artist
            required property url coverUrl
            required property string coverRevision
            property alias artwork: rowArt
            objectName: "upcomingDelegate"
            width: rows.width; height: panel.rowHeight
            Rectangle {
                x: 6; y: 1; width: parent.width - 12; height: parent.height - 3; radius: 7
                color: rowMouse.containsMouse ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08) : "transparent"
                border.width: rowMouse.containsMouse ? 1 : 0; border.color: Theme.accent
            }
            UiText { fontRegion: "next"; x: 8; width: 20; height: parent.height; text: row.index + 1; color: Theme.text; font.pixelSize: 15; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            Artwork {
                id: rowArt; x: 32; anchors.verticalCenter: parent.verticalCenter; width: panel.coverSize; height: width; radius: 5
                file: row.coverUrl; seed: row.title; revision: row.coverRevision; resolution: 100; animated: false
            }
            UiText { fontRegion: "next";
                id: rowTitle;objectName:"upcomingTitle-"+row.index; x: rowArt.x + rowArt.width + 8; width: panel.popupPresentation?Math.max(40,(rows.width-x-30)*.58):Math.min(titleMetrics.advanceWidth,Math.max(40,(rows.width-x-30)*.58)); height: parent.height
                text: row.title; textFormat: Text.PlainText; font.pixelSize: 15; color: Theme.text; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
            }
            UiText { fontRegion: "next"; id: dash; x: rowTitle.x + rowTitle.width; width: dashMetrics.advanceWidth; height: parent.height; text:" — "; font.pixelSize: 15; color: Theme.text; verticalAlignment: Text.AlignVCenter }
            UiText { fontRegion: "next";objectName:"upcomingArtist-"+row.index;
                x: dash.x + dash.width; width: Math.min(artistMetrics.advanceWidth, Math.max(0, parent.width - x - 12)); height: parent.height
                text: row.artist; textFormat: Text.PlainText; font.pixelSize: 15; color: Theme.text; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
            }
            MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: panel.activate(row.index) }
        }
        UiText { fontRegion: "next"; visible: upcomingModel.count === 0; x: 12; y: 0; width: rows.width - 24; height: rows.height; text: "暂无待播放歌曲"; color: Theme.text; font.pixelSize: 15; verticalAlignment: Text.AlignVCenter }
        ScrollBar.vertical: ScrollBar { width: 5; policy: panel.panelExpanded && upcomingModel.count > 5 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }
        SmoothWheelHandler { id: queueWheel; flickable: rows }
    }
}
