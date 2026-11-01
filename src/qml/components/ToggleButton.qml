import QtQuick
import QtQuick.Controls
import "../theme"

ShellButton {
    id: control
    checkable: true
    primary: checked
    readonly property bool reduceMotion: settingsBridge.values.reduceMotion
    property real markProgress: checked ? 1 : 0
    property real indicatorSize: 18
    property real indicatorSpacing: 6
    Behavior on markProgress { NumberAnimation { duration: control.reduceMotion ? 0 : 280; easing.type: Easing.OutCubic } }
    leftPadding: 12
    rightPadding: 12
    Accessible.role: Accessible.CheckBox
    Accessible.checked: checked
    contentItem: Item {
        implicitWidth: caption.implicitWidth + control.indicatorSize + control.indicatorSpacing
        implicitHeight: Math.max(18, caption.implicitHeight)
        Canvas {
            id: mark
            objectName: "toggleMark"
            x: Math.max(0, (parent.width - parent.implicitWidth) / 2)
            anchors.verticalCenter: parent.verticalCenter
            width: control.indicatorSize; height: control.indicatorSize
            property real progress: control.markProgress
            property color ink: !control.enabled ? Theme.disabled : control.checked ? Theme.accentText : Theme.text
            Behavior on ink { ColorAnimation { duration: control.reduceMotion ? 0 : 180 } }
            onProgressChanged: requestPaint()
            onInkChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.save(); ctx.scale(width / 18, height / 18)
                ctx.strokeStyle = ink; ctx.lineWidth = 1.7
                ctx.lineCap = "round"; ctx.lineJoin = "round"
                const p = Math.max(0, Math.min(1, progress))
                // Unwind the circle while drawing the two strokes of the check.
                if (p < 1) {
                    ctx.globalAlpha = 1 - p
                    ctx.beginPath()
                    ctx.arc(9, 9, 6, -Math.PI / 2 + p * Math.PI, -Math.PI / 2 + p * Math.PI + 2 * Math.PI * (1 - p))
                    ctx.stroke()
                }
                const tick = Math.max(0, (p - .15) / .85)
                if (tick > 0) {
                    const first = Math.sqrt(4 * 4 + 3.5 * 3.5)
                    const second = Math.sqrt(7 * 7 + 7 * 7)
                    const length = (first + second) * tick
                    const a = Math.min(1, length / first)
                    ctx.globalAlpha = Math.min(1, tick * 2)
                    ctx.beginPath(); ctx.moveTo(3.5, 9)
                    ctx.lineTo(3.5 + 4 * a, 9 + 3.5 * a)
                    if (length > first) {
                        const b = Math.min(1, (length - first) / second)
                        ctx.lineTo(7.5 + 7 * b, 12.5 - 7 * b)
                    }
                    ctx.stroke()
                }
                ctx.restore()
            }
        }
        UiText {
            id: caption
            x: Math.max(0, (parent.width - parent.implicitWidth) / 2) + control.indicatorSize + control.indicatorSpacing
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, Math.min(implicitWidth, parent.width - x))
            text: control.text; textFormat: Text.PlainText; font: control.font
            color: !control.enabled ? Theme.disabled : control.checked ? Theme.accentText : Theme.text
            Behavior on color { ColorAnimation { duration: control.reduceMotion ? 0 : 180 } }
            elide: Text.ElideRight
        }
    }
    background: Rectangle {
        radius: 8
        color: !control.enabled ? Theme.background
             : control.checked ? (control.down ? Qt.darker(Theme.accent, 1.2) : control.hovered ? Qt.lighter(Theme.accent, 1.1) : Theme.accent)
             : control.down ? Theme.pressed : control.hovered ? Theme.hover : Theme.card
        border.color: control.activeFocus || control.checked ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: control.reduceMotion ? 0 : 220; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: control.reduceMotion ? 0 : 180 } }
    }
}
