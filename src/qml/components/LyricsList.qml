import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
Item {
    id: panel
    objectName: "lyricsPanel"
    property bool manualScroll: false
    property bool reduceMotion: settingsBridge.values.reduceMotion
    function centerCurrent() {
        if (!manualScroll && lyricsModel.currentIndex >= 0)
            list.positionViewAtIndex(lyricsModel.currentIndex, ListView.Center)
    }
    Connections {
        target: lyricsModel
        function onCurrentIndexChanged() { panel.centerCurrent() }
        function onDocumentChanged() { if (lyricsModel.trackKey !== list.documentKey) { list.documentKey = lyricsModel.trackKey; panel.manualScroll = false }; Qt.callLater(panel.centerCurrent) }
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 8
        RowLayout {
            visible: lyricsModel.preview || panel.manualScroll || lyricsModel.loading || lyricsModel.error.length > 0
            Layout.fillWidth: true
            UiText { Layout.fillWidth: true; text: lyricsModel.status; wrapMode: Text.Wrap; color: lyricsModel.preview ? Theme.accent : Theme.secondary; font.pixelSize: 14; textFormat: Text.PlainText }
            ShellButton { objectName: "lyricsReturnCurrent"; visible: panel.manualScroll && lyricsModel.timed; text: "回到当前歌词"; onClicked: { panel.manualScroll = false; panel.centerCurrent() } }
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
            onDraggingChanged: if (dragging) panel.manualScroll = true
            ScrollBar.vertical: ScrollBar { onPressedChanged: if (pressed) panel.manualScroll = true }
            delegate: Item {
                id: row
                objectName: "lyricDelegate"
                required property int index
                required property string rowKey
                required property string original
                required property string translation
                required property bool current
                required property bool played
                readonly property real activeScale: current ? 1.2 : 1
                width: list.width
                // Reserve enlarged geometry up front: the font-scale animation
                // never changes the height of every following lyric row.
                height: Math.max(76, words.implicitHeight * 1.2 + 28)
                Column {
                    id: words
                    width: Math.max(1, row.width / 1.2 - 24)
                    anchors.centerIn: parent; spacing: 8
                    scale: row.activeScale; transformOrigin: Item.Center
                    opacity: row.current ? 1 : row.played ? 0.62 : 0.4
                    Behavior on scale { NumberAnimation { duration: panel.reduceMotion ? 0 : 180; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: panel.reduceMotion ? 0 : 180 } }
                    UiText {
                        width: parent.width; text: row.original; textFormat: Text.PlainText
                        wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: 22; font.family: { const f=settingsBridge.values.uiFont; return settingsBridge.fontFamilyFor("lyrics") || "Microsoft YaHei UI" } font.weight: Font.DemiBold
                        color: settingsBridge.values.lyricColor.length > 0 ? settingsBridge.values.lyricColor : Theme.text
                    }
                    UiText {
                        width: parent.width; text: row.translation; textFormat: Text.PlainText
                        visible: settingsBridge.values.showLyricTranslation && text.length > 0
                        wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                        color: Theme.secondary; font.pixelSize: 17
                    }
                }
                TapHandler { onTapped: { if (lyricsModel.seekRow(row.index)) { panel.manualScroll = false; panel.centerCurrent() } } }
            }
          }
          // A viewport sibling: ListView's contentItem may have zero width.
          MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton; onWheel: wheel => { panel.manualScroll = true; wheel.accepted = false } }
          EmptyState { anchors.centerIn: parent; width: Math.min(parent.width - 20, 420); visible: lyricsModel.count === 0; heading: lyricsModel.loading ? "正在读取" : "让音乐，留一点空白"; description: lyricsModel.status }
        }
    }
}
