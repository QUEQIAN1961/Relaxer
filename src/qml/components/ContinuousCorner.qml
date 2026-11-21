import QtQuick
import QtQuick.Shapes
import "../theme"

// Two cubics keep zero curvature at straight joins and matching derivatives
// at the diagonal join. The diagonal inset preserves the optical corner radius.
Shape {
    id: surface
    property real radius: Math.min(width,height)*Theme.artworkCornerRatio
    property color color: "white"
    readonly property real corner: Math.max(0,Math.min(radius*1.6,width/2,height/2))
    readonly property real inset: corner*.1830582617584078
    preferredRendererType: Shape.CurveRenderer
    antialiasing: true
    ShapePath {
        strokeWidth: -1; fillColor: surface.color
        startX: surface.corner; startY: 0
        PathLine {x:surface.width-surface.corner;y:0}
        PathCubic {control1X:surface.width-4*surface.inset;control1Y:0;control2X:surface.width-2*surface.inset;control2Y:0;x:surface.width-surface.inset;y:surface.inset}
        PathCubic {control1X:surface.width;control1Y:2*surface.inset;control2X:surface.width;control2Y:4*surface.inset;x:surface.width;y:surface.corner}
        PathLine {x:surface.width;y:surface.height-surface.corner}
        PathCubic {control1X:surface.width;control1Y:surface.height-4*surface.inset;control2X:surface.width;control2Y:surface.height-2*surface.inset;x:surface.width-surface.inset;y:surface.height-surface.inset}
        PathCubic {control1X:surface.width-2*surface.inset;control1Y:surface.height;control2X:surface.width-4*surface.inset;control2Y:surface.height;x:surface.width-surface.corner;y:surface.height}
        PathLine {x:surface.corner;y:surface.height}
        PathCubic {control1X:4*surface.inset;control1Y:surface.height;control2X:2*surface.inset;control2Y:surface.height;x:surface.inset;y:surface.height-surface.inset}
        PathCubic {control1X:0;control1Y:surface.height-2*surface.inset;control2X:0;control2Y:surface.height-4*surface.inset;x:0;y:surface.height-surface.corner}
        PathLine {x:0;y:surface.corner}
        PathCubic {control1X:0;control1Y:4*surface.inset;control2X:0;control2Y:2*surface.inset;x:surface.inset;y:surface.inset}
        PathCubic {control1X:2*surface.inset;control1Y:0;control2X:4*surface.inset;control2Y:0;x:surface.corner;y:0}
    }
}
