import QtQuick
import QtQuick.Controls as Controls
import "../theme"
Controls.Menu {
    id: menu
    popupType: Controls.Popup.Item
    property real expansion: 1
    readonly property real expandedHeight: implicitHeight
    height: expandedHeight * expansion
    clip: true
    padding: 6
    margins: 8
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSize
    font.weight: Font.Medium
    delegate: RoundMenuItem {}
    enter: Transition {
        NumberAnimation { target: menu; property: "expansion"; from: 0; to: 1; duration: settingsBridge.values.reduceMotion ? 0 : 240; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { target: menu; property: "expansion"; to: 0; duration: settingsBridge.values.reduceMotion ? 0 : 160; easing.type: Easing.InCubic }
    }
    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 40
        radius: Theme.radius
        color: Theme.background
        border.color: Theme.border
    }
}
