import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../theme"
Item {
    id:table
    required property var trackModel
    property string selectedKey:"0"
    property string currentKey:"0"
    property bool statisticsMode:false
    property string emptyHeading:"没有找到歌曲"
    property string emptyDescription:"导入你喜欢的音乐，开始慢慢听。"
    property bool bulkMode:false
    property var checkedKeys:[]
    readonly property real scrollY:list.contentY
    property var widthOverrides:settingsBridge.values.songColumnWidths||({})
    readonly property var columnKeys:["number","favorite","title","artist","album","duration",statisticsMode?"plays":"fileSize","importDate"]
    readonly property var columnLabels:statisticsMode?["","","歌曲","歌手","专辑","时长","播放次数"]:["","","歌曲","歌手","专辑","时长","文件大小","导入时间"]
    readonly property var columnWidths:fitColumns()
    signal selected(string key)
    signal playRequested(string key)
    signal favoriteRequested(string key)
    signal lyricsRequested(var keys)
    signal bulkChanged(bool active,var keys)
    function selectedKeys(){return checkedKeys.length?checkedKeys:selectedKey!=="0"?[selectedKey]:[]}
    function scrollToStart(){list.positionViewAtBeginning()}
    function clearBulk(){bulkMode=false;checkedKeys=[];bulkChanged(false,[])}
    function beginBulk(){bulkMode=true;checkedKeys=selectedKey!=="0"?[selectedKey]:[];bulkChanged(true,checkedKeys)}
    function toggleKey(key){let next=checkedKeys.slice();const i=next.indexOf(key);if(i<0)next.push(key);else next.splice(i,1);checkedKeys=next;bulkChanged(bulkMode,checkedKeys)}
    function fitColumns(){
        const count=statisticsMode?7:8;let widths=[bulkMode?48:34,32,260,120,120,64,88,104]
        for(let i=2;i<count;i++){const saved=Number(widthOverrides[columnKeys[i]]||0);if(saved>=40)widths[i]=Math.max(i===2?96:40,saved)}
        const available=Math.max(200,width-12)
        let other=0;for(let i=0;i<count;i++)if(i!==2)other+=widths[i]
        if(!widthOverrides.title)widths[2]=Math.max(96,available-other)
        let used=0;for(let i=0;i<count-1;i++)used+=widths[i]
        let shortage=Math.max(0,used+40-available)
        for(let i=2;i<count-1&&shortage>0;i++){const n=Math.min(shortage,Math.max(0,widths[i]-(i===2?96:40)));widths[i]-=n;shortage-=n;used-=n}
        widths[count-1]=Math.max(40,available-used);return widths
    }
    function sortColumn(column){
        if(column<2)return
        const sort=[0,0,1,2,3,4,statisticsMode?5:8,6][column]
        if(trackModel.sortKey===sort)trackModel.descending=!trackModel.descending
        else {trackModel.sortKey=sort;trackModel.descending=false}
    }
    function sizeText(bytes){const n=Number(bytes||0);return n>=1073741824?(n/1073741824).toFixed(1)+" GB":n>=1048576?(n/1048576).toFixed(1)+" MB":n>=1024?(n/1024).toFixed(0)+" KB":n+" B"}
    function timeText(ms){const s=Math.floor(Number(ms)/1000);return Math.floor(s/60)+":"+(s%60<10?"0":"")+s%60}
    ColumnLayout{
        anchors.fill:parent;spacing:0
        Row{
            Layout.fillWidth:true;Layout.preferredHeight:38;height:38
            Repeater{
                model:table.columnLabels
                delegate:Item{
                    required property int index;required property string modelData
                    width:table.columnWidths[index];height:38
                    UiText { fontRegion: "songs";anchors.fill:parent;anchors.leftMargin:8;text:table.bulkMode&&index===0?"选择":modelData;color:Theme.secondary;font.pixelSize: 13;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
                    MouseArea{anchors.fill:parent;onClicked:{if(index===0&&table.bulkMode){const all=[];for(let i=0;i<table.trackModel.count;i++)all.push(table.trackModel.idAt(i));table.checkedKeys=table.checkedKeys.length===all.length?[]:all;table.bulkChanged(true,table.checkedKeys)}else table.sortColumn(index)}}
                    MouseArea{
                        visible:index>=2;anchors.right:parent.right;width:6;height:parent.height;cursorShape:Qt.SplitHCursor
                        property real origin:0;property real startWidth:0
                        onPressed:mouse=>{origin=mapToGlobal(mouse.x,mouse.y).x;startWidth=parent.width}
                        onPositionChanged:mouse=>{if(!pressed)return;const next=Object.assign({},table.widthOverrides);next[table.columnKeys[index]]=Math.max(index===2?96:40,Math.min(2400,startWidth+mapToGlobal(mouse.x,mouse.y).x-origin));table.widthOverrides=next}
                        onReleased:settingsBridge.setPreference("songColumnWidths",table.widthOverrides)
                    }
                }
            }
        }
        Rectangle{Layout.fillWidth:true;height:1;color:Theme.border}
        ListView{
            id:list;objectName:"trackListView";Layout.fillWidth:true;Layout.fillHeight:true
            model:table.trackModel;reuseItems:true;cacheBuffer:132;clip:true;spacing:0;boundsBehavior:Flickable.StopAtBounds
            ScrollBar.vertical:ScrollBar{}
            delegate:Rectangle{
                id:row;objectName:"trackDelegate"
                required property int index
                required property string trackKey
                required property string title
                required property string artist
                required property string album
                required property real duration
                required property bool favorite
                required property url cover
                required property real plays
                required property real fileSize
                required property string importedAt
                property url rowCover:cover
                property real rowPlays:Number(plays)
                property real rowSize:Number(fileSize)
                property string rowAdded:importedAt
                width:list.width;height:66;radius:8
                color:table.checkedKeys.indexOf(trackKey)>=0||table.selectedKey===trackKey?Theme.selected:hover.containsMouse?Theme.hover:"transparent"
                MouseArea{
                    id:hover;anchors.fill:parent;hoverEnabled:true;acceptedButtons:Qt.LeftButton|Qt.RightButton
                    onPressed:table.forceActiveFocus()
                    onClicked:mouse=>{
                        if(mouse.button===Qt.RightButton){if(table.checkedKeys.indexOf(row.trackKey)<0){table.selected(row.trackKey);if(!table.bulkMode)table.checkedKeys=[]}context.keys=table.bulkMode?table.selectedKeys():[row.trackKey];context.targetKey=row.trackKey;const p=row.mapToItem(context.parent,mouse.x,mouse.y);context.popup(p.x,p.y)}
                        else if(table.bulkMode||mouse.modifiers&Qt.ControlModifier){table.selected(row.trackKey);table.toggleKey(row.trackKey)}
                        else{table.checkedKeys=[];table.selected(row.trackKey)}
                    }
                    onDoubleClicked:if(!table.bulkMode)table.playRequested(row.trackKey)
                }
                Row{
                    anchors.fill:parent
                    Item{
                        width:table.columnWidths[0];height:parent.height
                        CheckBox{visible:table.bulkMode;anchors.centerIn:parent;checked:table.checkedKeys.indexOf(row.trackKey)>=0;onClicked:table.toggleKey(row.trackKey)}
                        UiText { fontRegion: "songs";visible:!table.bulkMode&&!hover.containsMouse&&table.currentKey!==row.trackKey;anchors.centerIn:parent;text:row.index+1;color:Theme.secondary;font.pixelSize: 13}
                        IconButton{visible:!table.bulkMode&&(hover.containsMouse||table.currentKey===row.trackKey);anchors.centerIn:parent;width:28;height:28;symbol:table.currentKey===row.trackKey&&playbackBridge.playing?"volumeMedium":"play";tint:table.currentKey===row.trackKey?Theme.accent:Theme.secondary;onClicked:table.playRequested(row.trackKey)}
                    }
                    Item{width:table.columnWidths[1];height:parent.height;IconButton{anchors.centerIn:parent;width:28;height:28;symbol:row.favorite?"liked":"heart";tint:row.favorite?Theme.accent:Theme.secondary;onClicked:if(!table.bulkMode)table.favoriteRequested(row.trackKey)}}
                    Item{
                        width:table.columnWidths[2];height:parent.height
                        Artwork{x:6;anchors.verticalCenter:parent.verticalCenter;width:40;height:40;file:row.rowCover;seed:row.title;resolution:128;radius:6}
                        UiText { fontRegion: "songs";x:56;width:parent.width-62;anchors.verticalCenter:parent.verticalCenter;text:row.title;textFormat:Text.PlainText;color:table.currentKey===row.trackKey?Theme.accent:Theme.text;font.pixelSize: 15;font.weight:Font.DemiBold;elide:Text.ElideRight}
                    }
                    Repeater{
                        model:table.statisticsMode?4:5
                        delegate:Item{
                            required property int index
                            width:table.columnWidths[index+3];height:row.height
                            UiText { fontRegion: "songs";anchors.fill:parent;anchors.leftMargin:8;anchors.rightMargin:8;text:index===0?row.artist:index===1?row.album:index===2?table.timeText(row.duration):index===3?table.statisticsMode?row.rowPlays:table.sizeText(row.rowSize):row.rowAdded.length?row.rowAdded.slice(0,10):"—";textFormat:Text.PlainText;color:Theme.secondary;font.pixelSize: 14;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
                        }
                    }
                }
            }
            EmptyState{anchors.centerIn:parent;width:Math.min(parent.width-40,440);visible:list.count===0;heading:table.emptyHeading;description:table.emptyDescription}
        }
    }
    RoundMenu {
        id:context;parent:table.Overlay.overlay;property var keys:[];property string targetKey:"0"
        RoundMenuItem {text:"多选";onTriggered:table.beginBulk()}
        RoundMenuItem {objectName:"findLyrics";text:"查找歌词…";onTriggered:table.lyricsRequested(context.keys)}
        MenuSeparator{}
        RoundMenuItem {text:"播放";onTriggered:table.playRequested(context.targetKey)}
        RoundMenuItem {text:"收藏所选歌曲";onTriggered:libraryBridge.applyFavorite(context.keys,true)}
        RoundMenuItem {text:"取消收藏";onTriggered:libraryBridge.applyFavorite(context.keys,false)}
        RoundMenu {
            id:addMenu;title:"加入歌单"
            Instantiator{
                model:playlistModel
                delegate:RoundMenuItem {required property string name;required property string playlistKey;text:name;onTriggered:libraryBridge.addTracksToPlaylist(playlistKey,context.keys)}
                onObjectAdded:(index,object)=>addMenu.insertItem(index,object)
                onObjectRemoved:(index,object)=>addMenu.removeItem(object)
            }
            MenuSeparator{}
            RoundMenuItem {text:"创建新歌单并加入…";onTriggered:newList.open()}
        }
        MenuSeparator{}
        RoundMenuItem {text:"设置歌曲封面…";visible:context.keys.length===1;onTriggered:coverPicker.open()}
        RoundMenuItem {text:"导入本地歌词…";visible:context.keys.length===1;onTriggered:lyricsPicker.open()}
        RoundMenuItem {text:"打开音乐所在目录";visible:context.keys.length===1;onTriggered:libraryBridge.openTrackFolder(context.targetKey)}
        MenuSeparator{}
        RoundMenuItem {text:"在歌单中上移";visible:Number(libraryBridge.playlistId)>0;onTriggered:libraryBridge.moveTracksInPlaylist(context.keys,-1)}
        RoundMenuItem {text:"在歌单中下移";visible:Number(libraryBridge.playlistId)>0;onTriggered:libraryBridge.moveTracksInPlaylist(context.keys,1)}
        RoundMenuItem {text:"从此歌单移除";visible:Number(libraryBridge.playlistId)>0;onTriggered:libraryBridge.removeTracksFromPlaylist(context.keys)}
        RoundMenuItem {text:"从资料库移除…";onTriggered:removal.open()}
    }
    ConfirmDialog{
        id:removal;parent:table.Overlay.overlay;width:430;anchors.centerIn:parent;modal:true;title:"从资料库移除";standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:Label{text:"从资料库移除 "+context.keys.length+" 首歌曲及其歌单关联和统计？音乐文件会保留。";wrapMode:Text.Wrap}
        onAccepted:{libraryBridge.removeTracks(context.keys,false);table.clearBulk()}
    }
    ConfirmDialog{
        id:newList;objectName:"trackNewPlaylistDialog";parent:table.Overlay.overlay;width:400;anchors.centerIn:parent;modal:true;title:"创建新歌单并加入";standardButtons:Dialog.Save|Dialog.Cancel
        contentItem:EntryField {id:newListName;objectName:"trackNewPlaylistName";text:"新歌单";selectByMouse:true;maximumLength:120}
        onAccepted:{const id=libraryBridge.createPlaylist(newListName.text);if(id!=="0")libraryBridge.addTracksToPlaylist(id,context.keys)}
    }
    FileDialog{id:coverPicker;title:"歌曲封面";nameFilters:["图片 (*.png *.jpg *.jpeg *.webp)"];onAccepted:libraryBridge.setTrackCover(context.targetKey,selectedFile)}
    FileDialog{id:lyricsPicker;title:"导入本地歌词";nameFilters:["歌词 (*.lrc *.txt)"];onAccepted:{lyricsSearchBridge.setTargetTrack(context.targetKey);lyricsSearchBridge.importLocalLyrics(selectedFile,true);lyricsSearchBridge.setTargetTrack("0")}}
}
