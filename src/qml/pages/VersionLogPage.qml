import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"
Item {
    id:page;objectName:"versionLogPage"
    signal backRequested()
    property int shown:3
    ColumnLayout {
        anchors.fill:parent;anchors.leftMargin:34;anchors.rightMargin:34;anchors.topMargin:28;anchors.bottomMargin:28;spacing:16
        RowLayout {
            Layout.fillWidth:true
            UiText {text:"版本日志";font.pixelSize:28;font.weight:Font.DemiBold;color:Theme.text;Layout.fillWidth:true}
            ShellButton{text:"返回设置";flat:true;onClicked:page.backRequested()}
        }
        ScrollView {
            id:scroll;Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth;clip:true
            ColumnLayout {
                width:scroll.availableWidth;spacing:12
                Repeater {
                    model:settingsBridge.changelog.slice(0,page.shown)
                    delegate:ColumnLayout {
                        required property var modelData
                        Layout.fillWidth:true;spacing:8
                        UiText {Layout.fillWidth:true;text:modelData[0]+"  "+modelData[1];textFormat:Text.PlainText;font.weight:Font.DemiBold;font.pixelSize:15;color:Theme.text}
                        UiText {Layout.fillWidth:true;text:modelData[2];textFormat:Text.PlainText;wrapMode:Text.Wrap;color:Theme.secondary;font.pixelSize: 15}
                    }
                }
                RowLayout {
                    Layout.fillWidth:true
                    Item{Layout.fillWidth:true}
                    ShellButton{visible:page.shown>3;text:"收起";flat:true;onClicked:page.shown=3}
                    ShellButton{visible:page.shown<settingsBridge.changelog.length;text:"加载更多";flat:true;onClicked:page.shown+=3}
                }
                UiText {text:appBridge.version+" | © QUEQIAN. All rights reserved.";color:Theme.secondary;font.pixelSize: 14}
            }
        }
    }
}
