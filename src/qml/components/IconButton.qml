import QtQuick
import QtQuick.Controls
import "../theme"
Button {
    id: control
    property string symbol: ""
    property color iconColor: Theme.text
    property alias tint: control.iconColor
    property string tip: ""
    property bool circular: false
    property bool filled: false
    property bool showTooltip: false
    implicitWidth: circular ? 42 : 36
    implicitHeight: circular ? 42 : 36
    padding: circular ? 10 : 7
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: tip.length ? tip : text
    contentItem: Symbol {
        name: control.symbol
        color: control.enabled ? (control.filled ? Theme.accentText : control.iconColor) : Theme.disabled
    }
    background: Rectangle {
        radius: control.circular ? width / 2 : 8
        color: control.filled ? (control.down || control.hovered ? Qt.darker(Theme.accent, 1.1) : Theme.accent)
             : control.down || control.hovered ? Qt.rgba(0.5, 0.5, 0.5, 0.06) : "transparent"
        border.width: control.activeFocus ? 1 : 0
        border.color: Theme.accent
    }
    ToolTip.visible: (showTooltip || tip.length > 0) && hovered && (tip.length > 0 || text.length > 0)
    ToolTip.text: tip.length ? tip : text
    ToolTip.delay: 600
}
