import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
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
        contentHeight: settingsContent.implicitHeight + 16
        clip: true
        function stopAtEdges() { if (contentItem && contentItem.boundsBehavior !== undefined) { contentItem.boundsBehavior = Flickable.StopAtBounds; contentItem.boundsMovement = Flickable.StopAtBounds } }
        Component.onCompleted: Qt.callLater(stopAtEdges)
        onContentItemChanged: Qt.callLater(stopAtEdges)
        SmoothWheelHandler { flickable: scroll.contentItem as Flickable }
        ColumnLayout {
            id: settingsContent
            width: scroll.availableWidth
            height: implicitHeight
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
                    Flow {
                        id:basicGrid;objectName:"settingsBasicGrid"
                        Layout.fillWidth: true
                        spacing: 12
                        RowLayout {
                            objectName:"settingsAppearanceCell";width:Theme.compact?basicGrid.width:Math.min(286,basicGrid.width);spacing:8
                            UiText { text:"外观";color:Theme.secondary;font.pixelSize:14 }
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
                        RowLayout {
                            objectName:"settingsThresholdCell";width:Theme.compact?basicGrid.width:Math.min(248,basicGrid.width);spacing:8
                            UiText { text:"有效播放比例";color:Theme.secondary;font.pixelSize:14 }
                            SpinBox {
                                id:threshold;objectName: "qmlThresholdSetting"
                                Layout.fillWidth: true; Layout.preferredWidth:1; Layout.minimumWidth:0
                                implicitHeight:Theme.controlHeight;leftPadding:30;rightPadding:30
                                from: 70; to: 100
                                value: settingsBridge.values.threshold
                                editable: true
                                textFromValue: function(value) { return value + " %" }
                                valueFromText: function(text) { return Number(text.replace("%", "").trim()) }
                                onValueModified: settingsBridge.setPreference("threshold", value)
                                background:Rectangle {objectName:"thresholdFrame";radius:8;color:Theme.card;border.color:threshold.activeFocus?Theme.accent:Theme.border}
                                contentItem:TextInput {
                                    text:threshold.textFromValue(threshold.value,threshold.locale);font:threshold.font;color:Theme.text
                                    horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter
                                    readOnly:!threshold.editable;validator:threshold.validator;selectByMouse:true
                                    inputMethodHints:Qt.ImhDigitsOnly
                                }
                                up.indicator:Rectangle {
                                    objectName:"thresholdIncrease"
                                    x:threshold.width-width;width:30;height:threshold.height;radius:8;color:threshold.up.pressed?Theme.pressed:threshold.up.hovered?Theme.hover:"transparent"
                                    UiText {anchors.centerIn:parent;text:"+";font.pixelSize:20;color:Theme.text}
                                }
                                down.indicator:Rectangle {
                                    objectName:"thresholdDecrease"
                                    width:30;height:threshold.height;radius:8;color:threshold.down.pressed?Theme.pressed:threshold.down.hovered?Theme.hover:"transparent"
                                    UiText {anchors.centerIn:parent;text:"−";font.pixelSize:20;color:Theme.text}
                                }
                                HintBubble {
                                    visible: parent.hovered
                                    text: "整轮没有手动跳转，实际播放达到指定比例才计一次。暂停不累计，比例从下一轮生效。"
                                }
                            }
                        }
                        RowLayout {
                            objectName:"settingsCloseCell";width:Theme.compact?basicGrid.width:Math.min(280,basicGrid.width);spacing:8
                            UiText { text:"关闭窗口时";color:Theme.secondary;font.pixelSize:14 }
                            ChoiceBox {
                                objectName: "closeSetting"
                                Layout.fillWidth: true; Layout.preferredWidth:1; Layout.minimumWidth:0
                                model: ["退出播放器", "最小化到系统托盘"]
                                currentIndex: settingsBridge.values.closeToTray ? 1 : 0
                                onActivated: settingsBridge.setPreference("closeToTray", currentIndex === 1)
                            }
                        }
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        UiText {text:"歌词颜色";height:36;verticalAlignment:Text.AlignVCenter;color:Theme.secondary}
                        ShellButton {
                            objectName:"lyricColorSetting";text:"编辑颜色"
                            onClicked:lyricColorEditor.open()
                        }
                        ShellButton { objectName: "uiFontSetting"; text: "字体"; onClicked: fontDialog.openSettings() }
                        ToggleButton {
                            objectName: "playerCoverBackground"
                            text: "展开播放时使用封面背景"
                            checked: settingsBridge.values.playerCoverBackground
                            onClicked: settingsBridge.setPreference("playerCoverBackground", checked)
                        }
                        ToggleButton {
                                objectName: "reduceMotionSetting"
                                text: "减少动画"
                                checked: settingsBridge.values.reduceMotion
                                onClicked: settingsBridge.setPreference("reduceMotion", checked)
                            }
                            ToggleButton {
                                objectName: "mutedBoldSetting"
                                text: "右侧小字加粗"
                                checked: settingsBridge.values.mutedBold
                                onClicked: settingsBridge.setPreference("mutedBold", checked)
                            }
                        ToggleButton {objectName:"showFavoritesSetting";text:"展示红心";checked:settingsBridge.values.showFavorites;onClicked:settingsBridge.setPreference("showFavorites",checked)}
                    }
                    GridLayout {
                        objectName:"dataRootRow";Layout.fillWidth:true;columnSpacing:12;rowSpacing:8
                        columns:2
                        UiText {
                            id:directoryLabel;objectName:"dataRootText";Layout.fillWidth:true;Layout.minimumWidth:0
                            text: settingsBridge.dataRoot
                            textFormat: Text.PlainText
                            elide:Text.ElideMiddle
                            color: Theme.secondary
                            font.pixelSize: 14
                        }
                        ShellButton {id:openDirectory;objectName:"openDataRoot";Layout.alignment:Qt.AlignRight;text:"打开本地音乐目录";onClicked:settingsBridge.openDataRoot()}
                        UiText {
                            objectName:"downloadDirectoryText";Layout.fillWidth:true;Layout.minimumWidth:0
                            text:onlineDownloads.downloadDirectory;textFormat:Text.PlainText;elide:Text.ElideMiddle;color:Theme.secondary;font.pixelSize:14
                            HoverHandler {id:downloadDirectoryHover}
                            HintBubble {visible:downloadDirectoryHover.hovered;text:onlineDownloads.downloadDirectory}
                        }
                        ShellButton {objectName:"openDownloadDirectory";Layout.alignment:Qt.AlignRight;text:"打开默认下载目录";onClicked:onlineDownloads.openDownloadDirectory()}
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
                    UiText { Layout.fillWidth: true; text: "配置 DeepSeek、KIMI、Qwen 凭据、模型和歌词翻译。"; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 14 }
                }
            }
            ShortcutsPage { Layout.fillWidth: true }
            Rectangle {
                objectName: "settingsAboutCard"
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
                }
            }
            UiText { Layout.fillWidth: true; visible:text.length>0; text: settingsBridge.status; wrapMode: Text.Wrap; color: Theme.accent; textFormat: Text.PlainText }
        }
    }
    LyricColorDialog {id:lyricColorEditor;parent:page.Overlay.overlay}
    Pages.FontDialog { id: fontDialog }
}
