import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"
import "../theme"

ConfirmDialog {
    id: dialog
    objectName: "uiFontDialog"
    title: "界面字体"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(480, parent ? parent.width - 32 : 480)
    property url chosenFile: ""
    property var regions: ({ songs: false, lyrics: false, footer: false, next: false, rest: false })
    function openSettings() {
        chosenFile = ""
        const saved = settingsBridge.values.uiFont
        const next = {}
        for (const key of ["songs", "lyrics", "footer", "next", "rest"]) next[key] = !!saved[key]
        regions = next
        open()
    }
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            Layout.fillWidth: true
            text: dialog.chosenFile.toString() || settingsBridge.values.uiFont.file || "未安装字体，使用系统字体"
            wrapMode: Text.Wrap
        }
        ShellButton { text: "选择字体文件"; onClicked: fontPicker.open() }
        Repeater {
            model: [{key:"songs",name:"歌曲列表"},{key:"lyrics",name:"歌词"},{key:"footer",name:"底栏"},{key:"next",name:"下一首"},{key:"rest",name:"其余界面文字"}]
            delegate: CheckBox {
                required property var modelData
                text: modelData.name
                checked: !!dialog.regions[modelData.key]
                onClicked: { const copy = Object.assign({}, dialog.regions); copy[modelData.key] = checked; dialog.regions = copy }
            }
        }
        Label { Layout.fillWidth: true; text: settingsBridge.status; wrapMode: Text.Wrap; color: Theme.accent }
    }
    footer: RowLayout {
        spacing: 8
        ShellButton { text: "恢复默认"; onClicked: { if (settingsBridge.resetFont()) dialog.close() } }
        Item { Layout.fillWidth: true }
        ShellButton { text: "取消"; onClicked: dialog.close() }
        ShellButton { text: "保存"; primary: true; onClicked: { if (settingsBridge.saveFont(dialog.chosenFile, dialog.regions)) dialog.close() } }
    }
    FileDialog {
        id: fontPicker
        title: "选择字体"
        nameFilters: ["字体文件 (*.ttf *.otf *.ttc)"]
        onAccepted: dialog.chosenFile = selectedFile
    }
}
