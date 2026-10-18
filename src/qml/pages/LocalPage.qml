import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"
ColumnLayout {
    objectName: "localPage"
    spacing: 12
    RowLayout {
        Layout.fillWidth: true
        Text { text: libraryBridge.playlistName; font.pixelSize: Theme.headingSize; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
        ShellButton { text: "全部歌曲"; visible: libraryBridge.playlistId !== "0"; onClicked: libraryBridge.openPlaylist("0") }
        ShellButton { objectName: "playAllButton"; text: "播放全部"; primary: true; enabled: libraryModel.count > 0; onClicked: libraryBridge.playAll() }
    }
    RowLayout {
        Layout.fillWidth: true
        TextField {
            id: search; objectName: "librarySearch"
            Layout.fillWidth: true; placeholderText: "搜索歌曲、歌手、专辑"
            text: libraryModel.filterText; onTextEdited: libraryModel.filterText = text
            Accessible.name: "搜索音乐库"
        }
        ComboBox {
            objectName: "librarySort"
            model: ["原顺序", "歌曲名称", "歌手", "专辑", "时长"]
            currentIndex: libraryModel.sortKey; onActivated: libraryModel.sortKey = currentIndex
            Accessible.name: "列表排序"
        }
        ToolButton { text: libraryModel.descending ? "↓" : "↑"; Accessible.name: "反转排序"; onClicked: libraryModel.descending = !libraryModel.descending }
        ToolButton { text: "♥"; Accessible.name: "我喜欢"; onClicked: libraryBridge.openPlaylist("-1") }
    }
    Text { objectName: "librarySummary"; text: libraryModel.count + " 首可见 · 曲库共 " + appBridge.trackCount + " 首"; color: Theme.secondary }
    TrackList {
        objectName: "libraryListContainer"
        Layout.fillWidth: true; Layout.fillHeight: true
        trackModel: libraryModel; selectedKey: libraryModel.selectedId
        currentKey: playbackBridge.trackId
        emptyHeading: appBridge.trackCount === 0 ? "音乐，从这里开始" : "没有找到歌曲"
        onSelected: key => libraryModel.selectedId = key
        onPlayRequested: key => libraryBridge.play(key)
        onFavoriteRequested: key => libraryBridge.toggleFavorite(key)
    }
    Flow {
        Layout.fillWidth: true; spacing: 6
        ShellButton { text: "播放选中"; enabled: libraryModel.selectedRow >= 0; onClicked: libraryBridge.play(libraryModel.selectedId) }
        ComboBox { id: destination; width: 150; model: playlistModel; textRole: "name"; valueRole: "playlistKey"; displayText: currentIndex < 0 ? "选择歌单" : currentText; Accessible.name: "目标歌单" }
        ShellButton { text: "加入歌单"; enabled: libraryModel.selectedRow >= 0 && destination.currentIndex >= 0; onClicked: libraryBridge.addToPlaylist(destination.currentValue, libraryModel.selectedId) }
        ShellButton { text: "移出歌单"; visible: Number(libraryBridge.playlistId) > 0; enabled: libraryModel.selectedRow >= 0; onClicked: libraryBridge.removeFromPlaylist(libraryModel.selectedId) }
        ToolButton { text: "↑"; visible: Number(libraryBridge.playlistId) > 0; onClicked: libraryBridge.moveInPlaylist(libraryModel.selectedId, -1); Accessible.name: "歌单上移" }
        ToolButton { text: "↓"; visible: Number(libraryBridge.playlistId) > 0; onClicked: libraryBridge.moveInPlaylist(libraryModel.selectedId, 1); Accessible.name: "歌单下移" }
        ShellButton { text: "移除引用…"; enabled: libraryModel.selectedRow >= 0; onClicked: { removal.trackKey = libraryModel.selectedId; removal.open() } }
    }
    Dialog {
        id: removal; property string trackKey: "0"
        title: "从曲库移除"; modal: true; anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { text: "移除歌曲引用、歌单关联和统计，保留音乐文件。"; wrapMode: Text.Wrap }
        onAccepted: libraryBridge.removeTrack(trackKey)
    }
}
