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
    width: Math.min(680, parent ? parent.width - 32 : 680)
    property var files:({})
    property string targetRegion:"songs"
    readonly property var regions:[{key:"songs",name:"歌曲列表"},{key:"lyrics",name:"歌词"},{key:"footer",name:"底栏"},{key:"next",name:"下一首"},{key:"rest",name:"其余界面文字"}]
    function openSettings() {
        files=Object.assign({},settingsBridge.fontFiles)
        open()
    }
    function setFile(key,path){const copy=Object.assign({},files);copy[key]=path;files=copy}
    function fileName(path){
        if(!path)return "系统字体"
        const text=String(path)
        const name=(text.startsWith("file:")?decodeURIComponent(text):text).split(/[\\/]/).pop()
        return text.startsWith("Fonts/")?name.replace(/-[0-9a-f]{12}(?=\.[^.]+$)/,""):name
    }
    contentItem: ColumnLayout {
        spacing: 12
        UiText {Layout.fillWidth:true;text:"每个板块可单独选择字体文件。";wrapMode:Text.Wrap;color:Theme.secondary;font.pixelSize:14}
        Rectangle {
            Layout.fillWidth:true;implicitHeight:fontRows.implicitHeight+16;radius:10;color:Theme.card;border.color:Theme.border
            ColumnLayout {
                id:fontRows;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:8;spacing:8
                RowLayout {
                    Layout.fillWidth:true;spacing:8
                    UiText {text:"板块";Layout.preferredWidth:dialog.width<520?84:100;color:Theme.secondary;font.pixelSize:14}
                    UiText {text:"字体文件";Layout.fillWidth:true;color:Theme.secondary;font.pixelSize:14}
                    Item {Layout.preferredWidth:112;Layout.preferredHeight:1}
                }
                Rectangle {Layout.fillWidth:true;height:1;color:Theme.border}
                Repeater {
                    model:dialog.regions
                    delegate:RowLayout {
                        required property var modelData
                        objectName:"fontRegionRow-"+modelData.key;Layout.fillWidth:true;spacing:8
                        UiText {text:modelData.name;Layout.preferredWidth:dialog.width<520?84:100;color:Theme.text;font.pixelSize:14;wrapMode:Text.Wrap}
                        UiText {
                            objectName:"fontRegionFile-"+modelData.key;text:dialog.fileName(dialog.files[modelData.key]);textFormat:Text.PlainText;Layout.fillWidth:true;Layout.minimumWidth:0
                            elide:Text.ElideMiddle;color:Theme.secondary;font.pixelSize:14
                            HoverHandler {id:fileHover}
                            HintBubble {visible:fileHover.hovered;text:String(dialog.files[modelData.key]||"系统字体")}
                        }
                        ShellButton {objectName:"chooseFont-"+modelData.key;text:"选择";onClicked:{dialog.targetRegion=modelData.key;fontPicker.open()}}
                        ShellButton {objectName:"resetFont-"+modelData.key;text:"默认";onClicked:dialog.setFile(modelData.key,"")}
                    }
                }
            }
        }
        UiText { Layout.fillWidth: true; text: settingsBridge.status; visible:text.length>0;wrapMode: Text.Wrap; color: Theme.accent;font.pixelSize:14 }
    }
    footer: Item {
        implicitHeight: actions.implicitHeight + 28
        RowLayout {
            id: actions
            anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18; anchors.topMargin: 10; anchors.bottomMargin: 18
            spacing: 8
            ShellButton { objectName:"resetAllFonts";text: "恢复默认"; onClicked: { if (settingsBridge.resetFont()) dialog.close() } }
            Item { Layout.fillWidth: true }
            ShellButton { text: "取消"; onClicked: dialog.close() }
            ShellButton { objectName:"saveRegionFonts";text: "保存"; primary: true; onClicked: { if (settingsBridge.saveFonts(dialog.files)) dialog.close() } }
        }
    }
    FileDialog {
        id: fontPicker
        title: "选择字体"
        nameFilters: ["字体文件 (*.ttf *.otf *.ttc)"]
        onAccepted: dialog.setFile(dialog.targetRegion,selectedFile.toString())
    }
}
