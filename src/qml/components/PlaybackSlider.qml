import QtQuick
import QtQuick.Controls
import "../theme"
Slider {
    id: control
    implicitWidth: 120
    implicitHeight: 22
    padding: 0
    wheelEnabled: false
    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: 3
        radius: 1.5
        color: Theme.progressTrack
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: Theme.progress
        }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 10
        height: 10
        radius: 5
        color: Theme.progress
        opacity: control.enabled ? 1 : 0.4
        border.width: control.activeFocus ? 1 : 0
        border.color: Theme.text
    }
}
