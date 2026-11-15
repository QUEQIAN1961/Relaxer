import QtQuick
import QtQuick.Controls
import "../theme"

CheckBox {
    id: control
    implicitWidth: 32
    implicitHeight: 32
    padding: 0
    indicator: Item {
        width:28;height:28;x:(control.width-width)/2;y:(control.height-height)/2
        Rectangle {anchors.fill:parent;radius:14;color:control.hovered||control.down?Theme.hover:"transparent"}
        MorphCheck {objectName:"checkMorph";anchors.centerIn:parent;width:22;height:22;checked:control.checked;ink:!control.enabled?Theme.disabled:control.checked?Theme.accent:Theme.secondary}
    }
    contentItem: Item {}
    background:Rectangle {radius:8;color:"transparent";border.width:control.activeFocus?1:0;border.color:Theme.accent}
}
