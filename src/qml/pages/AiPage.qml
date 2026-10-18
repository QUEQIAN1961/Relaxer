import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"
ColumnLayout {
    id: page
    objectName: "aiPage"
    Layout.fillWidth: true; spacing: 12
    Text { Layout.fillWidth: true; text: "AI 服务配置"; color: Theme.text; font.pixelSize: 22; font.weight: Font.Bold }
    Text { Layout.fillWidth: true; text: "打开页面、保存设置或切歌不会发送付费请求。翻译前仍需逐次确认发送歌词。"; wrapMode: Text.Wrap; color: Theme.secondary }
    RowLayout {
        Layout.fillWidth: true
        Text { text: "默认供应商"; color: Theme.secondary }
        ComboBox { Layout.fillWidth: true; model: ["未配置", "DeepSeek"]; currentIndex: aiBridge.draft.providerId === "deepseek" ? 1 : 0; onActivated: aiBridge.edit("providerId", currentIndex === 1 ? "deepseek" : "") }
        ShellButton { objectName: "modelRefreshButton"; text: aiBridge.busy ? "连接中…" : "验证 / 刷新模型"; enabled: !aiBridge.busy; onClicked: connectConsent.open() }
    }
    ComboBox {
        objectName: "aiDefaultModel"; Layout.fillWidth: true; model: aiBridge.models; textRole: "label"; valueRole: "id"
        currentIndex: indexOfValue(aiBridge.draft.modelId)
        displayText: currentIndex >= 0 ? currentText : "请先刷新模型目录并明确选择"
        onActivated: { aiBridge.edit("providerId", "deepseek"); aiBridge.edit("modelId", currentValue) }
    }
    RowLayout {
        Layout.fillWidth: true
        CheckBox { text: "启用默认模型思考模式"; checked: aiBridge.draft.thinkingEnabled; enabled: aiBridge.defaultEfforts.length > 0 || checked; onClicked: aiBridge.edit("thinkingEnabled", checked) }
        ComboBox { Layout.fillWidth: true; model: aiBridge.defaultEfforts; currentIndex: model.indexOf(aiBridge.draft.reasoningEffort); displayText: currentIndex >= 0 ? currentText : "选择支持的强度"; enabled: aiBridge.draft.thinkingEnabled; onActivated: aiBridge.edit("reasoningEffort", currentText) }
    }
    CheckBox { text: "歌词翻译使用默认模型"; checked: aiBridge.draft.translationUsesDefaults; onClicked: aiBridge.edit("translationUsesDefaults", checked) }
    ColumnLayout {
        Layout.fillWidth: true; visible: !aiBridge.draft.translationUsesDefaults
        ComboBox { Layout.fillWidth: true; model: aiBridge.models; textRole: "label"; valueRole: "id"; currentIndex: indexOfValue(aiBridge.draft.translationModelId); onActivated: { aiBridge.edit("translationProviderId", "deepseek"); aiBridge.edit("translationModelId", currentValue) } }
        RowLayout {
            Layout.fillWidth: true
            CheckBox { text: "翻译专用思考模式"; checked: aiBridge.draft.translationThinkingEnabled; enabled: aiBridge.translationEfforts.length > 0 || checked; onClicked: aiBridge.edit("translationThinkingEnabled", checked) }
            ComboBox { Layout.fillWidth: true; model: aiBridge.translationEfforts; currentIndex: model.indexOf(aiBridge.draft.translationReasoningEffort); displayText: currentIndex >= 0 ? currentText : "选择支持的强度"; enabled: aiBridge.draft.translationThinkingEnabled; onActivated: aiBridge.edit("translationReasoningEffort", currentText) }
        }
    }
    Text { text: "翻译目标：简体中文"; color: Theme.secondary }
    TextArea { objectName: "translationStyle"; Layout.fillWidth: true; Layout.preferredHeight: 100; text: aiBridge.draft.translationStylePrompt; textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap; placeholderText: "翻译风格"; onTextChanged: if (activeFocus) aiBridge.edit("translationStylePrompt", text) }
    RowLayout {
        ShellButton { objectName: "aiSaveButton"; text: "保存配置"; primary: true; onClicked: aiBridge.save() }
        ShellButton { text: "取消修改"; onClicked: aiBridge.resetDraft() }
    }
    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
    Text { Layout.fillWidth: true; text: aiBridge.credentialStatus; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 12 }
    TextField { id: key; objectName: "apiKeyInput"; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: "输入新的 API Key；已存凭据不会回显"; selectByMouse: true }
    Flow {
        Layout.fillWidth: true; spacing: 8
        ShellButton { text: "安全保存 Key"; onClicked: { aiBridge.storeKey(key.text); key.clear() } }
        ShellButton { text: "检查本地凭据"; onClicked: aiBridge.checkCredential() }
        ShellButton { text: "移除凭据"; onClicked: removeConsent.open() }
    }
    Text { Layout.fillWidth: true; text: aiBridge.status; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.accent }
    ConfirmDialog {
        id: connectConsent; parent: page.Overlay.overlay; anchors.centerIn: parent; width: Math.max(300, Math.min(440, page.width - 20)); modal: true
        title: "连接 DeepSeek"; standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "将使用已存凭据连接官方服务读取模型目录。此操作不会发送歌词，也不会开始翻译。是否继续？"; wrapMode: Text.Wrap }
        onAccepted: aiBridge.refreshModels(true)
    }
    ConfirmDialog { id: removeConsent; parent: page.Overlay.overlay; anchors.centerIn: parent; width: Math.max(300, Math.min(440, page.width - 20)); modal: true; title: "移除已保存的 API Key？"; standardButtons: Dialog.Ok | Dialog.Cancel; contentItem: Label { text: "将从本地凭据管理器移除此供应商的 Key。"; wrapMode: Text.Wrap } onAccepted: aiBridge.removeKey() }
}
