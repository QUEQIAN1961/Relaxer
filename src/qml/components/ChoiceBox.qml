import QtQuick
import QtQuick.Controls as Controls
import "../theme"
Controls.ComboBox {
    id:control
    implicitHeight:38
    leftPadding:12;rightPadding:32
    contentItem:Text {
        leftPadding:0;rightPadding:0
        text:control.displayText;font:control.font
        color:control.enabled?Theme.text:Theme.disabled
        verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight
    }
    indicator:Symbol {
        x:control.width-width-10;y:(control.height-height)/2
        width:16;height:16;name:"collapse";color:Theme.secondary
    }
    background:Rectangle {
        radius:8;color:Theme.card;border.width:1
        border.color:control.activeFocus?Theme.accent:Theme.border
    }
}
