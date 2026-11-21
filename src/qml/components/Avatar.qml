import QtQuick
import QtQuick.Effects
import QtQuick.Window
import "../theme"
Item {
    id:avatar
    property url file
    property string seed:"Relaxer"
    property bool fallback:false
    property real radius:Math.min(width,height)*Theme.artworkCornerRatio
    readonly property alias imageItem:picture
    readonly property bool ready:picture.status===Image.Ready
    readonly property bool software:GraphicsInfo.api===GraphicsInfo.Software
    readonly property url placeholder:"image://relaxer/artwork?seed="+encodeURIComponent(seed)+"&resolution=256&radiusRatio=0"
    implicitWidth:32;implicitHeight:32
    onFileChanged:fallback=false
    Image {
        id:picture;anchors.fill:parent;visible:false;asynchronous:true;smooth:true;mipmap:true
        source:avatar.fallback||!String(avatar.file).length?avatar.placeholder:avatar.file
        sourceSize:Qt.size(256,256);fillMode:Image.PreserveAspectCrop
        onStatusChanged:if(status===Image.Error&&!avatar.fallback)avatar.fallback=true
    }
    ContinuousCorner {id:circle;anchors.fill:parent;radius:avatar.radius;color:"white";visible:false;layer.enabled:true}
    MultiEffect {anchors.fill:parent;visible:!avatar.software;source:picture;maskEnabled:true;maskThresholdMin:.5;maskSpreadAtMin:1;maskSource:circle}
    Canvas {
        id:fallbackPaint;visible:avatar.software
        readonly property real pixelRatio:Math.max(2,avatar.Window.window?avatar.Window.window.devicePixelRatio:1)
        width:avatar.width*pixelRatio;height:avatar.height*pixelRatio;scale:1/pixelRatio;transformOrigin:Item.TopLeft
        function load(){if(visible&&String(picture.source).length)loadImage(picture.source)}
        Component.onCompleted:load()
        onVisibleChanged:load()
        onWidthChanged:requestPaint()
        onHeightChanged:requestPaint()
        Connections {target:avatar;function onRadiusChanged(){fallbackPaint.requestPaint()}}
        onImageLoaded:requestPaint()
        Connections {target:picture;function onSourceChanged(){fallbackPaint.load()}function onStatusChanged(){fallbackPaint.load()}}
        onPaint:{
            const ctx=getContext("2d");ctx.clearRect(0,0,width,height)
            if(!isImageLoaded(picture.source))return
            ctx.save();ctx.beginPath();const r=Math.max(0,Math.min(avatar.radius*pixelRatio*1.6,width/2,height/2));const u=r*.1830582617584078;ctx.moveTo(r,0);ctx.lineTo(width-r,0);ctx.bezierCurveTo(width-4*u,0,width-2*u,0,width-u,u);ctx.bezierCurveTo(width,2*u,width,4*u,width,r);ctx.lineTo(width,height-r);ctx.bezierCurveTo(width,height-4*u,width,height-2*u,width-u,height-u);ctx.bezierCurveTo(width-2*u,height,width-4*u,height,width-r,height);ctx.lineTo(r,height);ctx.bezierCurveTo(4*u,height,2*u,height,u,height-u);ctx.bezierCurveTo(0,height-2*u,0,height-4*u,0,height-r);ctx.lineTo(0,r);ctx.bezierCurveTo(0,4*u,0,2*u,u,u);ctx.bezierCurveTo(2*u,0,4*u,0,r,0);ctx.closePath();ctx.clip();ctx.drawImage(picture.source,0,0,width,height);ctx.restore()
        }
    }
}
