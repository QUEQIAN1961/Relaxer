import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../theme"

Page {
    id: workspace
    objectName: "lyricsWorkspace"
    visible: false
    anchors.fill: parent
    focus: visible
    readonly property bool opened: visible
    signal aboutToShow()
    signal closed()
    signal batchInstrumentalRequested(var keys)
    function open() { aboutToShow(); visible = true; forceActiveFocus() }
    function close() { visible = false; closed() }
    padding: 18
    topPadding: 6; bottomPadding: 6
    spacing: 8
    component FilterToggle: CheckBox {
        id: control
        implicitHeight: 28; padding: 0; spacing: 3; font.pixelSize: 13
        implicitWidth: contentItem.implicitWidth
        indicator: Rectangle {
            x: 0; anchors.verticalCenter: parent.verticalCenter; width: 14; height: 14; radius: 3
            color: control.checked ? Theme.accent : Theme.card
            border.color: control.activeFocus ? Theme.accent : Theme.border
            UiText { anchors.centerIn: parent; text: "✓"; visible: control.checked; color: Theme.accentText; font.pixelSize: 12 }
        }
        contentItem: UiText { leftPadding: 17; text: control.text; font: control.font; color: Theme.text; verticalAlignment: Text.AlignVCenter }
    }
    property var selectedTracks: []
    property int targetRevision: 0
    ListModel { id: targetRows }
    function acceptsRow(row) {
        return (!missingLyrics.checked || !row.lyricsAvailable) && (!missingTranslation.checked || row.translationMissing)
    }
    function visibleKeys() {
        const revision = targetRevision
        const keys = []
        for (let i = 0; i < targetRows.count; ++i) {
            const row = targetRows.get(i)
            if (acceptsRow(row)) keys.push(row.trackKey)
        }
        return keys
    }
    function searchKeys() {
        const keys = visibleKeys()
        return multiSelect.checked ? keys.filter(key => selectedTracks.indexOf(key) >= 0) : keys
    }
    function toggleTrack(key) {
        const keys = selectedTracks.slice(); const index = keys.indexOf(key)
        if (index < 0) keys.push(key); else keys.splice(index, 1)
        selectedTracks = keys
    }
    function syncTargets() {
        const rows = lyricsSearchBridge.targetTracks
        let rebuild = rows.length !== targetRows.count
        for (let i = 0; !rebuild && i < rows.length; ++i)
            rebuild = rows[i].trackKey !== targetRows.get(i).trackKey
        if (rebuild) {
            targetRows.clear()
            for (let i = 0; i < rows.length; ++i) targetRows.append(rows[i])
            selectedTracks = selectedTracks.filter(key => rows.some(row => row.trackKey === key))
        } else {
            // Update roles in place: status signals must not reset ListView or its scroll position.
            for (let i = 0; i < rows.length; ++i) targetRows.set(i, rows[i])
        }
        ++targetRevision
    }
    function startTargetSearch() {
        if (lyricsSearchBridge.busy) { lyricsSearchBridge.stop(); return }
        if (multiSelect.checked || missingLyrics.checked || missingTranslation.checked)
            lyricsSearchBridge.searchTracks(searchKeys(), true)
        else lyricsSearchBridge.searchTargets(true)
    }
    Connections {
        target: lyricsSearchBridge
        function onChanged() { workspace.syncTargets() }
        function onReplacementRequested(token, reason) {
            replaceConfirm.exactToken = token
            replaceConfirm.reason = reason
            replaceConfirm.open()
        }
        function onSaved(trackKey) { workspace.advancePending(trackKey) }
    }
    property bool allowClose: false
    property string localStatus: ""
    property string fileTargetKey: "0"
    property string fileEditToken: ""
    readonly property bool compact: height < 680
    readonly property bool hasTarget: lyricsSearchBridge.targetTrackKey !== "0"
    readonly property bool taskNeedsAction: translationBridge.pending || translationBridge.preview
    readonly property bool sourceActionsEnabled: hasTarget && !taskNeedsAction && !lyricsSearchBridge.batchRunning
    function updateQuery() {
        songTitle.text = lyricsSearchBridge.targetTitle
        songArtist.text = lyricsSearchBridge.targetArtist
        songAlbum.text = libraryBridge.trackDetails(lyricsSearchBridge.targetTrackKey).album || ""
        fragment.text = ""
        localStatus = ""
    }
    function chooseTarget(key) {
        if (!lyricsSearchBridge.setTargetTrack(key)) return
        translationBridge.setTargetTrack(key)
        updateQuery()
    }
    function advancePending(savedTarget) {
        let previous = -1
        for (let i = 0; i < targetRows.count; ++i)
            if (targetRows.get(i).trackKey === savedTarget) { previous = i; break }
        for (let step = 1; step <= targetRows.count; ++step) {
            const index = (previous + step) % targetRows.count
            const row = targetRows.get(index)
            if (row.trackKey !== savedTarget && acceptsRow(row) && row.searchState === "choice") {
                chooseTarget(row.trackKey)
                previewTabs.currentIndex = 0
                candidates.positionViewAtBeginning()
                tracks.positionViewAtIndex(index, ListView.Contain)
                return
            }
        }
        localStatus = "已完成当前列表中的待确认歌词。"
    }
    function commitSelection(key, target) {
        if (target !== lyricsSearchBridge.targetTrackKey || key !== lyricsSearchBridge.selectedKey) return
        lyricsSearchBridge.saveSelected(key, true)
    }
    function applySelection() {
        const key = lyricsSearchBridge.selectedKey, target = lyricsSearchBridge.targetTrackKey
        commitSelection(key, target)
    }
    function requestClose() {
        if (taskNeedsAction && !allowClose) leaveConfirm.open()
        else close()
    }
    function prepareTranslation(browserProvider) {
        const task = browserProvider.length
                   ? translationBridge.prepareBrowser(browserProvider, ["auto", "en", "ja"][sourceLanguage.currentIndex])
                   : translationBridge.prepareAi()
        if (!task.length) return
        consent.exactTask = task
        consent.exactTarget = translationBridge.targetTrackKey
        consent.browser = browserProvider.length > 0
        consent.open()
    }
    function startSearch() {
        if (sourceActionsEnabled && !lyricsSearchBridge.busy)
            lyricsSearchBridge.searchDetailed(songTitle.text, songArtist.text, songAlbum.text, fragment.text, true, syncedOnly.checked, replaceExisting.checked)
    }
    onAboutToShow: { syncTargets(); allowClose = false; updateQuery(); previewTabs.currentIndex = translationBridge.preview ? 1 : 0 }
    onClosed: {
        offsetEditor.close()
        lyricsSearchBridge.cancelInstrumentalBatch()
        consent.close()
        lyricsSearchBridge.stop()
        translationBridge.declineConsent()
        lyricsSearchBridge.setTargetTrack("0")
        translationBridge.setTargetTrack("0")
        allowClose = false
    }
    Connections {
        target: translationBridge
        function onChanged() { if (workspace.opened && translationBridge.preview) previewTabs.currentIndex = 1 }
    }
    Shortcut {
        sequence: "Escape"
        enabled: workspace.opened && !consent.opened && !leaveConfirm.opened && !replaceConfirm.opened && !discardConfirm.opened && !pasteDialog.opened && !instrumentalConfirm.opened && !offsetEditor.opened && !lyricsSearchBridge.batchPrepared
        onActivated: workspace.requestClose()
    }
    background: Rectangle { color: Theme.background }
    header: Item {
        implicitHeight: 46
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 12
            ShellButton { objectName: "lyricsWorkspaceClose"; text: "‹ 返回"; flat: true; implicitHeight: 30; Accessible.name: "返回上一页"; onClicked: workspace.requestClose() }
            Label { text: "查找歌词"; color: Theme.text; font.pixelSize: 22; font.weight: Font.DemiBold; Layout.fillWidth: true }
            Item { Layout.preferredWidth: 126 }
        }
    }
    contentItem: RowLayout {
        spacing: 14
        Rectangle {
            Layout.preferredWidth: Math.max(220, workspace.availableWidth * .25)
            Layout.minimumWidth: 200; Layout.fillHeight: true
            color: Theme.panel; radius: 10
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 10; spacing: 8
                Label { text: "歌曲 · " + workspace.visibleKeys().length + (multiSelect.checked ? " · 已选 " + workspace.searchKeys().length : ""); color: Theme.text; font.weight: Font.DemiBold }
                RowLayout {
                  objectName: "lyricsFilterRow"; Layout.fillWidth: true; spacing: 3
                FilterToggle { id: missingLyrics; objectName: "lyricsMissingOnly"; text: "无歌词"; onToggled: if (checked) missingTranslation.checked = false }
                FilterToggle { id: missingTranslation; objectName: "lyricsMissingTranslationOnly"; text: "无翻译"; onToggled: if (checked) missingLyrics.checked = false }
                FilterToggle { id: multiSelect; objectName: "lyricsMultiSelect"; text: checked ? "取消" : "多选"; onToggled: if (!checked) workspace.selectedTracks = [] }
                ShellButton { objectName: "lyricsSelectAll"; text: "全选"; flat: true; font.pixelSize: 13; leftPadding: 4; rightPadding: 4; implicitWidth: 38; implicitHeight: 28; visible: multiSelect.checked; onClicked: workspace.selectedTracks = workspace.visibleKeys() }
                Item { Layout.fillWidth: true }
                }
                ShellButton {
                    objectName: "lyricsBatchInstrumental"; Layout.fillWidth: true; visible: multiSelect.checked
                    text: lyricsSearchBridge.batchRunning ? "取消纯音乐处理" : "批量标记纯音乐…"
                    enabled: lyricsSearchBridge.batchRunning || workspace.searchKeys().length > 0
                    onClicked: if (lyricsSearchBridge.batchRunning) lyricsSearchBridge.cancelInstrumentalBatch(); else workspace.batchInstrumentalRequested(workspace.searchKeys())
                }
                ListView {
                    id: tracks; objectName: "lyricsTargetTracks"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    clip: true; model: targetRows
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        required property string trackKey
                        required property string title
                        required property string artist
                        required property string status
                        required property string searchState
                        required property bool lyricsAvailable
                        required property bool translationMissing
                        objectName: "lyricsTarget-" + trackKey
                        readonly property bool matchesFilter: (!missingLyrics.checked || !lyricsAvailable) && (!missingTranslation.checked || translationMissing)
                        visible: matchesFilter
                        width: ListView.view.width; height: matchesFilter ? 74 : 0; padding: 9
                        highlighted: trackKey === lyricsSearchBridge.targetTrackKey
                        Accessible.name: title + " · " + artist + " · " + status
                        background: Rectangle { radius: 7; color: parent.highlighted ? Theme.selected : parent.hovered ? Theme.hover : "transparent" }
                        contentItem: Item {
                            implicitHeight: 54
                            CheckBox { objectName: "lyricsCheck-" + trackKey; x: 0; anchors.verticalCenter: parent.verticalCenter; width: 28; visible: multiSelect.checked; checked: workspace.selectedTracks.indexOf(trackKey) >= 0; onToggled: workspace.toggleTrack(trackKey) }
                            Item {
                                id: stateSlot; x: multiSelect.checked ? 30 : 0; anchors.verticalCenter: parent.verticalCenter; width: 26; height: 26
                                BusyIndicator { objectName: "lyricsStateSpinner-" + trackKey; anchors.fill: parent; visible: searchState === "searching"; running: visible }
                                UiText { objectName: "lyricsStateIcon-" + trackKey; anchors.fill: parent; text: searchState === "complete" ? "✓" : searchState === "choice" ? "⚠" : ""; visible: text.length > 0; color: searchState === "complete" ? "#249c58" : "#d78816"; font.pixelSize: 20; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }
                            Column {
                                objectName: "lyricsTargetText-" + trackKey
                                x: stateSlot.x + stateSlot.width + 9; anchors.verticalCenter: parent.verticalCenter; width: Math.max(0, parent.width - x); spacing: 4
                                UiText { width: parent.width; text: title; textFormat: Text.PlainText; color: Theme.text; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                UiText { width: parent.width; text: artist; textFormat: Text.PlainText; color: Theme.secondary; font.pixelSize: 13; elide: Text.ElideRight }
                            }
                        }
                        onClicked: { workspace.chooseTarget(trackKey); if (multiSelect.checked) workspace.toggleTrack(trackKey) }
                    }
                }
                ShellButton {
                    objectName: "batchLyricsSearchButton"
                    Layout.fillWidth: true; implicitHeight: 34
                    text: lyricsSearchBridge.busy ? "停止查找" : "开始查找"
                    enabled: lyricsSearchBridge.busy || (workspace.hasTarget && !workspace.taskNeedsAction && workspace.searchKeys().length > 0)
                    onClicked: workspace.startTargetSearch()
                }
                Label { Layout.fillWidth: true; text: lyricsSearchBridge.status; textFormat: Text.PlainText; color: Theme.secondary; wrapMode: Text.Wrap; font.pixelSize: 13; maximumLineCount: 4; elide: Text.ElideRight }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.preferredWidth: 340; Layout.minimumWidth: 250; Layout.fillHeight: true; spacing: workspace.compact ? 5 : 8
            Label { text: "查找 / 更换歌词"; color: Theme.text; font.pixelSize: 16; font.weight: Font.DemiBold }
            EntryField { id: songTitle; objectName: "lyricsSearchTitle"; Layout.fillWidth: true; implicitHeight: workspace.compact ? 30 : 34; placeholderText: "歌名"; selectByMouse: true; onAccepted: workspace.startSearch() }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                EntryField { id: songArtist; objectName: "lyricsSearchArtist"; Layout.fillWidth: true; implicitHeight: workspace.compact ? 30 : 34; placeholderText: "歌手（可留空）"; selectByMouse: true; onAccepted: workspace.startSearch() }
                EntryField { id: songAlbum; objectName: "lyricsSearchAlbum"; Layout.fillWidth: true; implicitHeight: workspace.compact ? 30 : 34; placeholderText: "专辑"; selectByMouse: true; onAccepted: workspace.startSearch() }
            }
            EntryField { id: fragment; objectName: "lyricsSearchFragment"; Layout.fillWidth: true; implicitHeight: workspace.compact ? 30 : 34; placeholderText: "记得的歌词片段（可留空）"; selectByMouse: true; onAccepted: workspace.startSearch() }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                ShellButton { objectName: "searchLyricsButton"; text: lyricsSearchBridge.busy ? "查找中…" : "手动搜索"; primary: true; implicitHeight: workspace.compact ? 30 : 34; Layout.fillWidth: true; enabled: workspace.sourceActionsEnabled && !lyricsSearchBridge.busy; onClicked: workspace.startSearch() }
                ShellButton { implicitHeight: workspace.compact ? 30 : 34;text: "更多操作 ⌄"; enabled: workspace.sourceActionsEnabled; onClicked: moreMenu.popup() }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                CheckBox { id: syncedOnly; objectName: "lyricsSyncedOnly"; onToggled: lyricsSearchBridge.selectRecommended(checked); text: "只看滚动歌词"; implicitHeight: workspace.compact ? 28 : 34; font.pixelSize: 14 }
                CheckBox { id: replaceExisting; objectName: "lyricsReplace"; text: "替换已有歌词"; implicitHeight: workspace.compact ? 28 : 34; font.pixelSize: 14 }
            }
            Label { text: "候选版本"; color: Theme.text; font.weight: Font.DemiBold }
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 90
                radius: 10; color: Theme.panel; border.color: Theme.border
                ListView {
                    id: candidates; objectName: "lyricsCandidateList"; anchors.fill: parent; anchors.margins: 6
                    clip: true; model: lyricsSearchBridge; boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        required property int index
                        required property string candidateKey
                        required property string title
                        required property string artist
                        required property string album
                        required property string source
                        required property bool timed
                        width: ListView.view.width; height: (!syncedOnly.checked || timed) ? 96 : 0; padding: 10
                        visible: !syncedOnly.checked || timed
                        highlighted: lyricsSearchBridge.selectedKey.endsWith(":" + candidateKey)
                        background: Rectangle { radius: 7; color: parent.highlighted ? Theme.selected : parent.hovered ? Theme.hover : "transparent" }
                        objectName: "lyricsCandidate-" + index
                        contentItem: Item {
                            implicitHeight: candidateText.implicitHeight
                            Column {
                                id: candidateText; objectName: "lyricsCandidateText-" + index; anchors.verticalCenter: parent.verticalCenter; width: parent.width; spacing: 4
                                RowLayout {
                                    width: parent.width; spacing: 6
                                    UiText { Layout.fillWidth: true; text: title; textFormat: Text.PlainText; color: Theme.text; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                    UiText { objectName: "lyricsRecommended-" + index; text: "★ 最推荐"; visible: index === 0; color: Theme.accent; font.pixelSize: 12 }
                                }
                                UiText { width: parent.width; text: artist + (album.length ? " · " + album : ""); textFormat: Text.PlainText; color: Theme.secondary; font.pixelSize: 13; elide: Text.ElideRight }
                                UiText { width: parent.width; text: source + (timed ? " · 滚动歌词" : " · 普通歌词"); textFormat: Text.PlainText; color: Theme.secondary; font.pixelSize: 13; elide: Text.ElideRight }
                            }
                        }
                        onClicked: { lyricsSearchBridge.select(index); previewTabs.currentIndex = 0 }
                    }
                    Label { anchors.centerIn: parent; width: parent.width - 24; visible: candidates.count === 0; text: lyricsSearchBridge.busy ? "正在查找候选版本…" : "暂无候选版本\n可修改歌名或歌手后搜索，也可以导入或粘贴本地歌词。"; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 14 }
                }
            }
            RowLayout {
                objectName: "lyricsApplyRow"; Layout.fillWidth: true; spacing: 8
                ShellButton {
                    objectName: "lyricsInstrumentalButton"; Layout.fillWidth: true; Layout.preferredWidth: 35; Layout.minimumWidth: 0
                    implicitHeight: 34; text: "标记为纯音乐"; font.pixelSize: workspace.compact ? 12 : 13; leftPadding: 5; rightPadding: 5
                    enabled: workspace.sourceActionsEnabled
                    onClicked: { instrumentalConfirm.exactTarget = lyricsSearchBridge.targetTrackKey; instrumentalConfirm.open() }
                }
                ShellButton {
                    objectName: "lyricsApplyButton"; Layout.fillWidth: true; Layout.preferredWidth: 65; Layout.minimumWidth: 0
                    implicitHeight: 34; text: "使用选中版本"; leftPadding: 5; rightPadding: 5
                    enabled: lyricsSearchBridge.selectedKey.length > 0
                    onClicked: workspace.applySelection()
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.preferredWidth: 360; Layout.minimumWidth: 250; Layout.fillHeight: true; spacing: workspace.compact ? 5 : 8
            Label { text: "歌词与翻译"; color: Theme.text; font.pixelSize: 16; font.weight: Font.DemiBold }
            ShellButton {
                objectName: "lyricsFileOffsetButton"; Layout.fillWidth: true; implicitHeight: 32; text: "编辑文件时间偏移…"
                enabled: workspace.hasTarget && !lyricsSearchBridge.busy && !lyricsSearchBridge.batchRunning
                onClicked: if (lyricsSearchBridge.beginFileOffset()) { offsetEditor.exactTarget = lyricsSearchBridge.targetTrackKey; fileOffsetAmount.value = 0; offsetDiscard.checked = false; offsetEditor.open() }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 2
                Repeater {
                    model: ["待判断", "需要翻译", "无需翻译"]
                    delegate: ShellButton {
                        required property int index; required property string modelData
                        Layout.fillWidth: true; text: modelData; checkable: true
                        implicitHeight: workspace.compact ? 30 : 36
                        checked: translationBridge.classification === index
                        primary: checked
                        enabled: workspace.hasTarget && !translationBridge.pending && !translationBridge.preview
                        onClicked: translationBridge.classify(index)
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                ShellButton { objectName: "translateAiButton"; Layout.fillWidth: true; implicitHeight: 34; text: "AI 翻译整首"; primary: true; enabled: workspace.hasTarget && translationBridge.classification === 1 && !workspace.taskNeedsAction; onClicked: workspace.prepareTranslation("") }
                ShellButton { implicitHeight:34;text: "浏览器 ⌄"; enabled: workspace.hasTarget && !workspace.taskNeedsAction; onClicked: browserMenu.popup() }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                Label { text: "原文语言"; color: Theme.secondary; font.pixelSize: 13 }
                ChoiceBox { id: sourceLanguage; objectName: "lyricsSourceLanguage"; Layout.fillWidth: true; implicitHeight: 30; model: ["自动识别", "英语", "日语"]; font.pixelSize: 13 }
                ShellButton { implicitHeight:30;text: "取消任务"; visible: translationBridge.pending; onClicked: translationBridge.cancel() }
            }
            RowLayout {
                Layout.fillWidth: true; visible: translationBridge.pending
                ProgressBar { Layout.fillWidth: true; from: 0; to: Math.max(1, translationBridge.totalLines); value: translationBridge.completedLines; indeterminate: translationBridge.completedLines === 0 }
                Label { text: translationBridge.completedLines + " / " + translationBridge.totalLines; color: Theme.secondary; font.pixelSize: 13 }
            }
            ShellButton { Layout.fillWidth: true; implicitHeight: 32; text: "继续人工验证"; visible: translationBridge.state === "NeedsAttention"; onClicked: translationBridge.resume() }
            Label { Layout.fillWidth: true; text: translationBridge.status; textFormat: Text.PlainText; color: Theme.secondary; wrapMode: Text.Wrap; font.pixelSize: 13; maximumLineCount: workspace.compact ? 2 : 3; elide: Text.ElideRight }
            TabBar {
                id: previewTabs; objectName: "lyricsPreviewTabs"; Layout.fillWidth: true; implicitHeight: 34
                SettingsTabButton { text: "歌词原文"; implicitHeight: 34 }
                SettingsTabButton { text: "双语预览"; implicitHeight: 34 }
            }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 80; clip: true
                TextArea {
                    objectName: "lyricsWorkspacePreview"; readOnly: true; selectByMouse: true
                    textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap; font.pixelSize: 14
                    color: Theme.text; background: Rectangle { radius: 8; color: Theme.panel; border.color: Theme.border }
                    text: previewTabs.currentIndex === 1 ? translationBridge.previewText : lyricsSearchBridge.previewText.length ? lyricsSearchBridge.previewText : lyricsSearchBridge.sourceText
                    placeholderText: previewTabs.currentIndex === 1 ? "译文完成后在这里预览，确认保存后才会写入本地歌词。" : "选中候选版本查看原文，或查看已有的本地歌词。"
                }
            }
            RowLayout {
                Layout.fillWidth: true
                ShellButton { objectName: "saveTranslationButton"; Layout.fillWidth: true; implicitHeight: 34; text: "保存 AI 翻译"; primary: true; enabled: translationBridge.preview; onClicked: translationBridge.save() }
                ShellButton {implicitHeight:34;objectName: "discardTranslationButton"; text: "放弃预览"; enabled: translationBridge.preview; onClicked: discardConfirm.open() }
            }
        }
    }
    footer: Item {
        implicitHeight: 28
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 18
            Label { Layout.fillWidth: true; text: lyricsSearchBridge.batchRunning ? "纯音乐处理：剩余 " + lyricsSearchBridge.batchResult.pending + " 首，可点击取消停止后续写入。" : workspace.localStatus || (translationBridge.preview ? "译文尚未保存。切换歌曲或关闭窗口后，本次会话仍会保留预览。" : "选择歌曲后查找歌词；当前播放不会改变。"); color: Theme.secondary; font.pixelSize: 13; wrapMode: Text.Wrap }

        }
    }
    RoundMenu {
        id: moreMenu
        RoundMenuItem { text: "导入本地歌词…"; onTriggered: { workspace.fileTargetKey = lyricsSearchBridge.targetTrackKey; workspace.fileEditToken = lyricsSearchBridge.beginLocalEdit(); if (workspace.fileEditToken.length) localLyrics.open() } }
        RoundMenuItem { text: "粘贴歌词…"; onTriggered: { pasteDialog.exactTarget = lyricsSearchBridge.targetTrackKey; pasteDialog.editToken = lyricsSearchBridge.beginLocalEdit(); pasted.text = lyricsSearchBridge.draftText; if (pasteDialog.editToken.length) pasteDialog.open() } }
    }
    RoundMenu {
        id: browserMenu
        RoundMenuItem { text: "必应翻译"; onTriggered: workspace.prepareTranslation("bing") }
        RoundMenuItem { text: "百度翻译"; onTriggered: workspace.prepareTranslation("baidu") }
    }
    FileDialog {
        id: localLyrics; objectName: "lyricsImportDialog"; title: "导入本地歌词"; fileMode: FileDialog.OpenFile; nameFilters: ["歌词 (*.lrc *.txt)"]
        onAccepted: {
            if (workspace.fileTargetKey !== lyricsSearchBridge.targetTrackKey) return
            lyricsSearchBridge.importLocalLyrics(selectedFile, true, workspace.fileEditToken)
        }
    }
    ConfirmDialog {
        id: consent; parent: workspace.parent; objectName: "translationConsent"
        property string exactTask: ""; property string exactTarget: "0"; property bool browser: false
        anchors.centerIn: parent; width: Math.min(490, parent.width - 40); modal: true
        title: browser ? "开始浏览器翻译？" : "发送整首歌词至 DeepSeek？"
        contentItem: Label { wrapMode: Text.Wrap; text: consent.browser ? "将向所选翻译网站提交歌词。遇到登录或验证时，请在打开的浏览器中操作。返回结果先保留为未保存预览。" : "将把这首歌的歌词、目标语言和翻译风格发送至 DeepSeek 官方服务，可能产生费用。不会上传音频、封面或本地路径。本次仅发送一次请求；译文需要你确认保存。" }
        footer: DialogButtonBox {
            ShellButton { objectName: "consentDeclineButton"; text: "不发送"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            ShellButton { objectName: "consentAcceptButton"; text: consent.browser ? "开始浏览器翻译" : "发送一次整首歌词"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            onAccepted: consent.accept(); onRejected: consent.reject()
        }
        onAccepted: if (exactTarget === translationBridge.targetTrackKey) translationBridge.start(exactTask)
        onClosed: translationBridge.declineConsent()
    }
    ConfirmDialog {
        id: leaveConfirm; parent: workspace.parent; anchors.centerIn: parent; width: Math.min(440, parent.width - 40); modal: true
        title: translationBridge.preview ? "暂时关闭未保存的预览？" : "关闭歌词窗口？"; standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "任务和未保存的预览会保留在本次会话中，重新打开这首歌可以继续处理。关闭这个窗口不会自动保存或放弃译文。"; wrapMode: Text.Wrap }
        onAccepted: { workspace.allowClose = true; workspace.close() }
    }
    ConfirmDialog {
        id: discardConfirm; parent: workspace.parent; anchors.centerIn: parent; width: Math.min(440, parent.width - 40); modal: true
        title: "放弃未保存的译文？"; standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "将放弃这首歌的翻译预览，已保存的本地歌词会保留。已发送的云端请求可能仍产生费用。"; wrapMode: Text.Wrap }
        onAccepted: translationBridge.discard()
    }
    ConfirmDialog {
        id: replaceConfirm; objectName: "lyricsTranslationConflict"; parent: workspace.parent
        property string exactToken: ""; property string reason: ""
        anchors.centerIn: parent; width: Math.min(460, parent.width - 40); modal: true; title: "替换歌词"
        contentItem: Label { text: replaceConfirm.reason; wrapMode: Text.Wrap }
        footer: DialogButtonBox {
            ShellButton { objectName: "lyricsReplaceExplicit"; text: "明确替换"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            ShellButton { objectName: "lyricsKeepExisting"; text: "保留现有版本"; onClicked: { lyricsSearchBridge.resolveReplacement(replaceConfirm.exactToken, 2); replaceConfirm.close() } }
            ShellButton { text: "取消"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: lyricsSearchBridge.resolveReplacement(exactToken, 1)
        onClosed: lyricsSearchBridge.cancelReplacement(exactToken)
    }
    ConfirmDialog {
        id: pasteDialog; parent: workspace.parent; property string exactTarget: "0"; property string editToken: ""
        anchors.centerIn: parent; width: Math.min(580, parent.width - 40); modal: true; title: "粘贴歌词"; standardButtons: Dialog.Save | Dialog.Cancel
        contentItem: ColumnLayout {
            Label { text: "保存会替换这首歌已有的本地歌词。"; color: Theme.secondary; wrapMode: Text.Wrap }
            ScrollView { Layout.fillWidth: true; Layout.preferredHeight: 250; TextArea { id: pasted; objectName: "lyricsPasteText"; placeholderText: "粘贴 LRC 或普通歌词"; textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap; selectByMouse: true } }
        }
        onAccepted: if (exactTarget === lyricsSearchBridge.targetTrackKey) lyricsSearchBridge.saveText(pasted.text, true, editToken)
    }
    ConfirmDialog {
        id: instrumentalConfirm; parent: workspace.parent; property string exactTarget: "0"
        anchors.centerIn: parent; width: Math.min(440, parent.width - 40); modal: true; title: "确认为纯音乐？"; standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "仅在确定没有人声歌词时使用。这会把本地歌词替换为纯音乐标记。"; wrapMode: Text.Wrap }
        onAccepted: if (exactTarget === lyricsSearchBridge.targetTrackKey) lyricsSearchBridge.markInstrumental(true)
    }
    ConfirmDialog {
        id: offsetEditor; objectName: "lyricsFileOffsetDialog"; parent: workspace.parent
        property string exactTarget: "0"
        readonly property var info: lyricsSearchBridge.fileOffset
        anchors.centerIn: parent; width: Math.min(760,parent.width-40); height: Math.min(640,parent.height-36)
        modal: true; title: "歌词文件时间偏移 · " + (info.title || "")
        contentItem: ColumnLayout {
            spacing: 8
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: "当前文件 offset：" + (offsetEditor.info.existingMs || 0) + " ms。这里保存 LRC 的绝对 [offset:]，不修改播放器的显示延时。" }
            RowLayout {
                Label { text: "文件偏移（ms）" }
                SpinBox { id: fileOffsetAmount; objectName: "lyricsFileOffsetAmount"; from: -5000; to: 5000; stepSize: 100; editable: true; onValueModified: lyricsSearchBridge.previewFileOffset(value) }
                Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: "正数延后，负数提前；0 保留原文件偏移。" }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: "预览（文件生效时间；未叠加播放显示延时）\n" + (offsetEditor.info.timeline || "") }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                TextArea { objectName: "lyricsFileOffsetPreview"; readOnly: true; selectByMouse: true; textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap; text: offsetEditor.info.preview || "" }
            }
            CheckBox { id: offsetDiscard; objectName: "lyricsFileOffsetTaskConsent"; visible: !!offsetEditor.info.confirmationRequired; text: "明确保存偏移；仅成功后放弃该曲翻译任务或未保存译文" }
            Label { objectName: "lyricsFileOffsetError"; Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.accent; text: offsetEditor.info.error || ""; visible: text.length > 0 }
        }
        footer: DialogButtonBox {
            ShellButton { objectName: "lyricsFileOffsetCancel"; text: "取消"; onClicked: offsetEditor.reject() }
            ShellButton { objectName: "lyricsFileOffsetSave"; text: "保存到歌词文件"; enabled: !!offsetEditor.info.valid && (!offsetEditor.info.confirmationRequired || offsetDiscard.checked); onClicked: if (lyricsSearchBridge.saveFileOffset(offsetDiscard.checked)) offsetEditor.close() }
        }
        onClosed: lyricsSearchBridge.cancelFileOffset()
        Connections { target: lyricsSearchBridge; function onChanged() { if (offsetEditor.opened && offsetEditor.exactTarget !== lyricsSearchBridge.targetTrackKey) offsetEditor.close() } }
    }
}
