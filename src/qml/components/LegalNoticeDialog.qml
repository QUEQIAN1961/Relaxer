import QtQuick
import QtQuick.Controls
import "../theme"
ConfirmDialog {
    id:dialog
    objectName:"legalNoticeDialog"
    title:"开源许可与致谢"
    width:Math.min(820,parent.width-32);height:Math.min(620,parent.height-40)
    anchors.centerIn:parent;standardButtons:Dialog.Close
    contentItem:ScrollView {
        clip:true;contentWidth:availableWidth
        TextArea {
            text:appBridge.legalNotice;textFormat:TextEdit.PlainText;readOnly:true;selectByMouse:true
            wrapMode:TextEdit.Wrap;color:Theme.text;font.pixelSize:13
            background:null
        }
    }
}
