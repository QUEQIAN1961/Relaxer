import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"
Item {
    id:page;objectName:"historyPage"
    property Item backdropSource:page
    property string query:""
    property string sortKey:"endedAt"
    property bool descending:true
    property var rows:[]
    property bool hasMore:false
    function savePageState(){return {query:query,sortKey:sortKey,descending:descending}}
    function restorePageState(state){query=state.query||"";sortKey=state.sortKey||"endedAt";descending=state.descending!==false;reload()}
    readonly property var columnKeys:["title","artist","endedAt","platform"]
    readonly property var columnLabels:["歌曲","歌手","播放时间","平台"]
    readonly property var columnWidths:{const width=tableViewport.contentWidth-10,time=166*Theme.fontScale;if(Theme.compact)return [Math.max(0,width-time),0,time,0];const platform=84*Theme.fontScale,artist=Math.max(100*Theme.fontScale,(width-time-platform)*.27);return [width-time-platform-artist,artist,time,platform]}
    function columnX(index){let x=0;for(let i=0;i<index;++i)x+=columnWidths[i];return x}
    function sortBy(key){descending=sortKey===key?!descending:key==="endedAt";sortKey=key;reload()}
    function reload(){rows=appBridge.playbackHistory(query,0,sortKey,descending);hasMore=rows.length===100;list.positionViewAtBeginning()}
    function more(){if(!hasMore)return;const next=appBridge.playbackHistory(query,rows.length,sortKey,descending);hasMore=next.length===100;rows=rows.concat(next)}
    readonly property var platformIcons:({netease:"qrc:/music-platforms/netease.ico",qqmusic:"qrc:/music-platforms/qq.ico",qishui:"qrc:/music-platforms/qishui.svg",kugou:"qrc:/music-platforms/kugou.png",bilibili:"image://relaxer/icon/video?color=%2300a1d6"})
    onVisibleChanged:if(visible)reload()
    Component.onCompleted:reload()
    Connections {target:appBridge;function onHistoryChanged(){if(page.visible)page.reload()}}
    ColumnLayout {
        anchors.fill:parent;anchors.margins:Theme.pageMargin;anchors.topMargin:28;anchors.bottomMargin:0;spacing:16
        RowLayout {
            Layout.fillWidth:true
            UiText {id:historyTitle;objectName:"historyHeading";text:"历史记录";basePixelSize:28;font.weight:Font.DemiBold;color:Theme.text;Layout.fillWidth:true;elide:Text.ElideRight;opacity:historySearch.opened&&parent.width<implicitWidth+280+parent.spacing?0:1}
            GlassSearch {id:historySearch;objectName:"historySearchControl";backdrop:page.backdropSource;searchName:"history";searchLabel:"搜索播放历史";query:page.query;placeholderText:"搜索歌曲、歌手、平台或日期";onQueryEdited:value=>{page.query=value;page.reload()};onAccepted:value=>{page.query=value;page.reload()}}
        }
        Flickable {
            id:tableViewport;objectName:"historyTableViewport";Layout.fillWidth:true;Layout.fillHeight:true;clip:true
            contentWidth:Theme.compact?width:Math.max(width,620*Theme.fontScale);contentHeight:height;flickableDirection:Flickable.HorizontalFlick;boundsBehavior:Flickable.StopAtBounds;interactive:!Theme.compact
            onContentWidthChanged:if(Theme.compact)contentX=0
            ScrollBar.horizontal:ScrollBar {policy:ScrollBar.AsNeeded;hoverEnabled:true}
            ColumnLayout {
                width:tableViewport.contentWidth;height:tableViewport.height-(tableViewport.contentWidth>tableViewport.width?12:0);spacing:0
                Item {
                    Layout.fillWidth:true;Layout.preferredHeight:38*Theme.fontScale
                    Repeater {model:page.columnKeys
                        Item {
                            id:heading;required property string modelData;required property int index
                            objectName:"historyHeading-"+modelData;x:page.columnX(index);width:page.columnWidths[index];height:parent.height
                            visible:!Theme.compact||index===0||index===2
                            UiText {id:label;x:8;anchors.verticalCenter:parent.verticalCenter;width:Math.max(0,Math.min(implicitWidth,parent.width-34));text:page.columnLabels[heading.index];color:Theme.secondary;basePixelSize:13;elide:Text.ElideRight}
                            Symbol {objectName:"historySort-"+heading.modelData;x:label.x+label.width+5;anchors.verticalCenter:parent.verticalCenter;width:14;height:18;name:page.sortKey!==heading.modelData?"sort-default":page.descending?"sort-descending":"sort-ascending";color:page.sortKey===heading.modelData?Theme.text:Theme.secondary}
                            MouseArea {anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:page.sortBy(heading.modelData)}
                        }
                    }
                }
                Rectangle {Layout.fillWidth:true;height:1;color:Theme.border}
                ListView {
                    id:list;objectName:"historyList";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:2;model:page.rows;reuseItems:true;boundsBehavior:Flickable.StopAtBounds
                    onAtYEndChanged:if(atYEnd)Qt.callLater(function(){if(list.atYEnd)page.more()})
                    ScrollBar.vertical:ListScrollBar{x:Math.min(list.width-width,tableViewport.contentX+tableViewport.width-width)}
                    SmoothWheelHandler {flickable:list}
                    delegate:HighlightSurface {
                        id:row;required property var modelData;width:list.width-10;height:Math.max(56,56*Theme.fontScale);radius:8;hovered:hover.hovered;positionRevision:list.contentY+row.y
                        objectName:"historyRow-"+modelData.historyId
                        HoverHandler {id:hover}
                        MouseArea {anchors.fill:parent;onDoubleClicked:if(row.modelData.available)playbackBridge.playHistory(row.modelData)}
                        Item {
                            width:page.columnWidths[0];height:parent.height
                            IconButton {objectName:"historyPlay-"+row.modelData.historyId;x:4;anchors.verticalCenter:parent.verticalCenter;symbol:"play";tip:"重新播放";showTooltip:true;enabled:row.modelData.available;width:30;height:30;padding:5;onClicked:playbackBridge.playHistory(row.modelData)}
                            Artwork {id:cover;x:38;anchors.verticalCenter:parent.verticalCenter;width:row.modelData.video?40*4/3:40;height:40;videoCover:row.modelData.video;file:row.modelData.coverUrl;contentKey:String(row.modelData.hash||row.modelData.trackId);seed:row.modelData.title;resolution:128;animated:false;}
                            UiText {objectName:"historyTitle-"+row.modelData.historyId;x:cover.x+cover.width+8;anchors.verticalCenter:parent.verticalCenter;width:Math.max(0,parent.width-x-8);text:row.modelData.title;elide:Text.ElideRight;textFormat:Text.PlainText;basePixelSize:14;font.weight:Font.DemiBold;color:Theme.text}
                        }
                        UiText {objectName:"historyArtist-"+row.modelData.historyId;visible:!Theme.compact;x:page.columnX(1)+8;width:Math.max(0,page.columnWidths[1]-16);anchors.verticalCenter:parent.verticalCenter;text:row.modelData.artist;elide:Text.ElideRight;textFormat:Text.PlainText;basePixelSize:13;color:Theme.secondary}
                        UiText {objectName:"historyTime-"+row.modelData.historyId;x:page.columnX(2)+8;width:page.columnWidths[2]-16;anchors.verticalCenter:parent.verticalCenter;text:row.modelData.endedAt;textFormat:Text.PlainText;basePixelSize:13;color:Theme.secondary}
                        Item {
                            objectName:"historyPlatform-"+row.modelData.historyId;visible:!Theme.compact;x:page.columnX(3)+8;width:Math.max(0,page.columnWidths[3]-16);height:parent.height
                            Accessible.name:row.modelData.source
                            UiText {anchors.centerIn:parent;visible:!row.modelData.cloud;text:"本地";basePixelSize:13;color:Theme.secondary}
                            Image {objectName:"historyPlatformLogo-"+row.modelData.historyId;anchors.centerIn:parent;width:24;height:24;visible:row.modelData.cloud;source:visible?page.platformIcons[String(row.modelData.hash).split(":")[0]]||"":"";sourceClipRect:String(row.modelData.hash).startsWith("kugou:")?Qt.rect(0,0,160,160):Qt.rect(0,0,0,0);fillMode:Image.PreserveAspectFit;smooth:true;mipmap:true;asynchronous:true}
                            HoverHandler {id:platformHover}
                            HintBubble {visible:platformHover.hovered;text:row.modelData.source}
                        }
                    }
                    EmptyState {objectName:"historyEmptyState";x:tableViewport.contentX+(tableViewport.width-width)/2;y:(parent.height-height)/2;width:Math.min(tableViewport.width-24,400);visible:!list.count;heading:page.query.length?"没有匹配的历史":"暂无播放历史";description:page.query.length?"试试其他关键词或日期。":"播放过的音乐和视频会在结束、切换或退出后记录在这里。"}
                }
            }
        }
    }
}
