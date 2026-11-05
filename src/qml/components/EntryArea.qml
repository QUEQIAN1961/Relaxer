import QtQuick
import QtQuick.Controls as Controls
import "../theme"
Controls.TextArea {
    id: control
    padding:12
    font.family:Theme.fontFamily
    font.pixelSize:Theme.fontSize
    color:Theme.text
    placeholderTextColor:Theme.secondary
    selectionColor:Theme.accent
    selectedTextColor:Theme.accentText
    selectByMouse:true
    Controls.ContextMenu.menu:EditingMenu { editor:control }
    background:Rectangle {
        radius:8
        color:Theme.card
        border.color:control.activeFocus?Theme.accent:Theme.border
    }
}
