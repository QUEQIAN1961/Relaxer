import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../components"
import "../theme"

Item {
    id:page;objectName:"discoverPage"
    property int sourceIndex:0
    readonly property bool qqSelected: sourceIndex === 1
    readonly property var musicSource: qqSelected ? qqMusic : onlineMusic
    readonly property string accountDisplayName:{const name=musicSource.nickname;return name.replace(/[\s\u200b\u3164\uffa0]/g,"").length?name:(qqSelected?"QQ 音乐账号":"网易云账号")}
    property var loginSource: onlineMusic
    property Item backdropSource:page
    property string searchQuery:""
    property string submittedQuery:""
    property bool searchMode:false
    property int searchCategory:0
    readonly property var searchKinds:["search","search-artists","search-albums","search-playlists"]
    readonly property bool searchCards:searchMode&&activeView.kind.startsWith("search")&&searchCategory>0
    property bool showPlaylists:true
    property string selectedSongKey:""
    property string pinnedKey:""
    property bool initialized:false
    property var sourceViews:[{kind:"featured",argument:""},{kind:"featured",argument:""}]
    property var sourceHistory:[[],[]]
    readonly property var activeView:sourceViews[sourceIndex]
    readonly property var qualityKeys:["standard","exhigh","lossless","hires"]
    readonly property var qualityLabels:["标准","极高","无损","Hi-Res"]
    function visibleStatus(text){
        if(/^正在获取(?:网易云| QQ 音乐)播放地址/.test(text)||/^正在播放 · (?:网易云音乐|QQ 音乐)$/.test(text))return ""
        // The header already contains the song count; retain warnings following it.
        return text.replace(/^(?:QQ 音乐)?歌单 · \d+ 首(?:歌曲)?(?: · )?/,"")
    }
    readonly property string pageStatus:visibleStatus(musicSource.status)
    readonly property bool showCatalogProgress:musicSource.catalogLoading&&musicSource.busy
    function startLogin(){loginSource=musicSource;login.open();loginSource.startLogin()}
    function closeAccountMenu(){accountMenu.dismiss()}
    function openAccountMenu(){
        search.close()
        if(accountMenu.visible){closeAccountMenu();return}
        downloadList.close()
        accountMenu.popup(account,account.width-accountMenu.width,account.height+6)
    }
    onVisibleChanged:if(!visible){
        closeAccountMenu();downloadList.close()
        if(searchMode){const views=sourceViews.slice();views[sourceIndex]={kind:"featured",argument:""};sourceViews=views;searchMode=false;const history=sourceHistory.slice();history[sourceIndex]=[];sourceHistory=history}
    }else if(initialized)showPage(activeView.kind,activeView.argument)
    function showPage(kind,argument,refresh){navigateView(kind,argument,refresh,false)}
    function navigateView(kind,argument,refresh,goingBack){
        if(!goingBack&&(kind!==activeView.kind||(argument||"")!==activeView.argument)){
            const history=sourceHistory.slice(),entries=history[sourceIndex].slice()
            entries.push({kind:activeView.kind,argument:activeView.argument,searchMode:searchMode,category:searchCategory})
            history[sourceIndex]=entries;sourceHistory=history
        }
        if(searchKinds.indexOf(kind)>=0){searchMode=true;searchCategory=searchKinds.indexOf(kind)}
        else if(["featured","mine","daily","ai"].indexOf(kind)>=0)searchMode=false
        const views=sourceViews.slice();views[sourceIndex]={kind:kind,argument:argument||""};sourceViews=views
        musicSource.activate();musicSource.showPage(kind,argument||"",!!refresh)
        showPlaylists=kind==="featured"||kind==="mine"||searchCards
        syncPinnedKey()
    }
    function backPage(){
        const history=sourceHistory.slice(),entries=history[sourceIndex].slice()
        if(!entries.length){showHome();return}
        const previous=entries.pop();history[sourceIndex]=entries;sourceHistory=history
        searchMode=previous.searchMode;searchCategory=previous.category
        navigateView(previous.kind,previous.argument,false,true)
    }
    function submitSearch(value){
        if(!value.trim().length)return
        submittedQuery=value.trim();searchQuery=value;showPage("search",submittedQuery)
    }
    function editSearch(value){searchQuery=value;if(!value.trim().length&&searchMode){submittedQuery="";showHome()}}
    function chooseSearchCategory(index){searchCategory=index;showPage(searchKinds[index],submittedQuery)}
    function chooseSource(index){
        closeAccountMenu();search.close();onlineMusic.cancelRecommendation();qqMusic.cancelRecommendation()
        sourceIndex=index;searchMode=false;showPage(activeView.kind,activeView.argument)
    }
    function syncPinnedKey(){
        let key=""
        if(!showPlaylists&&musicSource.currentPlaylist.id)for(let i=0;i<playlistModel.count;++i){const row=playlistModel.get(i);if(row.provider===(qqSelected?"qq":"netease")&&row.remoteId===String(musicSource.currentPlaylist.id)){key=row.playlistKey;break}}
        pinnedKey=key
    }
    onShowPlaylistsChanged:syncPinnedKey()
    function showHome(){pinnedKey="";showPage("featured","")}
    Component.onCompleted:{initialized=true;showHome()}
    ColumnLayout {
        anchors.fill:parent;anchors.margins:Theme.pageMargin;anchors.topMargin:28;anchors.bottomMargin:0;spacing:12
        Item {
            id:header;Layout.fillWidth:true;Layout.preferredHeight:compact?84:36;Layout.bottomMargin:Theme.pageHeadingGap-12
            readonly property real toolButtonsWidth:3*36+3*8
            readonly property bool compact:width<heading.implicitWidth+12+toolButtonsWidth+104
            UiText {id:heading;objectName:"discoverHeading";text:"发现音乐";height:36;verticalAlignment:Text.AlignVCenter;font.pixelSize:28;font.weight:Font.DemiBold;color:Theme.text}
            RowLayout {
                id:accountTools;objectName:"onlineAccountTools";anchors.right:parent.right;y:header.compact?48:0
                width:Math.min(header.toolButtonsWidth+Math.max(104,Math.min(220,account.implicitWidth)),parent.width-(header.compact?0:heading.implicitWidth+12));height:36;spacing:8
                Button {
                    id:downloadsButton;objectName:"onlineDownloadsButton";Layout.preferredWidth:36;Layout.preferredHeight:36;padding:7;hoverEnabled:true
                    Accessible.name:"下载列表"
                    contentItem:DownloadGlyph {id:downloadGlyph}
                    background:Rectangle {radius:8;color:downloadsButton.down||downloadsButton.hovered||downloadList.visible?Theme.hover:"transparent";border.width:downloadsButton.activeFocus?1:0;border.color:Theme.accent}
                    onClicked:{
                        page.closeAccountMenu();search.close()
                        if(downloadList.visible){downloadList.close();return}
                        downloadList.open()
                    }
                    HintBubble {visible:downloadsButton.hovered;text:"下载列表"}
                    Connections {target:onlineDownloads;function onTaskAdded(){downloadGlyph.added()}}
                }
                IconButton {objectName:"onlineRefresh";symbol:"refresh";tip:"刷新当前页面";showTooltip:true;Layout.preferredWidth:36;Layout.preferredHeight:36;enabled:!page.musicSource.catalogLoading;onClicked:page.showPage(page.activeView.kind,page.activeView.argument,true)}
                GlassSearch {
                    id:search;objectName:"onlineSearchControl";Layout.preferredWidth:36;Layout.preferredHeight:36
                    backdrop:page.backdropSource;searchName:"online";searchLabel:"搜索在线音乐";placeholderText:"搜索歌曲、歌手";query:page.searchQuery
                    onQueryEdited:value=>page.editSearch(value)
                    onAccepted:value=>page.submitSearch(value)
                }
                Button {
                    id:account;objectName:"onlineAccountButton"
                    Layout.preferredWidth:implicitWidth;Layout.minimumWidth:104;Layout.maximumWidth:Math.max(104,Math.min(220,accountTools.width-header.toolButtonsWidth))
                    Layout.preferredHeight:36;implicitWidth:accountName.implicitWidth+58
                    padding:6;leftPadding:8;rightPadding:8;hoverEnabled:true
                    Accessible.name:page.musicSource.loggedIn?page.accountDisplayName+"，账号菜单":"扫码登录"
                    contentItem:Item {
                        Artwork {
                            objectName:"onlineAccountAvatar";x:0;anchors.verticalCenter:parent.verticalCenter;width:28;height:28;radius:14;animated:false;resolution:64
                            visible:page.musicSource.loggedIn;file:page.musicSource.avatarUrl;seed:page.musicSource.nickname
                        }
                        Symbol {visible:!page.musicSource.loggedIn;anchors.verticalCenter:parent.verticalCenter;width:28;height:28;name:"person";color:Theme.secondary}
                        UiText {id:accountName;objectName:"onlineAccountName";x:36;width:Math.max(0,parent.width-x);anchors.verticalCenter:parent.verticalCenter;text:page.musicSource.loggedIn?page.accountDisplayName:"扫码登录";color:Theme.text;font.pixelSize:14;font.weight:Font.Medium;textFormat:Text.PlainText;elide:Text.ElideRight}
                    }
                    background:Rectangle {radius:8;color:account.down?Theme.pressed:account.hovered||accountMenu.visible?Theme.hover:"transparent";border.width:account.activeFocus?1:0;border.color:Theme.accent}
                    onClicked:if(page.musicSource.loggedIn)page.openAccountMenu();else page.startLogin()
                    TapHandler {acceptedButtons:Qt.RightButton;onTapped:page.openAccountMenu()}
                }
            }
        }
        Flow {
            objectName:"onlineDiscoveryTabs";Layout.fillWidth:true;spacing:8;visible:!page.searchMode
            ShellButton {objectName:"onlineFeatured";text:"推荐歌单";enabled:!page.musicSource.catalogLoading;onClicked:page.showHome()}
            ShellButton {objectName:"onlineDaily";text:"每日推荐";enabled:page.musicSource.loggedIn&&!page.musicSource.catalogLoading;onClicked:page.showPage("daily","")}
            ShellButton {objectName:"onlinePlaylists";text:"我的歌单";enabled:page.musicSource.loggedIn&&!page.musicSource.catalogLoading;onClicked:page.showPage("mine","")}
            ShellButton {objectName:"onlineAiRecommend";text:"AI推荐";primary:true;enabled:!page.musicSource.catalogLoading;onClicked:page.showPage("ai","")}
            ShellButton {text:"取消推荐";visible:page.musicSource.catalogLoading&&page.musicSource.status.startsWith("AI");onClicked:page.musicSource.cancelRecommendation()}
        }
        Flow {
            objectName:"onlineSearchTabs";Layout.fillWidth:true;spacing:8;visible:page.searchMode
            Repeater {
                model:["单曲","歌手","专辑","歌单"]
                delegate:ShellButton {required property int index;required property string modelData;objectName:"onlineSearchCategory-"+index;text:modelData;primary:page.searchCategory===index;onClicked:page.chooseSearchCategory(index)}
            }
        }
        RowLayout {
            objectName:"onlinePlaylistHeading";Layout.fillWidth:true;visible:!page.showPlaylists&&!!page.musicSource.currentPlaylist.id;spacing:10
            Artwork {Layout.preferredWidth:56;Layout.preferredHeight:56;animated:false;radius:8;file:page.musicSource.currentPlaylist.cover||"";seed:page.musicSource.currentPlaylist.name||"歌单";imageUrl:String(file).length?file:"image://relaxer/artwork?seed="+encodeURIComponent(seed)+"&resolution=128"}
            ColumnLayout {
                Layout.fillWidth:true
                UiText {Layout.fillWidth:true;text:page.musicSource.currentPlaylist.name||"歌单";font.pixelSize:18;font.weight:Font.DemiBold;elide:Text.ElideRight;textFormat:Text.PlainText}
                UiText {text:(page.qqSelected?"QQ 音乐":"网易云音乐")+" · "+Number(page.musicSource.currentPlaylist.count||0)+" 首";font.pixelSize:12;color:Theme.secondary}
            }
            Column {
                objectName:"onlinePlaylistActions";Layout.preferredWidth:32;Layout.preferredHeight:56;spacing:4
                IconButton {objectName:"onlinePlaylistBack";width:32;height:26;padding:3;symbol:"arrow-left";tip:"返回";showTooltip:true;onClicked:page.backPage()}
                IconButton {objectName:"pinOnlinePlaylist";width:32;height:26;padding:3;symbol:"pin";tip:page.pinnedKey.length?"移出侧栏":"加入侧栏";showTooltip:true;tint:page.pinnedKey.length?Theme.accent:Theme.text;visible:["artist","album"].indexOf(page.musicSource.currentPlaylist.kind)<0;onClicked:{if(page.pinnedKey.length){libraryBridge.deletePlaylist(page.pinnedKey);page.pinnedKey=""}else{const key=libraryBridge.pinCloudPlaylist(page.musicSource.currentPlaylist);if(key!=="0")page.pinnedKey=key}}}
            }
        }
        RowLayout {
            objectName:"onlineCatalogStatus";Layout.fillWidth:true;Layout.minimumHeight:20;spacing:8
            visible:page.pageStatus.length>0||page.showCatalogProgress
            BusyIndicator {objectName:"onlineCatalogSpinner";Layout.preferredWidth:20;Layout.preferredHeight:20;running:page.showCatalogProgress;visible:running}
            UiText {text:page.pageStatus;Layout.fillWidth:true;font.pixelSize:12;color:Theme.secondary;wrapMode:Text.Wrap}
        }
        GridView {
            id:playlistGrid;objectName:"onlinePlaylistCards";Layout.fillWidth:true;Layout.fillHeight:true;visible:page.showPlaylists
            clip:true;model:page.searchCards?page.musicSource.searchResults:page.musicSource.playlists;reuseItems:true;cellWidth:width/Math.max(2,Math.floor(width/180));cellHeight:cellWidth+46;boundsBehavior:Flickable.StopAtBounds
            SmoothWheelHandler {flickable:playlistGrid}
            ScrollBar.vertical:ScrollBar{}
            delegate:Item {
                id:card;required property var modelData;width:playlistGrid.cellWidth;height:playlistGrid.cellHeight
                objectName:"onlinePlaylist-"+modelData.id
                HoverHandler {id:cardHover}
                HighlightSurface {objectName:"onlinePlaylistBackground";width:parent.width-4;height:parent.height-8;radius:12;positionRevision:playlistGrid.contentY+card.x+card.y;hovered:cardHover.hovered}
                Artwork {x:5;y:5;width:parent.width-14;height:width;radius:10;animated:false;seed:card.modelData.name;file:card.modelData.cover||"";imageUrl:String(file).length?file:"image://relaxer/artwork?seed="+encodeURIComponent(seed)+"&resolution=256"}
                UiText {x:5;y:parent.width-4;width:parent.width-14;text:card.modelData.name;textFormat:Text.PlainText;font.pixelSize:14;font.weight:Font.DemiBold;maximumLineCount:2;wrapMode:Text.Wrap;elide:Text.ElideRight}
                MouseArea {
                    anchors.fill:parent;cursorShape:Qt.PointingHandCursor
                    onClicked:{page.pinnedKey="";page.showPage(card.modelData.kind||"playlist",String(card.modelData.id))}
                }
            }
            EmptyState {anchors.centerIn:parent;width:Math.min(parent.width-24,400);visible:!playlistGrid.count&&!page.musicSource.busy;heading:page.searchCards?"暂无搜索结果":"暂无歌单";description:page.searchCards?"试试其他关键词或搜索分类。":"可切换平台，或登录查看你的云端歌单。"}
        }
        ListView {
            id:songs;objectName:"onlineSongs";Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:60
            visible:!page.showPlaylists
            model:page.musicSource.songs;clip:true;spacing:4;reuseItems:true;boundsBehavior:Flickable.StopAtBounds
            ScrollBar.vertical:ScrollBar{}
            SmoothWheelHandler {flickable:songs}
            delegate:HighlightSurface {
                id:row;required property var modelData;required property int index
                readonly property bool current:playbackBridge.trackId===String((page.qqSelected?2000000000000:1000000000000)+Number(modelData.id))
                readonly property string songKey:(page.qqSelected?"qq:":"netease:")+String(modelData.id)
                selected:page.selectedSongKey===songKey
                objectName:"onlineSong-"+modelData.id;width:songs.width;height:modelData.reason.length?88:64;radius:8
                positionRevision:songs.contentY+row.y;hovered:rowHover.hovered
                HoverHandler {id:rowHover}
                MouseArea {anchors.fill:parent;onClicked:page.selectedSongKey=row.songKey;onDoubleClicked:page.musicSource.playSong(row.modelData.id)}
                RowLayout {
                    anchors.fill:parent;anchors.margins:8;spacing:10
                    IconButton {
                        objectName:"onlinePlay-"+row.modelData.id;Layout.preferredWidth:30;Layout.preferredHeight:30;padding:3;horizontalPadding:3;verticalPadding:3
                        symbol:row.current&&playbackBridge.playing?"pause":"play";tip:row.current&&playbackBridge.playing?"暂停":"播放";showTooltip:true
                        onClicked:if(row.current)playbackBridge.toggle();else page.musicSource.playSong(row.modelData.id)
                    }
                    Artwork {
                        objectName:"onlineCover-"+row.modelData.id
                        Layout.preferredWidth:40;Layout.preferredHeight:40;radius:6;resolution:128;animated:false
                        file:row.modelData.thumbnail||row.modelData.cover||"";seed:row.modelData.title
                        imageUrl:String(file).length?file:"image://relaxer/artwork?seed="+encodeURIComponent(seed)+"&resolution=128&radiusRatio=0"
                    }
                    ColumnLayout {
                        Layout.fillWidth:true;spacing:3
                        UiText {text:row.modelData.title+(page.qqSelected&&row.modelData.requiresSubscription?"  VIP":"");Layout.fillWidth:true;font.pixelSize:15;font.weight:Font.DemiBold;color:row.current?Theme.accent:Theme.text;elide:Text.ElideRight;textFormat:Text.PlainText}
                        UiText {text:row.modelData.artist+(row.modelData.album.length?" · "+row.modelData.album:"")+(row.current&&playbackBridge.matchedLocalTrackId!=="0"?" · 已关联本地":"");Layout.fillWidth:true;font.pixelSize:12;color:Theme.secondary;elide:Text.ElideRight;textFormat:Text.PlainText}
                        UiText {text:row.modelData.reason;visible:text.length>0;Layout.fillWidth:true;font.pixelSize:12;color:Theme.secondary;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight;textFormat:Text.PlainText}
                    }
                    OnlineDownloadButton {objectName:"downloadOnline-"+row.modelData.id;Layout.preferredWidth:36;Layout.preferredHeight:36;provider:page.qqSelected?"qq":"netease";song:row.modelData;quality:page.musicSource.quality}
                }
            }
            EmptyState {anchors.centerIn:parent;width:Math.min(parent.width-24,400);visible:!page.musicSource.songs.length;heading:"发现下一首喜欢的歌";description:"搜索在线歌曲，或登录"+(page.qqSelected?"QQ 音乐":"网易云")+"读取推荐和歌单。"}
        }
    }
    DownloadList {id:downloadList;parent:downloadsButton}
    TextMetrics {id:accountMenuText;font:accountMenu.font;text:"音乐音质"}
    TextMetrics {id:qualityMenuText;font:qualityMenu.font;text:"Hi-Res"}
    TextMetrics {id:sourceMenuText;font:sourceMenu.font;text:"网易云音乐"}
    RoundMenu {
        id:accountMenu;objectName:"onlineAccountMenu";parent:page.Overlay.overlay
        width:Math.ceil(accountMenuText.width)+48;padding:4;cascade:true
        delegate:RoundMenuItem {
            id:accountEntry;objectName:subMenu===qualityMenu?"onlineQualityEntry":"onlineSourceEntry";implicitHeight:32;leftPadding:28;rightPadding:10
            LayoutMirroring.enabled:false
            arrow:Symbol {objectName:accountEntry.subMenu===qualityMenu?"onlineQualityArrow":"onlineSourceArrow";visible:!!accountEntry.subMenu;name:"back";width:14;height:14;x:8;y:(accountEntry.height-height)/2;color:Theme.secondary}
        }
        closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutsideParent
        enter:Transition {NumberAnimation {target:accountMenu;property:"expansion";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:140;easing.type:Easing.OutCubic}}
        exit:Transition {NumberAnimation {target:accountMenu;property:"expansion";to:0;duration:settingsBridge.values.reduceMotion?0:90;easing.type:Easing.InCubic}}
        RoundMenu {
            id:sourceMenu;objectName:"onlineSourceMenu";width:Math.ceil(sourceMenuText.width)+46;padding:4;title:"音乐来源";cascade:true
            enter:Transition {NumberAnimation {target:sourceMenu;property:"expansion";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:140;easing.type:Easing.OutCubic}}
            exit:Transition {NumberAnimation {target:sourceMenu;property:"expansion";to:0;duration:settingsBridge.values.reduceMotion?0:90;easing.type:Easing.InCubic}}
            RoundMenuItem {objectName:"onlineSource-netease";text:"网易云音乐";checkable:true;checked:!page.qqSelected;implicitHeight:32;leftPadding:28;rightPadding:10;onTriggered:page.chooseSource(0)}
            RoundMenuItem {objectName:"onlineSource-qq";text:"QQ 音乐";checkable:true;checked:page.qqSelected;implicitHeight:32;leftPadding:28;rightPadding:10;onTriggered:page.chooseSource(1)}
        }
        RoundMenu {
            id:qualityMenu;objectName:"onlineQualityMenu";width:Math.ceil(qualityMenuText.width)+46;padding:4;title:"音乐音质";cascade:true
            enter:Transition {NumberAnimation {target:qualityMenu;property:"expansion";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:140;easing.type:Easing.OutCubic}}
            exit:Transition {NumberAnimation {target:qualityMenu;property:"expansion";to:0;duration:settingsBridge.values.reduceMotion?0:90;easing.type:Easing.InCubic}}
            Instantiator {
                model:page.qualityKeys
                delegate:RoundMenuItem {
                    required property string modelData;required property int index
                    objectName:"onlineQuality-"+modelData;text:page.qualityLabels[index];checkable:true;checked:page.musicSource.quality===modelData;implicitHeight:32;leftPadding:28;rightPadding:10
                    onTriggered:page.musicSource.quality=modelData
                }
                onObjectAdded:(index,object)=>qualityMenu.insertItem(index,object)
                onObjectRemoved:(index,object)=>qualityMenu.removeItem(object)
            }
        }
        MenuSeparator {padding:0;topPadding:3;bottomPadding:3;contentItem:Rectangle {implicitHeight:1;color:Theme.border}}
        RoundMenuItem {objectName:"onlineLogout";implicitHeight:32;leftPadding:10;rightPadding:10;text:page.musicSource.loggedIn?"退出账号":"扫码登录";destructive:page.musicSource.loggedIn;onTriggered:if(page.musicSource.loggedIn)page.musicSource.logout();else page.startLogin()}
    }
    ConfirmDialog {
        id:login;objectName:"neteaseLoginDialog";parent:page.Overlay.overlay;width:Math.min(360,parent.width-32);anchors.centerIn:parent
        title:page.loginSource===qqMusic?"QQ 音乐扫码登录":"网易云扫码登录";standardButtons:Dialog.Cancel
        onClosed:page.loginSource.stopLogin()
        contentItem:ColumnLayout {
            spacing:12
            ChoiceBox {
                objectName:"qqMusicLoginChannel";Layout.fillWidth:true;visible:page.loginSource===qqMusic
                model:["手机 QQ 扫码","微信扫码"]
                currentIndex:qqMusic.loginChannel==="wx"?1:0
                onActivated:{qqMusic.loginChannel=currentIndex===1?"wx":"qq";qqMusic.startLogin()}
            }
            Rectangle {
                Layout.alignment:Qt.AlignHCenter;Layout.preferredWidth:220;Layout.preferredHeight:220;color:"white";radius:8
                Image {anchors.fill:parent;anchors.margins:4;source:page.loginSource.qrImage;fillMode:Image.PreserveAspectFit;smooth:false}
            }
            UiText {text:page.loginSource.status;Layout.fillWidth:true;color:Theme.secondary;wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter}
            ShellButton {text:"刷新二维码";Layout.alignment:Qt.AlignHCenter;enabled:page.loginSource.qrImage.length>0||!page.loginSource.busy;onClicked:page.loginSource.startLogin()}
        }
    }
    Connections {target:page.loginSource;function onChanged(){if(page.loginSource.loggedIn&&login.visible)login.close()}}
    Connections {target:page.musicSource;function onSongsChanged(){if(page.musicSource.songs.length>0)page.showPlaylists=false}function onChanged(){page.syncPinnedKey()}}
    Connections {target:playlistModel;function onCountChanged(){page.syncPinnedKey()}}
    Connections {target:page.Window.window;function onActiveChanged(){if(!page.Window.window.active)page.closeAccountMenu()}}
}
