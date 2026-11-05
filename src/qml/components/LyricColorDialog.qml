import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../theme"
ConfirmDialog {
    id: dialog
    objectName:"lyricColorDialog"
    anchors.centerIn:parent
    width:Math.min(410,parent.width-36)
    modal:true;title:"编辑歌词颜色"
    property bool custom:false
    property color chosenColor:Theme.text
    ButtonGroup {id:colorModes}
    function selectColor(value) {chosenColor=value}
    onAboutToShow:{const saved=String(settingsBridge.values.lyricColor||"");custom=saved.length>0;selectColor(saved||String(Theme.text))}
    onAccepted:settingsBridge.setPreference("lyricColor",custom?String(chosenColor):"")
    Connections {
        target:settingsBridge;enabled:dialog.visible
        function onLyricColorChosen(color){dialog.selectColor(color)}
    }
    footer:DialogButtonBox {
        background:null;padding:18;topPadding:8;spacing:8
        ShellButton {objectName:"saveLyricColor";text:"保存";primary:true;DialogButtonBox.buttonRole:DialogButtonBox.AcceptRole}
        ShellButton {objectName:"cancelLyricColor";text:"取消";DialogButtonBox.buttonRole:DialogButtonBox.RejectRole}
    }
    contentItem:ColumnLayout {
        spacing:10
        ToggleButton {objectName:"lyricColorFollow";Layout.fillWidth:true;ButtonGroup.group:colorModes;text:"跟随文字颜色";checked:!dialog.custom;onClicked:dialog.custom=false}
        UiText {Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:12;color:Theme.secondary;text:"跟随当前主题的正文文字颜色，随深浅外观改变。"}
        ToggleButton {objectName:"lyricColorCustom";Layout.fillWidth:true;ButtonGroup.group:colorModes;text:"自定义";checked:dialog.custom;onClicked:dialog.custom=true}
        Rectangle {
            objectName:"lyricColorPicker";Layout.fillWidth:true
            implicitHeight:colorChoice.implicitHeight+24
            radius:10;color:Theme.card;border.color:Theme.border
            enabled:dialog.custom
            ColorSwatch {
                id:colorChoice;objectName:"chooseLyricDraftColor"
                anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12
                caption:"选择颜色";swatchColor:dialog.chosenColor
                onClicked:settingsBridge.chooseLyricColorDraft(String(dialog.chosenColor),colorChoice.Window.window)
            }
        }
    }
}
