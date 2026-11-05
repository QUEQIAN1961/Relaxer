import QtQuick
import QtQuick.Controls
import "../theme"
Rectangle {
    id: control
    objectName: "listPlayMode"
    property int selectedMode: 1
    property int arrowHalfTurns:0
    property real arrowRotation:180*arrowHalfTurns
    Behavior on arrowRotation {NumberAnimation{duration:settingsBridge.values.reduceMotion?0:240;easing.type:Easing.OutCubic}}
    readonly property var labels: ["顺序播放", "列表循环", "单曲播放", "随机播放"]
    readonly property var symbols: ["playlist", "repeat", "single", "shuffle"]
    implicitWidth: 160
    implicitHeight: Theme.controlHeight
    width: implicitWidth; height: implicitHeight
    radius: 8; color: Theme.card; border.color: Theme.border
    opacity: enabled ? 1 : .5
    signal playRequested(int mode)
    ShellButton {
        objectName: "listPlayModeStart"
        width: parent.width-32; height: parent.height; flat: true
        text: control.labels[control.selectedMode]; symbol:control.width<145?"":control.symbols[control.selectedMode]
        leftPadding: 10; rightPadding: 6
        background: Rectangle { radius:8;color:parent.hovered?Theme.hover:"transparent" }
        onClicked:{modes.close();control.playRequested(control.selectedMode)}
    }
    Rectangle { x:parent.width-33;y:8;width:1;height:parent.height-16;color:Theme.border }
    IconButton {
        objectName: "listPlayModeMenuButton"
        x:parent.width-width;width:32;height:parent.height;symbol:"down";text:"选择播放方式"
        contentItem:Symbol {objectName:"playModeArrow";name:"down";rotation:control.arrowRotation}
        onClicked:if(modes.visible)modes.close();else modes.popup(control,0,control.height+4)
    }
    RoundMenu {
        id:modes;objectName:"listPlayModes";width:control.width;enabled:control.enabled
        closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutsideParent
        onAboutToShow:control.arrowHalfTurns+=1
        onAboutToHide:control.arrowHalfTurns+=1
        onEnabledChanged:if(!enabled)close()
        Instantiator {
            model:[3,0,1,2]
            delegate:RoundMenuItem {
                required property int modelData
                objectName:"listModeOption-"+modelData;text:control.labels[modelData];checkable:true;checked:control.selectedMode===modelData
                onTriggered:control.selectedMode=modelData
            }
            onObjectAdded:(index,object)=>modes.insertItem(index,object)
            onObjectRemoved:(index,object)=>modes.removeItem(object)
        }
    }
}
