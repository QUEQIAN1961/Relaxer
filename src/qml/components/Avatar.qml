import QtQuick
import QtQuick.Effects
import QtQuick.Window
Item {
    id:avatar
    property url file
    property string seed:"Relaxer"
    property bool fallback:false
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
    Rectangle {id:circle;anchors.fill:parent;radius:Math.min(width,height)/2;color:"white";visible:false;layer.enabled:true}
    MultiEffect {anchors.fill:parent;visible:!avatar.software;source:picture;maskEnabled:true;maskSource:circle}
    Canvas {
        id:fallbackPaint;visible:avatar.software
        readonly property real pixelRatio:avatar.Window.window?avatar.Window.window.devicePixelRatio:1
        width:avatar.width*pixelRatio;height:avatar.height*pixelRatio;scale:1/pixelRatio;transformOrigin:Item.TopLeft
        function load(){if(visible&&String(picture.source).length)loadImage(picture.source)}
        Component.onCompleted:load()
        onVisibleChanged:load()
        onWidthChanged:requestPaint()
        onHeightChanged:requestPaint()
        onImageLoaded:requestPaint()
        Connections {target:picture;function onSourceChanged(){fallbackPaint.load()}function onStatusChanged(){fallbackPaint.load()}}
        onPaint:{
            const ctx=getContext("2d");ctx.clearRect(0,0,width,height)
            if(!isImageLoaded(picture.source))return
            ctx.save();ctx.beginPath();ctx.arc(width/2,height/2,Math.min(width,height)/2,0,Math.PI*2);ctx.clip();ctx.drawImage(picture.source,0,0,width,height);ctx.restore()
        }
    }
}
