import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"
import "../theme"
import "." as Pages

ColumnLayout {
    id: page
    objectName: "settingsPage"
    signal aiRequested()
    signal appearanceRequested()
    signal shortcutsRequested()
    signal versionLogRequested()
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumWidth: 0
    Layout.minimumHeight: 0
    spacing: 16

    UiText { text: "设置"; color: Theme.text; font.pixelSize: 30; font.weight: Font.DemiBold }
    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
    ScrollView {
        id: scroll
        objectName: "settingsScroll"
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 18
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: basic.implicitHeight + 40
                radius: 14
                color: Theme.card
                ColumnLayout {
                    id: basic
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
                    spacing: 16
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 18
                        rowSpacing: 8
                        UiText { text: "外观"; color: Theme.secondary; font.pixelSize: 14 }
                        UiText { text: "有效播放比例"; color: Theme.secondary; font.pixelSize: 14 }
                        UiText { text: "关闭窗口时"; color: Theme.secondary; font.pixelSize: 14 }
                        RowLayout {
                            Layout.fillWidth: true
                            ChoiceBox {
                                objectName: "qmlThemeSetting"
                                Layout.fillWidth: true
                                model: ["浅色", "深色", "跟随系统", "自定义"]
                                currentIndex: settingsBridge.values.theme
                                onActivated: {
                                    if (currentIndex === 3) {
                                        settingsBridge.beginAppearance()
                                        page.appearanceRequested()
                                    } else settingsBridge.setTheme(currentIndex)
                                }
                            }
                            ShellButton {
                                objectName: "editAppearance"
                                text: "编辑"
                                implicitWidth: 60
                                onClicked: { settingsBridge.beginAppearance(); page.appearanceRequested() }
                            }
                        }
                        SpinBox {
                            objectName: "qmlThresholdSetting"
                            Layout.fillWidth: true
                            from: 70; to: 100
                            value: settingsBridge.values.threshold
                            editable: true
                            textFromValue: function(value) { return value + " %" }
                            valueFromText: function(text) { return Number(text.replace("%", "").trim()) }
                            onValueModified: settingsBridge.setPreference("threshold", value)
                            HintBubble {
                                visible: parent.hovered
                                text: "整轮没有手动跳转，实际播放达到指定比例才计一次。暂停不累计，比例从下一轮生效。"
                            }
                        }
                        ChoiceBox {
                            objectName: "closeSetting"
                            Layout.fillWidth: true
                            model: ["退出播放器", "最小化到系统托盘"]
                            currentIndex: settingsBridge.values.closeToTray ? 1 : 0
                            onActivated: settingsBridge.setPreference("closeToTray", currentIndex === 1)
                        }
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        UiText { text: "歌词颜色"; height: 42; verticalAlignment: Text.AlignVCenter; color: Theme.secondary }
                        ShellButton {
                            objectName: "lyricColorSetting"
                            text: "选择颜色"
                            onClicked: { lyricColor.selectedColor = settingsBridge.values.lyricColor || Theme.text; lyricColor.open() }
                        }
                        ShellButton { objectName: "lyricColorReset"; text: "跟随文字颜色"; onClicked: settingsBridge.setPreference("lyricColor", "") }
                        ShellButton { objectName: "uiFontSetting"; text: "字体"; onClicked: fontDialog.openSettings() }
                        CheckBox {
                            objectName: "playerCoverBackground"
                            text: "展开播放时使用封面背景"
                            checked: settingsBridge.values.playerCoverBackground
                            onClicked: settingsBridge.setPreference("playerCoverBackground", checked)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        UiText {
                            Layout.fillWidth: true
                            text: settingsBridge.dataRoot
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            color: Theme.secondary
                            font.pixelSize: 14
                        }
                        ShellButton { objectName: "openDataRoot"; text: "打开音乐资料目录"; onClicked: settingsBridge.openDataRoot() }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: aiCard.implicitHeight + 36
                radius: 14
                color: Theme.card
                ColumnLayout {
                    id: aiCard
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 18 }
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        UiText { Layout.fillWidth: true; text: "AI 服务"; font.pixelSize: 22; font.weight: Font.DemiBold; color: Theme.text }
                        ShellButton { objectName: "openAiServiceSettings"; text: "管理 AI 服务"; onClicked: page.aiRequested() }
                    }
                    UiText { Layout.fillWidth: true; text: "配置 DeepSeek 凭据、模型和歌词翻译。"; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 14 }
                }
            }
            ShortcutsPage { Layout.fillWidth: true }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: about.implicitHeight + 40
                radius: 14
                color: Theme.card
                ColumnLayout {
                    id: about
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
                    spacing: 8
                    RowLayout{Layout.fillWidth:true;UiText { text: appBridge.version; color: Theme.text; font.pixelSize: 22; font.weight: Font.DemiBold;Layout.fillWidth:true } ShellButton{text:"版本日志";flat:true;onClicked:page.versionLogRequested()}}
                    UiText { text: "为本地音乐留一个安静的位置。"; color: Theme.secondary; font.pixelSize: 14 }
                    UiText { text: "本地音乐播放器 · 支持联网查找歌词"; color: Theme.secondary; font.pixelSize: 14 }
                    CheckBox {
                        objectName: "reduceMotionSetting"
                        text: "减少动画"
                        checked: settingsBridge.values.reduceMotion
                        onClicked: settingsBridge.setPreference("reduceMotion", checked)
                    }
                }
            }
            UiText { Layout.fillWidth: true; text: settingsBridge.status; wrapMode: Text.Wrap; color: Theme.accent; textFormat: Text.PlainText }
            Item { Layout.preferredHeight: 12 }
        }
    }
    ColorDialog {
        id: lyricColor
        title: "歌词颜色"
        options: ColorDialog.DontUseNativeDialog
        onAccepted: settingsBridge.setPreference("lyricColor", selectedColor.toString())
    }
    Pages.FontDialog { id: fontDialog }
}
