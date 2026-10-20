import QtQuick
import QtQuick.Controls as Controls
import "../theme"
Controls.MenuItem {
    id: control
    implicitHeight: 36
    height: visible ? implicitHeight : 0
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSize
    font.weight: Font.Medium
    contentItem: UiText {
        text: control.text
        font: control.font
        color: !control.enabled ? Theme.disabled : control.highlighted ? Theme.accent : Theme.text
        textFormat: Text.PlainText
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 6
        color: control.highlighted || control.hovered ? Theme.hover : "transparent"
    }
}
