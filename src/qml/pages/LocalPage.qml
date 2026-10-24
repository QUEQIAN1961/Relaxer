import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"
import "../theme"
Item {
    id: page
    objectName: "localPage"
    signal lyricsRequested(var keys, bool missingOnly)
    signal importRequested(var anchor)
    signal bulkChanged(bool active, var keys)
    signal editRequested(string key, string name)
    readonly property bool isPlaylist: Number(libraryBridge.playlistId)>0
    readonly property var details: libraryBridge.playlistDetails
    readonly property string shownPlaylist: libraryBridge.playlistId
    onShownPlaylistChanged: if(playlistHeader && trackList){ playlistHeader.collapsed=false; trackList.scrollToStart() }
    function editMetadata(){description.text=page.details.description||"";tagText.text=(page.details.tags||[]).join(" / ");metadata.open()}
    function focusSearch(){search.forceActiveFocus();search.selectAll()}
    function clearBulk(){trackList.clearBulk()}
    function setBulkKeys(keys){trackList.checkedKeys=keys.slice();trackList.bulkChanged(true,keys)}
    function sizeText(bytes){return bytes>=1073741824?(bytes/1073741824).toFixed(2)+" GB":(bytes/1048576).toFixed(1)+" MB"}
    onVisibleChanged: if(!visible)clearBulk()
    ColumnLayout {
        anchors.fill:parent
        anchors.leftMargin:34;anchors.rightMargin:34;anchors.topMargin:28;anchors.bottomMargin:16
        spacing:16
        RowLayout {
            Layout.fillWidth:true;spacing:16
            UiText { text:libraryBridge.playlistName;font.pixelSize:28;font.weight:Font.DemiBold;color:Theme.text;Layout.fillWidth:true;elide:Text.ElideRight }
            EntryField {
                id:search;objectName:"librarySearch";Layout.preferredWidth:255;implicitHeight:36
                placeholderText:"搜索歌曲、歌手、专辑";text:libraryModel.filterText
                onTextEdited:{page.clearBulk();libraryModel.filterText=text}
                selectByMouse:true;Accessible.name:"搜索音乐库"
            }
            ShellButton { objectName:"importMusicButton";text:"导入音乐";symbol:"plus";onClicked:page.importRequested(this) }
        }
        PlaylistHeader {
            id:playlistHeader;visible:page.isPlaylist
            Layout.fillWidth:true;Layout.preferredHeight:implicitHeight;Layout.topMargin:16
            details:page.details;playlistName:libraryBridge.playlistName;hasTracks:libraryModel.count>0
            reorderEnabled:trackList.selectedKeys().length>0 && !libraryModel.filterText && !libraryModel.artistFilter && !libraryModel.albumFilter && libraryModel.sortKey===0 && !libraryModel.descending
            onPlayRequested:libraryBridge.playAll()
            onShuffleRequested:libraryBridge.shuffleAll()
            onOptionsRequested:playlistOptions.popup()
            onMoveRequested:direction=>libraryBridge.moveTracksInPlaylist(trackList.selectedKeys(),direction)
            onMetadataRequested:page.editMetadata()
            onCoverRequested:coverPicker.open()
        }
        UiText {
            objectName:"librarySummary";visible:!page.isPlaylist;Layout.fillWidth:true
            text:libraryModel.count+" 首歌曲 · "+page.sizeText(libraryModel.totalFileSize)+(page.isPlaylist?" · "+(page.details.plays||0)+" 次播放":" · 本地音乐资料库")
            color:Theme.secondary;font.pixelSize: 14
        }
        RowLayout {
            visible:!page.isPlaylist;Layout.fillWidth:true;spacing:8
            ShellButton {objectName:"playAllButton";text:"播放全部";symbol:"play";primary:true;enabled:libraryModel.count>0;onClicked:libraryBridge.playAll()}
            ShellButton {text:"随机播放";symbol:"shuffle";enabled:libraryModel.count>0;onClicked:libraryBridge.shuffleAll()}
            ChoiceBox {
                visible:libraryBridge.playlistId==="0";Layout.preferredWidth:110
                model:["全部歌手"].concat(libraryModel.artists)
                currentIndex:Math.max(0,model.indexOf(libraryModel.artistFilter))
                displayText:libraryModel.artistFilter||"歌手"
                onActivated:{page.clearBulk();libraryModel.artistFilter=currentIndex>0?currentText:""}
            }
            ChoiceBox {
                visible:libraryBridge.playlistId==="0";Layout.preferredWidth:110
                model:["全部专辑"].concat(libraryModel.albums)
                currentIndex:Math.max(0,model.indexOf(libraryModel.albumFilter))
                displayText:libraryModel.albumFilter||"专辑"
                onActivated:{page.clearBulk();libraryModel.albumFilter=currentIndex>0?currentText:""}
            }
            ShellButton {visible:libraryBridge.playlistId==="0";text:"清空";flat:true;onClicked:{page.clearBulk();libraryModel.clearFilters()}}
            ShellButton {objectName:"findMissingLyrics";text:"查找缺失歌词";enabled:libraryModel.count>0;onClicked:page.lyricsRequested(libraryBridge.visibleTrackKeys(),true)}
            Item{Layout.fillWidth:true}
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
        id:playlistOptions;parent:page.Overlay.overlay
        RoundMenuItem {text:"编辑歌单信息…";onTriggered:page.editMetadata()}
        RoundMenuItem {text:"修改歌单名称…";onTriggered:page.editRequested(libraryBridge.playlistId,libraryBridge.playlistName)}
        RoundMenuItem {text:"更换封面…";onTriggered:coverPicker.open()}
        RoundMenuItem {text:"查找缺失歌词";enabled:libraryModel.count>0;onTriggered:page.lyricsRequested(libraryBridge.visibleTrackKeys(),true)}
    }
    FileDialog{id:coverPicker;title:"设置歌单封面";nameFilters:["图片 (*.png *.jpg *.jpeg *.webp *.bmp)"];onAccepted:libraryBridge.setPlaylistCover(libraryBridge.playlistId,selectedFile)}
    ConfirmDialog {
        id:metadata;parent:page.Overlay.overlay;anchors.centerIn:parent;width:460;modal:true;title:"编辑歌单信息";standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:ColumnLayout {
            UiText {text:"标签（用 / 分隔）";color:Theme.secondary}
            EntryField {id:tagText;Layout.fillWidth:true}
            UiText {text:"歌单简介";color:Theme.secondary}
            TextArea{id:description;Layout.fillWidth:true;Layout.preferredHeight:100;wrapMode:Text.Wrap}
        }
        onAccepted:{libraryBridge.setPlaylistTags(libraryBridge.playlistId,tagText.text.split("/").map(t=>t.trim()).filter(t=>t.length));libraryBridge.setPlaylistDescription(libraryBridge.playlistId,description.text)}
    }
}
