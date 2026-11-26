import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../theme"
Item {
    id:control
    required property Item backdrop
    signal songSelected()
    readonly property bool opened:searchPopup.visible
    readonly property bool presentationActive:visible&&enabled&&(!Window.window||(Window.window.visible&&Window.window.visibility!==Window.Minimized))
    property point anchorPoint:Qt.point(0,0)
    property bool dismissing:false
    property var frozenRows:[]
    property string frozenMessage:""
    property bool frozenHasQuery:false
    readonly property var rows:dismissing?frozenRows:equalizerBridge.songResults
    readonly property string message:dismissing?frozenMessage:equalizerBridge.songSearching?"正在检索歌曲…":equalizerBridge.songSearchMessage
    readonly property bool hasQuery:dismissing?frozenHasQuery:equalizerBridge.songQuery.trim().length>0
    implicitWidth:36;implicitHeight:Theme.controlHeight
    function reposition(){if(searchPopup&&searchPopup.visible)placeDelay.restart()}
    function place(){
        if(!backdrop)return false
        const point=searchButton.mapToItem(backdrop,0,0)
        if(point.y+searchButton.height<0||point.y>backdrop.height){close();return false}
        anchorPoint=point
        searchPopup.expandedWidth=Math.min(420,Math.max(36,backdrop.width-16))
        const left=Math.max(8,Math.min(point.x+searchButton.width-searchPopup.expandedWidth,backdrop.width-searchPopup.expandedWidth-8))
        searchPopup.targetX=left-point.x
        searchPopup.maximumHeight=Math.max(searchPopup.barHeight,backdrop.height-point.y-searchPopup.y-8)
        return true
    }
    function open(){if(presentationActive&&place())searchPopup.open()}
    function close(){if(searchPopup)searchPopup.close()}
    function choose(key){
        if(dismissing||equalizerBridge.songSearching)return
        if(equalizerBridge.selectSongResult(key)){close();songSelected()}
    }
    onPresentationActiveChanged:{if(!presentationActive)close();if(searchPopup)equalizerBridge.setSongSearchActive(presentationActive&&searchPopup.visible&&!dismissing)}
    onXChanged:reposition()
    onYChanged:reposition()
    Component.onCompleted:uiIntegration.registerDismissOnlyPopup(searchPopup)
    Timer{id:placeDelay;interval:0;onTriggered:if(control.opened)control.place()}
    Timer{id:focusDelay;interval:0;onTriggered:if(control.opened&&control.presentationActive&&!control.dismissing){input.forceActiveFocus();input.selectAll()}}
    Connections {target:control.backdrop;function onWidthChanged(){control.reposition()}function onHeightChanged(){control.reposition()}}
    Connections {target:settingsBridge;function onSettingsChanged(){control.reposition()}}
    IconButton {
        id:searchButton;objectName:"peqSongSearchButton";anchors.fill:parent
        text:"搜索歌曲适配";tip:"搜索歌曲、歌手或专辑";symbol:"search";showTooltip:true
        iconColor:control.opened||equalizerBridge.songQuery.length?Theme.accent:Theme.text
        onClicked:if(control.opened)control.close();else control.open()
    }
    Popup {
        id:searchPopup;objectName:"peqSongSearchPopup";parent:searchButton;popupType:Popup.Item
        property bool dismissalContainer:true
        property real reveal:0
        property real expandedWidth:360
        property real targetX:0
        property real maximumHeight:320
        readonly property real barHeight:Math.max(40,Theme.controlHeight)
        readonly property real wantedResultsHeight:Math.min(songResults.contentHeight,5*Theme.controlHeight)+(status.visible?status.implicitHeight+(songResults.count?6:0):0)+12
        x:targetX*reveal;y:(searchButton.height-barHeight)/2
        width:36+(expandedWidth-36)*reveal
        height:barHeight+(control.hasQuery?6+Math.min(wantedResultsHeight,Math.max(0,maximumHeight-barHeight-6)):0)
        padding:0;modal:false;dim:false;focus:true
        closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
        enter:Transition{NumberAnimation{target:searchPopup;property:"reveal";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:220;easing.type:Easing.OutCubic}}
        exit:Transition{NumberAnimation{target:searchPopup;property:"reveal";to:0;duration:settingsBridge.values.reduceMotion?0:120;easing.type:Easing.InCubic}}
        onAboutToShow:{
            control.dismissing=false;control.frozenRows=[];control.frozenMessage=""
            equalizerBridge.setSongSearchActive(control.presentationActive);focusDelay.restart()
        }
        onAboutToHide:{
            control.frozenRows=equalizerBridge.songResults.map(row=>Object.assign({},row))
            control.frozenMessage=equalizerBridge.songSearching?"正在检索歌曲…":equalizerBridge.songSearchMessage
            control.frozenHasQuery=equalizerBridge.songQuery.trim().length>0;control.dismissing=true
            focusDelay.stop();equalizerBridge.setSongSearchActive(false)
        }
        onClosed:{control.frozenRows=[];control.frozenMessage="";control.dismissing=false;equalizerBridge.setSongSearchActive(false)}
        background:Item{}
        contentItem:Item {
            clip:true;opacity:searchPopup.reveal
            Item {
                id:searchBar;objectName:"peqSongSearchBar";width:parent.width;height:searchPopup.barHeight
                GlassSurface {
                    anchors.fill:parent;objectName:"peqSearchGlass";backdrop:control.backdrop;active:control.opened||control.dismissing
                    effectName:"peqSearchBackdropBlur";tintOpacity:Theme.dark?.76:.68;radius:10
                    sampleRect:Qt.rect(control.anchorPoint.x+searchPopup.x,control.anchorPoint.y+searchPopup.y,width,height)
                }
                EntryField {
                    id:input;objectName:"peqSongSearch";anchors.fill:parent;background:Item{}
                    text:equalizerBridge.songQuery;maximumLength:120;placeholderText:"搜索歌曲、歌手或专辑"
                    leftPadding:38;rightPadding:clearButton.visible?36:12
                    Accessible.name:"检索单曲适配"
                    onTextEdited:equalizerBridge.songQuery=text
                    onAccepted:if(control.rows.length)control.choose(control.rows[0].key)
                    Keys.onDownPressed:if(songResults.count){songResults.currentIndex=0;songResults.forceActiveFocus()}
                }
                Symbol{x:12;anchors.verticalCenter:parent.verticalCenter;width:18;height:18;name:"search";color:Theme.secondary}
                IconButton {
                    id:clearButton;objectName:"peqClearSongSearch";anchors.right:parent.right;anchors.rightMargin:4;anchors.verticalCenter:parent.verticalCenter
                    width:28;height:28;padding:7;visible:input.text.length>0;enabled:!control.dismissing
                    text:"清空检索";symbol:"close";onClicked:{equalizerBridge.songQuery="";input.forceActiveFocus()}
                }
            }
            Rectangle {
                id:resultsBox;objectName:"peqSongResultsPanel";x:0;y:searchPopup.barHeight+6;width:parent.width;height:Math.max(0,parent.height-y)
                visible:control.hasQuery;clip:true;radius:10;color:Theme.card;border.color:Theme.border
                // Only the result buttons/input perform actions; unused result space exits search.
                MouseArea{anchors.fill:parent;acceptedButtons:Qt.LeftButton|Qt.RightButton;onClicked:control.close()}
                ColumnLayout {
                    anchors.left:parent.left;anchors.top:parent.top;anchors.bottom:parent.bottom;anchors.margins:6
                    width:Math.max(0,searchPopup.expandedWidth-12);spacing:6
                    ListView {
                        id:songResults;objectName:"peqSongResults";Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:0
                        Layout.preferredHeight:Math.min(contentHeight,5*Theme.controlHeight);clip:true
                        model:control.rows;currentIndex:-1;keyNavigationEnabled:true
                        Keys.onReturnPressed:if(currentItem)currentItem.choose()
                        Keys.onEnterPressed:if(currentItem)currentItem.choose()
                        delegate:ItemDelegate {
                            id:result;required property var modelData;required property int index
                            width:songResults.width;height:Theme.controlHeight;hoverEnabled:true;enabled:!control.dismissing&&!equalizerBridge.songSearching
                            function choose(){control.choose(modelData.key)}
                            Accessible.name:(modelData.title||modelData.name)+"，"+(modelData.artist||"")+"，"+modelData.source
                            onClicked:choose()
                            contentItem:RowLayout {
                                spacing:8
                                UiText{Layout.fillWidth:true;Layout.minimumWidth:0;text:(result.modelData.title||result.modelData.name)+(result.modelData.artist?" — "+result.modelData.artist:"");textFormat:Text.PlainText;elide:Text.ElideRight;basePixelSize:13}
                                UiText{Layout.maximumWidth:result.width*.3;text:result.modelData.source;textFormat:Text.PlainText;elide:Text.ElideRight;basePixelSize:11;color:Theme.secondary}
                            }
                            background:Rectangle{radius:6;color:result.modelData.key===equalizerBridge.targetKey?Theme.selected:result.hovered||songResults.currentIndex===result.index?Theme.hover:"transparent"}
                            HintBubble{visible:result.hovered;text:(result.modelData.title||result.modelData.name)+"\n"+(result.modelData.artist||"")+" · "+(result.modelData.album||"")+"\n"+result.modelData.source}
                        }
                        ScrollBar.vertical:ScrollBar{policy:songResults.contentHeight>songResults.height?ScrollBar.AsNeeded:ScrollBar.AlwaysOff}
                        SmoothWheelHandler{flickable:songResults}
                    }
                    UiText{id:status;Layout.fillWidth:true;text:control.message;visible:text.length>0;textFormat:Text.PlainText;basePixelSize:12;wrapMode:Text.Wrap;color:Theme.secondary}
                }
            }
        }
    }
}
