import QtQuick
import QtQuick.Window

Item {
    id:avatar
    property url file
    property string seed:"Relaxer Listener"
    property string revision:""
    property int resolution:512
    property real radius:Math.min(width,height)/2
    readonly property alias imageItem:picture
    readonly property bool presentationActive:visible&&(!Window.window||(Window.window.visible&&Window.window.visibility!==Window.Minimized))
    // Draw the full-resolution rounded image directly; a logical-size Canvas would
    // rasterize it before Windows applies display scaling.
    Image {
        id:picture;anchors.fill:parent
        source:avatar.presentationActive?"image://relaxer/artwork?file="+encodeURIComponent(String(avatar.file))+"&seed="+encodeURIComponent(avatar.seed)+"&rev="+encodeURIComponent(avatar.revision)+"&resolution="+avatar.resolution+"&radiusRatio="+Math.min(.5,avatar.radius/Math.max(1,Math.min(avatar.width,avatar.height))):""
        sourceSize:Qt.size(avatar.resolution,avatar.resolution)
        fillMode:Image.PreserveAspectFit;smooth:true;mipmap:true;asynchronous:true
    }
}
