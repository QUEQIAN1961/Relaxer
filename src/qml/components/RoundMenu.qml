import QtQuick
import QtQuick.Controls as Controls
import "../theme"
Controls.Menu {
    popupType: Controls.Popup.Item
    transformOrigin: Item.Top
    padding: 6
    margins: 8
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSize
    font.weight: Font.Medium
    delegate: RoundMenuItem {}
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: settingsBridge.values.reduceMotion ? 0 : 180; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: .92; to: 1; duration: settingsBridge.values.reduceMotion ? 0 : 240; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; to: 0; duration: settingsBridge.values.reduceMotion ? 0 : 160; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; to: .92; duration: settingsBridge.values.reduceMotion ? 0 : 160; easing.type: Easing.InCubic }
        }
    }
    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 40
        radius: Theme.radius
        color: Theme.background
        border.color: Theme.border
    }
}
