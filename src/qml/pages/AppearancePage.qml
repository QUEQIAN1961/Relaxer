import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"
import "../theme"

Item {
    id: page
    objectName: "appearancePage"
    signal backRequested()
    signal leaveRejected()
    property string imageKey: "sourceLeft"
    property string renameId: ""
    property string removeId: ""
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumWidth: 0
    Layout.minimumHeight: 0

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
        settingsBridge.chooseAppearanceColor(key, page.Window.window)
    }
    function chooseImage(key) { imageKey = key; imagePicker.open() }
    onVisibleChanged: if (!visible) settingsBridge.discardAppearance()

    ColumnLayout {
      anchors.fill: parent; anchors.leftMargin: 34; anchors.rightMargin: 34
      anchors.topMargin: 6; anchors.bottomMargin: 16; spacing: 18
      RowLayout {
        objectName: "appearanceToolbar"; Layout.fillWidth: true; spacing: 8
        ShellButton { objectName: "appearanceBack"; text: "返回设置"; font.pixelSize: 14; onClicked: page.requestBack() }
        UiText { Layout.fillWidth: true; Layout.minimumWidth: 0; text: "自定义外观"; color: Theme.text; font.pixelSize: 23; font.weight: Font.DemiBold; elide: Text.ElideRight }
        ShellButton { objectName: "appearanceNewSave"; text: "新建并保存"; font.pixelSize: 14; visible: settingsBridge.appearanceMode >= 3; onClicked: if (settingsBridge.saveAppearance(false)) page.backRequested() }
        ShellButton { objectName: "appearanceOverwriteSave"; text: "覆盖并保存"; font.pixelSize: 14; visible: settingsBridge.appearanceMode >= 3; enabled: settingsBridge.activeAppearanceId.length > 0; onClicked: if (settingsBridge.saveAppearance(true)) page.backRequested() }
        ShellButton { objectName: "appearanceSavePreset"; text: "保存并退出"; font.pixelSize: 14; visible: settingsBridge.appearanceMode < 3; onClicked: if (settingsBridge.saveAppearance(false)) page.backRequested() }
        ShellButton { objectName: "appearanceReset"; text: "恢复默认配色"; font.pixelSize: 14; onClicked: settingsBridge.resetAppearance() }
      }
      RowLayout {
        Layout.fillWidth: true; spacing: 18
        Repeater {
            model: ["浅色", "深色", "跟随系统", "调色盘", "图片"]
            delegate: ShellButton {
                required property string modelData
                required property int index
                objectName: "appearanceMode" + index
                Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.minimumWidth: 0
                text: modelData; primary: settingsBridge.appearanceMode === index
                onClicked: settingsBridge.setAppearanceMode(index)
            }
        }
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
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: [{key:"left",name:"左侧底色"},{key:"right",name:"右侧底色"},{key:"accent",name:"主题色"}]
                        delegate: ColorSwatch {
                            required property var modelData
                            objectName: "appearanceColor-" + modelData.key
                            caption: modelData.name; swatchColor: settingsBridge.appearanceDraft[modelData.key] || "#ffffff"
                            onClicked: page.chooseColor(modelData.key)
                        }
                    }
                }
                CheckBox { text: "自定义文字颜色"; checked: !!settingsBridge.appearanceDraft.customText; onClicked: settingsBridge.editAppearance("customText", checked) }
                RowLayout {
                    visible: !!settingsBridge.appearanceDraft.customText
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: [{key:"leftInk",name:"左侧文字"},{key:"rightInk",name:"右侧主要文字"},{key:"muted",name:"右侧小字"}]
                        delegate: ColorSwatch {
                            required property var modelData
                            objectName: "appearanceColor-" + modelData.key
                            caption: modelData.name; swatchColor: settingsBridge.appearanceDraft[modelData.key] || "#ffffff"
                            onClicked: page.chooseColor(modelData.key)
                        }
                    }
                }
                CheckBox {
                    objectName: "appearanceCustomProgressColors"
                    text: "自定义进度条颜色"
                    checked: !!settingsBridge.appearanceDraft.customProgressColors
                    onClicked: settingsBridge.editAppearance("customProgressColors", checked)
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    enabled: !!settingsBridge.appearanceDraft.customProgressColors
                    Repeater {
                        model: [{key:"progressFill",name:"已填充与滑块"},{key:"progressTrack",name:"未填充"}]
                        delegate: ColorSwatch {
                            required property var modelData
                            objectName: "appearanceColor-" + modelData.key
                            caption: modelData.name
                            swatchColor: settingsBridge.appearanceDraft[settingsBridge.appearanceDraft.customProgressColors ? modelData.key : modelData.key === "progressFill" ? "recommendedProgressFill" : "recommendedProgressTrack"] || "#ffffff"
                            onClicked: page.chooseColor(modelData.key)
                        }
                    }
                }
                UiText { Layout.fillWidth: true; text: "同时用于歌曲进度条和音量条；未勾选时使用推荐颜色。"; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 14 }
                CheckBox {
                    visible: settingsBridge.appearanceMode >= 3
                    objectName: "appearanceCustomFooter"
                    text: "自定义底栏颜色"
                    checked: !!settingsBridge.appearanceDraft.customFooter
                    onClicked: settingsBridge.editAppearance("customFooter", checked)
                }
                ColorSwatch {
                    objectName: "appearanceColor-footer"
                    visible: settingsBridge.appearanceMode >= 3
                    enabled: !!settingsBridge.appearanceDraft.customFooter
                    caption: "底栏颜色"; swatchColor: settingsBridge.appearanceDraft.footer || "#ffffff"
                    onClicked: page.chooseColor("footer")
                }
                CheckBox { text: "右侧小字加粗"; checked: !!settingsBridge.appearanceDraft.mutedBold; onClicked: settingsBridge.editAppearance("mutedBold", checked) }
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
                        text: "底栏与页面共用图片（连续铺图）"
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
                RowLayout {
                    Layout.fillWidth: true; spacing: 16
                    Repeater {
                        model: [{key:"blurLeft",name:"左侧列表模糊"},{key:"blurRight",name:"右侧列表模糊"},{key:"blurExpanded",name:"展开播放背景模糊"}]
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.minimumWidth: 0
                            visible: settingsBridge.appearanceMode === 4 || modelData.key === "blurExpanded"
                            UiText { Layout.fillWidth: true; text: modelData.name; color: Theme.secondary; wrapMode: Text.Wrap }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 6
                                Slider {
                                    Layout.fillWidth: true; Layout.minimumWidth: 20
                                    from: 0; to: 50; stepSize: 0.1
                                    value: settingsBridge.appearanceDraft[modelData.key] || 0
                                    onMoved: settingsBridge.editAppearance(modelData.key, value)
                                }
                                SpinBox {
                                    objectName: "appearanceBlur-" + modelData.key
                                    Layout.preferredWidth: 94; implicitHeight: 36
                                    from: 0; to: 500; editable: true
                                    value: Math.round(Number(settingsBridge.appearanceDraft[modelData.key] || 0) * 10)
                                    validator: DoubleValidator { bottom: 0; top: 50; decimals: 1 }
                                    textFromValue: function(value, locale) { return Number(value / 10).toLocaleString(locale, 'f', 1) }
                                    valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text) * 10) }
                                    onValueModified: settingsBridge.editAppearance(modelData.key, value / 10)
                                }
                            }
                        }
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
    } // editor layout
    FileDialog { id: imagePicker; title: "选择背景图片"; nameFilters: ["图片 (*.png *.jpg *.jpeg *.bmp *.webp)"]; onAccepted: settingsBridge.editAppearance(page.imageKey, selectedFile) }
    ConfirmDialog {
        id: unsaved
        objectName: "appearanceUnsavedDialog"
        parent: page.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(460, page.width - 20)
        modal: true
        title: "外观尚未保存"
        onRejected: page.leaveRejected()
        contentItem: Label { text: "修改尚未保存，是否退出？"; wrapMode: Text.Wrap }
        footer: DialogButtonBox {
            objectName: "appearanceUnsavedActions"
            alignment: Qt.AlignRight; spacing: 8; leftPadding: 18; rightPadding: 18; topPadding: 10; bottomPadding: 16
            ShellButton { objectName: "appearanceLeaveCancel"; text: "取消"; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: unsaved.reject() }
            ShellButton { objectName: "appearanceLeaveDiscard"; text: "放弃修改"; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: { settingsBridge.discardAppearance(); unsaved.close(); page.backRequested() } }
            ShellButton { objectName: "appearanceLeaveSave"; text: "保存"; primary: true; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: { if (settingsBridge.saveAppearance(false)) { unsaved.close(); page.backRequested() } } }
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
