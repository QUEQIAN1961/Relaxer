import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import QtQuick.Effects
import "../components"
import "../theme"

Item {
    id: page
    objectName: "profilePage"
    property bool firstFramePresented:false
    Connections {
        target:page.Window.window
        function onFrameSwapped(){if(page.visible)page.firstFramePresented=true}
    }
    signal lyricsRequested(var keys)
    signal bulkChanged(bool active, var keys)
    property int avatarRevision: 0
    readonly property bool compactHeader:Theme.compact||width<620
    property real headerProgress:compactHeader?1:0
    Behavior on headerProgress {enabled:page.firstFramePresented&&page.visible;NumberAnimation{duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.OutCubic}}
    property bool statisticsExpanded:false
    property real collapseProgress:!compactHeader&&statisticsExpanded?0:1
    onVisibleChanged: { if(!visible)trackList.clearBulk();statisticsExpanded=false }
    Connections { target:Theme;function onCompactChanged(){page.statisticsExpanded=false} }
    readonly property var checkedKeys: trackList.checkedKeys
    readonly property bool bulkMode: trackList.bulkMode
    function setBulkKeys(keys) { trackList.checkedKeys=keys.slice(); trackList.bulkChanged(true,keys) }
    function clearBulk() { trackList.clearBulk() }
    function handleStatisticsWheel(event) {
        if(page.compactHeader||!page.visible)return
        const delta=event.pixelDelta.y||event.angleDelta.y
        if(delta>0&&trackList.atScrollStart&&!page.statisticsExpanded){page.statisticsExpanded=true;event.accepted=true}
        else if(delta<0&&page.statisticsExpanded){page.statisticsExpanded=false;event.accepted=true}
    }
    function heardText(ms) {
        if(Theme.compact)return (Math.max(0,Number(ms)||0)/3600000).toFixed(1).replace(/\.0$/,"")+"h"
        const minutes = Math.floor(Math.max(0, ms || 0) / 60000)
        return minutes >= 60 ? Math.floor(minutes / 60) + " 小时 " + minutes % 60 + " 分" : minutes + " 分钟"
    }
    function selectTab(scope) {
        trackList.clearBulk()
        libraryBridge.openPlaylist(scope)
    }
    function openProfileEditor(){
        nickname.text=libraryBridge.profile.nickname||""
        description.text=libraryBridge.profile.description||""
        profileEditor.pendingAvatar="";editError.text="";profileEditor.open()
    }
    Behavior on collapseProgress {enabled:page.firstFramePresented&&page.visible; NumberAnimation { duration: settingsBridge.values.reduceMotion ? 0 : 180; easing.type: Easing.OutCubic } }
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pageMargin; anchors.topMargin: 28; anchors.rightMargin: Theme.pageMargin; anchors.bottomMargin: 0
        spacing: 20
        UiText { text: "个人"; color: Theme.text; font.pixelSize: 28; font.weight: Font.DemiBold }
        Item {
            id: profileHeader
            objectName: "profileHeader"
            readonly property real compactStatisticsTop:identity.height+20
            Layout.fillWidth: true; Layout.preferredHeight:Math.max(identity.height,(192-112*page.collapseProgress)*(1-page.headerProgress)+(compactStatisticsTop+36)*page.headerProgress)
            Item {
                id:identity;objectName:"profileIdentity"
                width:Math.max(0,profileHeader.width-profileTabs.width-18-(page.compactHeader?0:238*page.collapseProgress))
                readonly property real descriptionTop:page.compactHeader?34:48
                readonly property real contentHeight:Math.max(76-20*page.headerProgress,descriptionTop+biography.implicitHeight)
                readonly property real contentInset:(height-contentHeight)/2
                height:Math.max(contentHeight,profileTabs.height-16)
                Accessible.role:Accessible.Button;Accessible.name:"编辑个人资料";activeFocusOnTab:true
                Keys.onReturnPressed:page.openProfileEditor()
                Keys.onSpacePressed:page.openProfileEditor()
                Loader {
                    anchors.fill:parent;anchors.margins:-8;active:identityMouse.containsMouse&&!Theme.customHighlights
                    sourceComponent:RectangularShadow {radius:12;blur:10;offset:Qt.vector2d(0,2);color:Qt.rgba(0,0,0,Theme.dark?.28:.12)}
                }
                HighlightSurface {anchors.fill:parent;anchors.margins:-8;radius:12;hovered:identityMouse.containsMouse}
                ProfileAvatar {
                    objectName:"profileAvatar";y:identity.contentInset;width:76-20*page.headerProgress;height:width;radius:width/2;resolution:512;
                    file:libraryBridge.profile.avatar||"";seed:libraryBridge.profile.nickname||"Relaxer Listener";revision:String(page.avatarRevision)
                }
                UiText {
                    objectName:"profileName";x:96-24*page.headerProgress;y:4+identity.contentInset;width:Math.max(0,identity.width-x)
                    text:libraryBridge.profile.nickname||"Relaxer Listener";textFormat:Text.PlainText
                    font.pixelSize:22;font.weight:Font.DemiBold;color:Theme.text;elide:Text.ElideRight
                }
                UiText {
                    id:biography;objectName:"profileDescription";x:96-24*page.headerProgress;y:identity.descriptionTop+identity.contentInset
                    width:Math.max(0,Math.min(page.compactHeader?identity.width:325,identity.width-x))
                    text:libraryBridge.profile.description||"";textFormat:Text.PlainText;font.pixelSize:14;color:Theme.secondary
                    wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight
                    HintBubble {visible:identityMouse.containsMouse&&biography.truncated;text:biography.text}
                }
                MouseArea {id:identityMouse;objectName:"profileIdentityMouse";anchors.fill:parent;acceptedButtons:Qt.LeftButton;hoverEnabled:true;cursorShape:Qt.PointingHandCursor;onDoubleClicked:page.openProfileEditor()}
            }
            Column {
                id:profileTabs;objectName:"profileTabs";anchors.right:parent.right;anchors.verticalCenter:identity.verticalCenter;spacing:6
                ShellButton {objectName:"profileRecent";text:"最近播放";primary:libraryBridge.playlistId!=="-3";onClicked:page.selectTab("-2")}
                ShellButton {objectName:"profileFrequent";text:"最长播放";primary:libraryBridge.playlistId==="-3";onClicked:page.selectTab("-3")}
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
                    x:(index*(expandedWidth+20)*(1-progress)+(profileHeader.width-profileTabs.width-18-220)*progress)*(1-page.headerProgress)+index*(compactWidth+12)*page.headerProgress
                    y:(96*(1-progress)+(5+index*24)*progress)*(1-page.headerProgress)+profileHeader.compactStatisticsTop*page.headerProgress
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
        TrackList {
            id: trackList; objectName: "recentTable"
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 80
            trackModel: libraryModel; selectedKey: libraryModel.selectedId; currentKey: playbackBridge.trackId
            statisticsMode: true
            emptyHeading: libraryBridge.playlistId === "-3" ? "还没有累计播放时长记录" : "还没有最近播放记录"
            emptyDescription: "从本地音乐或歌单开始播放，听过的歌曲会出现在这里。"
            onScrollYChanged:if(!trackList.atScrollStart)page.statisticsExpanded=false
            onWheelRequested:event=>page.handleStatisticsWheel(event)
            onSelected: key => libraryModel.selectedId = key
            onPlayRequested: key => libraryBridge.play(key)
            onFavoriteRequested: key => libraryBridge.toggleFavorite(key)
            onLyricsRequested: keys => page.lyricsRequested(keys)
            onBulkChanged: (active, keys) => page.bulkChanged(active, keys)
        }
    }
    WheelHandler {
        target:null;blocking:true;enabled:!page.compactHeader&&page.visible
        onWheel:event=>{event.accepted=false;page.handleStatisticsWheel(event)}
    }
    DragHandler {
        target:null;enabled:!page.compactHeader&&page.visible;xAxis.enabled:false
        onActiveTranslationChanged:if(activeTranslation.y>48&&trackList.scrollY<=0)page.statisticsExpanded=true
    }
    FileDialog {
        id: avatarPicker; objectName: "profileAvatarDialog"; title: "选择头像"; fileMode: FileDialog.OpenFile
        nameFilters: ["图片 (*.png *.jpg *.jpeg *.webp)"]
        onAccepted:profileEditor.pendingAvatar=selectedFile
    }
    ConfirmDialog {
        id:profileEditor;objectName:"profileEditDialog";parent:page.Overlay.overlay
        property url pendingAvatar:""
        anchors.centerIn:parent;width:Math.min(600,parent.width-32);height:Math.min(340,parent.height-32)
        padding:0;modal:true;title:"编辑个人资料";header:null;footer:null
        function save(){
            if(!libraryBridge.saveProfile(nickname.text,description.text)){editError.text=libraryBridge.error;return}
            if(String(pendingAvatar).length){if(!libraryBridge.setProfileAvatar(pendingAvatar)){editError.text=libraryBridge.error;return}++page.avatarRevision}
            accept()
        }
        contentItem:Item {
            Item {
                id:avatarRegion;objectName:"profileEditorAvatarRegion";width:parent.width*.35;height:parent.height
                Button {
                    objectName:"profileChooseAvatar";anchors.fill:parent;hoverEnabled:true;Accessible.name:"更换头像"
                    background:Rectangle {radius:12;color:parent.hovered?Theme.hover:Theme.panel;Rectangle {anchors.right:parent.right;width:12;height:parent.height;color:parent.color}}
                    contentItem:Item {
                        ProfileAvatar {
                            objectName:"profileEditorAvatar";anchors.centerIn:parent;width:Math.max(32,Math.min(parent.width-28,180));height:width;radius:width/2
                            file:String(profileEditor.pendingAvatar).length?profileEditor.pendingAvatar:libraryBridge.profile.avatar||"";seed:nickname.text||"Relaxer Listener"
                            revision:String(page.avatarRevision);resolution:512;
                        }
                        UiText {anchors.horizontalCenter:parent.horizontalCenter;anchors.bottom:parent.bottom;anchors.bottomMargin:20;text:"更换头像";font.pixelSize:13;color:Theme.secondary}
                    }
                    onClicked:avatarPicker.open()
                }
            }
            Item {
                id:nameRegion;objectName:"profileEditorNameRegion";x:avatarRegion.width;width:parent.width-x;height:parent.height*.35
                EntryField {
                    id:nickname;objectName:"profileNicknameEdit";anchors.fill:parent;maximumLength:60;placeholderText:"用户名";selectByMouse:true
                    leftPadding:18;rightPadding:18;verticalAlignment:Text.AlignVCenter;font.pixelSize:20;background:null;onAccepted:profileEditor.save()
                }
            }
            Item {
                id:descriptionRegion;objectName:"profileEditorDescriptionRegion";x:avatarRegion.width;y:nameRegion.height;width:parent.width-x;height:parent.height-y
                ScrollView {
                    id:descriptionScroll;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.bottom:editError.visible?editError.top:editorActions.top;anchors.bottomMargin:8
                    contentWidth:availableWidth;clip:true
                    EntryArea {id:description;objectName:"profileDescriptionEdit";width:descriptionScroll.availableWidth;height:Math.max(implicitHeight,descriptionScroll.availableHeight);placeholderText:"简介";textFormat:TextEdit.PlainText;wrapMode:TextEdit.Wrap;leftPadding:18;rightPadding:18;background:null}
                }
                UiText {id:editError;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:18;anchors.bottom:editorActions.top;anchors.bottomMargin:8;visible:text.length>0;color:Theme.accent;font.pixelSize:12;wrapMode:Text.Wrap}
                Row {
                    id:editorActions;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:14;spacing:8
                    ShellButton {objectName:"profileCancel";text:"取消";onClicked:profileEditor.reject()}
                    ShellButton {objectName:"profileSave";text:"保存";primary:true;onClicked:profileEditor.save()}
                }
            }
            Rectangle {x:avatarRegion.width;width:1;height:parent.height;color:Theme.border}
            Rectangle {x:avatarRegion.width;y:nameRegion.height;width:parent.width-x;height:1;color:Theme.border}
        }
    }
}
