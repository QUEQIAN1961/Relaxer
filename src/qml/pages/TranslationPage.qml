import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"
ScrollView {
    id: page
    objectName: "translationPage"
    contentWidth: availableWidth
    ColumnLayout {
        width: page.availableWidth; spacing: 16
        Text { Layout.fillWidth: true; text: "歌词与翻译 · " + playbackBridge.title; textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 24; elide: Text.ElideRight }
        Text { Layout.fillWidth: true; text: "本地原文和已保存译文直接显示。联网检索、浏览器翻译和可能付费的 AI 翻译均需手动启动。"; color: Theme.secondary; wrapMode: Text.Wrap }
        Flow {
            Layout.fillWidth: true; spacing: 8
            ShellButton { text: "需要翻译"; primary: translationBridge.classification === 1; enabled: playbackBridge.hasTrack; onClicked: translationBridge.classify(1) }
            ShellButton { text: "无需翻译"; primary: translationBridge.classification === 2; enabled: playbackBridge.hasTrack; onClicked: translationBridge.classify(2) }
            ShellButton { objectName: "translateAiButton"; text: "AI 翻译整首"; enabled: playbackBridge.hasTrack && !translationBridge.pending && !translationBridge.preview; onClicked: { const task = translationBridge.prepareAi(); if (task.length) { consent.exactTask = task; consent.browser = false; consent.open() } } }
            ShellButton { text: "取消任务"; enabled: translationBridge.pending; onClicked: translationBridge.cancel() }
            ShellButton { text: "继续人工验证"; visible: translationBridge.state === "NeedsAttention"; onClicked: translationBridge.resume() }
        }
        RowLayout {
            Layout.fillWidth: true
            ComboBox { id: provider; model: ["bing", "baidu"]; Layout.fillWidth: true }
            ComboBox { id: language; model: ["en", "ja", "auto"] }
            ShellButton { text: "浏览器翻译"; enabled: playbackBridge.hasTrack && !translationBridge.pending && !translationBridge.preview; onClicked: { const task = translationBridge.prepareBrowser(provider.currentText, language.currentText); if (task.length) { consent.exactTask = task; consent.browser = true; consent.open() } } }
        }
        Text { Layout.fillWidth: true; text: translationBridge.status + (translationBridge.details.length ? "\n" + translationBridge.details : ""); textFormat: Text.PlainText; color: Theme.secondary; wrapMode: Text.Wrap }
        RowLayout {
            visible: translationBridge.preview; Layout.fillWidth: true
            ShellButton { objectName: "saveTranslationButton"; text: "保存双语歌词"; primary: true; onClicked: translationBridge.save() }
            ShellButton { objectName: "discardTranslationButton"; text: "放弃预览"; onClicked: discardConsent.open() }
            Text { Layout.fillWidth: true; text: "未保存预览保留在当前会话，切回该歌曲可继续处理。"; color: Theme.secondary; wrapMode: Text.Wrap; font.pixelSize: 12 }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
        RowLayout {
            ShellButton { objectName: "searchLyricsButton"; text: "联网检索当前歌曲歌词"; enabled: playbackBridge.hasTrack && !lyricsSearchBridge.busy; onClicked: searchConsent.open() }
            ShellButton { text: "停止检索"; enabled: lyricsSearchBridge.busy; onClicked: lyricsSearchBridge.stop() }
        }
        Text { Layout.fillWidth: true; text: lyricsSearchBridge.status; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.secondary }
        ListView {
            Layout.fillWidth: true; Layout.preferredHeight: count ? 180 : 36; clip: true; model: lyricsSearchBridge
            delegate: ItemDelegate {
                required property int index; required property string title; required property string artist; required property string source; required property bool timed
                width: ListView.view.width; text: title + " · " + artist + " · " + source + (timed ? " · 时间轴" : " · 文本")
                onClicked: lyricsSearchBridge.select(index)
            }
            Text { anchors.centerIn: parent; visible: parent.count === 0; text: "暂无候选版本"; color: Theme.secondary }
        }
        TextArea { Layout.fillWidth: true; Layout.preferredHeight: 140; readOnly: true; text: lyricsSearchBridge.previewText; textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap; selectByMouse: true; placeholderText: "选择候选版本查看原文" }
        ShellButton { text: "保存所选歌词版本"; enabled: lyricsSearchBridge.selectedKey.length > 0; onClicked: { replaceConsent.exactKey = lyricsSearchBridge.selectedKey; replaceConsent.open() } }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
        AiPage { Layout.fillWidth: true }
        Item { Layout.preferredHeight: 20 }
    }
    ConfirmDialog {
        id: consent; parent: page.Overlay.overlay; objectName: "translationConsent"; property string exactTask: ""; property bool browser: false
        anchors.centerIn: parent; width: Math.max(300, Math.min(480, page.width - 20)); modal: true
        title: browser ? "使用既有浏览器翻译流程" : "发送整首歌词至 DeepSeek"
        footer: DialogButtonBox {
            Button { objectName: "consentDeclineButton"; text: "不发送"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { objectName: "consentAcceptButton"; text: consent.browser ? "开始浏览器翻译" : "发送一次整首歌词"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            onAccepted: consent.accept()
            onRejected: consent.reject()
        }
        contentItem: Label { wrapMode: Text.Wrap; text: consent.browser ? "将打开专用浏览器资料，向所选来源提交歌词。遇到登录或验证时请自行操作，不会绕过网站限制。返回结果仅为未保存预览。" : "将把歌词文本、目标语言和风格提示发往 DeepSeek 官方服务，可能产生费用。不会上传音频、封面或本地路径；不会自动拆分、重试或切换模型。取消不能撤销服务端已经收到的请求。译文仅在点击保存后写入。是否发送这一次？" }
        onAccepted: translationBridge.start(exactTask)
        onClosed: translationBridge.declineConsent()
    }
    ConfirmDialog { id: discardConsent; parent: page.Overlay.overlay; anchors.centerIn: parent; width: Math.max(300, Math.min(440, page.width - 20)); modal: true; title: "放弃尚未保存的翻译预览？"; standardButtons: Dialog.Ok | Dialog.Cancel; contentItem: Label { text: "尚未保存的译文将丢弃，本地歌词文件保持原样。"; wrapMode: Text.Wrap } onAccepted: translationBridge.discard() }
    ConfirmDialog { id: searchConsent; parent: page.Overlay.overlay; anchors.centerIn: parent; width: Math.max(300, Math.min(440, page.width - 20)); modal: true; title: "联网检索歌词"; standardButtons: Dialog.Ok | Dialog.Cancel; contentItem: Label { text: "将通过既有歌词服务检索当前歌曲。这不会启动付费 AI 翻译，候选结果需要你明确保存。"; wrapMode: Text.Wrap } onAccepted: lyricsSearchBridge.search(true) }
    ConfirmDialog {
        id: replaceConsent; parent: page.Overlay.overlay; property string exactKey: ""; anchors.centerIn: parent; width: Math.max(300, Math.min(440, page.width - 20)); modal: true
        title: "替换当前歌曲的本地歌词？"; standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "将保存所选版本。当前歌曲的翻译任务或未保存预览会被明确放弃；已经发送的请求可能仍产生费用。"; wrapMode: Text.Wrap }
        onAccepted: { if (translationBridge.taskKey.length) translationBridge.discard(); lyricsSearchBridge.saveSelected(exactKey, true) }
    }
}
