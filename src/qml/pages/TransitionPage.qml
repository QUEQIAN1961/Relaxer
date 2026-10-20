import QtQuick
import QtQuick.Layouts
import "../components"
import "../theme"
ColumnLayout {
    required property string heading
    required property string description
    spacing: 24
    UiText { text: heading; font.pixelSize: Theme.headingSize; font.weight: Font.DemiBold; color: Theme.text }
    Rectangle {
        Layout.fillWidth: true; Layout.fillHeight: true
        color: Theme.panel; radius: Theme.radius; border.color: Theme.border
        EmptyState { anchors.centerIn: parent; width: Math.min(parent.width - 64, 520); heading: "迁移准备中"; description: parent.parent.description }
    }
}
