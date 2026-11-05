import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../theme"
Dialog {
    id: dialog
    readonly property bool miniSuspended:!!(dialog.parent&&dialog.parent.Window.window&&(dialog.parent.Window.window.miniMode||dialog.parent.Window.window.miniSwitching))
    Binding {
        target:dialog;property:"modal";value:false
        when:dialog.miniSuspended
        restoreMode:Binding.RestoreBindingOrValue
    }
    // The overlay already disables input while mini mode is active. Binding
    // enabled here would save its inherited disabled value during suspension.
    popupType: Popup.Item
    padding: 18
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSize
    background: Rectangle { radius: 12; color: Theme.background; border.color: Theme.border }
    header: Label {
        text: dialog.title; visible: text.length > 0
        padding: 18; bottomPadding: 8
        font.family: dialog.font.family; font.pixelSize: 16; font.weight: Font.DemiBold
        color: Theme.text; wrapMode: Text.Wrap
        background: null
    }
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: settingsBridge.values.reduceMotion ? 0 : 180; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: .94; to: 1; duration: settingsBridge.values.reduceMotion ? 0 : 240; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; to: 0; duration: settingsBridge.values.reduceMotion ? 0 : 120; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; to: .97; duration: settingsBridge.values.reduceMotion ? 0 : 120; easing.type: Easing.InCubic }
        }
    }
    footer: DialogButtonBox {
        standardButtons: dialog.standardButtons
        visible: count > 0
        background: null
        padding: 18; topPadding: 8; spacing: 8
        delegate: ShellButton { objectName: "confirmationButton" + DialogButtonBox.buttonRole }
        // Dialog connects its DialogButtonBox footer automatically. Connecting
        // these signals again executes every confirmation twice.
    }
    // Measure the actual text/header/buttons. Hidden StackLayout pages must not
    // let a styled popup's background feed its height back into implicitHeight.
    implicitHeight: Math.max(100, topPadding + bottomPadding
                            + (contentItem ? contentItem.implicitHeight : 0)
                            + (header && header.visible ? header.implicitHeight + spacing : 0)
                            + (footer && footer.visible ? footer.implicitHeight + spacing : 0))
}
