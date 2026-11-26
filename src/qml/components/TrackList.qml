import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../theme"
Item {
    id:table
    Layout.topMargin:-6
    required property var trackModel
    property string selectedKey:"0"
    property string currentKey:"0"
    readonly property int headerHeight:Math.max(24,Math.round(21*Theme.fontScale))
    property bool statisticsMode:false
    property bool sourceColumn:false
    readonly property int sourceColumnWidth:Math.ceil(96+Math.max(0,Theme.fontScale-1)*48)
    readonly property var sourceLogos:({
        "网易云":"qrc:/music-platforms/netease.ico",
        "QQ 音乐":"qrc:/music-platforms/qq.ico",
        "酷狗":"qrc:/music-platforms/kugou.png",
        "汽水":"qrc:/music-platforms/qishui.svg",
        "B站":"qrc:/music-platforms/bilibili.ico"
    })
    property bool cloudMode:false
    property bool allowDownload:true
    property bool playlistMode:false
    property Item scrollingHeader:null
    readonly property Item headerHost:list.headerItem?list.headerItem.host:null
    readonly property bool showCovers:!playlistMode||settingsBridge.values.playlistCovers
    property var downloadState:function(key){return false}
    signal downloadRequested(string key)
    signal endReached()
    property string emptyHeading:"没有找到歌曲"
    property string emptyDescription:"导入你喜欢的音乐，开始慢慢听。"
    property bool bulkMode:false
    property var checkedKeys:[]
    readonly property real scrollY:list.contentY
    readonly property bool atScrollStart:list.atYBeginning||list.contentY<=list.originY+.5
    property var widthOverrides:settingsBridge.values.songColumnWidths||({})
    readonly property var columnKeys:["number","favorite","title","artist","album","duration",statisticsMode?"plays":"fileSize","importDate"]
    readonly property var columnLabels:statisticsMode?["","","歌曲","歌手","专辑","时长","播放次数"]:["","","歌曲","歌手","专辑","时长",sourceColumn?"来源":"文件大小","导入时间"]
    readonly property var columnWidths:fitColumns()
    property bool showFavoriteColumn:false
    signal selected(string key)
    signal playRequested(string key)
    signal favoriteRequested(string key)
    signal lyricsRequested(var keys)
    signal bulkChanged(bool active,var keys)
    signal wheelRequested(var event)
    function selectedKeys(){return checkedKeys.length?checkedKeys:selectedKey!=="0"?[selectedKey]:[]}
    function scrollToKey(key){
        for(let i=0;i<trackModel.count;++i)if(trackModel.idAt(i)===key){listWheel.stop();list.cancelFlick();list.positionViewAtIndex(i,ListView.Contain);return true}
        return false
    }
    function scrollToStart(){listWheel.stop();list.cancelFlick();list.positionViewAtBeginning()}
    function clearBulk(){bulkMode=false;checkedKeys=[];bulkChanged(false,[])}
    function beginBulk(){bulkMode=true;checkedKeys=selectedKey!=="0"?[selectedKey]:[];bulkChanged(true,checkedKeys)}
    function toggleKey(key){let next=checkedKeys.slice();const i=next.indexOf(key);if(i<0)next.push(key);else next.splice(i,1);checkedKeys=next;bulkChanged(bulkMode,checkedKeys)}
    function fitColumns(){
        const showFavorites=showFavoriteColumn
        const count=statisticsMode?7:8;let widths=[bulkMode?48:34,showFavorites?32:0,260,120,120,64,88,104]
        const sourceWidth=sourceColumn&&!statisticsMode?sourceColumnWidth:0
        if(sourceWidth)widths[6]=sourceWidth
        for(let i=2;i<count;i++){const saved=Number(widthOverrides[columnKeys[i]]||0);if(saved>=40)widths[i]=Math.max(i===6&&sourceWidth?sourceWidth:i===2?96:40,saved)}
        const available=Math.max(0,width-12)
        for(const column of [7,6,4,5]) {
            if(column>=count||(column===6&&sourceWidth))continue
            let minimum=0;for(let index=0;index<count;index++)minimum+=index===2?180:index===3?100:widths[index]
            if(minimum<=available)break
            widths[column]=0
        }
        let other=0;for(let i=0;i<count;i++)if(i!==2)other+=widths[i]
        if(!widthOverrides.title)widths[2]=Math.max(96,available-other)
        let used=0;for(let i=0;i<count-1;i++)used+=widths[i]
        const lastMinimum=widths[count-1]>0?40:0
        let shortage=Math.max(0,used+lastMinimum-available)
        for(let i=2;i<count-1&&shortage>0;i++){if(!widths[i])continue;const n=Math.min(shortage,Math.max(0,widths[i]-(i===6&&sourceWidth?sourceWidth:i===2?96:40)));widths[i]-=n;shortage-=n;used-=n}
        widths[count-1]=lastMinimum?Math.max(lastMinimum,available-used):0
        if(!lastMinimum)widths[2]+=Math.max(0,available-used)
        const number=bulkMode?42:30,favorite=showFavorites?32:0,artist=Math.min(150,Math.max(92,available*.28))
        const compactArtist=sourceWidth?Math.min(artist,Math.max(48,available-number-favorite-sourceWidth-96)):artist
        const compact=[number,favorite,Math.max(96,available-number-favorite-compactArtist-sourceWidth),compactArtist,0,0,sourceWidth,0]
        return widths.map((wide,index)=>wide+(compact[index]-wide)*Theme.layoutProgress)
    }
    function sortColumn(column){
        if(column<2)return
        const sort=[0,0,1,2,3,4,statisticsMode?5:8,6][column]
        trackModel.cycleSort(sort)
    }
    function sizeText(bytes){const n=Number(bytes);return !Number.isFinite(n)||n<0?"—":n>=1073741824?(n/1073741824).toFixed(1)+" GB":n>=1048576?(n/1048576).toFixed(1)+" MB":n>=1024?(n/1024).toFixed(0)+" KB":n+" B"}
    function timeText(ms){const s=Math.floor(Number(ms)/1000);return Math.floor(s/60)+":"+(s%60<10?"0":"")+s%60}
    Item{
        anchors.fill:parent
        Rectangle {x:0;y:headings.y;width:parent.width;height:table.headerHeight+1;z:2;visible:!!table.scrollingHeader;color:Theme.background}
        Row{
            id:headings;z:3;width:parent.width;height:table.headerHeight
            y:table.scrollingHeader?Math.max(0,list.headerItem.y+list.headerItem.height-list.contentY-table.headerHeight-1):0
            Repeater{
                model:table.columnLabels
                delegate:Item{
                    required property int index;required property string modelData
                    objectName:"trackHeading-"+index
                    width:table.columnWidths[index];height:table.headerHeight;visible:width>0;clip:true

                    readonly property int sortKey:[0,0,1,2,3,4,table.statisticsMode?5:8,6][index]
                    readonly property bool sortable:index>=2
                    HoverHandler {id:headingHover}
                    UiText {id:headingText;fontRegion:"songs";anchors.fill:parent;anchors.leftMargin:8;anchors.rightMargin:parent.sortable?26:8;text:table.bulkMode&&index===0?"选择":modelData;color:Theme.secondary;basePixelSize:13;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
                    Symbol {
                        objectName:"trackSort-"+index;visible:parent.sortable&&headingHover.hovered
                        x:Math.min(parent.width-width-8,headingText.x+Math.min(headingText.implicitWidth,headingText.width)+5);anchors.verticalCenter:parent.verticalCenter;width:14;height:18
                        name:table.trackModel.sortKey!==parent.sortKey?"sort-default":table.trackModel.descending?"sort-descending":"sort-ascending"
                        color:table.trackModel.sortKey===parent.sortKey?Theme.text:Theme.secondary
                    }
                    MouseArea{anchors.fill:parent;onClicked:{if(index===0&&table.bulkMode){const all=[];for(let i=0;i<table.trackModel.count;i++)all.push(table.trackModel.idAt(i));table.checkedKeys=table.checkedKeys.length===all.length?[]:all;table.bulkChanged(true,table.checkedKeys)}else table.sortColumn(index)}}
                    MouseArea{
                        visible:!Theme.compact&&index>=2;anchors.right:parent.right;width:6;height:parent.height;cursorShape:Qt.SplitHCursor
                        property real origin:0;property real startWidth:0
                        onPressed:mouse=>{origin=mapToGlobal(mouse.x,mouse.y).x;startWidth=parent.width}
                        onPositionChanged:mouse=>{if(!pressed)return;const next=Object.assign({},table.widthOverrides);const minimum=index===2?96:index===6&&table.sourceColumn?table.sourceColumnWidth:40;next[table.columnKeys[index]]=Math.max(minimum,Math.min(2400,startWidth+mapToGlobal(mouse.x,mouse.y).x-origin));table.widthOverrides=next}
                        onReleased:settingsBridge.setPreference("songColumnWidths",table.widthOverrides)
                    }
                }
            }
        }
        Rectangle{y:headings.y+table.headerHeight;width:parent.width;height:1;color:Theme.border;z:3}
        ListView{
            id:list;objectName:"trackListView";
            onAtYEndChanged:if(atYEnd&&contentHeight>height)table.endReached()
            onMovementEnded:if(atYEnd&&contentHeight>height)table.endReached()
width:parent.width;y:table.scrollingHeader?0:table.headerHeight+1;height:parent.height-y
            header:Item {
                id:listHeader;readonly property alias host:scrollingHeaderHost;width:list.width;height:table.scrollingHeader?table.scrollingHeader.implicitHeight+table.headerHeight+1:0
                Item {id:scrollingHeaderHost;width:parent.width;height:table.scrollingHeader?table.scrollingHeader.implicitHeight:0}
            }
            model:table.trackModel;reuseItems:true;cacheBuffer:132;clip:true;spacing:0;boundsBehavior:Flickable.StopAtBounds
            ScrollBar.vertical:ListScrollBar {
                parent:table;hoverTarget:list
                x:list.width-width;y:headings.y+table.headerHeight+1
                height:Math.max(0,table.height-y)
            }
            SmoothWheelHandler { id:listWheel;flickable:list;onWheelRequested:event=>table.wheelRequested(event) }
            delegate:HighlightSurface{
                id:row;objectName:"trackDelegate"
                required property int index
                required property string trackKey
                required property string title
                required property bool videoTrack
                required property bool requiresSubscription
                required property string artist
                required property string album
                required property real duration
                required property bool favorite
                required property url cover
                required property real plays
                required property real fileSize
                required property string importedAt
                readonly property string sourceLabel:libraryBridge.trackSourceLabel(trackKey)
                property url rowCover:cover
                property real rowPlays:Number(plays)
                property real rowSize:Number(fileSize)
                property string rowAdded:importedAt
                width:list.width;height:table.showCovers?Math.max(66,Theme.controlHeight+24):Math.max(32,Math.round(36*Theme.fontScale));radius:8
                positionRevision:list.contentY+row.y;selected:table.checkedKeys.indexOf(trackKey)>=0||table.selectedKey===trackKey;hovered:rowHover.hovered
                // A passive handler keeps row hover when a child button takes
                // the pointer; revealing that button must not hide it again.
                HoverHandler { id:rowHover }
                MouseArea{
                    anchors.fill:parent;acceptedButtons:Qt.LeftButton|Qt.RightButton
                    onPressed:table.forceActiveFocus()
                    onClicked:mouse=>{
                        if(mouse.button===Qt.RightButton){if(table.checkedKeys.indexOf(row.trackKey)<0){table.selected(row.trackKey);if(!table.bulkMode)table.checkedKeys=[]}context.keys=table.bulkMode?table.selectedKeys():[row.trackKey];context.targetKey=row.trackKey;const p=row.mapToItem(context.parent,mouse.x,mouse.y);context.popup(p.x,p.y)}
                        else if(!table.cloudMode&&(table.bulkMode||mouse.modifiers&Qt.ControlModifier)){table.selected(row.trackKey);table.toggleKey(row.trackKey)}
                        else{table.checkedKeys=[];table.selected(row.trackKey)}
                    }
                    onDoubleClicked:if(!table.bulkMode)table.playRequested(row.trackKey)
                }
                Row{
                    anchors.fill:parent
                    Item{
                        width:table.columnWidths[0];height:parent.height

                        CheckBox{visible:table.bulkMode;anchors.centerIn:parent;checked:table.checkedKeys.indexOf(row.trackKey)>=0;onClicked:table.toggleKey(row.trackKey)}
                        UiText { objectName:"trackNumber-"+row.trackKey;fontRegion: "songs";visible:!table.bulkMode&&!rowHover.hovered&&table.currentKey!==row.trackKey;anchors.centerIn:parent;text:row.index+1;color:Theme.secondary;basePixelSize: 13}
                        IconButton{objectName:"trackPlayback-"+row.trackKey;visible:!table.bulkMode&&(rowHover.hovered||table.currentKey===row.trackKey);anchors.centerIn:parent;width:30;height:30;padding:3;horizontalPadding:3;verticalPadding:3;symbol:table.currentKey===row.trackKey&&playbackBridge.playing?"pause":"play";tint:table.currentKey===row.trackKey?Theme.accent:Theme.secondary;onClicked:if(table.currentKey===row.trackKey)playbackBridge.toggle();else table.playRequested(row.trackKey)}
                    }
                    Item{width:table.columnWidths[1];height:parent.height;visible:width>0;clip:true;
                        IconButton {
                            objectName:"trackFavorite-"+row.trackKey
                            visible:table.showFavoriteColumn
                            anchors.centerIn:parent;width:30;height:30;padding:3
                            symbol:row.favorite?"liked":"heart";tint:row.favorite?Theme.accent:Theme.secondary
                            text:row.favorite?"取消喜欢":"加入我喜欢"
                            onClicked:if(!table.bulkMode)table.favoriteRequested(row.trackKey)
                        }
                    }
                    Item{
                        width:table.columnWidths[2];height:parent.height

                        id:titleCell
                        readonly property real textLeft:table.showCovers?(row.videoTrack?40*4/3+16:56):8
                        Artwork{objectName:"trackCover-"+row.trackKey;contentKey:row.trackKey;animated:false;visible:table.showCovers;x:6;anchors.verticalCenter:parent.verticalCenter;width:row.videoTrack?40*4/3:40;height:40;videoCover:row.videoTrack;file:row.rowCover;seed:row.title;resolution:128;}
                        UiText {id:songTitle;objectName:"trackTitle-"+row.trackKey;fontRegion:"songs";x:titleCell.textLeft;width:Math.max(0,parent.width-x-8-(vip.visible?vip.width+6:0));anchors.verticalCenter:parent.verticalCenter;text:row.title+(!table.cloudMode&&!table.sourceColumn&&row.sourceLabel.length?" · "+row.sourceLabel:"");textFormat:Text.PlainText;color:table.currentKey===row.trackKey?Theme.accent:Theme.text;basePixelSize:15;font.weight:Font.DemiBold;elide:Text.ElideRight}
                        Rectangle {
                            id:vip;objectName:"trackVip-"+row.trackKey;visible:row.requiresSubscription&&settingsBridge.values.showSongVip
                            x:Math.max(titleCell.textLeft,Math.min(titleCell.width-width-8,songTitle.x+songTitle.implicitWidth+6));anchors.verticalCenter:parent.verticalCenter
                            width:Math.ceil(27*Theme.fontScale);height:Math.ceil(15*Theme.fontScale);radius:3;color:"transparent";border.color:"#c69545"
                            UiText {anchors.centerIn:parent;text:"VIP";basePixelSize:10;color:"#c69545"}
                        }
                    }
                    Repeater{
                        model:table.statisticsMode?4:5
                        delegate:Item{
                            required property int index
                            readonly property bool sourceCell:index===3&&table.sourceColumn&&!table.statisticsMode
                            width:table.columnWidths[index+3];height:row.height;visible:width>0;clip:true
                            UiText { visible:!parent.sourceCell;fontRegion: "songs";anchors.fill:parent;anchors.leftMargin:8;anchors.rightMargin:8;text:index===0?row.artist:index===1?row.album:index===2?table.timeText(row.duration):index===3?table.statisticsMode?row.rowPlays:table.sizeText(row.rowSize):row.rowAdded.length?row.rowAdded.slice(0,10):"—";textFormat:Text.PlainText;color:Theme.secondary;basePixelSize: 14;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
                            Loader {
                                active:parent.sourceCell
                                anchors.fill:parent
                                anchors.leftMargin:8
                                anchors.rightMargin:8
                                sourceComponent:RowLayout {
                                    objectName:"trackSource-"+row.trackKey
                                    spacing:6
                                    readonly property bool cloud:row.sourceLabel.length>0
                                    readonly property int markerSize:Math.ceil(18*Theme.fontScale)
                                    Rectangle {
                                        Layout.preferredWidth:parent.markerSize
                                        Layout.preferredHeight:parent.markerSize
                                        visible:!parent.cloud
                                        color:"transparent"
                                        border.color:Theme.secondary
                                        UiText {anchors.centerIn:parent;text:"本";basePixelSize:12;color:Theme.secondary}
                                    }
                                    Image {
                                        Layout.preferredWidth:parent.markerSize
                                        Layout.preferredHeight:parent.markerSize
                                        visible:parent.cloud
                                        source:visible?table.sourceLogos[row.sourceLabel]||"":""
                                        sourceClipRect:row.sourceLabel==="酷狗"?Qt.rect(0,0,160,160):Qt.rect(0,0,0,0)
                                        fillMode:Image.PreserveAspectFit
                                        mipmap:true
                                        Accessible.name:row.sourceLabel
                                    }
                                    UiText {
                                        Layout.fillWidth:true
                                        Layout.minimumWidth:0
                                        text:parent.cloud?"云端":table.sizeText(row.rowSize)
                                        fontRegion:"songs"
                                        basePixelSize:13
                                        color:Theme.secondary
                                        elide:Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }
            EmptyState{anchors.centerIn:parent;width:Math.min(parent.width-40,440);visible:list.count===0;heading:table.emptyHeading;description:table.emptyDescription}
        }
    }
    RoundMenu {
        id:context;parent:table.Overlay.overlay;property var keys:[];property string targetKey:"0"
        readonly property bool downloaded:{if(!table.cloudMode)return false;onlineDownloads.revision;return table.downloadState(targetKey)}
        readonly property bool hasOnline:table.cloudMode||libraryBridge.hasOnlineSelection(keys)
        RoundMenuItem {text:"多选";visible:!table.cloudMode;onTriggered:table.beginBulk()}
        RoundMenuItem {objectName:"findLyrics";visible:!context.hasOnline;text:"查找歌词…";onTriggered:table.lyricsRequested(context.keys)}
        MenuSeparator{visible:!table.cloudMode}
        RoundMenuItem {text:"播放";onTriggered:table.playRequested(context.targetKey)}
        RoundMenuItem {objectName:"trackEqualizer";text:"歌曲均衡器…";visible:context.keys.length===1;onTriggered:equalizerBridge.requestSong(context.targetKey)}
        RoundMenuItem {objectName:"trackPlayNext";text:"下一首播放";onTriggered:libraryBridge.enqueueNext(context.keys)}
        RoundMenuItem {text:"追加播放队列";onTriggered:libraryBridge.appendQueue(context.keys)}
        RoundMenuItem {objectName:"cloudTrackDownload";visible:table.cloudMode&&table.allowDownload;text:context.downloaded?"已下载":"下载";enabled:table.cloudMode&&!onlineDownloads.busy&&!context.downloaded;onTriggered:table.downloadRequested(context.targetKey)}
        RoundMenuItem {text:"加入我喜欢";onTriggered:libraryBridge.applyFavorite(context.keys,true)}
        RoundMenuItem {text:"取消喜欢";onTriggered:libraryBridge.applyFavorite(context.keys,false)}
        RoundMenu {
            id:addMenu;title:"加入歌单"
            Instantiator{
                model:playlistModel
                delegate:RoundMenuItem {required property string name;required property string playlistKey;required property bool cloud;text:name;visible:!cloud;onTriggered:libraryBridge.addTracksToPlaylist(playlistKey,context.keys)}
                onObjectAdded:(index,object)=>addMenu.insertItem(index,object)
                onObjectRemoved:(index,object)=>addMenu.removeItem(object)
            }
            MenuSeparator{}
            RoundMenuItem {text:"创建新歌单并加入…";onTriggered:newList.open()}
        }
        MenuSeparator{visible:!table.cloudMode}
        RoundMenuItem {text:"设置歌曲封面…";visible:!context.hasOnline&&context.keys.length===1;onTriggered:coverPicker.open()}
        RoundMenuItem {text:"导入本地歌词…";visible:!context.hasOnline&&context.keys.length===1;onTriggered:lyricsSearchBridge.requestLocalImport(context.targetKey)}
        RoundMenuItem {text:"打开音乐所在目录";visible:!context.hasOnline&&context.keys.length===1;onTriggered:libraryBridge.openTrackFolder(context.targetKey)}
        MenuSeparator{visible:!table.cloudMode}
        RoundMenuItem {text:"在歌单中上移";visible:Number(libraryBridge.playlistId)>0;enabled:!table.cloudMode;onTriggered:libraryBridge.moveTracksInPlaylist(context.keys,-1)}
        RoundMenuItem {text:"在歌单中下移";visible:Number(libraryBridge.playlistId)>0;enabled:!table.cloudMode;onTriggered:libraryBridge.moveTracksInPlaylist(context.keys,1)}
        RoundMenuItem {text:"从此歌单移除";visible:!table.cloudMode&&Number(libraryBridge.playlistId)>0;onTriggered:libraryBridge.removeTracksFromPlaylist(context.keys)}
        RoundMenuItem {text:"从资料库移除…";visible:!context.hasOnline;onTriggered:removal.open()}
    }
    ConfirmDialog{
        id:removal;parent:table.Overlay.overlay;width:430;anchors.centerIn:parent;modal:true;title:"从资料库移除";standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:Label{text:"从资料库移除 "+context.keys.length+" 首歌曲及其歌单关联和统计？音乐文件会保留。";wrapMode:Text.Wrap}
        onAccepted:{libraryBridge.removeTracks(context.keys,false);table.clearBulk()}
    }
    ConfirmDialog{
        id:newList;objectName:"trackNewPlaylistDialog";parent:table.Overlay.overlay;width:400;anchors.centerIn:parent;modal:true;title:"创建新歌单并加入";standardButtons:Dialog.Save|Dialog.Cancel
        contentItem:EntryField {id:newListName;objectName:"trackNewPlaylistName";text:"新歌单";selectByMouse:true;maximumLength:120}
        onAccepted:libraryBridge.createPlaylistWithTracks(newListName.text,context.keys)
    }
    FileDialog{id:coverPicker;title:"歌曲封面";nameFilters:["图片 (*.png *.jpg *.jpeg *.webp)"];onAccepted:libraryBridge.setTrackCover(context.targetKey,selectedFile)}
}
