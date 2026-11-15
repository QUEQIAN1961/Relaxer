import QtQuick
import QtQuick.Layouts
import "../theme"
ColumnLayout {
    property string heading
    property string description
    spacing: Theme.spacing
    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        width: 64; height: 64; radius: 20
        color: Theme.selected
        UiText { anchors.centerIn: parent; text: "♪"; basePixelSize: 32; color: Theme.accent }
    }
    UiText {
        Layout.fillWidth: true
        text: heading
        color: Theme.text
        basePixelSize: 20
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
    }
    UiText {
        Layout.fillWidth: true
        text: description
        color: Theme.secondary
        basePixelSize:15
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        lineHeight: 1.5
    }
}
