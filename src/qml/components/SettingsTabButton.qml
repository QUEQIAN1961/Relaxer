import QtQuick
import QtQuick.Controls
import "../theme"
TabButton {
    id: control
    implicitHeight: 40; padding: 10
    contentItem: UiText {
        text: control.text; textFormat: Text.PlainText
        color: control.checked ? Theme.accent : Theme.text
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        font.family: control.font.family; font.pixelSize: Theme.fontSize
    }
    background: Rectangle { radius: 8; color: control.checked ? Theme.selected : Theme.panel; border.color: Theme.border }
}
