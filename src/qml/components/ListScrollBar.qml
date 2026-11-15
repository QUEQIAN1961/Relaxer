import QtQuick
import QtQuick.Controls
import "../theme"

ScrollBar {
    id:bar
    objectName:"listScrollBar"
    property Item hoverTarget:parent
    HoverHandler {id:listHover;parent:bar.hoverTarget}
    policy:ScrollBar.AsNeeded
    visible:size<1&&(listHover.hovered||hovered||pressed)
    active:true
    opacity:1
    hoverEnabled:true
    width:10
    padding:2
    z:8
    minimumSize:Math.min(1,24/Math.max(1,availableHeight))
    contentItem:Rectangle {
        implicitWidth:6;implicitHeight:24;radius:3
        color:bar.pressed?Theme.accent:Theme.secondary
        opacity:bar.hovered||bar.pressed?1:.65
    }
    background:Item {}
}
