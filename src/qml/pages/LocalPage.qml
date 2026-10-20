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
    readonly property bool isPlaylist: Number(libraryBridge.playlistId)>0
    readonly property var details: libraryBridge.playlistDetails
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
        RowLayout {
            visible:page.isPlaylist;Layout.fillWidth:true;spacing:22
            Artwork {
                width:170;height:170;radius:16;resolution:384;file:page.details.cover||"";seed:libraryBridge.playlistName
                TapHandler{onTapped:coverPicker.open()}
            }
            ColumnLayout {
                Layout.fillWidth:true;spacing:12
                UiText {text:libraryBridge.playlistName;font.pixelSize:24;font.weight:Font.DemiBold;color:Theme.text;Layout.fillWidth:true;elide:Text.ElideRight}
                UiText {text:(page.details.tags||[]).join(" / ");font.pixelSize: 14;color:Theme.secondary;Layout.fillWidth:true;elide:Text.ElideRight}
                UiText {text:page.details.description||"还没有歌单简介";font.pixelSize: 15;color:Theme.secondary;wrapMode:Text.Wrap;maximumLineCount:3;Layout.fillWidth:true}
                ShellButton{text:"编辑歌单信息";flat:true;onClicked:{description.text=page.details.description||"";tagText.text=(page.details.tags||[]).join(" / ");metadata.open()}}
            }
        }
        UiText {
            objectName:"librarySummary";Layout.fillWidth:true
            text:libraryModel.count+" 首歌曲 · "+page.sizeText(libraryModel.totalFileSize)+(page.isPlaylist?" · "+(page.details.plays||0)+" 次播放":" · 本地音乐资料库")
            color:Theme.secondary;font.pixelSize: 14
        }
        RowLayout {
            Layout.fillWidth:true;spacing:8
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
            emptyHeading:appBridge.trackCount===0?"音乐，从这里开始":"没有找到歌曲"
            onSelected:key=>libraryModel.selectedId=key
            onPlayRequested:key=>libraryBridge.play(key)
            onFavoriteRequested:key=>libraryBridge.toggleFavorite(key)
            onLyricsRequested:keys=>page.lyricsRequested(keys,false)
            onBulkChanged:(active,keys)=>page.bulkChanged(active,keys)
        }
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
