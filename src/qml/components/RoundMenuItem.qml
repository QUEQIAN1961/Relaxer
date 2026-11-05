import QtQuick
import QtQuick.Controls as Controls
import "../theme"
Controls.MenuItem {
    id: control
    property bool destructive:false
    implicitHeight: 36
    height: visible ? implicitHeight : 0
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSize
    font.weight: Font.Medium
    leftPadding:checkable?36:12
    rightPadding:12
    indicator:Canvas {
        objectName:"menuCheckMark";x:10;y:(control.height-height)/2;width:18;height:18
        visible:control.checkable&&control.checked
        property color ink:!control.enabled?Theme.disabled:control.highlighted?Theme.accent:Theme.text
        onInkChanged:requestPaint()
        onPaint:{
            const ctx=getContext("2d");ctx.clearRect(0,0,width,height)
            ctx.strokeStyle=ink;ctx.lineWidth=1.7;ctx.lineCap="round";ctx.lineJoin="round"
            ctx.beginPath();ctx.moveTo(3.5,9);ctx.lineTo(7.5,12.5);ctx.lineTo(14.5,5.5);ctx.stroke()
        }
    }
    contentItem: UiText {
        text: control.text
        font: control.font
        color:!control.enabled?Theme.disabled:control.destructive?(Theme.dark?"#ff6b7a":"#d63e52"):control.highlighted?Theme.accent:Theme.text
        textFormat: Text.PlainText
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 6
        color: control.highlighted || control.hovered ? Theme.hover : "transparent"
    }
}
