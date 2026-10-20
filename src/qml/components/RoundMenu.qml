import QtQuick
import QtQuick.Controls as Controls
import "../theme"
Controls.Menu {
    popupType: Controls.Popup.Item
    padding: 6
    margins: 8
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSize
    font.weight: Font.Medium
    delegate: RoundMenuItem {}
    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 40
        radius: Theme.radius
        color: Theme.background
        border.color: Theme.border
    }
}
