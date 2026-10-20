import QtQuick
import QtQuick.Controls
import "../theme"
Button {
    id: control
    required property string marker
    property bool selected: false
    property bool compact: false
    implicitHeight: 36
    implicitWidth: 145
    leftPadding: 10
    rightPadding: 10
    topPadding: 7
    bottomPadding: 7
    hoverEnabled: true
    Accessible.name: text
    font.pixelSize: Theme.fontSize
    contentItem: Item {
        implicitWidth: 30 + caption.implicitWidth
        implicitHeight: 22
        Symbol {
            width: 20; height: 20; anchors.verticalCenter: parent.verticalCenter
            name: control.marker
            color: !control.enabled ? Theme.disabled : control.selected ? Theme.accent : Theme.text
        }
        UiText {
            id: caption; x: 30; width: Math.max(0, parent.width - x); anchors.verticalCenter: parent.verticalCenter
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
