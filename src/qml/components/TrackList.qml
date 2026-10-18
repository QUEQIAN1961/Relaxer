import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
Rectangle {
    id: table
    required property var trackModel
    property string selectedKey: "0"
    property string currentKey: "0"
    property string emptyHeading: "没有找到歌曲"
    property string emptyDescription: "试试其他搜索词，或在 Widgets 中导入本地音乐。"
    readonly property bool showAlbum: width > 720
    signal selected(string key)
    signal playRequested(string key)
    signal favoriteRequested(string key)
    color: Theme.panel; radius: Theme.radius; border.color: Theme.border
    clip: true
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 10; spacing: 4
        RowLayout {
            Layout.fillWidth: true; Layout.preferredHeight: 32; spacing: 12
            Item { Layout.preferredWidth: 32 }
            Text { text: "歌曲"; color: Theme.secondary; Layout.fillWidth: true }
            Text { text: "歌手"; color: Theme.secondary; Layout.preferredWidth: table.showAlbum ? 140 : 100 }
            Text { visible: table.showAlbum; text: "专辑"; color: Theme.secondary; Layout.preferredWidth: 140 }
            Text { text: "时长"; color: Theme.secondary; Layout.preferredWidth: 58 }
            Item { Layout.preferredWidth: 36 }
        }
        ListView {
            id: list
            objectName: "trackListView"
            Layout.fillWidth: true; Layout.fillHeight: true
            model: table.trackModel
            reuseItems: true; cacheBuffer: 112; clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: row
                objectName: "trackDelegate"
                required property string trackKey
                required property string title
                required property string artist
                required property string album
                required property real duration
                required property bool favorite
                width: ListView.view.width; height: 56; radius: 8
                color: table.selectedKey === trackKey ? Theme.selected : hover.containsMouse ? Theme.hover : "transparent"
                border.width: table.currentKey === trackKey ? 1 : 0; border.color: Theme.accent
                MouseArea {
                    id: hover; anchors.fill: parent; hoverEnabled: true
                    onClicked: table.selected(row.trackKey)
                    onDoubleClicked: table.playRequested(row.trackKey)
                }
                RowLayout {
                    anchors.fill: parent; anchors.margins: 4; spacing: 12
                    ToolButton {
                        Layout.preferredWidth: 32; text: table.currentKey === row.trackKey ? "●" : "▷"
                        Accessible.name: "播放 " + row.title
                        onClicked: table.playRequested(row.trackKey)
                    }
                    Text { text: row.title; color: table.currentKey === row.trackKey ? Theme.accent : Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text { text: row.artist; color: Theme.secondary; Layout.preferredWidth: table.showAlbum ? 140 : 100; elide: Text.ElideRight }
                    Text { visible: table.showAlbum; text: row.album; color: Theme.secondary; Layout.preferredWidth: 140; elide: Text.ElideRight }
                    Text {
                        text: Math.floor(row.duration / 60000) + ":" + (Math.floor(row.duration / 1000) % 60 < 10 ? "0" : "") + Math.floor(row.duration / 1000) % 60
                        color: Theme.secondary; Layout.preferredWidth: 58
                    }
                    ToolButton {
                        Layout.preferredWidth: 36; text: row.favorite ? "♥" : "♡"
                        Accessible.name: row.favorite ? "取消收藏" : "收藏"
                        onClicked: table.favoriteRequested(row.trackKey)
                    }
                }
            }
            Text {
                anchors.centerIn: parent; width: Math.min(parent.width - 40, 440)
                visible: list.count === 0
                text: table.emptyHeading + "\n\n" + table.emptyDescription
                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                color: Theme.secondary
            }
        }
    }
}
