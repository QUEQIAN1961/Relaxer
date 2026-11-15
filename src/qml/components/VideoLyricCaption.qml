import QtQuick
import QtQuick.Effects
import QtQuick.Window
import "../theme"
Item {
    id:caption
    required property Item videoItem
    property string text:""
    property string translation:""
    readonly property bool software:GraphicsInfo.api===GraphicsInfo.Software
    readonly property real imageWidth:Math.min(videoItem.width,videoItem.height*videoItem.videoAspect)
    readonly property real imageHeight:Math.min(videoItem.height,videoItem.width/videoItem.videoAspect)
    width:Math.max(0,Math.min(imageWidth-16,Math.min(720,imageWidth*.86)))
    height:Math.max(76,words.implicitHeight+48)
    x:videoItem.x+(videoItem.width-width)/2
    y:videoItem.y+(videoItem.height+imageHeight)/2-height-8
    function update(){text=lyricsModel.instrumental?"":lyricsModel.textAt(lyricsModel.currentIndex>=0?lyricsModel.currentIndex:lyricsModel.timed?-1:0);translation=lyricsModel.translationAt(lyricsModel.currentIndex>=0?lyricsModel.currentIndex:lyricsModel.timed?-1:0)}
    Component.onCompleted:update()
    Connections {target:lyricsModel;function onDocumentChanged(){caption.update()}function onCurrentIndexChanged(){caption.update()}}
    ShaderEffectSource {
        id:sample;anchors.fill:parent;visible:false;sourceItem:caption.videoItem;hideSource:false;live:caption.visible&&!caption.software
        sourceRect:{const at=caption.mapToItem(caption.videoItem,0,0);return Qt.rect(at.x,at.y,caption.width,caption.height)}
    }
    Canvas {
        id:fade;anchors.fill:parent;visible:caption.software;layer.enabled:true
        onWidthChanged:requestPaint();onHeightChanged:requestPaint()
        onPaint:{
            const c=getContext("2d");c.clearRect(0,0,width,height);if(width<=0||height<=0)return
            c.save();c.scale(width/2,height/2);const gradient=c.createRadialGradient(1,1,.25,1,1,1)
            gradient.addColorStop(0,"rgba(0,0,0,0.86)");gradient.addColorStop(.5,"rgba(0,0,0,0.6)");gradient.addColorStop(1,"transparent")
            c.fillStyle=gradient;c.fillRect(0,0,2,2);c.restore()
        }
    }
    MultiEffect {
        objectName:"videoLyricBlur";anchors.fill:parent;visible:!caption.software
        source:sample;blurEnabled:true;blurMax:32;blur:.7;brightness:-.24;autoPaddingEnabled:false
        maskEnabled:true;maskSource:fade;maskThresholdMin:.5;maskSpreadAtMin:1
    }
    Column {
        id:words;anchors.centerIn:parent;width:Math.max(1,parent.width-72);spacing:6
        UiText {objectName:"videoCaptionOriginal";width:parent.width;text:caption.text;textFormat:Text.PlainText;fontRegion:"lyrics";basePixelSize:22;font.weight:Font.DemiBold;color:"white";wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight;horizontalAlignment:Text.AlignHCenter;style:Text.Outline;styleColor:"#88000000"}
        UiText {objectName:"videoCaptionTranslation";width:parent.width;text:caption.translation;visible:settingsBridge.values.showLyricTranslation&&text.length>0;textFormat:Text.PlainText;fontRegion:"lyrics";basePixelSize:14;color:"#eeeeee";wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight;horizontalAlignment:Text.AlignHCenter;style:Text.Outline;styleColor:"#88000000"}
    }
}
