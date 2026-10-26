import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
Item {
    id: panel
    objectName: "feedbackPanel"
    anchors.fill: parent
    property var service: feedbackBridge
    property bool suspended: false
    onSuspendedChanged: updateFeedback()
    property var selected: ({})
    property string lastEventId: ""
    function openDetails(entry) { selected = entry || service.current; details.open() }
    function updateFeedback() {
        if (service.visible) {
            if (lastEventId !== String(service.current.eventId)) {
                if (suspended) { expiry.stop(); return }
                lastEventId = String(service.current.eventId)
                expiry.restart()
            }
            if (!suspended) service.presented(String(service.current.eventId))
        } else expiry.stop()
    }
    Component.onCompleted: updateFeedback()
    Connections { target: panel.service; function onChanged() { panel.updateFeedback() } }
    Timer { id: expiry; objectName: "feedbackExpiry"; interval: 10000; onTriggered: panel.service.dismiss() }
    Rectangle {
        id: notice
        objectName: "feedbackNotice"
        visible: panel.service.visible && !panel.suspended
        anchors.bottom: parent.bottom; anchors.bottomMargin: 84
        anchors.right: parent.right; anchors.rightMargin: 20
        width: Math.min(520, parent.width - 40)
        height: noticeContent.implicitHeight + 24
        radius: 10; color: Theme.card; border.color: Theme.border
        ColumnLayout {
            id: noticeContent
            anchors.fill: parent; anchors.margins: 12; spacing: 6
            RowLayout {
                Layout.fillWidth: true
                UiText {
                    objectName: "feedbackMessage"
                    Layout.fillWidth: true; text: panel.service.current.message || ""
                    textFormat: Text.PlainText; color: Theme.text; wrapMode: Text.Wrap
                    maximumLineCount: 3; elide: Text.ElideRight
                }
                ShellButton { objectName: "feedbackDetailsButton"; text: "详情"; flat: true; onClicked: panel.openDetails(panel.service.current) }
                ShellButton { objectName: "feedbackDismissButton"; text: "关闭"; flat: true; onClicked: panel.service.dismiss() }
            }
            UiText {
                objectName: "feedbackReason"
                Layout.fillWidth: true; text: panel.service.current.reason || ""
                visible: text.length > 0; textFormat: Text.PlainText; color: Theme.secondary
                wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
            }
        }
    }
    ShellButton {
        objectName: "feedbackHistoryButton"
        anchors.right: parent.right; anchors.rightMargin: 16
        anchors.bottom: parent.bottom; anchors.bottomMargin: 84
        visible: panel.service.history.length > 0 && !panel.service.visible && !panel.suspended
        text: "操作记录"; flat: true
        onClicked: panel.openDetails(panel.service.history[0])
    }
    ConfirmDialog {
        id: details
        objectName: "feedbackDetailsDialog"
        parent: panel.parent; anchors.centerIn: parent
        width: Math.min(680, parent.width - 40); height: Math.min(520, parent.height - 70)
        modal: false; title: "操作详情"; standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 10
            UiText { Layout.fillWidth: true; text: "本次会话最近 20 条；详情仅保留在本机内存中。"; color: Theme.secondary; wrapMode: Text.Wrap }
            ComboBox {
                objectName: "feedbackHistoryChoice"; Layout.fillWidth: true
                displayText: panel.selected.message || ""
                model: panel.service.history; textRole: "message"
                onActivated: index => panel.selected = panel.service.history[index]
            }
            UiText {
                objectName: "feedbackDetailIdentity"; Layout.fillWidth: true
                text: (panel.selected.timestamp || "") + " · " + (panel.selected.source || "")
                    + " · 事件 " + (panel.selected.eventId || "")
                    + (panel.selected.targetId ? " · 目标 " + panel.selected.targetId : "")
                textFormat: Text.PlainText; color: Theme.secondary; wrapMode: Text.Wrap
            }
            UiText {
                objectName: "feedbackDetailSummary"; Layout.fillWidth: true
                text: panel.selected.message || ""; textFormat: Text.PlainText
                color: Theme.text; wrapMode: Text.Wrap
            }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                TextArea {
                    objectName: "feedbackDetailText"
                    readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    text: panel.selected.details || "无附加详情"
                    color: Theme.text; background: null
                }
            }
            UiText { visible: !!panel.selected.detailsTruncated; text: "详情较长，仅显示前 16384 个字符。"; color: Theme.secondary }
        }
    }
}
