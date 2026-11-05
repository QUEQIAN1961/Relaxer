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
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 9
            RowLayout {
                Layout.fillWidth: true; spacing: 18;visible:!Theme.compact
                UiText { text: "功能"; Layout.minimumWidth: 100; Layout.maximumWidth: 100; color: Theme.secondary; font.pixelSize: 14 }
                UiText { text: "应用内"; Layout.fillWidth: true; Layout.preferredWidth: 1; color: Theme.secondary; font.pixelSize: 14 }
                UiText { text: "全局"; Layout.fillWidth: true; Layout.preferredWidth: 1; color: Theme.secondary; font.pixelSize: 14 }
            }
            Repeater {
                model: settingsBridge.shortcutActions
                delegate: Item {
                    id: actionRow
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: content.width
                    Layout.preferredHeight:Theme.compact&&!modelData.appOnly?92:42
                    width:content.width
                    height:Theme.compact&&!modelData.appOnly?92:42
                    UiText { text: modelData.name; width: 100; height: 42; verticalAlignment: Text.AlignVCenter; color: Theme.text }
                    Repeater {
                        model: actionRow.modelData.appOnly?["app"]:["app", "global"]
                        delegate: Row {
                            id: binding
                            required property string modelData
                            required property int index
                            readonly property string scope: modelData
                            readonly property string action: actionRow.modelData.key
                            x:Theme.compact?112:118+index*(width+18)
                            y:Theme.compact?index*46:0
                            width:Theme.compact?Math.max(0,content.width-112):Math.max(0,(content.width-136)/2)
                            height:42;spacing:4
                            UiText { visible:Theme.compact;width:Theme.compact?48:0;height:42;verticalAlignment:Text.AlignVCenter;text:binding.scope==="app"?"应用内":"全局";color:Theme.secondary }
                            EntryField {
                                id: input
                                objectName: "shortcut-" + binding.scope + "-" + binding.action
                                width: Math.max(0, binding.width-clearBinding.width-4-(Theme.compact?52:0))
                                height: 42
                                horizontalAlignment: TextInput.AlignLeft
                                onTextChanged: cursorPosition = 0
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
                            ToolButton { id: clearBinding; width: 28; height: 42; text: "×"; Accessible.name: "清除此快捷键"; onClicked: page.setBinding(binding.scope, binding.action, "") }
                        }
                    }
                    UiText {
                        visible:actionRow.modelData.appOnly&&!Theme.compact
                        x:136+Math.max(0,(content.width-136)/2);height:42;verticalAlignment:Text.AlignVCenter
                        text:"仅应用内";color:Theme.secondary
                    }
                }
            }
        }
        GridLayout {
            Layout.fillWidth:true;columns:Theme.compact?1:2;columnSpacing:16;rowSpacing:12
            ColumnLayout {
                objectName:"shortcutsHelp";Layout.fillWidth:true;Layout.minimumWidth:0;spacing:8
                UiText { Layout.fillWidth:true;text:uiIntegration.shortcutStatus;wrapMode:Text.Wrap;color:Theme.secondary;font.pixelSize:14 }
                UiText { Layout.fillWidth:true;text:"录入后点击保存；输入文字时不触发应用内播放快捷键。";wrapMode:Text.Wrap;color:Theme.secondary;font.pixelSize:14 }
            }
            Flow {
                objectName:"shortcutsActions";Layout.fillWidth:Theme.compact
                Layout.preferredWidth:Theme.compact?1:globalToggle.width+resetActions.width+8
                Layout.alignment:Qt.AlignVCenter;spacing:8
                ToggleButton {
                    id:globalToggle;objectName:"globalShortcutsEnabled";text:"启用全局快捷键";checked:page.configuration.globalEnabled
                    onClicked:{const copy=JSON.parse(JSON.stringify(page.configuration));copy.globalEnabled=checked;page.configuration=copy;page.dirty=true}
                }
                Row {
                    id:resetActions;spacing:8
                    ShellButton {objectName:"shortcutsReset";text:"恢复默认";onClicked:{page.configuration=settingsBridge.defaultShortcuts();page.dirty=true}}
                    ShellButton {objectName:"shortcutsSave";text:"保存快捷键";onClicked:{if(uiIntegration.saveShortcuts(page.configuration))page.load()}}
                }
            }
        }
    }
}
