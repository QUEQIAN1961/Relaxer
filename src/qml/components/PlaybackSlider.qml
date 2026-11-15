import QtQuick
import QtQuick.Controls
import "../theme"
Slider {
    id: control
    implicitWidth: 120
    implicitHeight: 22
    padding: 0
    wheelEnabled: false
    // Opt in only for media progress; volume sliders retain their two colors.
    property bool showBuffer: false
    property var bufferedRanges: []
    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: 3
        radius: 1.5
        color: control.showBuffer ? Theme.progressUnloaded : Theme.progressTrack
        Repeater {
            model: control.showBuffer ? control.bufferedRanges : []
            delegate: Rectangle {
                required property var modelData
                readonly property real startFraction: Math.max(0, Math.min(1, (modelData.start - control.from) / Math.max(1, control.to - control.from)))
                readonly property real endFraction: Math.max(startFraction, Math.min(1, (modelData.end - control.from) / Math.max(1, control.to - control.from)))
                objectName: "bufferedProgressSegment"
                x: (control.mirrored ? 1 - endFraction : startFraction) * parent.width
                width: (endFraction - startFraction) * parent.width
                height: parent.height
                radius: parent.radius
                color: Theme.progressTrack
            }
        }
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
