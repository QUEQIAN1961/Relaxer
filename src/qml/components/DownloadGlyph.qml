import QtQuick
import QtQuick.Shapes
import QtQuick.Window
import "../theme"

// Download / circle / circle-check, using the Lucide geometry shown by Morphicons.
Item {
    id:glyph
    property bool active:false
    property bool complete:false
    property real progress:0
    property bool indeterminate:false
    property color ink:Theme.text
    property bool ready:false
    property real arrowOffset:0
    readonly property real circleTarget:active||complete?1:0
    property real circleAmount:circleTarget
    property real checkAmount:complete?1:0
    property real displayedProgress:progress
    readonly property bool animating:slide.running
    implicitWidth:24;implicitHeight:24
    Component.onCompleted:ready=true
    Behavior on circleAmount {enabled:glyph.ready&&!settingsBridge.values.reduceMotion;NumberAnimation {duration:240;easing.type:Easing.InOutCubic}}
    Behavior on checkAmount {enabled:glyph.ready&&!settingsBridge.values.reduceMotion;NumberAnimation {duration:220;easing.type:Easing.OutCubic}}
    Behavior on displayedProgress {enabled:glyph.ready&&!settingsBridge.values.reduceMotion;NumberAnimation {duration:180;easing.type:Easing.OutCubic}}
    function added(){if(!settingsBridge.values.reduceMotion)slide.restart()}
    SequentialAnimation {
        id:slide
        NumberAnimation {target:glyph;property:"arrowOffset";from:0;to:20;duration:170;easing.type:Easing.InCubic}
        PropertyAction {target:glyph;property:"arrowOffset";value:-20}
        NumberAnimation {target:glyph;property:"arrowOffset";to:0;duration:240;easing.type:Easing.OutCubic}
    }
    readonly property var tray:[[3,15],[3,19],[5,21],[19,21],[21,19],[21,15]]
    function outline(t){
        const lengths=[4,2.828,14,2.828,4],sum=27.656
        let path=""
        for(let i=0;i<=48;++i){
            let distance=i/48*sum,segment=0
            while(segment<4&&distance>lengths[segment]){distance-=lengths[segment];++segment}
            const f=distance/lengths[segment],a=tray[segment],b=tray[segment+1],angle=-Math.PI/2+i/48*Math.PI*2
            const x=(a[0]+(b[0]-a[0])*f)*(1-t)+(12+9*Math.cos(angle))*t
            const y=(a[1]+(b[1]-a[1])*f)*(1-t)+(12+9*Math.sin(angle))*t
            path+=(i?"L":"M")+x.toFixed(3)+","+y.toFixed(3)
        }
        return path
    }
    Item {
        anchors.centerIn:parent;width:24;height:24;scale:Math.min(glyph.width,glyph.height)/24;clip:true
        // Keep the arrow clipped and antialiased. GeometryRenderer recreates its nodes
        // when the window releases graphics resources; CurveRenderer leaves idle paths blank.
        layer.enabled:true;layer.smooth:true;layer.samples:4
        layer.textureSize:Qt.size(Math.ceil(glyph.width*Screen.devicePixelRatio),Math.ceil(glyph.height*Screen.devicePixelRatio))
        Shape {
            anchors.fill:parent;preferredRendererType:Shape.GeometryRenderer
            ShapePath {
                strokeWidth:1.8;strokeColor:glyph.complete?Theme.accent:glyph.circleAmount>0.01?Theme.disabled:glyph.ink
                fillColor:"transparent";capStyle:ShapePath.RoundCap;joinStyle:ShapePath.RoundJoin
                Behavior on strokeColor {ColorAnimation {duration:settingsBridge.values.reduceMotion?0:140}}
                PathSvg {path:glyph.outline(glyph.circleAmount)}
            }
        }
        Shape {
            anchors.fill:parent;preferredRendererType:Shape.GeometryRenderer;opacity:glyph.circleAmount*(1-glyph.checkAmount)
            rotation:glyph.indeterminate?spin.angle:0
            ShapePath {
                strokeWidth:1.8;strokeColor:Theme.accent;fillColor:"transparent";capStyle:ShapePath.RoundCap
                PathAngleArc {centerX:12;centerY:12;radiusX:9;radiusY:9;startAngle:-90;sweepAngle:glyph.indeterminate?78:360*Math.max(0,Math.min(1,glyph.displayedProgress))}
            }
        }
        Item {
            anchors.fill:parent
            transform:Translate {y:glyph.arrowOffset}
            Shape {
                anchors.fill:parent;preferredRendererType:Shape.GeometryRenderer
                opacity:Math.max(0,1-glyph.circleAmount+glyph.checkAmount)
                ShapePath {
                    strokeWidth:1.8;strokeColor:glyph.complete?Theme.accent:glyph.ink;fillColor:"transparent";capStyle:ShapePath.RoundCap;joinStyle:ShapePath.RoundJoin
                    PathSvg {path:"M"+(7+glyph.checkAmount)+","+(10+2*glyph.checkAmount)+" L"+(12-glyph.checkAmount)+",15 L17,"+(10-glyph.checkAmount)}
                }
                ShapePath {
                    strokeWidth:1.8;strokeColor:Qt.rgba(glyph.ink.r,glyph.ink.g,glyph.ink.b,1-glyph.circleAmount);fillColor:"transparent";capStyle:ShapePath.RoundCap
                    PathSvg {path:"M12,3 L12,"+(15-12*glyph.circleAmount)}
                }
            }
        }
    }
    QtObject {id:spin;property real angle:0}
    NumberAnimation {target:spin;property:"angle";from:0;to:360;duration:1000;loops:Animation.Infinite;running:glyph.visible&&glyph.active&&glyph.indeterminate&&!settingsBridge.values.reduceMotion}
}
