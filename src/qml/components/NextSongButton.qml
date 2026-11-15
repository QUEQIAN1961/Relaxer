import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import "../theme"
import "NextMorphFrames.js" as Morph
Button {
    id:button
    property bool confirmed:false
    property real progress:confirmed?1:0
    signal requested()
    property string tip:"下一首播放"
    property string songKey:""
    readonly property color ink:enabled?(confirmed?Theme.accent:Theme.text):Theme.disabled
    function points(part){
        const frame=progress*(Morph.frames.length-1),index=Math.floor(frame),t=frame-index,a=Morph.frames[index][part],b=Morph.frames[Math.min(index+1,Morph.frames.length-1)][part],result=[]
        for(let i=0;i<a.length;i+=2)result.push(Qt.point(a[i]+(b[i]-a[i])*t,a[i+1]+(b[i+1]-a[i+1])*t))
        return result
    }
    readonly property var firstPath:points(0)
    readonly property var secondPath:points(1)
    onSongKeyChanged:confirmed=false
    padding:8;hoverEnabled:true;Accessible.name:tip
    function confirm(){confirmed=true;reset.restart()}
    Timer {id:reset;interval:900;onTriggered:button.confirmed=false}
    Behavior on progress {NumberAnimation {duration:settingsBridge.values.reduceMotion?0:280;easing.type:Easing.OutCubic}}
    onClicked:requested()
    contentItem:Item {
        implicitWidth:20;implicitHeight:20
        Shape {
            objectName:"nextSongGlyph";preferredRendererType:Shape.CurveRenderer;antialiasing:true;width:24;height:24;x:(parent.width-20)/2;y:(parent.height-20)/2
            scale:20/24;transformOrigin:Item.TopLeft
            ShapePath {strokeColor:button.ink;strokeWidth:2;fillColor:"transparent";capStyle:ShapePath.RoundCap;joinStyle:ShapePath.RoundJoin;PathPolyline {path:button.firstPath}}
            ShapePath {strokeColor:button.ink;strokeWidth:2;fillColor:"transparent";capStyle:ShapePath.RoundCap;joinStyle:ShapePath.RoundJoin;PathPolyline {path:button.secondPath}}
        }
    }
    background:Rectangle {radius:8;color:button.down?Theme.pressed:button.hovered?Theme.hover:"transparent"}
    HintBubble {visible:button.hovered;text:button.tip}
}
