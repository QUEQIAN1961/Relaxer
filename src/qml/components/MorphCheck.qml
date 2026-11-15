import QtQuick
import "../theme"
import "CheckMorphFrames.js" as Morph

Canvas {
    id: mark
    property bool checked: false
    property bool animated: true
    property real progress: checked ? 1 : 0
    property color ink: Theme.accent
    property real strokeWidth: 1.8
    implicitWidth: 20
    implicitHeight: 20
    Behavior on progress {enabled:mark.animated;NumberAnimation {duration:settingsBridge.values.reduceMotion?0:280;easing.type:Easing.OutCubic}}
    Behavior on ink {ColorAnimation {duration:settingsBridge.values.reduceMotion?0:180}}
    onProgressChanged: requestPaint()
    onInkChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx=getContext("2d"),p=Math.max(0,Math.min(1,progress))
        ctx.clearRect(0,0,width,height)
        const frame=p*(Morph.frames.length-1),index=Math.floor(frame),a=Morph.frames[index],b=Morph.frames[Math.min(index+1,Morph.frames.length-1)],t=frame-index
        ctx.save();ctx.scale(width/24,height/24)
        ctx.strokeStyle=ink;ctx.lineWidth=strokeWidth;ctx.lineCap="round";ctx.lineJoin="round"
        ctx.beginPath()
        for(let i=0;i<a.length;i+=2){const x=a[i]+(b[i]-a[i])*t,y=a[i+1]+(b[i+1]-a[i+1])*t;if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y)}
        if(p===0)ctx.closePath()
        ctx.stroke();ctx.restore()
    }
}
