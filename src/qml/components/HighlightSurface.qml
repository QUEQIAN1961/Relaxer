import QtQuick
import "../theme"

Rectangle {
    id: surface
    property bool selected: false
    property bool hovered: false
    property bool pressed: false
    property Item backdrop: Theme.pageBackdrop
    readonly property bool highlighted: selected || hovered || pressed
    readonly property bool glass: Theme.customHighlights && !!backdrop
    property real positionRevision: 0
    readonly property rect sample: sampleRectangle(positionRevision + x + y + width + height)
    function sampleRectangle(revision) {
        if (!backdrop) return Qt.rect(0, 0, width, height)
        const p = mapToItem(backdrop, 0, 0)
        return Qt.rect(p.x, p.y, width, height)
    }
    color: !highlighted ? "transparent" : glass ? "transparent"
         : pressed ? Theme.pressed : selected ? Theme.selected : Theme.hover
    radius: 8
    // Each caller tracks its layout/scroll offset. A stationary highlight does
    // not need a frame animation or a continuous stream of repaint requests.
    Loader {
        anchors.fill: parent
        active: surface.glass && surface.highlighted && surface.visible
        sourceComponent: GlassSurface {
            objectName: "highlightBackdrop"
            backdrop: surface.backdrop; sampleRect: surface.sample; radius: surface.radius
            tintColor: "white"; tintOpacity: surface.selected ? .18 : surface.pressed ? .15 : .08
            borderWidth: 0; effectName: "highlightGaussianBlur"
        }
    }
}
