import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"
import "../theme"
Item {
    id: page
    objectName: "localPage"
    property Item backdropSource:page
    signal lyricsRequested(var keys, bool missingOnly)
    signal importRequested(var anchor)
    signal bulkChanged(bool active, var keys)
    signal editRequested(string key, string name)
    property bool optionsExpanded:false
    readonly property bool isFavorites:libraryBridge.playlistId==="-1"
    property real optionsReveal:!Theme.compact||optionsExpanded?1:0
    Behavior on optionsReveal { NumberAnimation { duration:settingsBridge.values.reduceMotion?0:300;easing.type:Easing.OutCubic } }
    property real playlistOptionsReveal:page.isPlaylist&&optionsExpanded?1:0
    Behavior on playlistOptionsReveal { NumberAnimation { duration:settingsBridge.values.reduceMotion?0:300;easing.type:Easing.OutCubic } }
    readonly property real filtersReveal:page.isPlaylist?playlistOptionsReveal:optionsReveal
    readonly property bool isPlaylist: Number(libraryBridge.playlistId)>0
    readonly property var details: libraryBridge.playlistDetails
    readonly property bool isCloud:!!details.cloud
    readonly property var cloudSource:details.provider==="qq"?qqMusic:onlineMusic
    function cloudSong(key){if(!isCloud)return ({});const id=String(Number(key)-(details.provider==="qq"?2000000000000:1000000000000));for(const value of cloudSource.songs)if(String(value.id)===id)return value;return ({})}
    readonly property string shownPlaylist: libraryBridge.playlistId
    onShownPlaylistChanged: { optionsExpanded=false;search.close();if(trackList)trackList.scrollToStart() }
    function handlePlaylistWheel(event) {
        if(!page.isPlaylist||!page.visible)return
        const delta=event.pixelDelta.y||event.angleDelta.y
        if(delta<0&&!playlistHeader.collapsed){playlistHeader.collapsed=true;event.accepted=true}
        else if(delta>0&&trackList.atScrollStart&&playlistHeader.collapsed){playlistHeader.collapsed=false;event.accepted=true}
    }
    function editMetadata(){description.text=page.details.description||"";tagText.text=(page.details.tags||[]).join(" / ");metadata.open()}
    function focusSearch(){search.open()}
    function clearBulk(){trackList.clearBulk()}
    function setBulkKeys(keys){trackList.checkedKeys=keys.slice();trackList.bulkChanged(true,keys)}
    function sizeText(bytes){return bytes>=1073741824?(bytes/1073741824).toFixed(2)+" GB":(bytes/1048576).toFixed(1)+" MB"}
    onVisibleChanged:if(!visible){clearBulk();optionsExpanded=false;search.close()}
    ColumnLayout {
        id:libraryLayout
        anchors.fill:parent
        anchors.leftMargin:Theme.pageMargin;anchors.rightMargin:Theme.pageMargin;anchors.topMargin:28;anchors.bottomMargin:0
        spacing:page.isPlaylist?16-8*Theme.layoutProgress:16
        RowLayout {
            Layout.fillWidth:true;Layout.bottomMargin:Theme.pageHeadingGap-libraryLayout.spacing;spacing:16
            Item {
                Layout.fillWidth:true;Layout.minimumWidth:0;Layout.preferredHeight:heading.implicitHeight
                UiText {id:heading;objectName:"libraryHeading";width:Math.min(implicitWidth,Math.max(0,parent.width-(page.isCloud?sourceLabel.width+12:0)));text:libraryBridge.playlistName;font.pixelSize:28;font.weight:Font.DemiBold;color:Theme.text;elide:Text.ElideRight}
                UiText {id:sourceLabel;objectName:"cloudPlaylistSource";visible:page.isCloud;x:heading.width+12;y:(heading.height-height)/2;width:implicitWidth;text:page.details.provider==="qq"?"QQ 音乐":"网易云音乐";textFormat:Text.PlainText;font.pixelSize:12;color:Theme.secondary}
            }
            GlassSearch {
                id:search;objectName:"librarySearchControl";Layout.preferredWidth:36;Layout.preferredHeight:36
                backdrop:page.backdropSource;query:libraryModel.filterText
                onQueryEdited:value=>{page.clearBulk();libraryModel.filterText=value}
            }
            ShellButton { objectName:"importMusicButton";visible:libraryBridge.playlistId==="0";text:page.width<390?"导入":"导入音乐";Accessible.name:"导入音乐";symbol:"plus";onClicked:page.importRequested(this) }
        }
        PlaylistHeader {
            id:playlistHeader;visible:page.isPlaylist
            Layout.fillWidth:true;Layout.preferredHeight:implicitHeight
            details:page.details;playlistName:libraryBridge.playlistName;hasTracks:libraryModel.count>0
            reorderEnabled:!page.isCloud && trackList.selectedKeys().length>0 && !libraryModel.filterText && !libraryModel.artistFilter && !libraryModel.albumFilter && libraryModel.sortKey===0 && !libraryModel.descending
            onPlayRequested:libraryBridge.playAll()
            onShuffleRequested:libraryBridge.shuffleAll()
            onPlayModeRequested:mode=>{playbackBridge.setMode(mode);if(mode===3)libraryBridge.shuffleAll();else libraryBridge.playAll()}
            onOptionsRequested:anchor=>playlistOptions.popup(anchor,0,anchor.height+6)
            onMoveRequested:direction=>libraryBridge.moveTracksInPlaylist(trackList.selectedKeys(),direction)
        }
        UiText {
            objectName:"librarySummary";visible:!page.isPlaylist;Layout.fillWidth:true
            text:libraryModel.count+" 首歌曲 · "+page.sizeText(libraryModel.totalFileSize)+(page.isPlaylist?" · "+(page.details.plays||0)+" 次播放":" · 本地音乐资料库")
            color:Theme.secondary;font.pixelSize: 14
        }
        Item {
            id:listActions
            readonly property bool wrapped:Theme.compact||width<650
            visible:!page.isPlaylist||page.filtersReveal>0;Layout.fillWidth:true
            Layout.preferredHeight:page.isPlaylist?secondaryOptions.implicitHeight*page.filtersReveal:Theme.controlHeight+(listActions.wrapped?(secondaryOptions.implicitHeight+10)*page.filtersReveal:0)
            Row {
                id:primaryActions;visible:!page.isPlaylist;spacing:8
                ShellButton {id:playAllAction;objectName:"playAllButton";text:"播放全部";symbol:"play";primary:true;enabled:libraryModel.count>0;onClicked:libraryBridge.playAll()}
                PlayModeButton {
                    width:Math.min(160,Math.max(128,listActions.width-playAllAction.width-(Theme.compact?moreOptions.width+16:8)))
                    enabled:libraryModel.count>0
                    onPlayRequested:mode=>{playbackBridge.setMode(mode);if(mode===3)libraryBridge.shuffleAll();else libraryBridge.playAll()}
                }
                ShellButton {id:moreOptions;objectName:"moreLibraryOptions";visible:Theme.compact;text:page.optionsExpanded?"收起选项":"更多选项";onClicked:page.optionsExpanded=!page.optionsExpanded}
            }
            Item {
                objectName:"libraryFilterRow"
                x:page.isPlaylist||listActions.wrapped?0:primaryActions.width+8;y:page.isPlaylist?0:listActions.wrapped?Theme.controlHeight+10:0
                width:Math.max(0,parent.width-x);height:secondaryOptions.implicitHeight*(page.isPlaylist||Theme.compact?page.filtersReveal:1)
                clip:true;opacity:page.filtersReveal;enabled:page.isPlaylist?page.optionsExpanded:!Theme.compact||page.optionsExpanded
                Flow {
                    id:secondaryOptions;width:parent.width
                    y:page.isPlaylist||Theme.compact?-implicitHeight*(1-page.filtersReveal):0;spacing:8
                    ChoiceBox {
                        objectName:"artistFilter";width:Theme.compact?110:100
                        model:["全部歌手"].concat(libraryModel.artists)
                        currentIndex:Math.max(0,model.indexOf(libraryModel.artistFilter));displayText:libraryModel.artistFilter||"歌手"
                        onActivated:{page.clearBulk();libraryModel.artistFilter=currentIndex>0?currentText:""}
                    }
                    ChoiceBox {
                        objectName:"albumFilter";width:Theme.compact?110:100
                        model:["全部专辑"].concat(libraryModel.albums)
                        currentIndex:Math.max(0,model.indexOf(libraryModel.albumFilter));displayText:libraryModel.albumFilter||"专辑"
                        onActivated:{page.clearBulk();libraryModel.albumFilter=currentIndex>0?currentText:""}
                    }
                    ShellButton {objectName:"findMissingLyrics";visible:!page.isFavorites&&!page.isPlaylist;text:"查找缺失歌词";enabled:!Theme.compact&&libraryModel.count>0;onClicked:page.lyricsRequested(libraryBridge.visibleTrackKeys(),true)}
                    ShellButton {objectName:"collapsePlaylistFilters";visible:page.isPlaylist;text:"收起";onClicked:page.optionsExpanded=false}
                }
            }
        }
        TrackList {
            id:trackList;objectName:"libraryListContainer"
            Layout.fillWidth:true;Layout.fillHeight:true
            trackModel:libraryModel;selectedKey:libraryModel.selectedId;currentKey:playbackBridge.trackId
            cloudMode:page.isCloud
            downloadState:key=>onlineDownloads.isDownloaded(page.details.provider,page.cloudSong(key))
            onDownloadRequested:key=>{const song=page.cloudSong(key);if(song.id)page.cloudSource.downloadSong(String(song.id))}
            statisticsMode:page.isPlaylist
            emptyHeading:page.isCloud?"没有找到歌曲":appBridge.trackCount===0?"音乐，从这里开始":"没有找到歌曲"
            emptyDescription:page.isCloud?"可调整筛选，或稍后重新打开歌单。":"导入你喜欢的音乐，开始慢慢听。"
            onSelected:key=>libraryModel.selectedId=key
            onPlayRequested:key=>libraryBridge.play(key)
            onFavoriteRequested:key=>libraryBridge.toggleFavorite(key)
            onLyricsRequested:keys=>page.lyricsRequested(keys,false)
            onBulkChanged:(active,keys)=>page.bulkChanged(active,keys)
            onScrollYChanged:if(page.isPlaylist&&!trackList.atScrollStart)playlistHeader.collapsed=true
            onWheelRequested:event=>page.handlePlaylistWheel(event)
        }
    }
    WheelHandler {
        target:null;blocking:true;enabled:page.isPlaylist && page.visible
        onWheel:event=>{event.accepted=false;page.handlePlaylistWheel(event)}
    }
    RoundMenu {
        id:playlistOptions;objectName:"playlistOptionsMenu";parent:page.Overlay.overlay;width:Math.min(244,page.width-24)
        RoundMenuItem {objectName:"playlistEditEntry";text:page.isCloud?"编辑备注/标记…":"编辑名称/图标/封面…";onTriggered:page.editRequested(libraryBridge.playlistId,libraryBridge.playlistName)}
        RoundMenuItem {objectName:"playlistMetadataEntry";text:"编辑标签/简介…";onTriggered:page.editMetadata()}
        RoundMenuItem {objectName:"playlistFilterEntry";text:"歌手/专辑查找";onTriggered:page.optionsExpanded=true}
        MenuSeparator{visible:playlistHeader.compactLayout}
        RoundMenuItem {objectName:"playlistMoveUp";visible:playlistHeader.compactLayout;text:"上移";enabled:playlistHeader.reorderEnabled;onTriggered:libraryBridge.moveTracksInPlaylist(trackList.selectedKeys(),-1)}
        RoundMenuItem {objectName:"playlistMoveDown";visible:playlistHeader.compactLayout;text:"下移";enabled:playlistHeader.reorderEnabled;onTriggered:libraryBridge.moveTracksInPlaylist(trackList.selectedKeys(),1)}
        MenuSeparator{}
        RoundMenuItem {objectName:"deletePlaylistEntry";text:"删除此歌单";destructive:true;onTriggered:playlistDeletion.review()}
    }
    ConfirmDialog {
        id:playlistDeletion;objectName:"playlistDeletionConfirmation"
        parent:page.Overlay.overlay;anchors.centerIn:parent;width:Math.min(420,parent.width-36)
        modal:true;title:"删除此歌单？";standardButtons:Dialog.Ok|Dialog.Cancel
        property string targetId:"0"
        property string targetName:""
        function review(){targetId=libraryBridge.playlistId;targetName=libraryBridge.playlistName;open()}
        contentItem:UiText {text:"确认删除“"+playlistDeletion.targetName+"”？\n"+(page.isCloud?"仅移出左侧列表，平台中的歌单和歌曲保留。":"只删除歌单及关联，音乐仍保留在曲库。");textFormat:Text.PlainText;wrapMode:Text.Wrap;color:Theme.text}
        onAccepted:libraryBridge.deletePlaylist(targetId)
    }
    ConfirmDialog {
        id:metadata;objectName:"playlistMetadataDialog";parent:page.Overlay.overlay;anchors.centerIn:parent;width:Math.min(460,parent.width-32);modal:true;title:"编辑标签/简介";standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:ColumnLayout {
            UiText {text:"标签（用 / 分隔）";color:Theme.secondary}
            EntryField {id:tagText;Layout.fillWidth:true}
            UiText {text:"歌单简介";color:Theme.secondary}
            EntryArea{id:description;objectName:"playlistDescriptionEdit";Layout.fillWidth:true;Layout.preferredHeight:100;wrapMode:Text.Wrap}
        }
        onAccepted:{libraryBridge.setPlaylistTags(libraryBridge.playlistId,tagText.text.split("/").map(t=>t.trim()).filter(t=>t.length));libraryBridge.setPlaylistDescription(libraryBridge.playlistId,description.text)}
    }
}
