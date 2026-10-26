import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

ConfirmDialog {
    id: dialog
    objectName: "batchInstrumentalDialog"
    anchors.centerIn: parent
    width: Math.min(620, parent.width - 40)
    modal: true
    title: "批量标记纯音乐"
    readonly property var overview: lyricsSearchBridge.batchOverview
    function review(keys) {
        if (!lyricsSearchBridge.prepareInstrumentalBatch(keys)) return
        riskConsent.checked = false
        open()
    }
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            objectName: "batchInstrumentalOverview"; Layout.fillWidth: true; wrapMode: Text.Wrap
            text: "目标 " + (dialog.overview.total || 0) + " 首：可安全处理 " + (dialog.overview.safe || 0)
                + "，已经标记 " + (dialog.overview.already || 0) + "，需要额外授权 " + (dialog.overview.risk || 0)
                + "，无法处理 " + (dialog.overview.unavailable || 0) + "。"
        }
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: "仅在确认这些歌曲没有人声歌词时使用。逐曲独立保存；未授权的风险目标会跳过，失败不会回退其他已成功歌曲。" }
        CheckBox {
            id: riskConsent; objectName: "batchInstrumentalRiskConsent"
            Layout.fillWidth: true; visible: dialog.overview.risk > 0
            indicator: Rectangle {
                implicitWidth: 18; implicitHeight: 18; x: 0; y: (riskConsent.height-height)/2
                radius: 3; color: riskConsent.checked ? Theme.accent : Theme.card; border.color: Theme.border
                UiText { anchors.centerIn: parent; text: "✓"; visible: riskConsent.checked; color: Theme.accentText }
            }
            contentItem: Label {
                leftPadding: riskConsent.indicator.width + riskConsent.spacing
                wrapMode: Text.Wrap
                text: "明确授权覆盖上述 " + (dialog.overview.risk || 0) + " 首风险目标的已有歌词和译文；仅在保存成功后放弃相关翻译任务或未保存预览。"
            }
        }
    }
    footer: DialogButtonBox {
        ShellButton { objectName: "batchInstrumentalCancel"; text: "取消"; onClicked: dialog.reject() }
        ShellButton {
            objectName: "batchInstrumentalCommit"
            text: riskConsent.checked ? "按上述授权处理" : "处理安全目标，跳过风险"
            onClicked: if (lyricsSearchBridge.startInstrumentalBatch(riskConsent.checked)) dialog.close()
        }
    }
    onClosed: if (lyricsSearchBridge.batchPrepared) lyricsSearchBridge.cancelInstrumentalBatch()
}
