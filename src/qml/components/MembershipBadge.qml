import QtQuick
import "../theme"

Rectangle {
    id:badge
    property string membership:""
    implicitWidth:label.implicitWidth+10
    implicitHeight:Math.max(20,label.implicitHeight+4)
    radius:4;color:"transparent";border.color:"#bf9142"
    UiText {id:label;anchors.centerIn:parent;text:badge.membership.indexOf("超级")>=0?"SVIP":"VIP";basePixelSize:11;font.weight:Font.DemiBold;color:"#bf9142"}
    HoverHandler {id:hover}
    HintBubble {visible:hover.hovered;text:badge.membership}
}
