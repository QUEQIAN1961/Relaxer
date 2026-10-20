import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"

Rectangle {
    id: page
    objectName: "shortcutsPage"
    property var configuration: ({ app: {}, global: {}, globalEnabled: false })
    property bool dirty: false
    implicitHeight: content.implicitHeight + 40
    radius: 14
    color: Theme.card

    function load() {
        configuration = JSON.parse(JSON.stringify(settingsBridge.values.shortcuts))
        dirty = false
    }
    function setBinding(scope, action, text) {
        const copy = JSON.parse(JSON.stringify(configuration))
        copy[scope][action] = text
        configuration = copy
        dirty = true
    }
    Component.onCompleted: load()
    Connections { target: settingsBridge; function onSettingsChanged() { if (!page.dirty) page.load() } }
    onVisibleChanged: if (!visible) uiIntegration.setShortcutRecording(false)
    ColumnLayout {
        id: content
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
        spacing: 12
        UiText { text: "快捷键"; color: Theme.text; font.pixelSize: 22; font.weight: Font.DemiBold }
        UiText {
            Layout.fillWidth: true
            text: "应用内：播放器窗口激活时生效。全局：切换应用或最小化后也能使用。音量每次调整 5%。"
            wrapMode: Text.Wrap
            color: Theme.secondary
            font.pixelSize: 14
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: 18
            rowSpacing: 9
            UiText { text: "功能"; color: Theme.secondary; font.pixelSize: 14 }
            UiText { text: "应用内"; color: Theme.secondary; font.pixelSize: 14 }
            UiText { text: "全局"; color: Theme.secondary; font.pixelSize: 14 }
            Repeater {
                model: settingsBridge.shortcutActions
                delegate: RowLayout {
                    id: actionRow
                    required property var modelData
                    Layout.columnSpan: 3
                    Layout.fillWidth: true
                    spacing: 18
                    UiText { text: modelData.name; Layout.preferredWidth: 100; color: Theme.text }
                    Repeater {
                        model: ["app", "global"]
                        delegate: RowLayout {
                            id: binding
                            required property string modelData
                            readonly property string scope: modelData
                            readonly property string action: actionRow.modelData.key
                            Layout.fillWidth: true
                            Layout.preferredWidth: Math.max(160,(page.width-180)/2)
                            spacing: 4
                            EntryField {
                                id: input
                                objectName: "shortcut-" + binding.scope + "-" + binding.action
                                Layout.fillWidth: true
                                Layout.minimumWidth: 100
                                Layout.preferredWidth: 180
                                readOnly: true
                                placeholderText: "点击后按组合键"
                                text: page.configuration[binding.scope][binding.action] || ""
                                onActiveFocusChanged: uiIntegration.setShortcutRecording(activeFocus)
                                Keys.onPressed: function(event) {
                                    if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab) return
                                    event.accepted = true
                                    if (event.isAutoRepeat || event.key === Qt.Key_Control || event.key === Qt.Key_Shift || event.key === Qt.Key_Alt || event.key === Qt.Key_Meta || event.key === Qt.Key_unknown) return
                                    page.setBinding(binding.scope, binding.action, settingsBridge.shortcutText(event.key, event.modifiers))
                                }
                            }
                            ToolButton { text: "×"; Accessible.name: "清除此快捷键"; onClicked: page.setBinding(binding.scope, binding.action, "") }
                        }
                    }
                }
            }
        }
        UiText { Layout.fillWidth: true; text: uiIntegration.shortcutStatus; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 14 }
        UiText { Layout.fillWidth: true; text: "录入后点击保存；输入文字时不触发应用内播放快捷键。"; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 14 }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            CheckBox {
                objectName: "globalShortcutsEnabled"
                text: "启用全局快捷键"
                checked: page.configuration.globalEnabled
                onClicked: { const copy = JSON.parse(JSON.stringify(page.configuration)); copy.globalEnabled = checked; page.configuration = copy; page.dirty = true }
            }
            ShellButton { objectName: "shortcutsReset"; text: "恢复默认"; onClicked: { page.configuration = settingsBridge.defaultShortcuts(); page.dirty = true } }
            ShellButton { objectName: "shortcutsSave"; text: "保存快捷键"; onClicked: { if (uiIntegration.saveShortcuts(page.configuration)) page.load() } }
        }
    }
}
