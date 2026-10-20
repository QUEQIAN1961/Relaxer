import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import "../theme"
Controls.TextField {
    id:control
    implicitHeight: Theme.controlHeight
    Layout.minimumHeight: implicitHeight
    Layout.preferredHeight: implicitHeight
    Layout.maximumHeight: implicitHeight
    Layout.alignment: Qt.AlignVCenter
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSize
    font.weight: Font.Medium
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
