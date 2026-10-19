import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"
Item {
    id: page
    objectName: "playlistsPage"
    signal openRequested(string key)
    signal editRequested(string key, string name)
    signal createRequested()
    property string contextKey: "0"
    property string contextName: ""
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 34; anchors.topMargin: 28; anchors.rightMargin: 34; anchors.bottomMargin: 16
        spacing: 18
        RowLayout {
            Layout.fillWidth: true; spacing: 20
            ColumnLayout {
                Layout.fillWidth: true; spacing: 16
                Text { text: "歌单"; color: Theme.text; font.pixelSize: 28; font.weight: Font.DemiBold }
                Text { text: "给不同的心情，留一张歌单。"; color: Theme.secondary; font.pixelSize: 12 }
            }
            ShellButton { objectName: "createPlaylistButton"; text: "创建歌单"; primary: true; onClicked: page.createRequested() }
        }
        GridView {
            id: grid; objectName: "playlistCards"
            Layout.fillWidth: true; Layout.fillHeight: true
            cellWidth: 204; cellHeight: 230
            clip: true; model: playlistModel; boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: Item {
                id: card
                required property string playlistKey
                required property string name
                required property int trackCount
                required property url cover
                required property var tags
                required property string description
                required property real plays
                width: grid.cellWidth; height: grid.cellHeight
                Rectangle { x: 0; y: 0; width: 190; height: 222; radius: 12; color: cardMouse.containsMouse ? Theme.hover : "transparent" }
                Artwork { x: 7; y: 7; width: 170; height: 170; radius: 12; file: card.cover; seed: card.name; resolution: 350; animated: false }
                Text { x: 7; y: 187; width: 177; text: card.name; textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold; elide: Text.ElideRight }
                Text { x: 7; y: 208; width: 177; text: card.trackCount + " 首歌曲"; color: Theme.secondary; font.pixelSize: 11; elide: Text.ElideRight }
                MouseArea {
                    id: cardMouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.LeftButton | Qt.RightButton; cursorShape: Qt.PointingHandCursor
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton) { page.contextKey = card.playlistKey; page.contextName = card.name; playlistMenu.popup() }
                        else page.openRequested(card.playlistKey)
                    }
                }
            }
            Text { anchors.centerIn: parent; width: Math.min(400, parent.width - 40); visible: playlistModel.count === 0; text: "还没有歌单。\n创建一个，收集喜欢的音乐。"; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 14; lineHeight: 1.6 }
        }
    }
    Menu {
        id: playlistMenu
        MenuItem { text: "打开歌单"; onTriggered: page.openRequested(page.contextKey) }
        MenuItem { text: "编辑歌单…"; onTriggered: page.editRequested(page.contextKey, page.contextName) }
        MenuSeparator {}
        MenuItem { text: "删除歌单…"; onTriggered: { deletion.listKey = page.contextKey; deletion.listName = page.contextName; deletion.open() } }
    }
    ConfirmDialog {
        id: deletion; parent: page.Overlay.overlay; property string listKey: "0"; property string listName: ""
        anchors.centerIn: parent; width: Math.min(430, parent.width - 40); modal: true; title: "删除歌单"; standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "删除「" + deletion.listName + "」？音乐文件和播放统计会保留。"; textFormat: Text.PlainText; wrapMode: Text.Wrap }
        onAccepted: libraryBridge.deletePlaylist(listKey)
    }
}
