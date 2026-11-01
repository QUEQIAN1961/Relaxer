import QtQuick
import QtQuick.Controls
import "../theme"

ShellButton {
    id: control
    checkable: true
    primary: checked
    leftPadding: 12
    rightPadding: 12
    Accessible.role: Accessible.CheckBox
    Accessible.checked: checked
    contentItem: Item {
        implicitWidth: caption.implicitWidth + 24
        implicitHeight: Math.max(18, caption.implicitHeight)
        UiText {
            x: Math.max(0, (parent.width - parent.implicitWidth) / 2)
            anchors.verticalCenter: parent.verticalCenter
            width: 18; text: control.checked ? "✓" : ""
            color: control.enabled ? Theme.accentText : Theme.disabled
            font.pixelSize: control.font.pixelSize
            font.weight: Font.DemiBold
        }
        UiText {
            id: caption
            x: Math.max(0, (parent.width - parent.implicitWidth) / 2) + 24
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, Math.min(implicitWidth, parent.width - x))
            text: control.text; textFormat: Text.PlainText; font: control.font
            color: !control.enabled ? Theme.disabled : control.checked ? Theme.accentText : Theme.text
            elide: Text.ElideRight
        }
    }
}
