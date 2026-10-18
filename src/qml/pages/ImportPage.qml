import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"
import "../theme"
ScrollView {
    id: page
    objectName: "importPage"
    contentWidth: availableWidth
    ColumnLayout {
        width: page.availableWidth; spacing: 18
        Text { Layout.fillWidth: true; text: "把喜欢的音乐带进来"; font.pixelSize: 24; font.weight: Font.Bold; color: Theme.text }
        Text { Layout.fillWidth: true; text: "文件会交由现有导入服务处理。重复文件保留原曲目，导入完成后自动刷新音乐库。"; color: Theme.secondary; wrapMode: Text.Wrap }
        RowLayout {
            Layout.fillWidth: true
            ShellButton { objectName: "importFilesButton"; text: "选择音频文件"; primary: true; enabled: !importBridge.busy && !importBridge.closing; onClicked: files.open() }
            ShellButton { objectName: "importFolderButton"; text: "选择文件夹"; enabled: !importBridge.busy && !importBridge.closing; onClicked: folder.open() }
            ShellButton { objectName: "importCancelButton"; text: "取消导入"; enabled: importBridge.busy && !importBridge.closing; onClicked: importBridge.cancel() }
        }
        RowLayout {
            Layout.fillWidth: true
            Text { text: "同时加入歌单"; color: Theme.secondary }
            ComboBox { id: target; Layout.fillWidth: true; model: playlistModel; textRole: "name"; valueRole: "playlistKey"; enabled: !importBridge.busy && addToPlaylist.checked }
            CheckBox { id: addToPlaylist; text: "启用"; enabled: playlistModel.count > 0 && !importBridge.busy }
        }
        ProgressBar { objectName: "importProgress"; Layout.fillWidth: true; from: 0; to: Math.max(1, importBridge.total); value: importBridge.current; indeterminate: importBridge.busy && importBridge.total === 0 }
        Text { Layout.fillWidth: true; text: importBridge.busy ? "正在处理 " + importBridge.current + " / " + importBridge.total : "导入任务"; color: Theme.text }
        Text { Layout.fillWidth: true; text: importBridge.status; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.secondary }
        Text { Layout.fillWidth: true; visible: importBridge.result.errors !== undefined; text: importBridge.result.errors ? importBridge.result.errors.join("\n") : ""; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.accent }
        Text { Layout.fillWidth: true; text: "可在导入期间切换页面。取消会等待当前后台操作安全结束，已经导入的歌曲会保留。"; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 12 }
        Item { Layout.preferredHeight: 20 }
    }
    FileDialog {
        id: files; title: "选择音乐文件"; fileMode: FileDialog.OpenFiles
        nameFilters: ["音频 (*.mp3 *.flac *.wav *.m4a *.aac *.ogg *.opus *.aiff *.wma *.ape *.mid *.midi)", "所有文件 (*)"]
        onAccepted: importBridge.start(selectedFiles, addToPlaylist.checked ? String(target.currentValue) : "0")
    }
    FolderDialog { id: folder; title: "选择音乐文件夹"; onAccepted: importBridge.start([selectedFolder], addToPlaylist.checked ? String(target.currentValue) : "0") }
}
