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
    property real bottomInset: 20
    readonly property real edgeMargin: 20
    readonly property real pressedScale: .96
    property real noticeProgress: 0
    property var noticeEntry: ({})
    property bool dismissing: false
    property bool detailHandoff: false
    onSuspendedChanged: updateFeedback()
    property var selected: ({})
    property string lastEventId: ""
    property bool historyAvailable: false
    property bool hadNotice: false
    function hideHistory() { historyAvailable=false; historyExpiry.stop() }
    function pressNotice() { if (!dismissing && !detailMotion.running) notice.scale = panel.pressedScale }
    function releaseNotice() { if (!detailMotion.running) notice.scale = 1 }
    function openDetails(entry) {
        selected = entry || service.current
        expiry.stop()
        if (service.visible && selected.eventId && String(selected.eventId) === String(noticeEntry.eventId) && !detailHandoff) detailMotion.restart()
        else details.open()
    }
    function dismissNotice() {
        if (dismissing) return
        expiry.stop(); dismissing = true; noticeProgress = 0
        dismissDelay.restart()
    }
    function updateFeedback() {
        if (suspended) { expiry.stop(); noticeProgress = 0; return }
        if (service.visible) {
            if (lastEventId !== String(service.current.eventId)) {
                if (suspended) { expiry.stop(); return }
                lastEventId = String(service.current.eventId)
                historyAvailable = true
                historyExpiry.stop()
                expiry.restart()
            }
            if (!dismissing && !details.visible && !detailMotion.running) {
                noticeEntry = service.current
                detailHandoff = false; notice.scale = 1; noticeProgress = 1
                if (!expiry.running) expiry.restart()
            }
            hadNotice = true
            if (!suspended) service.presented(String(service.current.eventId))
        } else {
            noticeProgress = 0
            expiry.stop()
            if (hadNotice) { hadNotice=false; if(historyAvailable) historyExpiry.restart() }
        }
    }
    Component.onCompleted: updateFeedback()
    Connections { target: panel.service; function onChanged() { panel.updateFeedback() } }
    Behavior on noticeProgress { NumberAnimation { duration: settingsBridge.values.reduceMotion ? 0 : 320; easing.type: Easing.OutCubic } }
    Timer { id: expiry; objectName: "feedbackExpiry"; interval: 10000; onTriggered: panel.dismissNotice() }
    Timer {
        id: dismissDelay; interval: settingsBridge.values.reduceMotion ? 0 : 320
        onTriggered: {
            if (String(panel.service.current.eventId) === String(panel.noticeEntry.eventId)) panel.service.dismiss()
            panel.dismissing = false; panel.updateFeedback()
        }
    }
    NumberAnimation {
        id: detailMotion; target: notice; property: "scale"; to: panel.pressedScale
        duration: settingsBridge.values.reduceMotion ? 0 : 140; easing.type: Easing.OutCubic
        onFinished: {
            panel.detailHandoff = true; panel.noticeProgress = 0
            if (String(panel.service.current.eventId) === String(panel.noticeEntry.eventId)) panel.service.dismiss()
            details.open()
        }
    }
    Rectangle {
        id: notice
        objectName: "feedbackNotice"
        visible: panel.noticeProgress > 0 && !panel.detailHandoff
        anchors.bottom: parent.bottom; anchors.bottomMargin: panel.bottomInset
        x: parent.width - width - panel.edgeMargin + (width + panel.edgeMargin) * (1 - panel.noticeProgress)
        width: Math.min(520, parent.width - 40)
        height: noticeContent.implicitHeight + 24
        radius: 10; color: Theme.card; border.color: Theme.border
        Behavior on scale { enabled: !detailMotion.running; NumberAnimation { duration: settingsBridge.values.reduceMotion ? 0 : 110; easing.type: Easing.OutCubic } }
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
            onPressed: panel.pressNotice(); onReleased: panel.releaseNotice(); onCanceled: panel.releaseNotice()
            onClicked: panel.openDetails(panel.noticeEntry)
        }
        ColumnLayout {
            id: noticeContent
            anchors.fill: parent; anchors.margins: 12; spacing: 6
            RowLayout {
                Layout.fillWidth: true
                UiText {
                    objectName: "feedbackMessage"
                    Layout.fillWidth: true; text: panel.noticeEntry.message || ""
                    textFormat: Text.PlainText; color: Theme.text; wrapMode: Text.Wrap
                    maximumLineCount: 3; elide: Text.ElideRight
                }
                ShellButton { objectName: "feedbackDetailsButton"; text: "详情"; flat: true; onPressed: panel.pressNotice(); onReleased: panel.releaseNotice(); onCanceled: panel.releaseNotice(); onClicked: panel.openDetails(panel.noticeEntry) }
                ShellButton { objectName: "feedbackDismissButton"; text: "关闭"; flat: true; onClicked: panel.dismissNotice() }
            }
            UiText {
                objectName: "feedbackReason"
                Layout.fillWidth: true; text: panel.noticeEntry.reason || ""
                visible: text.length > 0; textFormat: Text.PlainText; color: Theme.secondary
                wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
            }
        }
    }
    Timer { id: historyExpiry; objectName: "feedbackHistoryExpiry"; interval: 10000; onTriggered: panel.hideHistory() }
    // The shell observes window presses without consuming them. A full-window
    // overlay pointer handler would prevent buttons underneath from receiving input.
    function pointerPressed(x,y) {
            const content=panel.Window.window.contentItem
            if(details.visible) {
                const popup=details.contentItem.parent
                const d=popup.mapFromItem(content,x,y)
                if(d.x<0 || d.y<0 || d.x>popup.width || d.y>popup.height) details.close()
            }
            const p=historyControls.mapFromItem(content,x,y)
            if(p.x<0 || p.y<0 || p.x>historyControls.width || p.y>historyControls.height) panel.hideHistory()
    }
    RowLayout {
        id: historyControls
        objectName: "feedbackHistoryControls"
        anchors.right: parent.right; anchors.rightMargin: panel.edgeMargin
        anchors.bottom: parent.bottom; anchors.bottomMargin: panel.bottomInset
        visible: panel.historyAvailable && panel.service.history.length > 0 && !panel.service.visible && !panel.suspended
        ShellButton {
            objectName: "feedbackHistoryButton"; text: "操作记录"; flat: true
            onClicked: { historyExpiry.stop(); panel.openDetails(panel.service.history[0]) }
        }
        ShellButton { objectName: "feedbackHistoryDismiss"; text: "关闭"; flat: true; onClicked: panel.hideHistory() }
    }
    ConfirmDialog {
        id: details
        objectName: "feedbackDetailsDialog"
        parent: panel.parent; anchors.centerIn: parent
        width: Math.min(680, parent.width - 40); height: Math.min(520, parent.height - 70)
        modal: false; title: "操作详情"; standardButtons: Dialog.Close
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: panel.hideHistory()
        contentItem: ColumnLayout {
            spacing: 10
            UiText { Layout.fillWidth: true; text: "本次会话最近 20 条；详情仅保留在本机内存中。"; color: Theme.secondary; wrapMode: Text.Wrap }
            ChoiceBox {
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
