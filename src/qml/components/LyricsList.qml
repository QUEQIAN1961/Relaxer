import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
Item {
    id: panel
    objectName: "lyricsPanel"
    property bool manualScroll: false
    property bool reduceMotion: settingsBridge.values.reduceMotion
    readonly property int previewIndex: {
        const center = list.contentY + list.height / 2
        let nearest = -1, distance = Number.MAX_VALUE
        const items = list.contentItem.children
        for (let i = 0; i < items.length; ++i) {
            const item = items[i]
            if (item.objectName !== "lyricDelegate") continue
            const delta = Math.abs(item.y + item.height / 2 - center)
            if (delta < distance) { nearest = item.index; distance = delta }
        }
        return nearest
    }
    function takeOver() {
        followMotion.stop()
        manualScroll = true
        returnIdle.restart()
    }
    function returnCurrent() {
        returnIdle.stop()
        manualScroll = false
        panel.centerCurrent()
    }
    function centerCurrent() {
        if (manualScroll || lyricsModel.currentIndex < 0) return
        followMotion.stop()
        const before = list.contentY
        list.forceLayout()
        list.positionViewAtIndex(lyricsModel.currentIndex, ListView.Center)
        const destination = list.contentY
        list.contentY = before
        if (Math.abs(destination - before) < 0.5) return
        followMotion.from = before
        followMotion.to = destination
        followMotion.restart()
    }
    NumberAnimation { id: followMotion; objectName: "lyricsFollowMotion"; target: list; property: "contentY"; duration: 360; easing.type: Easing.OutCubic }
    Timer { id: returnIdle; interval: 3000; onTriggered: { if (list.dragging || list.flicking || lyricBar.pressed) restart(); else panel.returnCurrent() } }
    Connections {
        target: lyricsModel
        function onModelAboutToBeReset() { followMotion.stop(); list.cancelFlick() }
        function onCurrentIndexChanged() { panel.centerCurrent() }
        function onDocumentChanged() { if (lyricsModel.trackKey !== list.documentKey) { list.documentKey = lyricsModel.trackKey; panel.manualScroll = false; returnIdle.stop() }; Qt.callLater(panel.centerCurrent) }
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 8
        RowLayout {
            visible: lyricsModel.preview || panel.manualScroll || lyricsModel.loading || lyricsModel.error.length > 0
            Layout.fillWidth: true
            UiText { Layout.fillWidth: true; text: lyricsModel.status; wrapMode: Text.Wrap; color: lyricsModel.preview ? Theme.accent : Theme.secondary; font.pixelSize: 14; textFormat: Text.PlainText }
            ShellButton { objectName: "lyricsReturnCurrent"; visible: panel.manualScroll && lyricsModel.timed; text: "回到当前歌词"; onClicked: panel.returnCurrent() }
        }
        UiText { Layout.fillWidth: true; visible: lyricsModel.error.length > 0; text: lyricsModel.error; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.accent }
        Item {
          Layout.fillWidth: true; Layout.fillHeight: true
          ListView {
            id: list
            objectName: "lyricsList"
            property string documentKey: "0"
            anchors.fill: parent
            clip: true; model: lyricsModel; cacheBuffer: 150
            spacing: 4; boundsBehavior: Flickable.StopAtBounds
            header: Item { width: 1; height: list.height / 2 }
            footer: Item { width: 1; height: list.height / 2 }
            onDraggingChanged: { if (dragging) panel.takeOver(); else if (panel.manualScroll) returnIdle.restart() }
            onFlickingChanged: if (!flicking && panel.manualScroll) returnIdle.restart()
            ScrollBar.vertical: ScrollBar { id: lyricBar; onPressedChanged: { if (pressed) panel.takeOver(); else if (panel.manualScroll) returnIdle.restart() } }
            delegate: Item {
                id: row
                objectName: "lyricDelegate"
                required property int index
                required property string rowKey
                required property string original
                required property string translation
                required property bool current
                required property bool played
                readonly property bool scrollSelected: panel.manualScroll && panel.previewIndex === index
                readonly property bool emphasized: panel.manualScroll ? scrollSelected : current
                readonly property real activeScale: emphasized ? 1.2 : 1
                width: list.width
                // Reserve enlarged geometry up front: the font-scale animation
                // never changes the height of every following lyric row.
                height: Math.max(76, words.implicitHeight * 1.2 + 28)
                Column {
                    id: words
                    width: Math.max(1, row.width / 1.2 - 24)
                    anchors.centerIn: parent; spacing: 8
                    scale: row.activeScale; transformOrigin: Item.Center
                    opacity: row.emphasized ? 1 : row.played ? 0.62 : 0.4
                    Behavior on scale { NumberAnimation { duration: panel.reduceMotion ? 0 : 180; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: panel.reduceMotion ? 0 : 180 } }
                    UiText {
                        fontRegion: "lyrics"
                        width: parent.width; text: row.original; textFormat: Text.PlainText
                        wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: 22; font.weight: Font.DemiBold
                        color: settingsBridge.values.lyricColor.length > 0 ? settingsBridge.values.lyricColor : Theme.text
                    }
                    UiText {
                        fontRegion: "lyrics"
                        width: parent.width; text: row.translation; textFormat: Text.PlainText
                        visible: settingsBridge.values.showLyricTranslation && text.length > 0
                        wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                        color: row.scrollSelected ? Theme.text : Theme.secondary; font.pixelSize: 17
                    }
                }
                TapHandler { onTapped: if (lyricsModel.seekRow(row.index)) panel.returnCurrent() }
            }
          }
          Rectangle {
              objectName: "lyricsPreviewGuide"
              visible: panel.manualScroll && lyricsModel.timed && panel.previewIndex >= 0
              anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
              anchors.leftMargin: previewTime.width + 12; anchors.rightMargin: 20
              height: 1; color: Theme.text; opacity: .28
          }
          ShellButton {
              id: previewTime; objectName: "lyricsPreviewTime"
              anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
              z: 3; visible: panel.manualScroll && lyricsModel.timed && panel.previewIndex >= 0
              text: {
                  const ms = panel.previewIndex >= 0 ? Number(lyricsModel.timestampAt(panel.previewIndex)) : 0
                  return Math.floor(ms / 60000) + ":" + String(Math.floor(ms / 1000) % 60).padStart(2, "0") + "  ▶"
              }
              Accessible.name: "播放中线对应歌词 " + text
              onClicked: { const index = panel.previewIndex; if (lyricsModel.seekRow(index)) panel.returnCurrent() }
          }
          // A viewport sibling: ListView's contentItem may have zero width.
          MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton; onWheel: wheel => { panel.takeOver(); wheel.accepted = false } }
          EmptyState { anchors.centerIn: parent; width: Math.min(parent.width - 20, 420); visible: lyricsModel.count === 0; heading: lyricsModel.loading ? "正在读取" : "让音乐，留一点空白"; description: lyricsModel.status }
        }
    }
}
