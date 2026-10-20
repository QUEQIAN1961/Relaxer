import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"
import "../theme"

ColumnLayout {
    id: page
    objectName: "appearancePage"
    signal backRequested()
    signal leaveRejected()
    property string colorKey: "left"
    property string imageKey: "sourceLeft"
    property string renameId: ""
    property string removeId: ""
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumWidth: 0
    Layout.minimumHeight: 0
    spacing: 16

    function requestBack() {
        if (settingsBridge.appearanceDirty) unsaved.open()
        else { settingsBridge.discardAppearance(); backRequested() }
    }
    function confirmLeave() {
        if (settingsBridge.appearanceDirty) { unsaved.open(); return false }
        settingsBridge.discardAppearance()
        return true
    }
    function chooseColor(key) {
        colorKey = key
        colorPicker.selectedColor = settingsBridge.appearanceDraft[key] || "#ffffff"
        colorPicker.open()
    }
    function chooseImage(key) { imageKey = key; imagePicker.open() }
    onVisibleChanged: if (!visible) settingsBridge.discardAppearance()

    RowLayout {
        Layout.fillWidth: true
        ShellButton { objectName: "appearanceBack"; text: "返回设置"; onClicked: page.requestBack() }
        UiText { Layout.fillWidth: true; text: "自定义外观"; color: Theme.text; font.pixelSize: 25; font.weight: Font.DemiBold }
    }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        Repeater {
            model: ["浅色", "深色", "跟随系统", "调色盘", "图片"]
            delegate: ShellButton {
                required property string modelData
                required property int index
                objectName: "appearanceMode" + index
                text: modelData
                primary: settingsBridge.appearanceMode === index
                onClicked: settingsBridge.setAppearanceMode(index)
            }
        }
    }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        ShellButton {
            objectName: "appearanceNewSave"
            text: "新建并保存"
            visible: settingsBridge.appearanceMode >= 3
            onClicked: if (settingsBridge.saveAppearance(false)) page.backRequested()
        }
        ShellButton {
            objectName: "appearanceOverwriteSave"
            text: "覆盖并保存"
            visible: settingsBridge.appearanceMode >= 3
            enabled: settingsBridge.activeAppearanceId.length > 0
            onClicked: if (settingsBridge.saveAppearance(true)) page.backRequested()
        }
        ShellButton {
            objectName: "appearanceSavePreset"
            text: "保存并退出"
            visible: settingsBridge.appearanceMode < 3
            onClicked: if (settingsBridge.saveAppearance(false)) page.backRequested()
        }
        ShellButton { text: "恢复默认配色"; onClicked: settingsBridge.resetAppearance() }
    }
    ScrollView {
        id: scroll
        objectName: "appearanceScroll"
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 16
            ColumnLayout {
                visible: settingsBridge.appearanceMode >= 3
                Layout.fillWidth: true
                spacing: 14
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: [{key:"left",name:"左侧底色"},{key:"right",name:"右侧底色"},{key:"accent",name:"主题色"}]
                        delegate: ShellButton {
                            required property var modelData
                            objectName: "appearanceColor-" + modelData.key
                            text: modelData.name + "  " + settingsBridge.appearanceDraft[modelData.key]
                            onClicked: page.chooseColor(modelData.key)
                        }
                    }
                }
                CheckBox { text: "自定义文字颜色"; checked: !!settingsBridge.appearanceDraft.customText; onClicked: settingsBridge.editAppearance("customText", checked) }
                Flow {
                    visible: !!settingsBridge.appearanceDraft.customText
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: [{key:"leftInk",name:"左侧文字"},{key:"rightInk",name:"右侧主要文字"},{key:"muted",name:"右侧小字"}]
                        delegate: ShellButton {
                            required property var modelData
                            objectName: "appearanceColor-" + modelData.key
                            text: modelData.name + "  " + settingsBridge.appearanceDraft[modelData.key]
                            onClicked: page.chooseColor(modelData.key)
                        }
                    }
                }
                CheckBox { text: "右侧小字加粗"; checked: !!settingsBridge.appearanceDraft.mutedBold; onClicked: settingsBridge.editAppearance("mutedBold", checked) }
                CheckBox {
                    objectName: "appearanceCustomProgressColors"
                    text: "自定义进度条颜色"
                    checked: !!settingsBridge.appearanceDraft.customProgressColors
                    onClicked: settingsBridge.editAppearance("customProgressColors", checked)
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    enabled: !!settingsBridge.appearanceDraft.customProgressColors
                    Repeater {
                        model: [{key:"progressFill",name:"已填充与滑块"},{key:"progressTrack",name:"未填充"}]
                        delegate: ShellButton {
                            required property var modelData
                            text: modelData.name + "  " + settingsBridge.appearanceDraft[modelData.key]
                            onClicked: page.chooseColor(modelData.key)
                        }
                    }
                }
                UiText { Layout.fillWidth: true; text: "同时用于歌曲进度条和音量条；未勾选时使用推荐颜色。"; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 14 }
                CheckBox {
                    visible: settingsBridge.appearanceMode === 3
                    objectName: "appearanceCustomFooter"
                    text: "自定义底栏颜色"
                    checked: !!settingsBridge.appearanceDraft.customFooter
                    onClicked: settingsBridge.editAppearance("customFooter", checked)
                }
                ShellButton {
                    visible: settingsBridge.appearanceMode === 3
                    enabled: !!settingsBridge.appearanceDraft.customFooter
                    text: "底栏颜色  " + settingsBridge.appearanceDraft.footer
                    onClicked: page.chooseColor("footer")
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: settingsBridge.appearanceMode === 4
                    spacing: 12
                    CheckBox {
                        objectName: "appearanceShareLR"
                        text: "左右同一张（连续铺图）"
                        checked: !!settingsBridge.appearanceDraft.shareLR
                        onClicked: settingsBridge.editAppearance("shareLR", checked)
                    }
                    Repeater {
                        model: [{key:"sourceLeft",name:"左侧图片"},{key:"sourceRight",name:"右侧图片"}]
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            visible: modelData.key !== "sourceRight" || !settingsBridge.appearanceDraft.shareLR
                            UiText { text: modelData.name; Layout.preferredWidth: 72; color: Theme.secondary }
                            EntryField { Layout.fillWidth: true; readOnly: true; selectByMouse: true; text: settingsBridge.appearanceDraft[modelData.key] || ""; placeholderText: "请选择背景图片" }
                            ShellButton { text: "选择图片"; onClicked: page.chooseImage(modelData.key) }
                        }
                    }
                    CheckBox {
                        objectName: "appearanceShareFooter"
                        text: "底栏与页面共用图片"
                        checked: !!settingsBridge.appearanceDraft.shareFooter
                        onClicked: settingsBridge.editAppearance("shareFooter", checked)
                    }
                    RowLayout {
                        visible: !settingsBridge.appearanceDraft.shareFooter
                        Layout.fillWidth: true
                        UiText { text: "底栏图片"; Layout.preferredWidth: 72; color: Theme.secondary }
                        EntryField { Layout.fillWidth: true; readOnly: true; selectByMouse: true; text: settingsBridge.appearanceDraft.sourceFooter || "" }
                        ShellButton { text: "选择图片"; onClicked: page.chooseImage("sourceFooter") }
                    }
                }
                Repeater {
                    model: [{key:"blurLeft",name:"左侧模糊"},{key:"blurRight",name:"右侧模糊"},{key:"blurExpanded",name:"展开播放背景模糊"}]
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        visible: settingsBridge.appearanceMode === 4 || modelData.key === "blurExpanded"
                        UiText { text: modelData.name; Layout.preferredWidth: 140; color: Theme.secondary }
                        Slider {
                            Layout.fillWidth: true
                            from: 0; to: 50; stepSize: 0.1
                            value: settingsBridge.appearanceDraft[modelData.key] || 0
                            onMoved: settingsBridge.editAppearance(modelData.key, value)
                        }
                        UiText { text: Number(settingsBridge.appearanceDraft[modelData.key] || 0).toFixed(1); Layout.preferredWidth: 35; color: Theme.text }
                    }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
            UiText { text: "已保存的外观"; color: Theme.text; font.pixelSize: 20; font.weight: Font.DemiBold }
            Repeater {
                model: settingsBridge.appearances
                delegate: Rectangle {
                    id: history
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: historyBody.implicitHeight + 28
                    color: Theme.card
                    radius: 10
                    border.color: history.modelData.active ? Theme.accent : Theme.border
                    ColumnLayout {
                        id: historyBody
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            UiText {
                                Layout.fillWidth: true
                                text: history.modelData.name + (history.modelData.mode === "image" ? " · 图片" : " · 调色盘") + (history.modelData.active ? " · 正在使用" : "")
                                elide: Text.ElideRight
                                color: Theme.text
                                font.weight: Font.DemiBold
                            }
                            Image {
                                visible: history.modelData.mode === "image"
                                source: history.modelData.leftImageUrl || ""
                                sourceSize: Qt.size(128, 84)
                                Layout.preferredWidth: 64
                                Layout.preferredHeight: 42
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }
                            Repeater {
                                model: ["left", "right", "accent"]
                                delegate: Rectangle {
                                    required property string modelData
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 34
                                    color: history.modelData[modelData] || "#ffffff"
                                    border.color: Theme.border
                                    radius: 4
                                }
                            }
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            ShellButton { text: "使用"; onClicked: if (settingsBridge.useAppearance(history.modelData.id)) page.backRequested() }
                            ShellButton { text: "重命名"; onClicked: { page.renameId = history.modelData.id; renameField.text = history.modelData.name; renameDialog.open() } }
                            ShellButton { text: "删除"; onClicked: { page.removeId = history.modelData.id; removeDialog.open() } }
                        }
                    }
                }
            }
            UiText { Layout.fillWidth: true; text: settingsBridge.status; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.accent }
            Item { Layout.preferredHeight: 16 }
        }
    }
    ColorDialog { id: colorPicker; title: "选择颜色"; options: ColorDialog.DontUseNativeDialog; onAccepted: settingsBridge.editAppearance(page.colorKey, selectedColor.toString()) }
    FileDialog { id: imagePicker; title: "选择背景图片"; nameFilters: ["图片 (*.png *.jpg *.jpeg *.bmp *.webp)"]; onAccepted: settingsBridge.editAppearance(page.imageKey, selectedFile) }
    ConfirmDialog {
        id: unsaved
        parent: page.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(460, page.width - 20)
        modal: true
        title: "外观尚未保存"
        onRejected: page.leaveRejected()
        contentItem: Label { text: "修改尚未保存，是否退出？"; wrapMode: Text.Wrap }
        footer: RowLayout {
            spacing: 8
            ShellButton { text: "取消"; onClicked: unsaved.reject() }
            ShellButton { text: "放弃修改"; onClicked: { settingsBridge.discardAppearance(); unsaved.close(); page.backRequested() } }
            ShellButton { text: "保存"; primary: true; onClicked: { if (settingsBridge.saveAppearance(false)) { unsaved.close(); page.backRequested() } } }
        }
    }
    ConfirmDialog {
        id: renameDialog
        parent: page.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, page.width - 20)
        modal: true
        title: "重命名外观"
        contentItem: EntryField { id: renameField; selectByMouse: true }
        footer: RowLayout {
            spacing: 8
            ShellButton { text: "取消"; onClicked: renameDialog.close() }
            ShellButton { text: "保存"; primary: true; onClicked: if (settingsBridge.renameAppearance(page.renameId, renameField.text)) renameDialog.close() }
        }
    }
    ConfirmDialog {
        id: removeDialog
        parent: page.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, page.width - 20)
        modal: true
        title: "删除保存的外观？"
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "只删除外观记录，保留原始图片文件。"; wrapMode: Text.Wrap }
        onAccepted: settingsBridge.removeAppearance(page.removeId)
    }
}
