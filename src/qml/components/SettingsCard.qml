import QtQuick
import QtQuick.Layouts
import "../theme"
Rectangle {
    id:card
    property string title
    default property alias rows:body.data
    Layout.fillWidth:true
    implicitHeight:body.implicitHeight+40
    radius:14;color:Theme.card
    ColumnLayout {
        id:body;anchors {left:parent.left;right:parent.right;top:parent.top;margins:20}
        spacing:16
        UiText {text:card.title;basePixelSize:22;font.weight:Font.DemiBold}
    }
}
