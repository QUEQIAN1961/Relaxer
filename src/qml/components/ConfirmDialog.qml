import QtQuick
import QtQuick.Controls
Dialog {
    // Measure the actual text/header/buttons. Hidden StackLayout pages must not
    // let a styled popup's background feed its height back into implicitHeight.
    implicitHeight: Math.max(100, topPadding + bottomPadding
                            + (contentItem ? contentItem.implicitHeight : 0)
                            + (header && header.visible ? header.implicitHeight + spacing : 0)
                            + (footer && footer.visible ? footer.implicitHeight + spacing : 0))
}
