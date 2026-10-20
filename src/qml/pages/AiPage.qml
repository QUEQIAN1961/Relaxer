import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"

ColumnLayout {
    id: page
    objectName: "aiPage"
    property string subpage: "home"
    signal backRequested()
    Layout.fillWidth: true
    spacing: 18
    enabled: aiBridge.available

    function saveField(name, value) {
        if (!aiBridge.edit(name, value) || !aiBridge.save())
            aiBridge.resetDraft()
    }
    function flushStyle() {
        styleSave.stop()
        if (style.dirty) {
            aiBridge.edit("translationStylePrompt", style.text)
            if (aiBridge.save()) style.dirty = false
        }
    }
    function openHome() { flushStyle(); subpage = "home" }
    function openTranslation() { subpage = "translation" }
    function leavePage() { flushStyle(); aiBridge.cancelModels(); backRequested() }
    function effortValue(translation) {
        const enabled = translation ? aiBridge.draft.translationThinkingEnabled : aiBridge.draft.thinkingEnabled
        const effort = translation ? aiBridge.draft.translationReasoningEffort : aiBridge.draft.reasoningEffort
        const choices = translation ? aiBridge.translationEfforts : aiBridge.defaultEfforts
        return enabled ? (choices.indexOf(effort) >= 0 ? effort : "invalid") : "off"
    }
    onVisibleChanged: if (!visible) { flushStyle(); aiBridge.cancelModels() }
    Component.onDestruction: { flushStyle(); aiBridge.cancelModels() }

    RowLayout {
        Layout.fillWidth: true
        UiText {
            Layout.fillWidth: true
            text: page.subpage === "translation" ? "歌词翻译" : "AI 服务"
            color: Theme.text
            font.pixelSize: 25
            font.weight: Font.DemiBold
        }
        ShellButton {
            objectName: "aiSettingsBack"
            text: page.subpage === "translation" ? "返回 AI 服务" : "返回设置"
            onClicked: page.subpage === "translation" ? page.openHome() : page.leavePage()
        }
    }

    ColumnLayout {
        visible: page.subpage === "home"
        Layout.fillWidth: true
        spacing: 16

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: providerContent.implicitHeight + 36
            radius: 10
            color: Theme.panel
            border.color: Theme.border
            ColumnLayout {
                id: providerContent
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 18 }
                spacing: 12
                UiText { text: "供应商管理"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
                RowLayout {
                    Layout.fillWidth: true
                    UiText { text: "供应商"; Layout.preferredWidth: 74; color: Theme.secondary }
                    ChoiceBox {
                        objectName: "aiProviderSelection"
                        Layout.fillWidth: true
                        model: ["请选择供应商", "DeepSeek 官方 API"]
                        currentIndex: aiBridge.draft.providerId === "deepseek" ? 1 : 0
                        onActivated: page.saveField("providerId", currentIndex === 1 ? "deepseek" : "")
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    UiText { text: "API Key"; Layout.preferredWidth: 74; color: Theme.secondary }
                    EntryField {
                        id: key
                        objectName: "apiKeyInput"
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: "在 Relaxer 中输入 API Key"
                        selectByMouse: true
                        enabled: aiBridge.draft.providerId === "deepseek"
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    ShellButton {
                        objectName: "aiCredentialSave"
                        text: "保存 Key"
                        enabled: aiBridge.draft.providerId === "deepseek" && key.text.length > 0
                        onClicked: { aiBridge.storeKey(key.text); key.clear() }
                    }
                    ShellButton {
                        objectName: "aiCredentialDelete"
                        text: "删除 Key"
                        enabled: aiBridge.draft.providerId === "deepseek"
                        onClicked: removeConsent.open()
                    }
                    ShellButton { text: "检查本地凭据"; enabled: aiBridge.draft.providerId === "deepseek"; onClicked: aiBridge.checkCredential() }
                }
                UiText {
                    objectName: "aiCredentialStatus"
                    Layout.fillWidth: true
                    text: aiBridge.credentialStatus
                    wrapMode: Text.Wrap
                    color: Theme.secondary
                    font.pixelSize: 14
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    ShellButton {
                        objectName: "aiProviderVerify"
                        text: aiBridge.busy ? "正在连接…" : "验证连接"
                        enabled: !aiBridge.busy && aiBridge.draft.providerId === "deepseek"
                        onClicked: aiBridge.refreshModels(true)
                    }
                    ShellButton {
                        objectName: "modelRefreshButton"
                        text: aiBridge.busy ? "正在刷新…" : "刷新模型"
                        enabled: !aiBridge.busy && aiBridge.draft.providerId === "deepseek"
                        onClicked: aiBridge.refreshModels(true)
                    }
                }
                UiText {
                    Layout.fillWidth: true
                    text: "连接验证只读取模型列表，不会请求翻译。"
                    wrapMode: Text.Wrap
                    color: Theme.secondary
                    font.pixelSize: 14
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: modelContent.implicitHeight + 36
            radius: 10
            color: Theme.panel
            border.color: Theme.border
            ColumnLayout {
                id: modelContent
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 18 }
                spacing: 12
                UiText { text: "默认模型配置"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
                RowLayout {
                    Layout.fillWidth: true
                    UiText { text: "模型"; Layout.preferredWidth: 74; color: Theme.secondary }
                    ChoiceBox {
                        objectName: "aiDefaultModel"
                        Layout.fillWidth: true
                        model: aiBridge.defaultModels
                        textRole: "label"
                        valueRole: "id"
                        currentIndex: indexOfValue(aiBridge.draft.modelId)
                        enabled: aiBridge.draft.providerId.length > 0
                        onActivated: page.saveField("modelId", currentValue)


                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    UiText { text: "思考模式"; Layout.preferredWidth: 74; color: Theme.secondary }
                    ChoiceBox {
                        objectName: "aiDefaultThinking"
                        Layout.fillWidth: true
                        model: aiBridge.defaultThinkingChoices
                        textRole: "label"
                        valueRole: "id"
                        currentIndex: indexOfValue(page.effortValue(false))
                        enabled: aiBridge.defaultThinkingEditable || aiBridge.draft.thinkingEnabled
                        onActivated: aiBridge.setThinkingEffort(false, currentValue)
                    }
                }
                UiText {
                    objectName: "aiDefaultModelCapabilities"
                    Layout.fillWidth: true
                    text: aiBridge.defaultModelCapabilities
                    wrapMode: Text.Wrap
                    color: Theme.secondary
                    font.pixelSize: 14
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: featuresContent.implicitHeight + 36
            radius: 10
            color: Theme.panel
            border.color: Theme.border
            ColumnLayout {
                id: featuresContent
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 18 }
                spacing: 12
                UiText { text: "AI 功能"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
                ShellButton { objectName: "aiLyricsTranslationSettings"; text: "歌词翻译设置"; onClicked: page.openTranslation() }
                UiText {
                    Layout.fillWidth: true
                    text: "歌词翻译可使用全局默认配置或单独配置，请求完成后先预览再保存。"
                    wrapMode: Text.Wrap
                    color: Theme.secondary
                    font.pixelSize: 14
                }
            }
        }
    }

    ColumnLayout {
        visible: page.subpage === "translation"
        Layout.fillWidth: true
        spacing: 16
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: translationContent.implicitHeight + 36
            radius: 10
            color: Theme.panel
            border.color: Theme.border
            ColumnLayout {
                id: translationContent
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 18 }
                spacing: 12
                UiText { text: "歌词翻译配置"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
                CheckBox {
                    objectName: "aiTranslationUseGlobal"
                    text: "使用全局默认配置"
                    checked: aiBridge.draft.translationUsesDefaults
                    onClicked: page.saveField("translationUsesDefaults", checked)
                }
                RowLayout {
                    Layout.fillWidth: true
                    UiText { text: "供应商覆盖"; Layout.preferredWidth: 88; color: Theme.secondary }
                    ChoiceBox {
                        objectName: "aiTranslationProvider"
                        Layout.fillWidth: true
                        model: ["请选择供应商", "DeepSeek 官方 API"]
                        currentIndex: aiBridge.draft.translationProviderId === "deepseek" ? 1 : 0
                        enabled: !aiBridge.draft.translationUsesDefaults
                        onActivated: page.saveField("translationProviderId", currentIndex === 1 ? "deepseek" : "")
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    UiText { text: "模型覆盖"; Layout.preferredWidth: 88; color: Theme.secondary }
                    ChoiceBox {
                        objectName: "aiTranslationModel"
                        Layout.fillWidth: true
                        model: aiBridge.translationModels
                        textRole: "label"
                        valueRole: "id"
                        currentIndex: indexOfValue(aiBridge.draft.translationModelId)
                        enabled: !aiBridge.draft.translationUsesDefaults && aiBridge.draft.translationProviderId.length > 0
                        onActivated: page.saveField("translationModelId", currentValue)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    UiText { text: "思考模式覆盖"; Layout.preferredWidth: 88; color: Theme.secondary }
                    ChoiceBox {
                        objectName: "aiTranslationThinking"
                        Layout.fillWidth: true
                        model: aiBridge.translationThinkingChoices
                        textRole: "label"
                        valueRole: "id"
                        currentIndex: indexOfValue(page.effortValue(true))
                        enabled: !aiBridge.draft.translationUsesDefaults && (aiBridge.translationThinkingEditable || aiBridge.draft.translationThinkingEnabled)
                        onActivated: aiBridge.setThinkingEffort(true, currentValue)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    UiText { text: "目标语言"; Layout.preferredWidth: 88; color: Theme.secondary }
                    ChoiceBox {
                        objectName: "aiTranslationLanguage"
                        Layout.fillWidth: true
                        model: aiBridge.draft.targetLanguage === "zh-CN"
                            ? ["简体中文"] : ["暂不支持：" + aiBridge.draft.targetLanguage, "简体中文"]
                        currentIndex: 0
                        onActivated: page.saveField("targetLanguage", "zh-CN")
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: styleContent.implicitHeight + 36
            radius: 10
            color: Theme.panel
            border.color: Theme.border
            ColumnLayout {
                id: styleContent
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 18 }
                spacing: 12
                UiText { text: "翻译风格"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
                UiText {
                    Layout.fillWidth: true
                    text: "此处只调整表达风格。行 ID、行数、时间轴与返回格式由程序校验。"
                    wrapMode: Text.Wrap
                    color: Theme.secondary
                    font.pixelSize: 14
                }
                TextArea {
                    id: style
                    objectName: "translationStyle"
                    property bool dirty: false
                    Layout.fillWidth: true
                    Layout.preferredHeight: 145
                    text: aiBridge.draft.translationStylePrompt
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.Wrap
                    placeholderText: "翻译风格"
                    onTextChanged: if (activeFocus && text !== aiBridge.draft.translationStylePrompt) {
                        dirty = true
                        aiBridge.edit("translationStylePrompt", text)
                        styleSave.restart()
                    }
                }
                ShellButton {
                    objectName: "aiTranslationStyleReset"
                    text: "恢复默认提示词"
                    onClicked: { style.dirty = false; styleSave.stop(); aiBridge.restoreTranslationStyle() }
                }
            }
        }
        UiText {
            Layout.fillWidth: true
            text: aiBridge.configurationStatus
            wrapMode: Text.Wrap
            color: Theme.secondary
            font.pixelSize: 14
        }
        UiText {
            Layout.fillWidth: true
            text: "启动 AI 翻译后，整首歌词、歌曲名和歌手将发送至选定供应商，可能产生费用。每次发送前会单独确认。"
            wrapMode: Text.Wrap
            color: Theme.secondary
            font.pixelSize: 14
        }
    }

    UiText {
        objectName: "aiProviderStatus"
        Layout.fillWidth: true
        text: aiBridge.status
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: Theme.accent
    }
    Timer { id: styleSave; interval: 250; onTriggered: page.flushStyle() }
    ConfirmDialog {
        id: removeConsent
        parent: page.Overlay.overlay
        anchors.centerIn: parent
        width: Math.max(300, Math.min(440, page.width - 20))
        modal: true
        title: "删除已保存的 API Key？"
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "将从本地凭据管理器中删除 DeepSeek 的 Key。"; wrapMode: Text.Wrap }
        onAccepted: aiBridge.removeKey()
    }
}
