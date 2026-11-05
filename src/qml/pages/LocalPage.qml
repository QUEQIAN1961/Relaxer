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
    readonly property string shownPlaylist: libraryBridge.playlistId
    onShownPlaylistChanged: { optionsExpanded=false;search.close();if(playlistHeader && trackList){ playlistHeader.collapsed=false; trackList.scrollToStart() }}
    function editMetadata(){description.text=page.details.description||"";tagText.text=(page.details.tags||[]).join(" / ");metadata.open()}
    function focusSearch(){search.open()}
    function clearBulk(){trackList.clearBulk()}
    function setBulkKeys(keys){trackList.checkedKeys=keys.slice();trackList.bulkChanged(true,keys)}
    function sizeText(bytes){return bytes>=1073741824?(bytes/1073741824).toFixed(2)+" GB":(bytes/1048576).toFixed(1)+" MB"}
    onVisibleChanged:if(!visible){clearBulk();optionsExpanded=false;search.close()}
    ColumnLayout {
        anchors.fill:parent
        anchors.leftMargin:Theme.pageMargin;anchors.rightMargin:Theme.pageMargin;anchors.topMargin:28;anchors.bottomMargin:0
        spacing:page.isPlaylist?16-8*Theme.layoutProgress:16
        RowLayout {
            Layout.fillWidth:true;spacing:16
            UiText { objectName:"libraryHeading";text:libraryBridge.playlistName;font.pixelSize:28;font.weight:Font.DemiBold;color:Theme.text;Layout.fillWidth:true;Layout.minimumWidth:0;elide:Text.ElideRight }
            GlassSearch {
                id:search;objectName:"librarySearchControl";Layout.preferredWidth:36;Layout.preferredHeight:36
                backdrop:page.backdropSource;query:libraryModel.filterText
                onQueryEdited:value=>{page.clearBulk();libraryModel.filterText=value}
            }
            ShellButton { objectName:"importMusicButton";visible:libraryBridge.playlistId==="0";text:page.width<390?"导入":"导入音乐";Accessible.name:"导入音乐";symbol:"plus";onClicked:page.importRequested(this) }
        }
        PlaylistHeader {
            id:playlistHeader;visible:page.isPlaylist
            Layout.fillWidth:true;Layout.preferredHeight:implicitHeight;Layout.topMargin:16*(1-Theme.layoutProgress)
            details:page.details;playlistName:libraryBridge.playlistName;hasTracks:libraryModel.count>0
            reorderEnabled:trackList.selectedKeys().length>0 && !libraryModel.filterText && !libraryModel.artistFilter && !libraryModel.albumFilter && libraryModel.sortKey===0 && !libraryModel.descending
            onPlayRequested:libraryBridge.playAll()
            onShuffleRequested:libraryBridge.shuffleAll()
            onPlayModeRequested:mode=>{playbackBridge.setMode(mode);if(mode===3)libraryBridge.shuffleAll();else libraryBridge.playAll()}
            onOptionsRequested:anchor=>playlistOptions.popup(anchor,0,anchor.height+6)
            onMoveRequested:direction=>libraryBridge.moveTracksInPlaylist(trackList.selectedKeys(),direction)
            onMetadataRequested:page.editMetadata()
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
            statisticsMode:page.isPlaylist
            emptyHeading:appBridge.trackCount===0?"音乐，从这里开始":"没有找到歌曲"
            onSelected:key=>libraryModel.selectedId=key
            onPlayRequested:key=>libraryBridge.play(key)
            onFavoriteRequested:key=>libraryBridge.toggleFavorite(key)
            onLyricsRequested:keys=>page.lyricsRequested(keys,false)
            onBulkChanged:(active,keys)=>page.bulkChanged(active,keys)
            onScrollYChanged:if(page.isPlaylist && scrollY>0)playlistHeader.collapsed=true
        }
    }
    WheelHandler {
        target:null;blocking:true;enabled:page.isPlaylist && page.visible
        onWheel:event=>{
            const delta=event.angleDelta.y || event.pixelDelta.y
            // Use the first step for the header, then let the list scroll.
            // Otherwise a child can retain the wheel grab as the cover shrinks.
            if(delta<0 && !playlistHeader.collapsed){playlistHeader.collapsed=true;event.accepted=true}
            else if(delta>0 && trackList.scrollY<=0 && playlistHeader.collapsed){playlistHeader.collapsed=false;event.accepted=true}
            else event.accepted=false
        }
    }
    RoundMenu {
        id:playlistOptions;objectName:"playlistOptionsMenu";parent:page.Overlay.overlay;width:Math.min(192,page.width-24)
        RoundMenuItem {text:"编辑歌单信息…";onTriggered:page.editMetadata()}
        RoundMenuItem {text:"修改歌单名称…";onTriggered:page.editRequested(libraryBridge.playlistId,libraryBridge.playlistName)}
        RoundMenuItem {objectName:"playlistCoverEntry";text:"更换封面…";onTriggered:coverPicker.open()}
        RoundMenuItem {objectName:"playlistMarkerEntry";text:"歌单标记…";onTriggered:page.editRequested(libraryBridge.playlistId,libraryBridge.playlistName)}
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
        contentItem:UiText {text:"确认删除“"+playlistDeletion.targetName+"”？\n只删除歌单及关联，音乐仍保留在曲库。";textFormat:Text.PlainText;wrapMode:Text.Wrap;color:Theme.text}
        onAccepted:libraryBridge.deletePlaylist(targetId)
    }
    FileDialog{id:coverPicker;objectName:"playlistCoverPicker";title:"设置歌单封面";nameFilters:["图片 (*.png *.jpg *.jpeg *.webp *.bmp)"];onAccepted:libraryBridge.setPlaylistCover(libraryBridge.playlistId,selectedFile)}
    ConfirmDialog {
        id:metadata;objectName:"playlistMetadataDialog";parent:page.Overlay.overlay;anchors.centerIn:parent;width:Math.min(460,parent.width-32);modal:true;title:"编辑歌单信息";standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:ColumnLayout {
            UiText {text:"标签（用 / 分隔）";color:Theme.secondary}
            EntryField {id:tagText;Layout.fillWidth:true}
            UiText {text:"歌单简介";color:Theme.secondary}
            EntryArea{id:description;objectName:"playlistDescriptionEdit";Layout.fillWidth:true;Layout.preferredHeight:100;wrapMode:Text.Wrap}
        }
        onAccepted:{libraryBridge.setPlaylistTags(libraryBridge.playlistId,tagText.text.split("/").map(t=>t.trim()).filter(t=>t.length));libraryBridge.setPlaylistDescription(libraryBridge.playlistId,description.text)}
    }
}
