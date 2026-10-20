import QtQuick
import QtQuick.Controls
import "../theme"
ToolTip {
    id: hint
    objectName: "meaningfulHint"
    popupType: Popup.Item
    delay: 800
    timeout: 6000
    padding: 10
    implicitWidth: Math.min(420, implicitContentWidth + leftPadding + rightPadding)
    contentItem: UiText {
        text: hint.text
        color: Theme.text
        font.pixelSize: 14
        wrapMode: Text.Wrap
    }
    background: Rectangle { radius: 8; color: Theme.card; border.color: Theme.border }
}
