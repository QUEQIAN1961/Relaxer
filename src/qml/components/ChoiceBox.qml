import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQuick.Window
import "../theme"
Controls.ComboBox {
    id:control
    implicitHeight: Theme.controlHeight
    Layout.minimumHeight: implicitHeight
    Layout.preferredHeight: implicitHeight
    Layout.maximumHeight: implicitHeight
    Layout.alignment: Qt.AlignVCenter
    leftPadding:12;rightPadding:32
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSize
    font.weight: Font.Medium
    contentItem:UiText {
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
    delegate: Controls.ItemDelegate {
        id: choiceDelegate
        required property var model
        required property int index
        width: ListView.view ? ListView.view.width : control.width - 12
        height: 36
        text: control.textRole ? String(model[control.textRole]) : String(model.modelData)
        font: control.font
        highlighted: control.highlightedIndex === index
        hoverEnabled: true
        contentItem: UiText {
            text: choiceDelegate.text
            font: choiceDelegate.font
            color: choiceDelegate.enabled ? Theme.text : Theme.disabled
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle { radius: 6; color: choiceDelegate.highlighted || choiceDelegate.hovered ? Theme.selected : "transparent" }
    }
    popup: Controls.Popup {
        id: choices
        objectName: control.objectName + "Popup"
        popupType: Controls.Popup.Item
        y: control.height + 4
        width: control.width
        height: Math.min(choiceList.contentHeight + topPadding + bottomPadding, control.Window.height - 20)
        margins: 8
        padding: 6
        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutsideParent
        contentItem: ListView {
            id: choiceList
            clip: true
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
            Controls.ScrollIndicator.vertical: Controls.ScrollIndicator {}
        }
        background: Rectangle { radius: Theme.radius; color: Theme.background; border.color: Theme.border }
    }
}
