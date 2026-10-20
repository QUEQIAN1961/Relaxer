import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
Button {
    id: control
    property bool primary: false
    flat: false
    property string symbol: ""
    implicitHeight: Theme.controlHeight
    Layout.minimumHeight: implicitHeight
    Layout.preferredHeight: implicitHeight
    Layout.maximumHeight: implicitHeight
    Layout.alignment: Qt.AlignVCenter
    implicitWidth: contentItem.implicitWidth + 30
    leftPadding: 15
    rightPadding: 15
    topPadding: 8
    bottomPadding: 8
    hoverEnabled: true
    font.pixelSize: Theme.fontSize
    font.weight: Font.DemiBold
    Accessible.name: text
    contentItem: Item {
        implicitWidth: caption.implicitWidth + (control.symbol.length ? 28 : 0)
        implicitHeight: Math.max(18, caption.implicitHeight)
        Symbol {
            visible: control.symbol.length > 0
            width: 20; height: 20; anchors.verticalCenter: parent.verticalCenter
            x: Math.max(0, (parent.width - parent.implicitWidth) / 2)
            name: control.symbol
            color: !control.enabled ? Theme.disabled : control.primary ? Theme.accentText : Theme.text
        }
        UiText {
            id: caption; anchors.verticalCenter: parent.verticalCenter
            x: Math.max(0, (parent.width - parent.implicitWidth) / 2) + (control.symbol.length ? 28 : 0)
            width: Math.max(0, Math.min(implicitWidth, parent.width - x))
            text: control.text; textFormat: Text.PlainText; font: control.font
            color: !control.enabled ? Theme.disabled : control.primary ? Theme.accentText : Theme.text
            horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
        }
    }
    background: Rectangle {
        radius: 8
        color: !control.enabled ? Theme.background
             : control.primary ? (control.down ? Qt.darker(Theme.accent, 1.2) : control.hovered ? Qt.lighter(Theme.accent, 1.1) : Theme.accent)
             : control.down ? Theme.pressed : control.hovered ? Theme.hover : control.flat ? "transparent" : Theme.card
        border.color: control.activeFocus ? Theme.accent : control.primary ? Theme.accent : Theme.border
        border.width: 1
    }
}
