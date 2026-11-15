import QtQuick
import QtQuick.Controls
import "../theme"

ShellButton {
    id: control
    checkable: true
    primary: checked
    readonly property bool reduceMotion: settingsBridge.values.reduceMotion
    property real markProgress: checked ? 1 : 0
    property real indicatorSize: 18
    property real indicatorSpacing: 6
    Behavior on markProgress { NumberAnimation { duration: control.reduceMotion ? 0 : 280; easing.type: Easing.OutCubic } }
    leftPadding: 12
    rightPadding: 12
    Accessible.role: Accessible.CheckBox
    Accessible.checked: checked
    contentItem: Item {
        implicitWidth: caption.implicitWidth + control.indicatorSize + control.indicatorSpacing
        implicitHeight: Math.max(18, caption.implicitHeight)
        MorphCheck {
            objectName:"toggleMark"
            x:Math.max(0,(parent.width-parent.implicitWidth)/2)
            anchors.verticalCenter:parent.verticalCenter;width:control.indicatorSize;height:control.indicatorSize
            progress:control.markProgress;animated:false
            ink:!control.enabled?Theme.disabled:control.checked?Theme.accentText:Theme.text
        }
        UiText {
            id: caption
            x: Math.max(0, (parent.width - parent.implicitWidth) / 2) + control.indicatorSize + control.indicatorSpacing
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, Math.min(implicitWidth, parent.width - x))
            text: control.text; textFormat: Text.PlainText; font: control.font
            color: !control.enabled ? Theme.disabled : control.checked ? Theme.accentText : Theme.text
            Behavior on color { ColorAnimation { duration: control.reduceMotion ? 0 : 180 } }
            elide: Text.ElideRight
        }
    }
    background: Rectangle {
        radius: 8
        color: !control.enabled ? Theme.background
             : control.checked ? (control.down ? Qt.darker(Theme.accent, 1.2) : control.hovered ? Qt.lighter(Theme.accent, 1.1) : Theme.accent)
             : control.down ? Theme.pressed : control.hovered ? Theme.hover : Theme.card
        border.color: control.activeFocus || control.checked ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: control.reduceMotion ? 0 : 220; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: control.reduceMotion ? 0 : 180 } }
    }
}
