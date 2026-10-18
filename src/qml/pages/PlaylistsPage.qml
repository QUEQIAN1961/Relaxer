import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"
ColumnLayout {
    objectName: "playlistsPage"
    RowLayout {
        Layout.fillWidth: true
        Text { text: "歌单"; color: Theme.text; font.pixelSize: Theme.headingSize; font.weight: Font.DemiBold; Layout.fillWidth: true }
        ShellButton { text: "新建歌单"; primary: true; onClicked: { editor.listKey = "0"; nameInput.text = ""; editor.open() } }
    }
    ListView {
        Layout.fillWidth: true; Layout.fillHeight: true
        model: playlistModel; clip: true; reuseItems: true
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            required property string playlistKey
            required property string name
            required property int trackCount
            width: ListView.view.width; height: 84; radius: Theme.radius; color: Theme.panel
            RowLayout {
                anchors.fill: parent; anchors.margins: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: name; textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 18; elide: Text.ElideRight; Layout.fillWidth: true }
                    Text { text: trackCount + " 首歌曲"; color: Theme.secondary }
                }
                ShellButton { text: "打开"; onClicked: { if (libraryBridge.openPlaylist(playlistKey)) appBridge.navigate(0) } }
                ToolButton { text: "重命名"; onClicked: { editor.listKey = playlistKey; nameInput.text = name; editor.open() } }
                ToolButton { text: "删除…"; onClicked: { deletion.listKey = playlistKey; deletion.open() } }
            }
        }
        Text { anchors.centerIn: parent; visible: playlistModel.count === 0; text: "还没有歌单。创建一个，收集喜欢的音乐。"; color: Theme.secondary }
    }
    Dialog {
        id: editor; property string listKey: "0"
        title: listKey === "0" ? "新建歌单" : "重命名歌单"
        modal: true; anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Ok | Dialog.Cancel
        TextField { id: nameInput; placeholderText: "歌单名称"; selectByMouse: true; maximumLength: 200; onAccepted: editor.accept() }
        onAccepted: { if (listKey === "0") libraryBridge.createPlaylist(nameInput.text); else libraryBridge.renamePlaylist(listKey, nameInput.text) }
    }
    Dialog {
        id: deletion; property string listKey: "0"
        title: "删除歌单"; modal: true; anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { text: "仅删除歌单及关联，歌曲仍保留在曲库。" }
        onAccepted: libraryBridge.deletePlaylist(listKey)
    }
}
