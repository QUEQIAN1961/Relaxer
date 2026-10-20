import QtQuick
import QtQuick.Controls
import "../theme"
Dialog {
    id: dialog
    popupType: Popup.Item
    padding: 18
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSize
    background: Rectangle { radius: 12; color: Theme.background; border.color: Theme.border }
    footer: DialogButtonBox {
        standardButtons: dialog.standardButtons
        visible: count > 0
        delegate: ShellButton {}
        onAccepted: dialog.accept()
        onRejected: dialog.reject()
    }
    // Measure the actual text/header/buttons. Hidden StackLayout pages must not
    // let a styled popup's background feed its height back into implicitHeight.
    implicitHeight: Math.max(100, topPadding + bottomPadding
                            + (contentItem ? contentItem.implicitHeight : 0)
                            + (header && header.visible ? header.implicitHeight + spacing : 0)
                            + (footer && footer.visible ? footer.implicitHeight + spacing : 0))
}
