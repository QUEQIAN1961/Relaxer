import QtQuick
import QtQuick.Controls
import "../theme"
Item {
    id: panel
    objectName: expandedPlayer ? "nextQueueLarge" : "nextQueueCompact"
    property bool expandedPlayer: false
    property bool panelExpanded: false
    property bool initialized: false
    property bool relocating: false
    readonly property int headerHeight: 34
    readonly property int rowHeight: panelExpanded ? (expandedPlayer ? 46 : 38) : 56
    readonly property int coverSize: panelExpanded ? (expandedPlayer ? 36 : 26) : 40
    readonly property real preferredWidth: Math.max(112, (expandedPlayer && !panelExpanded ? 80 : 76) + titleMetrics.advanceWidth + dashMetrics.advanceWidth + artistMetrics.advanceWidth + 12)
    signal activated(int row)
    implicitWidth: preferredWidth
    implicitHeight: !panelExpanded ? (expandedPlayer ? 94 : 34) : headerHeight + (upcomingModel.count ? Math.min(5, upcomingModel.count) * rowHeight : 28) + 4
    width: implicitWidth
    height: implicitHeight
    clip: true
    Behavior on height { enabled: !panel.relocating; NumberAnimation { duration: settingsBridge.values.reduceMotion ? 0 : 300; easing.type: Easing.OutCubic } }
    readonly property string regionFontFamily: { const saved=settingsBridge.values.uiFont; return settingsBridge.fontFamilyFor("next") || "Microsoft YaHei UI" }
    TextMetrics { font.family: panel.regionFontFamily; id: titleMetrics; text: "歌歌歌歌歌歌歌歌"; font.pixelSize: 15 }
    TextMetrics { font.family: panel.regionFontFamily; id: artistMetrics; text: "歌歌歌歌歌"; font.pixelSize: 15 }
    TextMetrics { font.family: panel.regionFontFamily; id: dashMetrics; text: "—"; font.pixelSize: 15 }
    function coverItem(index) {
        if (!expandedPlayer && !panelExpanded && index === 0) return compactArt
        const row = rows.itemAtIndex(index)
        return row ? row.artwork : null
    }
    function activate(index) {
        if (!upcomingModel.activate(index)) return
        if (expandedPlayer) panelExpanded = false
        activated(index)
    }
    function resetScroll() { if (!initialized) return; wheelMotion.stop(); rows.contentY = 0; rows.scrollTarget = 0 }
    onPanelExpandedChanged: resetScroll()
    onExpandedPlayerChanged: { panelExpanded = false; resetScroll() }
    Component.onCompleted: initialized = true
    Connections { target: upcomingModel; function onCountChanged() { if (rows.contentY > Math.max(0, rows.contentHeight - rows.height)) panel.resetScroll() } }
    Rectangle {
        anchors.fill: parent; radius: 12
        color: panel.panelExpanded ? Qt.rgba(Theme.panel.r, Theme.panel.g, Theme.panel.b, 0.97) : "transparent"
        border.width: panel.panelExpanded ? 1 : 0
        border.color: Theme.border
    }
    Item {
        width: parent.width; height: panel.headerHeight
        Rectangle {
            x: panel.expandedPlayer ? 0 : arrow.width; y: 1
            width: parent.width - arrow.width; height: parent.height - 2; radius: 7
            color: headerMouse.containsMouse ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08) : "transparent"
        }
        UiText { fontRegion: "next";
            x: panel.expandedPlayer ? 12 : panel.panelExpanded ? 38 : 30
            height: parent.height; text: "下一首"; font.pixelSize: 15; color: Theme.text; verticalAlignment: Text.AlignVCenter
        }
        Artwork {
            id: compactArt; visible: !panel.expandedPlayer && !panel.panelExpanded && upcomingModel.count > 0
            x: 80; y: 5; width: 24; height: 24; radius: 5; resolution: 80; animated: false
            file: upcomingModel.firstCoverUrl; seed: upcomingModel.firstTitle; revision: upcomingModel.firstCoverRevision
        }
        UiText { fontRegion: "next";
            visible: !panel.expandedPlayer && !panel.panelExpanded
            x: upcomingModel.count ? 112 : 80; width: Math.max(0, parent.width - x - 12); height: parent.height
            text: upcomingModel.count ? upcomingModel.firstTitle : "暂无待播放歌曲"
            textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 15; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
        }
        MouseArea {
            id: headerMouse
            x: panel.expandedPlayer ? 0 : arrow.width
            width: parent.width - arrow.width; height: parent.height; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
            onClicked: if (upcomingModel.count) panel.activate(0)
        }
        Item {
            id: arrow
            x: panel.expandedPlayer ? parent.width - width : 0
            width: !panel.expandedPlayer && !panel.panelExpanded ? 28 : 34
            height: parent.height
            Rectangle { anchors.fill: parent; anchors.margins: 1; radius: 7; color: arrowMouse.containsMouse ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08) : "transparent" }
            UiText { fontRegion: "next"; anchors.centerIn: parent; text: panel.panelExpanded ? "⌄" : "⌃"; color: Theme.text; font.pixelSize: 15 }
            MouseArea { id: arrowMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: panel.panelExpanded = !panel.panelExpanded }
        }
    }
    ListView {
        id: rows; objectName: "upcomingRows"
        x: 0; y: panel.headerHeight; width: panel.width; height: Math.max(0, panel.height - y - 4)
        visible: panel.expandedPlayer || panel.panelExpanded
        clip: true; boundsBehavior: Flickable.StopAtBounds; interactive: panel.panelExpanded
        cacheBuffer: 0; reuseItems: true; model: upcomingModel
        property real scrollTarget: 0
        onMovementEnded: scrollTarget = contentY
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
                x: 7; y: 1; width: parent.width - 14; height: parent.height - 3; radius: 7
                color: rowMouse.containsMouse ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08) : "transparent"
                border.width: rowMouse.containsMouse ? 1 : 0; border.color: Theme.accent
            }
            UiText { fontRegion: "next"; x: 8; width: 20; height: parent.height; text: row.index + 1; color: Theme.text; font.pixelSize: 15; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            Artwork {
                id: rowArt; x: 32; anchors.verticalCenter: parent.verticalCenter; width: panel.coverSize; height: width; radius: 5
                file: row.coverUrl; seed: row.title; revision: row.coverRevision; resolution: 100; animated: false
            }
            UiText { fontRegion: "next";
                id: rowTitle; x: rowArt.x + rowArt.width + 8; width: titleMetrics.advanceWidth; height: parent.height
                text: row.title; textFormat: Text.PlainText; font.pixelSize: 15; color: Theme.text; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
            }
            UiText { fontRegion: "next"; id: dash; x: rowTitle.x + rowTitle.width; width: dashMetrics.advanceWidth; height: parent.height; text: "—"; font.pixelSize: 15; color: Theme.text; verticalAlignment: Text.AlignVCenter }
            UiText { fontRegion: "next";
                x: dash.x + dash.width; width: Math.min(artistMetrics.advanceWidth, Math.max(0, parent.width - x - 12)); height: parent.height
                text: row.artist; textFormat: Text.PlainText; font.pixelSize: 15; color: Theme.text; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
            }
            MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: panel.activate(row.index) }
        }
        UiText { fontRegion: "next"; visible: upcomingModel.count === 0; x: 12; y: 0; width: rows.width - 24; height: rows.height; text: "暂无待播放歌曲"; color: Theme.text; font.pixelSize: 15; verticalAlignment: Text.AlignVCenter }
        ScrollBar.vertical: ScrollBar { width: 5; policy: panel.panelExpanded && upcomingModel.count > 5 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }
        WheelHandler {
            target: null
            onWheel: event => {
                if (panel.panelExpanded) {
                    const pixels = event.pixelDelta.y !== 0
                    const distance = pixels ? event.pixelDelta.y : event.angleDelta.y / 120 * panel.rowHeight
                    rows.scrollTarget = Math.max(0, Math.min(Math.max(0, rows.contentHeight - rows.height), rows.scrollTarget - distance))
                    wheelMotion.stop(); wheelMotion.from = rows.contentY; wheelMotion.to = rows.scrollTarget
                    wheelMotion.duration = settingsBridge.values.reduceMotion ? 0 : pixels ? 85 : 170; wheelMotion.start()
                }
                event.accepted = true
            }
        }
    }
    NumberAnimation { id: wheelMotion; target: rows; property: "contentY"; easing.type: Easing.OutCubic }
}
