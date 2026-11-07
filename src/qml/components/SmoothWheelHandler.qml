import QtQuick
import QtQuick.Controls

Item {
    id: wheel
    required property Flickable flickable
    property real notchDistance: Qt.styleHints.wheelScrollLines * 24
    property bool reduceMotion: settingsBridge.values.reduceMotion
    property real scrollTarget: 0
    property int scrollDirection: 0
    signal scrolling()
    signal wheelRequested(var event)
    anchors.fill: parent
    enabled: flickable && flickable.interactive

    function stop() {
        motion.stop()
        scrollTarget = flickable ? flickable.contentY : 0
        scrollDirection = 0
    }
    function constrainTarget() {
        if (!motion.running) return
        const top = flickable.originY
        const bottom = top + Math.max(0, flickable.contentHeight - flickable.height)
        const next = Math.max(top, Math.min(bottom, scrollTarget))
        if (next === scrollTarget) return
        motion.stop(); scrollTarget = next
        motion.from = flickable.contentY; motion.to = next; motion.restart()
    }
    // Keep the handler on the viewport: ListView otherwise reparents declared
    // pointer handlers into its moving (and sometimes zero-width) contentItem.
    Component.onCompleted: parent = Qt.binding(function() { return wheel.flickable })
    onEnabledChanged: if (!enabled) stop()
    property WheelHandler handler: WheelHandler {
        parent: wheel
        target: null
        onWheel: event => {
            // Page headers get first refusal, including when the pointer is
            // over a row and this handler receives the event before the page.
            event.accepted = false
            wheel.wheelRequested(event)
            if (event.accepted) {
                wheel.stop(); wheel.flickable.cancelFlick(); return
            }
            // Precision devices already provide distance and momentum. Passing
            // those events through avoids adding a second layer of inertia.
            if (event.pixelDelta.x !== 0 || event.pixelDelta.y !== 0 ||
                event.modifiers & (Qt.ControlModifier | Qt.ShiftModifier) ||
                !event.angleDelta.y || Math.abs(event.angleDelta.x) > Math.abs(event.angleDelta.y)) {
                wheel.stop()
                if (event.pixelDelta.y !== 0 && !(event.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))) wheel.scrolling()
                event.accepted = false; return
            }
            wheel.scrolling()
            const view = wheel.flickable, animation = wheel.motion
            const direction = Math.sign(event.angleDelta.y)
            // Accumulate a burst, but reverse from the painted position rather
            // than spending the next few notches undoing an old destination.
            const base = animation.running && direction === wheel.scrollDirection ? wheel.scrollTarget : view.contentY
            const top = view.originY
            const bottom = top + Math.max(0, view.contentHeight - view.height)
            const next = Math.max(top, Math.min(bottom, base - event.angleDelta.y / 120 * wheel.notchDistance))
            if (Math.abs(next - view.contentY) < .01) {
                wheel.stop(); view.contentY = next; event.accepted = false; return
            }
            view.cancelFlick()
            animation.stop()
            wheel.scrollTarget = next; wheel.scrollDirection = direction
            animation.from = view.contentY; animation.to = next; animation.restart()
            event.accepted = true
        }
    }
    property NumberAnimation motion: NumberAnimation {
        target: wheel.flickable; property: "contentY"
        duration: wheel.reduceMotion ? 0 : 170
        easing.type: Easing.OutCubic
    }
    property Connections movement: Connections {
        target: wheel.flickable
        function onDraggingChanged() { if (wheel.flickable.dragging) wheel.stop() }
        function onFlickingChanged() { if (wheel.flickable.flicking) wheel.stop() }
        function onVisibleChanged() { if (!wheel.flickable.visible) wheel.stop() }
        function onHeightChanged() { wheel.constrainTarget() }
        function onContentHeightChanged() { wheel.constrainTarget() }
    }
    property Connections scrollbar: Connections {
        id: barConnection
        target: wheel.flickable ? wheel.flickable.ScrollBar.vertical : null
        function onPressedChanged() { if (barConnection.target.pressed) wheel.stop() }
    }
}
