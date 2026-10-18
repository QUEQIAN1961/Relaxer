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
        Text { anchors.centerIn: parent; text: "♪"; font.pixelSize: 32; color: Theme.accent }
    }
    Text {
        Layout.fillWidth: true
        text: heading
        color: Theme.text
        font.pixelSize: 20
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
    }
    Text {
        Layout.fillWidth: true
        text: description
        color: Theme.secondary
        font.pixelSize: Theme.fontSize
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        lineHeight: 1.5
    }
}
