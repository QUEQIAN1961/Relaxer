import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

Popup {
    id:popup;objectName:"onlineDownloadList"
    popupType:Popup.Item;padding:8;margins:8;focus:true
    width:Math.min(330,Overlay.overlay.width-16)
    x:{const origin=parent.mapToItem(Overlay.overlay,0,0);return Math.max(8-origin.x,Math.min(Overlay.overlay.width-width-8-origin.x,parent.width-width))}
    y:parent.height+6
    height:Math.min(onlineDownloads.count?44+Math.min(onlineDownloads.count,4)*68:104,Math.max(80,Overlay.overlay.height-parent.mapToItem(Overlay.overlay,0,y).y-12))
    closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutsideParent
    function byteText(value){const mb=Math.max(0,Number(value))/1000000;return mb.toFixed(mb>=10?1:mb<1?2:1).replace(/\.0$/,"")+"MB"}
    background:Rectangle {radius:Theme.radius;color:Theme.background;border.color:Theme.border}
    enter:Transition {NumberAnimation {property:"opacity";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:140}}
    exit:Transition {NumberAnimation {property:"opacity";to:0;duration:settingsBridge.values.reduceMotion?0:90}}
    contentItem:ColumnLayout {
        spacing:4
        UiText {text:"下载";Layout.fillWidth:true;Layout.leftMargin:6;Layout.preferredHeight:24;font.pixelSize:14;font.weight:Font.DemiBold}
        ListView {
            id:list;objectName:"onlineDownloadTasks";Layout.fillWidth:true;Layout.fillHeight:true
            clip:true;model:onlineDownloads;boundsBehavior:Flickable.StopAtBounds;spacing:0
            SmoothWheelHandler {flickable:list}
            ScrollBar.vertical:ScrollBar{}
            delegate:Item {
                id:entry;required property var model;width:list.width;height:68
                readonly property string taskState:model.state
                readonly property bool pending:["queued","resolving","downloading","converting"].indexOf(taskState)>=0
                objectName:"downloadTask-"+model.taskId
                RowLayout {
                    anchors.fill:parent;anchors.margins:6;spacing:10
                    Artwork {Layout.preferredWidth:34;Layout.preferredHeight:34;radius:5;animated:false;file:entry.model.cover||"";seed:entry.model.title;resolution:80}
                    ColumnLayout {
                        Layout.fillWidth:true;Layout.minimumWidth:0;spacing:3
                        UiText {Layout.fillWidth:true;text:entry.model.title;textFormat:Text.PlainText;font.pixelSize:13;elide:Text.ElideRight}
                        UiText {
                            objectName:"downloadBytes-"+entry.model.taskId;Layout.fillWidth:true;font.pixelSize:11;color:Theme.secondary;elide:Text.ElideRight;textFormat:Text.PlainText
                            text:entry.taskState==="queued"?"等待下载":entry.taskState==="resolving"?"正在获取资源…":entry.taskState==="converting"?"正在保存音频…":entry.taskState==="cancelled"?"已取消":entry.taskState==="failed"?entry.model.error:popup.byteText(entry.model.received)+"/"+(entry.model.total>0?popup.byteText(entry.model.total):"—")
                        }
                    }
                    DownloadGlyph {Layout.preferredWidth:20;Layout.preferredHeight:20;active:entry.pending;complete:entry.taskState==="completed";progress:entry.model.fraction;indeterminate:entry.taskState==="resolving"||entry.taskState==="converting"||entry.taskState==="downloading"&&!(entry.model.total>0)}
                    IconButton {Layout.preferredWidth:24;Layout.preferredHeight:24;padding:5;symbol:"close";tip:"取消下载";showTooltip:true;visible:entry.pending;onClicked:onlineDownloads.cancelTask(entry.model.taskId)}
                }
            }
            UiText {anchors.centerIn:parent;visible:onlineDownloads.count===0;text:"暂无下载";color:Theme.secondary;font.pixelSize:13}
        }
    }
}
