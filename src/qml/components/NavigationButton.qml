import QtQuick
import QtQuick.Controls
import "../theme"
Button {
    id: control
    required property string marker
    property string textMarker: ""
    property bool selected: false
    property bool compact: false
    property real compactProgress:compact?1:0
    Behavior on compactProgress {NumberAnimation{duration:settingsBridge.values.reduceMotion?0:280;easing.type:Easing.OutCubic}}
    implicitHeight: 36
    implicitWidth: 145
    leftPadding: 10
    rightPadding: 10
    topPadding: 7
    bottomPadding: 7
    hoverEnabled: true
    HintBubble { visible:control.compact&&control.hovered; text:control.text }
    Accessible.name: text
    font.pixelSize: Theme.fontSize
    contentItem: Item {
        implicitWidth: 30 + caption.implicitWidth
        implicitHeight: 22
        Symbol {
            visible:!control.textMarker.length
            width:20;height:20;x:(parent.width-width)/2*control.compactProgress;anchors.verticalCenter:parent.verticalCenter
            name: control.marker
            color: !control.enabled ? Theme.disabled : control.selected ? Theme.accent : Theme.text
        }
        UiText {
            visible:control.textMarker.length>0
            x:(parent.width-width)/2*control.compactProgress;width:20;height:22;anchors.verticalCenter:parent.verticalCenter
            text:control.textMarker;textFormat:Text.PlainText;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;font.pixelSize:18;font.weight:Font.DemiBold
            color:!control.enabled?Theme.disabled:control.selected?Theme.accent:Theme.text
        }
        UiText {
            id:caption;objectName:"navigationCaption";opacity:1-control.compactProgress;visible:opacity>0
            x:30-8*control.compactProgress; width: Math.max(0, parent.width - x); anchors.verticalCenter: parent.verticalCenter
            text: control.text; textFormat: Text.PlainText; elide: Text.ElideRight
            font: control.font
            color: !control.enabled ? Theme.disabled : control.selected ? Theme.accent : Theme.text
        }
    }
    background: Rectangle {
        radius: 8
        color: control.down ? Theme.pressed : control.selected ? Theme.selected
             : control.hovered ? Theme.hover : "transparent"
        border.color: control.activeFocus ? Theme.accent : "transparent"
    }
}
