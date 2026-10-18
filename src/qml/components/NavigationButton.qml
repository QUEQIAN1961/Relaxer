import QtQuick
import QtQuick.Controls
import "../theme"
Button {
    id: control
    required property string marker
    property bool selected: false
    property bool compact: false
    implicitHeight: compact ? 38 : 48
    padding: compact ? 8 : 14
    Accessible.name: text
    font.pixelSize: Theme.fontSize
    contentItem: Text {
        text: control.marker + "   " + control.text
        font.pixelSize: control.font.pixelSize
        font.family: control.font.family
        font.weight: control.selected ? Font.DemiBold : Font.Normal
        color: !control.enabled ? Theme.disabled : control.selected ? Theme.accent : Theme.text
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 10
        color: control.down ? Theme.pressed : control.selected ? Theme.selected
             : control.hovered ? Theme.hover : "transparent"
        border.color: control.activeFocus ? Theme.accent : "transparent"
    }
}
