import QtQuick
import QtQuick.Controls as Controls
import "../theme"
Controls.TextField {
    id:control
    implicitHeight:38
    leftPadding:12;rightPadding:12
    color:Theme.text;placeholderTextColor:Theme.secondary
    selectByMouse:true
    background:Rectangle {
        radius:8
        color:Theme.card
        border.width:1
        border.color:control.activeFocus?Theme.accent:Theme.border
    }
}
