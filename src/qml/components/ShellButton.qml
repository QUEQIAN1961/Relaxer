import QtQuick
import QtQuick.Controls
import "../theme"
Button {
    id: control
    property bool primary: false
    implicitHeight: 42
    implicitWidth: Math.max(104, contentItem.implicitWidth + 32)
    padding: 12
    font.pixelSize: Theme.fontSize
    Accessible.name: text
    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? Theme.disabled : control.primary ? Theme.accentText : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 10
        color: !control.enabled ? Theme.background
             : control.primary ? (control.down ? Qt.darker(Theme.accent, 1.2) : control.hovered ? Qt.lighter(Theme.accent, 1.1) : Theme.accent)
             : control.down ? Theme.pressed : control.hovered ? Theme.hover : Theme.panel
        border.color: control.activeFocus ? Theme.accent : Theme.border
        border.width: control.activeFocus ? 2 : 1
    }
}
