import QtQuick
import "../theme"
Item {
    id: image
    property string name: ""
    property color color: Theme.text
    implicitWidth: 22
    implicitHeight: 22
    Image {
        anchors.fill: parent
        source: image.name.length ? "image://relaxer/icon/" + image.name + "?color=" + encodeURIComponent(String(image.color)) : ""
        sourceSize.width: 64
        sourceSize.height: 64
        fillMode: Image.PreserveAspectFit
        smooth: true
    }
}
