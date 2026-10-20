import QtQuick
import QtQuick.Controls
import "../theme"
Rectangle{
    id:toast
    property string message:""
    function show(text){message=text;display.restart()}
    visible:display.running
    width:Math.min(parent.width-40,label.implicitWidth+36);height:label.implicitHeight+24;radius:10;color:Theme.card;border.color:Theme.border
    UiText {id:label;anchors.fill:parent;anchors.margins:12;text:toast.message;color:Theme.text;horizontalAlignment:Text.AlignHCenter;wrapMode:Text.Wrap}
    Timer{id:display;interval:3800}
}
