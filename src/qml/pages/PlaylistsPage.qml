import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"
Item {
    id: page
    objectName: "playlistsPage"
    readonly property real reflow:Theme.layoutProgress
    readonly property real shrink:Math.min(1,2*reflow)
    readonly property real arrange:Math.max(0,2*reflow-1)
    signal openRequested(string key)
    signal editRequested(string key, string name)
    signal createRequested()
    property string contextKey: "0"
    property string contextName: ""
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pageMargin; anchors.topMargin: 28; anchors.rightMargin: Theme.pageMargin; anchors.bottomMargin: 16
        spacing: 18
        RowLayout {
            Layout.fillWidth: true; spacing: 20
            ColumnLayout {
                Layout.fillWidth: true; spacing: 16
                UiText { text: "歌单"; color: Theme.text; font.pixelSize: 28; font.weight: Font.DemiBold }
                UiText { text: "给不同的心情，留一张歌单。"; color: Theme.secondary; font.pixelSize: 14 }
            }
            ShellButton { objectName: "createPlaylistButton"; text: "创建歌单"; primary: true; onClicked: page.createRequested() }
        }
        GridView {
            id: grid; objectName: "playlistCards"
            Layout.fillWidth: true; Layout.fillHeight: true
            cellWidth:Theme.compact?width/2:Math.min(204,width);cellHeight:246-174*page.reflow
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
                Rectangle {
                    objectName:"playlistCardBackground";x:0;y:0;width:card.width-14+10*page.reflow;height:grid.cellHeight-12+6*page.reflow
                    radius:12;color:cardMouse.containsMouse?Theme.hover:"transparent"
                }
                Artwork {
                    id:cardArt;objectName:"playlistCardCover-"+card.playlistKey
                    x:7;y:7;width:Math.min(170-122*page.shrink,Math.max(0,card.width-14));height:width;radius:12
                    file:card.cover;seed:card.name;resolution:350;animated:false
                }
                UiText {
                    id:cardTitle;objectName:"playlistCardTitle-"+card.playlistKey
                    x:7+64*page.shrink;y:(cardArt.height+17)*(1-page.arrange)+7*page.arrange
                    width:Math.max(0,Math.min(177+(card.width-90-177)*page.arrange,card.width-x-14))
                    text:card.name;textFormat:Text.PlainText;color:Theme.text;font.pixelSize:15;font.weight:Font.DemiBold;elide:Text.ElideRight
                }
                UiText {
                    id:cardCount;x:cardTitle.x;y:(cardTitle.y+cardTitle.implicitHeight+4)*(1-page.arrange)+(55-implicitHeight)*page.arrange
                    width:cardTitle.width;text:card.trackCount+" 首歌曲"+(Theme.compact&&card.description.length?" · "+card.description:"")
                    color:Theme.secondary;font.pixelSize:13;elide:Text.ElideRight
                }
                MouseArea {
                    id: cardMouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.LeftButton | Qt.RightButton; cursorShape: Qt.PointingHandCursor
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton) { page.contextKey = card.playlistKey; page.contextName = card.name; playlistMenu.popup() }
                        else page.openRequested(card.playlistKey)
                    }
                }
            }
            UiText { anchors.centerIn: parent; width: Math.min(400, parent.width - 40); visible: playlistModel.count === 0; text: "还没有歌单。\n创建一个，收集喜欢的音乐。"; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 14; lineHeight: 1.6 }
        }
    }
    RoundMenu {
        id: playlistMenu
        RoundMenuItem { text: "打开歌单"; onTriggered: page.openRequested(page.contextKey) }
        RoundMenuItem { text: "编辑歌单…"; onTriggered: page.editRequested(page.contextKey, page.contextName) }
        MenuSeparator {}
        RoundMenuItem { text: "删除歌单…"; onTriggered: { deletion.listKey = page.contextKey; deletion.listName = page.contextName; deletion.open() } }
    }
    ConfirmDialog {
        id: deletion; parent: page.Overlay.overlay; property string listKey: "0"; property string listName: ""
        anchors.centerIn: parent; width: Math.min(430, parent.width - 40); modal: true; title: "删除歌单"; standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "删除「" + deletion.listName + "」？音乐文件和播放统计会保留。"; textFormat: Text.PlainText; wrapMode: Text.Wrap }
        onAccepted: libraryBridge.deletePlaylist(listKey)
    }
}
