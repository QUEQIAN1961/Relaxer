import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"
import "../theme"

Item {
    id: page
    objectName: "profilePage"
    signal lyricsRequested(var keys)
    signal bulkChanged(bool active, var keys)
    property int avatarRevision: 0
    readonly property bool compactHeader:Theme.compact||width<620
    property real headerProgress:compactHeader?1:0
    Behavior on headerProgress {NumberAnimation{duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.OutCubic}}
    property bool statisticsExpanded:false
    property real collapseProgress:!compactHeader&&statisticsExpanded?0:1
    onVisibleChanged: { if(!visible)trackList.clearBulk();statisticsExpanded=false }
    Connections { target:Theme;function onCompactChanged(){page.statisticsExpanded=false} }
    readonly property var checkedKeys: trackList.checkedKeys
    readonly property bool bulkMode: trackList.bulkMode
    function setBulkKeys(keys) { trackList.checkedKeys=keys.slice(); trackList.bulkChanged(true,keys) }
    function clearBulk() { trackList.clearBulk() }
    function heardText(ms) {
        const minutes = Math.floor(Math.max(0, ms || 0) / 60000)
        return minutes >= 60 ? Math.floor(minutes / 60) + " 小时 " + minutes % 60 + " 分" : minutes + " 分钟"
    }
    function selectTab(scope) {
        trackList.clearBulk()
        libraryBridge.openPlaylist(scope)
    }
    Behavior on collapseProgress { NumberAnimation { duration: settingsBridge.values.reduceMotion ? 0 : 180; easing.type: Easing.OutCubic } }
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pageMargin; anchors.topMargin: 28; anchors.rightMargin: Theme.pageMargin; anchors.bottomMargin: 0
        spacing: 20
        UiText { text: "个人"; color: Theme.text; font.pixelSize: 28; font.weight: Font.DemiBold }
        Item {
            id: profileHeader
            objectName: "profileHeader"
            Layout.fillWidth: true; Layout.preferredHeight:(192-112*page.collapseProgress)*(1-page.headerProgress)+110*page.headerProgress
            Artwork {
                objectName: "profileAvatar"; x: 0; y: 0; width:76-20*page.headerProgress; height: width; radius: 38; resolution: 256
                file: libraryBridge.profile.avatar || ""; seed: libraryBridge.profile.nickname || "Relaxer Listener"
                revision: String(page.avatarRevision)
                TapHandler { onTapped: avatarPicker.open() }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
            UiText {
                objectName: "profileName"; x:96-24*page.headerProgress; y: 4
                width: page.compactHeader?Math.max(0,profileHeader.width-x-editProfile.width-12):Math.max(120,profileHeader.width-96-editProfile.width-18-238*page.collapseProgress)
                text: libraryBridge.profile.nickname || "Relaxer Listener"; textFormat: Text.PlainText
                font.pixelSize: 22; font.weight: Font.DemiBold; color: Theme.text; elide: Text.ElideRight
            }
            UiText {
                id: biography; objectName: "profileDescription"; x:96-24*page.headerProgress; y:page.compactHeader?34:48
                width:page.compactHeader?Math.max(0,profileHeader.width-x-editProfile.width-12):Math.min(325,Math.max(120,profileHeader.width-96-editProfile.width-18-238*page.collapseProgress))
                text: libraryBridge.profile.description || ""; textFormat: Text.PlainText
                font.pixelSize: 14; color: Theme.secondary; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
                HoverHandler { id: bioHover }
                HintBubble {visible: bioHover.hovered && biography.truncated;text: biography.text}
            }
            ShellButton {
                id: editProfile; objectName: "profileEdit"; anchors.right: parent.right; y:page.compactHeader?8:18; implicitHeight: Theme.controlHeight
                text: "编辑资料"
                onClicked: { nickname.text = libraryBridge.profile.nickname || ""; description.text = libraryBridge.profile.description || ""; editError.text = ""; profileEditor.open() }
            }
            Repeater {
                model: [
                    {caption:"歌曲数量", value: String(libraryBridge.statistics.trackCount || 0)},
                    {caption:"累计循环", value: String(libraryBridge.statistics.totalPlays || 0)},
                    {caption:"累计时长", value: page.heardText(libraryBridge.statistics.totalHeard)}
                ]
                delegate: Item {
                    required property int index
                    required property var modelData
                    readonly property real progress: page.collapseProgress
                    readonly property real expandedWidth:(profileHeader.width-40)/3
                    readonly property real compactWidth:(profileHeader.width-24)/3
                    objectName:"profileStatisticCell-"+index
                    x:(index*(expandedWidth+20)*(1-progress)+(profileHeader.width-editProfile.width-18-220)*progress)*(1-page.headerProgress)+index*(compactWidth+12)*page.headerProgress
                    y:(96*(1-progress)+(5+index*24)*progress)*(1-page.headerProgress)+74*page.headerProgress
                    width:(expandedWidth*(1-progress)+220*progress)*(1-page.headerProgress)+compactWidth*page.headerProgress
                    height:(96*(1-progress)+22*progress)*(1-page.headerProgress)+28*page.headerProgress
                    Rectangle { anchors.fill: parent; radius: 14; color: Theme.card; opacity: 1 - parent.progress }
                    UiText {
                        id:statCaption;objectName:"profileStatisticCaption-"+index
                        x:22*(1-parent.progress)*(1-page.headerProgress)
                        y:60*(1-parent.progress)*(1-page.headerProgress)+(parent.height-implicitHeight)/2*page.headerProgress
                        width:Math.min(implicitWidth,Math.max(0,parent.width-30))
                        text: modelData.caption + (parent.progress > .5 ? "：" : "")
                        color:Theme.secondary;font.pixelSize:14-page.headerProgress;elide:Text.ElideRight
                    }
                    UiText {
                        objectName:"profileStatistic"
                        x:(22*(1-parent.progress)+108*parent.progress)*(1-page.headerProgress)+(statCaption.width+4)*page.headerProgress
                        y:18*(1-parent.progress)*(1-page.headerProgress)+(parent.height-implicitHeight)/2*page.headerProgress
                        width:Math.max(0,parent.width-x-10*(1-page.headerProgress));text:modelData.value; textFormat: Text.PlainText
                        color: Theme.text; font.pixelSize: 28 - 16 * parent.progress; font.weight: parent.progress < .5 ? Font.Bold : Font.Normal
                        elide: Text.ElideRight
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            ShellButton { objectName: "profileRecent"; text: "最近播放"; primary: libraryBridge.playlistId !== "-3"; implicitHeight: 36; onClicked: page.selectTab("-2") }
            ShellButton { objectName: "profileFrequent"; text: "最常播放"; primary: libraryBridge.playlistId === "-3"; implicitHeight: 36; onClicked: page.selectTab("-3") }
            Item { Layout.fillWidth: true }
        }
        TrackList {
            id: trackList; objectName: "recentTable"
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 80
            trackModel: libraryModel; selectedKey: libraryModel.selectedId; currentKey: playbackBridge.trackId
            statisticsMode: true
            emptyHeading: libraryBridge.playlistId === "-3" ? "还没有经常播放的歌曲" : "还没有最近播放记录"
            emptyDescription: "从本地音乐或歌单开始播放，听过的歌曲会出现在这里。"
            onScrollYChanged:if(scrollY>0)page.statisticsExpanded=false
            onSelected: key => libraryModel.selectedId = key
            onPlayRequested: key => libraryBridge.play(key)
            onFavoriteRequested: key => libraryBridge.toggleFavorite(key)
            onLyricsRequested: keys => page.lyricsRequested(keys)
            onBulkChanged: (active, keys) => page.bulkChanged(active, keys)
        }
    }
    WheelHandler {
        target:null;blocking:true;enabled:!page.compactHeader&&page.visible
        onWheel:event=>{
            const delta=event.angleDelta.y||event.pixelDelta.y
            if(delta>0&&trackList.scrollY<=0&&!page.statisticsExpanded){page.statisticsExpanded=true;event.accepted=true}
            else if(delta<0&&page.statisticsExpanded){page.statisticsExpanded=false;event.accepted=true}
            else event.accepted=false
        }
    }
    DragHandler {
        target:null;enabled:!page.compactHeader&&page.visible;xAxis.enabled:false
        onActiveTranslationChanged:if(activeTranslation.y>48&&trackList.scrollY<=0)page.statisticsExpanded=true
    }
    FileDialog {
        id: avatarPicker; objectName: "profileAvatarDialog"; title: "选择头像"; fileMode: FileDialog.OpenFile
        nameFilters: ["图片 (*.png *.jpg *.jpeg *.webp)"]
        onAccepted: if (libraryBridge.setProfileAvatar(selectedFile)) ++page.avatarRevision
    }
    ConfirmDialog {
        id: profileEditor; objectName: "profileEditDialog"; parent: page.Overlay.overlay
        anchors.centerIn: parent; width: Math.min(580, parent.width - 40); modal: true; title: "编辑资料"
        contentItem: ColumnLayout {
            spacing: 10
            Label { text: "昵称"; color: Theme.secondary }
            EntryField { id: nickname; objectName: "profileNicknameEdit"; Layout.fillWidth: true; maximumLength: 60; selectByMouse: true }
            Label { text: "个人简介"; color: Theme.secondary }
            ScrollView { Layout.fillWidth: true; Layout.preferredHeight: 150; EntryArea { id: description; objectName: "profileDescriptionEdit"; textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap; selectByMouse: true } }
            Label { text: "简介最多 1000 字"; color: Theme.secondary; font.pixelSize: 13 }
            Label { id: editError; Layout.fillWidth: true; visible: text.length > 0; color: Theme.accent; wrapMode: Text.Wrap }
        }
        footer: DialogButtonBox {
            background: null
            standardButtons: Dialog.Cancel
            ShellButton { text: "保存"; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: { if (libraryBridge.saveProfile(nickname.text, description.text)) profileEditor.accept(); else editError.text = libraryBridge.error } }
            onRejected: profileEditor.reject()
        }
    }
}
