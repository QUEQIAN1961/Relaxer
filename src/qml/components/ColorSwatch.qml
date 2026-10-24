import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

ShellButton {
    id: control
    property color swatchColor: "#ffffff"
    property string caption: ""
    readonly property real gray: swatchColor.r * .299 + swatchColor.g * .587 + swatchColor.b * .114
    readonly property color displayColor: enabled ? swatchColor : Qt.rgba(swatchColor.r * .55 + gray * .45, swatchColor.g * .55 + gray * .45, swatchColor.b * .55 + gray * .45, 1)
    readonly property color swatchInk: displayColor.r * .299 + displayColor.g * .587 + displayColor.b * .114 > .58 ? "#202027" : "#ffffff"
    Layout.fillWidth: true
    Layout.preferredWidth: 1
    Layout.minimumWidth: 0
    font.pixelSize: 14
    font.weight: Font.Normal
    text: caption + "  " + swatchColor
    contentItem: UiText {
        text: control.text; textFormat: Text.PlainText; font: control.font
        color: control.swatchInk; opacity: control.enabled ? 1 : .5
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 8; color: control.displayColor
        border.color: control.activeFocus ? Theme.accent : Theme.border
        border.width: control.activeFocus ? 2 : 1
        Rectangle { anchors.fill: parent; radius: parent.radius; color: control.swatchInk; opacity: control.down ? .14 : control.hovered ? .06 : 0 }
        Rectangle { anchors.fill: parent; radius: parent.radius; color: Theme.card; opacity: control.enabled ? 0 : .25 }
    }
}
