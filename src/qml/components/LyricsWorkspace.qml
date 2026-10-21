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
    function open() { aboutToShow(); visible = true; forceActiveFocus() }
    function close() { visible = false; closed() }
    padding: 18
    spacing: 12
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
    Connections { target: lyricsSearchBridge; function onChanged() { workspace.syncTargets() } }
    property bool allowClose: false
    property string localStatus: ""
    property string fileTargetKey: "0"
    readonly property bool compact: height < 680
    readonly property bool hasTarget: lyricsSearchBridge.targetTrackKey !== "0"
    readonly property bool taskNeedsAction: translationBridge.pending || translationBridge.preview
    readonly property bool sourceActionsEnabled: hasTarget && !taskNeedsAction
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
        enabled: workspace.opened && !consent.opened && !leaveConfirm.opened && !replaceConfirm.opened && !discardConfirm.opened && !pasteDialog.opened && !instrumentalConfirm.opened
        onActivated: workspace.requestClose()
    }
    background: Rectangle { color: Theme.background }
    header: Item {
        implicitHeight: 45
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 12
            ShellButton { objectName: "lyricsWorkspaceClose"; text: "‹ 返回"; flat: true; implicitHeight: 36; Accessible.name: "返回上一页"; onClicked: workspace.requestClose() }
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
                CheckBox { id: missingLyrics; objectName: "lyricsMissingOnly"; text: "仅显示无歌词"; implicitHeight: 28; onToggled: if (checked) missingTranslation.checked = false }
                CheckBox { id: missingTranslation; objectName: "lyricsMissingTranslationOnly"; text: "仅显示缺失翻译"; implicitHeight: 28; onToggled: if (checked) missingLyrics.checked = false }
                RowLayout {
                    Layout.fillWidth: true; spacing: 2
                    CheckBox { id: multiSelect; objectName: "lyricsMultiSelect"; text: "多选"; implicitHeight: 30; onToggled: if (!checked) workspace.selectedTracks = [] }
                    ShellButton { objectName: "lyricsSelectAll"; text: "全选"; flat: true; implicitHeight: 30; visible: multiSelect.checked; onClicked: workspace.selectedTracks = workspace.visibleKeys() }
                    ShellButton { objectName: "lyricsClearSelection"; text: "清空"; flat: true; implicitHeight: 30; visible: multiSelect.checked; onClicked: workspace.selectedTracks = [] }
                }
                ListView {
                    objectName: "lyricsTargetTracks"
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
                CheckBox { id: syncedOnly; objectName: "lyricsSyncedOnly"; text: "只看滚动歌词"; implicitHeight: workspace.compact ? 28 : 34; font.pixelSize: 14 }
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
            ShellButton {
                objectName: "lyricsApplyButton"; Layout.fillWidth: true; implicitHeight: 34; text: "使用选中版本"
                enabled: lyricsSearchBridge.selectedKey.length > 0
                onClicked: { replaceConfirm.exactKey = lyricsSearchBridge.selectedKey; replaceConfirm.exactTarget = lyricsSearchBridge.targetTrackKey; replaceConfirm.open() }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.preferredWidth: 360; Layout.minimumWidth: 250; Layout.fillHeight: true; spacing: workspace.compact ? 5 : 8
            Label { text: "歌词与翻译"; color: Theme.text; font.pixelSize: 16; font.weight: Font.DemiBold }
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
        implicitHeight: 45
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 18
            Label { Layout.fillWidth: true; text: workspace.localStatus || (translationBridge.preview ? "译文尚未保存。切换歌曲或关闭窗口后，本次会话仍会保留预览。" : "选择歌曲后查找歌词；当前播放不会改变。"); color: Theme.secondary; font.pixelSize: 13; wrapMode: Text.Wrap }

        }
    }
    RoundMenu {
        id: moreMenu
        RoundMenuItem { text: "导入本地歌词…"; onTriggered: { workspace.fileTargetKey = lyricsSearchBridge.targetTrackKey; localLyrics.open() } }
        RoundMenuItem { text: "粘贴歌词…"; onTriggered: { pasteDialog.exactTarget = lyricsSearchBridge.targetTrackKey; pasted.text = ""; pasteDialog.open() } }
        RoundMenuItem { text: "标记为纯音乐…"; onTriggered: { instrumentalConfirm.exactTarget = lyricsSearchBridge.targetTrackKey; instrumentalConfirm.open() } }
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
            importConfirm.file = selectedFile; importConfirm.exactTarget = workspace.fileTargetKey; importConfirm.open()
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
        id: replaceConfirm; parent: workspace.parent; property string exactKey: ""; property string exactTarget: "0"
        anchors.centerIn: parent; width: Math.min(460, parent.width - 40); modal: true; title: "使用所选歌词版本？"; standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "所选版本会替换这首歌的本地歌词。该歌曲的翻译任务或未保存预览会被放弃；已经发送的请求可能仍产生费用。"; wrapMode: Text.Wrap }
        onAccepted: {
            if (exactTarget !== lyricsSearchBridge.targetTrackKey || exactKey !== lyricsSearchBridge.selectedKey) return
            if (translationBridge.taskKey.length) translationBridge.discard()
            lyricsSearchBridge.saveSelected(exactKey, true)
        }
    }
    ConfirmDialog {
        id: importConfirm; parent: workspace.parent; property url file; property string exactTarget: "0"
        anchors.centerIn: parent; width: Math.min(440, parent.width - 40); modal: true; title: "导入并替换本地歌词？"; standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "将保存所选歌词文件到这首歌，替换已有的本地歌词。"; wrapMode: Text.Wrap }
        onAccepted: if (exactTarget === lyricsSearchBridge.targetTrackKey) lyricsSearchBridge.importLocalLyrics(file, true)
    }
    ConfirmDialog {
        id: pasteDialog; parent: workspace.parent; property string exactTarget: "0"
        anchors.centerIn: parent; width: Math.min(580, parent.width - 40); modal: true; title: "粘贴歌词"; standardButtons: Dialog.Save | Dialog.Cancel
        contentItem: ColumnLayout {
            Label { text: "保存会替换这首歌已有的本地歌词。"; color: Theme.secondary; wrapMode: Text.Wrap }
            ScrollView { Layout.fillWidth: true; Layout.preferredHeight: 250; TextArea { id: pasted; objectName: "lyricsPasteText"; placeholderText: "粘贴 LRC 或普通歌词"; textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap; selectByMouse: true } }
        }
        onAccepted: if (exactTarget === lyricsSearchBridge.targetTrackKey) lyricsSearchBridge.saveText(pasted.text, true)
    }
    ConfirmDialog {
        id: instrumentalConfirm; parent: workspace.parent; property string exactTarget: "0"
        anchors.centerIn: parent; width: Math.min(440, parent.width - 40); modal: true; title: "确认为纯音乐？"; standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: "仅在确定没有人声歌词时使用。这会把本地歌词替换为纯音乐标记。"; wrapMode: Text.Wrap }
        onAccepted: if (exactTarget === lyricsSearchBridge.targetTrackKey) lyricsSearchBridge.markInstrumental(true)
    }
}
