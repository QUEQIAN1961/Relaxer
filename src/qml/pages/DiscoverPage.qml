import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../components"
import "../theme"

Item {
    id:page;objectName:"discoverPage"
    readonly property bool presentationActive:visible&&(!Window.window||(Window.window.visible&&Window.window.visibility!==Window.Minimized))
    property bool deferInitialCatalog:false
    signal queueLocateFailed(string message)
    property string queueLocateKey:""
    property string queueLocateProvider:""
    property string queueLocateKind:""
    property string queueLocateArgument:""
    property int queueLocateTicks:0
    function locateQueueTrack(key,origin){
        const index=["netease","qq","qishui","kugou","bilibili"].indexOf(origin.provider)
        if(index<0){queueLocateFailed("无法识别歌曲的来源平台");return}
        queueLocateTimer.stop();queueLocateKey="";selectionMode=false;clearSelection();sourceIndex=index
        const kind=origin.kind||"playlist",argument=String(origin.id||origin.title||"")
        showPage(kind,argument,false)
        queueLocateKey=key;queueLocateProvider=provider;queueLocateKind=kind;queueLocateArgument=argument;queueLocateTicks=0;queueLocateTimer.restart()
    }
    Timer {
        id:queueLocateTimer;interval:80;repeat:true
        onTriggered:{
            if(!page.visible||page.provider!==page.queueLocateProvider||page.activeView.kind!==page.queueLocateKind||page.activeView.argument!==page.queueLocateArgument){stop();page.queueLocateKey="";return}
            for(let i=0;i<page.visibleSongs.length;++i){const row=page.visibleSongs[i];if(String(page.songTrackId(row))===page.queueLocateKey){page.selectedSongKey=page.provider+":"+String(row.id);songs.currentIndex=i;songs.positionViewAtIndex(i,ListView.Contain);page.queueLocateKey="";stop();return}}
            page.queueLocateTicks++
            if(page.musicSource.catalogLoading&&page.queueLocateTicks<250)return
            if(page.videoSource&&bilibili.hasMore&&page.queueLocateTicks<250){bilibili.loadMore();return}
            const status=page.musicSource.status,detail=/失败|权限|登录|不可|删除|403|404/.test(status)?"："+status:""
            page.queueLocateFailed("无法在来源歌单中找到该歌曲，可能已被移除或无法访问"+detail);page.queueLocateKey="";stop()
        }
    }
    property int sourceIndex:0
    readonly property bool qqSelected: sourceIndex === 1
    readonly property bool videoSource:sourceIndex===4
    readonly property var musicSource: [onlineMusic,qqMusic,sodaMusic,kugouMusic,bilibili][sourceIndex]
    readonly property string provider: ["netease","qq","qishui","kugou","bilibili"][sourceIndex]
    readonly property string platformName: ["网易云音乐","QQ 音乐","汽水音乐","酷狗音乐","哔哩哔哩"][sourceIndex]
    readonly property string accountDisplayName:{const name=musicSource.nickname;return name.replace(/[\s\u200b\u3164\uffa0]/g,"").length?name:platformName+"账号"}
    readonly property bool showAccountVip:settingsBridge.values.showAccountVip&&musicSource.loggedIn&&musicSource.vip
    property var loginSource: onlineMusic
    property Item backdropSource:page
    property string searchQuery:""
    property string submittedQuery:""
    property bool searchMode:false
    property int searchCategory:0
    readonly property var searchKinds:videoSource?["search","search-users"]:["search","search-artists","search-albums","search-playlists"]
    readonly property bool searchCards:searchMode&&activeView.kind.startsWith("search")&&searchCategory>0
    property bool showPlaylists:true
    property string selectedSongKey:""
    property bool selectionMode:false
    property var selectedIds:[]
    property int selectionAnchor:-1
    property string batchStatus:""
    property var contextSong:({})
    property int contextIndex:-1
    readonly property bool contextIsCurrent:contextRows().length===1&&songTrackId(contextSong)===playbackBridge.trackId
    function contextRows(){return selectionMode&&selectedIds.indexOf(String(contextSong.id))>=0?selectedSongs():[contextSong]}
    function openSongMenu(song,index,anchor,x,y){contextSong=song;contextIndex=index;songMenu.popup(anchor,x,y)}
    function contextAction(action){actOnSongs(contextRows(),action)}
    function beginSelection(){
        if(!selectionMode){selectionMode=true;selectedIds=[String(contextSong.id)];selectionAnchor=contextIndex}
        else if(selectedIds.indexOf(String(contextSong.id))<0){selectedIds=selectedIds.concat([String(contextSong.id)])}
        page.forceActiveFocus()
    }
    function clearSelection(){selectedIds=[];selectionAnchor=-1;batchStatus=""}
    function chooseSong(id,index,modifiers){
        id=String(id)
        if(!selectionMode&&(modifiers&(Qt.ControlModifier|Qt.ShiftModifier)))selectionMode=true
        if(!selectionMode){selectedSongKey=provider+":"+id;return}
        let ids=selectedIds.slice()
        if((modifiers&Qt.ShiftModifier)&&selectionAnchor>=0){
            const rows=visibleSongs,from=Math.min(selectionAnchor,index),to=Math.max(selectionAnchor,index)
            for(let i=from;i<=to;++i){const key=String(rows[i].id);if(ids.indexOf(key)<0)ids.push(key)}
        }else{const at=ids.indexOf(id);if(at>=0)ids.splice(at,1);else ids.push(id);selectionAnchor=index}
        selectedIds=ids;batchStatus=""
    }
    function selectAll(invert){
        const ids=[];for(const song of visibleSongs){const id=String(song.id);if(!invert||selectedIds.indexOf(id)<0)ids.push(id)}
        selectedIds=ids;batchStatus=""
    }
    function songTrackId(song){return song.internalId||String((qqSelected?2000000000000:1000000000000)+Number(song.id))}
    function selectedSongs(){return musicSource.songs.filter(song=>selectedIds.indexOf(String(song.id))>=0)}
    function actOnSelection(action){
        actOnSongs(selectedSongs(),action)
    }
    function songOrigin(){
        const current=musicSource.currentPlaylist
        return {id:String(current.id||activeView.argument||""),kind:activeView.kind,name:current.name||""}
    }
    function choosePlaylistFor(rows){
        if(!rows.length||!rows[0].id)return
        addPlaylistDialog.provider=page.provider
        addPlaylistDialog.entries=rows.slice()
        addPlaylistDialog.origin=page.songOrigin()
        newDiscoveryPlaylistName.text=""
        addPlaylistDialog.open()
    }
    function actOnSongs(rows,action){
        if(!rows.length)return
        if(action==="copy"){
            appBridge.copyText(rows.map(song=>song.title+" — "+song.artist).join("\n"));batchStatus="已复制 "+rows.length+" 首歌曲信息";return
        }
        if(action==="download"&&videoSource){batchStatus="B站视频暂不支持下载，可加入播放队列";return}
        if(action==="download"){
            let added=0,skipped=0
            for(const song of rows){const task=onlineDownloads.task(provider,song);if(onlineDownloads.isDownloaded(provider,song)||["queued","resolving","downloading","converting"].indexOf(task.state)>=0){++skipped;continue}onlineDownloads.enqueue(provider,song,musicSource.quality);++added}
            batchStatus="已加入下载队列 "+added+" 首"+(skipped?"，跳过已有或排队歌曲 "+skipped+" 首":"");return
        }
        const count=action==="next"||action==="append"?rows.filter(song=>songTrackId(song)!==playbackBridge.trackId).length:rows.length
        const ok=musicSource.batchSongs(rows.map(song=>String(song.id)),action)
        batchStatus=ok?({play:"播放所选",shuffle:"随机播放所选",next:"已插入下一首",append:"已追加到播放队列"}[action])+" · "+count+" 首"+(count<rows.length?"（已跳过当前歌曲）":""):"没有可添加的歌曲，当前播放歌曲不会重复加入"
    }
    Keys.onPressed:event=>{
        if(!selectionMode||showPlaylists)return
        if(event.key===Qt.Key_Escape){selectionMode=false;clearSelection();event.accepted=true}
        else if(event.key===Qt.Key_A&&(event.modifiers&Qt.ControlModifier)){selectAll(false);event.accepted=true}
    }
    property string collectionQuery:""
    readonly property var visibleSongs:{
        const query=collectionQuery.trim().toLocaleLowerCase()
        return query.length?musicSource.songs.filter(song=>[song.title,song.artist,song.album].some(value=>String(value||"").toLocaleLowerCase().indexOf(query)>=0)):musicSource.songs
    }
    function filterCollection(value){collectionQuery=value;clearSelection();songs.positionViewAtBeginning();loadAtEnd(songs)}
    property string pinnedKey:""
    property bool initialized:false
    property var sourceViews:[{kind:"featured",argument:""},{kind:"featured",argument:""},{kind:"featured",argument:""},{kind:"featured",argument:""},{kind:"mine",argument:""}]
    property var sourceHistory:[[],[],[],[],[]]
    readonly property var activeView:sourceViews[sourceIndex]
    readonly property var qualityKeys:videoSource?["16","32","64","80","112","116","120","125","126","127"]:sourceIndex===2?["standard"]:["standard","exhigh","lossless","hires"]
    readonly property var qualityLabels:videoSource?["360P","480P","720P","1080P","1080P 高码率","1080P 60帧","4K","HDR","杜比视界","8K"]:["标准","极高","无损","Hi-Res"]
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
        closeAccountMenu();searchCollection.close();downloadList.close();batchMenu.dismiss();songMenu.dismiss();addPlaylistDialog.close()
        if(searchMode){const views=sourceViews.slice();views[sourceIndex]={kind:videoSource?"mine":"featured",argument:""};sourceViews=views;searchMode=false;const history=sourceHistory.slice();history[sourceIndex]=[];sourceHistory=history}
    }else if(initialized&&!deferInitialCatalog)showPage(activeView.kind,activeView.argument)
    function showPage(kind,argument,refresh){navigateView(kind,argument,refresh,false)}
    function navigateView(kind,argument,refresh,goingBack){
        pagingView=null;collectionQuery="";searchCollection.close()
        if(!goingBack&&(kind!==activeView.kind||(argument||"")!==activeView.argument)){
            const history=sourceHistory.slice(),entries=history[sourceIndex].slice()
            entries.push({kind:activeView.kind,argument:activeView.argument,searchMode:searchMode,category:searchCategory})
            history[sourceIndex]=entries;sourceHistory=history
        }
        if(searchKinds.indexOf(kind)>=0){searchMode=true;searchCategory=searchKinds.indexOf(kind)}
        else if(["featured","mine","daily","ai"].indexOf(kind)>=0)searchMode=false
        clearSelection()
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
        submittedQuery=value.trim();searchQuery=value;showPage(searchKinds[Math.min(searchCategory,searchKinds.length-1)],submittedQuery)
    }
    function editSearch(value){searchQuery=value;if(!value.trim().length&&searchMode){submittedQuery="";showHome()}}
    function chooseSearchCategory(index){searchCategory=index;showPage(searchKinds[index],submittedQuery)}
    property var pagingView:null
    property real pagingY:0
    property int pagingSongCount:0
    function finishPaging(){
        if(!pagingView||bilibili.catalogLoading)return
        const view=pagingView,y=pagingY,loaded=musicSource.songs.length>pagingSongCount
        Qt.callLater(function(){
            if(page.pagingView!==view)return
            view.forceLayout()
            view.contentY=Math.min(y,Math.max(view.originY,view.contentHeight-view.height+view.originY))
            page.pagingView=null
            if(loaded&&page.collectionQuery.trim().length)page.loadAtEnd(view)
        })
    }
    Connections {target:bilibili;function onChanged(){page.finishPaging()}}
    function loadAtEnd(view){
        Qt.callLater(function(){
            if(page.pagingView||!page.visible||!page.videoSource||!bilibili.hasMore||bilibili.catalogLoading||!view.visible||!view.atYEnd||(!page.collectionQuery.trim().length&&view.contentHeight<=view.height))return
            page.pagingView=view;page.pagingY=view.contentY;page.pagingSongCount=page.musicSource.songs.length
            bilibili.loadMore()
        })
    }
    function chooseSource(index){
        pagingView=null
        closeAccountMenu();search.close();onlineMusic.cancelRecommendation();qqMusic.cancelRecommendation();sodaMusic.cancelRecommendation();kugouMusic.cancelRecommendation()
        const next=index===4?{kind:"mine",argument:""}:sourceViews[index]
        selectionMode=false;clearSelection();sourceIndex=index;searchCategory=0;searchMode=false
        showPage(next.kind,next.argument)
    }
    function syncPinnedKey(){
        let key=""
        if(!showPlaylists&&musicSource.currentPlaylist.id)for(let i=0;i<playlistModel.count;++i){const row=playlistModel.get(i);if(row.provider===page.provider&&row.remoteId===String(musicSource.currentPlaylist.id)){key=row.playlistKey;break}}
        pinnedKey=key
    }
    onShowPlaylistsChanged:syncPinnedKey()
    function showHome(){pinnedKey="";showPage(videoSource?"mine":"featured","")}
    Component.onCompleted:{initialized=true;if(!deferInitialCatalog)showHome()}
    ColumnLayout {
        anchors.fill:parent;anchors.margins:Theme.pageMargin;anchors.topMargin:28;anchors.bottomMargin:0;spacing:12
        Item {
            id:header;Layout.fillWidth:true;Layout.preferredHeight:compact?Theme.controlHeight*2+12:Theme.controlHeight;Layout.bottomMargin:Theme.pageHeadingGap-12
            readonly property real toolButtonsWidth:3*36+3*8
            readonly property bool compact:width<heading.implicitWidth+12+toolButtonsWidth+52
            UiText {id:heading;objectName:"discoverHeading";text:page.videoSource?"发现视频":"发现音乐";height:Theme.controlHeight;verticalAlignment:Text.AlignVCenter;basePixelSize:28;font.weight:Font.DemiBold;color:Theme.text}
            RowLayout {
                id:accountTools;objectName:"onlineAccountTools";anchors.right:parent.right;y:header.compact?Theme.controlHeight+12:0
                width:Math.min(header.toolButtonsWidth+Math.max(52,Math.min(220,account.implicitWidth)),parent.width-(header.compact?0:heading.implicitWidth+12));height:Theme.controlHeight;spacing:8
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
                    backdrop:page.backdropSource;searchName:"online";searchLabel:page.videoSource?"搜索 B站视频或用户":"搜索在线音乐";placeholderText:page.videoSource?"搜索视频或用户":"搜索歌曲、歌手";query:page.searchQuery
                    onQueryEdited:value=>page.editSearch(value)
                    onAccepted:value=>page.submitSearch(value)
                }
                Button {
                    id:account;objectName:"onlineAccountButton"
                    Layout.preferredWidth:implicitWidth;Layout.minimumWidth:52;Layout.maximumWidth:Math.max(52,Math.min(220,accountTools.width-header.toolButtonsWidth))
                    Layout.preferredHeight:Theme.controlHeight;implicitWidth:Math.ceil(accountName.implicitWidth)+52+(accountBadge.visible?accountBadge.width+6:0)
                    padding:6;leftPadding:8;rightPadding:8;hoverEnabled:true
                    Accessible.name:page.musicSource.loggedIn?page.accountDisplayName+"，账号菜单":"扫码登录"
                    contentItem:Item {
                        MembershipBadge {id:accountBadge;objectName:"onlineAccountMembership";x:0;anchors.verticalCenter:parent.verticalCenter;visible:page.showAccountVip;width:implicitWidth;height:implicitHeight;membership:page.musicSource.membership}
                        Avatar {
                            id:accountAvatar;objectName:"onlineAccountAvatar";x:accountBadge.visible?accountBadge.width+6:0;anchors.verticalCenter:parent.verticalCenter;width:28;height:28
                            visible:page.musicSource.loggedIn;file:page.musicSource.avatarUrl;seed:page.musicSource.nickname
                        }
                        Symbol {visible:!page.musicSource.loggedIn;anchors.verticalCenter:parent.verticalCenter;width:28;height:28;name:"person";color:Theme.secondary}
                        UiText {id:accountName;objectName:"onlineAccountName";x:accountAvatar.x+36;width:Math.max(0,parent.width-x);anchors.verticalCenter:parent.verticalCenter;text:page.musicSource.loggedIn?page.accountDisplayName:"扫码登录";color:Theme.text;basePixelSize:14;font.weight:Font.Medium;textFormat:Text.PlainText;elide:Text.ElideRight}
                    }
                    background:Rectangle {radius:8;color:account.down?Theme.pressed:account.hovered||accountMenu.visible?Theme.hover:"transparent";border.width:account.activeFocus?1:0;border.color:Theme.accent}
                    onClicked:if(page.musicSource.loggedIn)page.openAccountMenu();else page.startLogin()
                    TapHandler {acceptedButtons:Qt.RightButton;onTapped:page.openAccountMenu()}
                }
            }
        }
        Flow {
            objectName:"onlineDiscoveryTabs";Layout.fillWidth:true;spacing:8;visible:!page.searchMode&&!page.selectionMode
            ShellButton {objectName:"onlineFeatured";text:page.videoSource?"视频搜索":"推荐歌单";primary:page.videoSource&&page.activeView.kind==="featured";enabled:!page.musicSource.catalogLoading;Accessible.role:page.videoSource?Accessible.RadioButton:Accessible.Button;Accessible.checked:primary;onClicked:if(page.videoSource)page.showPage("featured","");else page.showHome()}
            ShellButton {objectName:"onlineDaily";visible:!page.videoSource;text:"每日推荐";enabled:page.musicSource.loggedIn&&!page.musicSource.catalogLoading;onClicked:page.showPage("daily","")}
            ShellButton {objectName:"onlinePlaylists";text:page.videoSource?"我的收藏夹":"我的歌单";primary:page.videoSource&&page.activeView.kind==="mine";enabled:(page.videoSource||page.musicSource.loggedIn)&&!page.musicSource.catalogLoading;Accessible.role:page.videoSource?Accessible.RadioButton:Accessible.Button;Accessible.checked:primary;onClicked:page.showPage("mine","")}
            ShellButton {objectName:"onlineAiRecommend";visible:!page.videoSource;text:"AI推荐";primary:true;enabled:page.sourceIndex<2&&!page.musicSource.catalogLoading;onClicked:page.showPage("ai","")}
            ShellButton {text:"取消推荐";visible:page.musicSource.catalogLoading&&page.musicSource.status.startsWith("AI");onClicked:page.musicSource.cancelRecommendation()}
        }
        Flow {
            objectName:"onlineSearchTabs";Layout.fillWidth:true;spacing:8;visible:page.searchMode&&!page.selectionMode
            Repeater {
                model:page.videoSource?["视频","用户"]:["单曲","歌手","专辑","歌单"]
                delegate:ShellButton {required property int index;required property string modelData;objectName:"onlineSearchCategory-"+index;text:modelData;enabled:page.videoSource||page.sourceIndex<2||index===0;primary:page.searchCategory===index;onClicked:page.chooseSearchCategory(index)}
            }
        }
        Flow {
            objectName:"onlineSelectionTools";Layout.fillWidth:true;spacing:6;visible:page.selectionMode&&!page.showPlaylists
            ShellButton {objectName:"onlineFinishSelection";text:"完成";flat:true;onClicked:{page.selectionMode=false;page.clearSelection()}}
            UiText {visible:page.selectionMode;text:"已选 "+page.selectedIds.length+" 首";height:Theme.controlHeight;verticalAlignment:Text.AlignVCenter;color:Theme.secondary}
            ShellButton {objectName:"onlineSelectAll";visible:page.selectionMode;text:"全选";flat:true;onClicked:page.selectAll(false)}
            ShellButton {objectName:"onlineInvertSelection";visible:page.selectionMode&&!Theme.compact;text:"反选";flat:true;onClicked:page.selectAll(true)}
            ShellButton {visible:page.selectionMode&&!Theme.compact;text:"清空";flat:true;onClicked:page.clearSelection()}
            ShellButton {id:batchMenuButton;objectName:"onlineBatchMenuButton";visible:page.selectionMode&&Theme.compact;text:"批量操作";onClicked:batchMenu.popup(batchMenuButton,0,height)}
        }
        Flow {
            objectName:"onlineBatchActions";Layout.fillWidth:true;spacing:6;visible:page.selectionMode&&!page.showPlaylists&&!Theme.compact
            ShellButton {objectName:"onlineBatchPlay";text:"播放所选";enabled:page.selectedIds.length>0;onClicked:page.actOnSelection("play")}
            ShellButton {objectName:"onlineBatchShuffle";text:"随机播放";enabled:page.selectedIds.length>0;onClicked:page.actOnSelection("shuffle")}
            ShellButton {objectName:"onlineBatchNext";text:"下一首";enabled:page.selectedIds.length>0;onClicked:page.actOnSelection("next")}
            ShellButton {objectName:"onlineBatchAppend";text:"加入队列";enabled:page.selectedIds.length>0;onClicked:page.actOnSelection("append")}
            ShellButton {objectName:"onlineBatchDownload";visible:!page.videoSource;text:"下载";enabled:page.selectedIds.length>0;onClicked:page.actOnSelection("download")}
            ShellButton {objectName:"onlineBatchCopy";text:"复制信息";enabled:page.selectedIds.length>0;onClicked:page.actOnSelection("copy")}
        }
        RowLayout {
            objectName:"onlinePlaylistHeading";Layout.fillWidth:true;visible:!page.showPlaylists&&!!page.musicSource.currentPlaylist.id;spacing:10
            Artwork {Layout.preferredWidth:page.videoSource&&page.musicSource.currentPlaylist.kind!=="user"?56*4/3:56;Layout.preferredHeight:56;videoCover:page.videoSource&&page.musicSource.currentPlaylist.kind!=="user";animated:false;contentKey:page.provider+":"+(page.musicSource.currentPlaylist.kind||"playlist")+":"+String(page.musicSource.currentPlaylist.id||"");resolution:128;file:page.musicSource.currentPlaylist.cover||"";seed:page.musicSource.currentPlaylist.name||"歌单";imageUrl:String(file).length?file:"image://relaxer/artwork?seed="+encodeURIComponent(seed)+"&resolution=128&radiusRatio=0"}
            ColumnLayout {
                Layout.fillWidth:true
                UiText {Layout.fillWidth:true;text:page.musicSource.currentPlaylist.name||"歌单";basePixelSize:18;font.weight:Font.DemiBold;elide:Text.ElideRight;textFormat:Text.PlainText}
                UiText {text:page.platformName+" · "+Number(page.musicSource.currentPlaylist.count||0)+" 首";basePixelSize:12;color:Theme.secondary}
            }
            RowLayout {
                objectName:"onlinePlaylistActions";Layout.alignment:Qt.AlignRight|Qt.AlignVCenter;spacing:4
                GlassSearch {
                    id:searchCollection;objectName:"onlineCollectionSearchControl";Layout.preferredWidth:32;Layout.preferredHeight:32
                    iconPadding:3
                    backdrop:page.backdropSource;searchName:"onlineCollection";searchLabel:page.videoSource?"搜索当前列表视频":"搜索当前列表歌曲";placeholderText:page.videoSource?"搜索本列表的视频、作者":"搜索本列表的歌曲、歌手、专辑";query:page.collectionQuery
                    onQueryEdited:value=>page.filterCollection(value)
                    onAccepted:value=>page.filterCollection(value)
                }
                IconButton {objectName:"pinOnlinePlaylist";Layout.preferredWidth:32;Layout.preferredHeight:32;padding:3;symbol:"pin";tip:page.pinnedKey.length?"移出侧栏":"加入侧栏";showTooltip:true;tint:page.pinnedKey.length?Theme.accent:Theme.text;visible:["artist","album","user"].indexOf(page.musicSource.currentPlaylist.kind)<0;onClicked:{if(page.pinnedKey.length){libraryBridge.deletePlaylist(page.pinnedKey);page.pinnedKey=""}else{const key=libraryBridge.pinCloudPlaylist(page.musicSource.currentPlaylist);if(key!=="0")page.pinnedKey=key}}}
                IconButton {objectName:"onlinePlaylistBack";Layout.preferredWidth:32;Layout.preferredHeight:32;padding:5;symbol:"log-out";tip:"返回";showTooltip:true;onClicked:page.backPage()}
            }
        }
        RowLayout {
            objectName:"onlineCatalogStatus";Layout.fillWidth:true;Layout.minimumHeight:20;spacing:8
            visible:page.batchStatus.length>0||page.pageStatus.length>0||page.showCatalogProgress
            BusyIndicator {objectName:"onlineCatalogSpinner";Layout.preferredWidth:20;Layout.preferredHeight:20;running:page.showCatalogProgress&&page.presentationActive;visible:page.showCatalogProgress}
            UiText {text:page.batchStatus||page.pageStatus;Layout.fillWidth:true;basePixelSize:12;color:Theme.secondary;wrapMode:Text.Wrap}
        }
        ShellButton {objectName:"bilibiliVerification";visible:page.videoSource&&bilibili.needsVerification;text:"在 B站官网查看";onClicked:Qt.openUrlExternally("https://search.bilibili.com/all?keyword="+encodeURIComponent(page.searchQuery))}
        GridView {
            id:playlistGrid;objectName:"onlinePlaylistCards";
            onContentYChanged:page.loadAtEnd(playlistGrid)
            onAtYEndChanged:page.loadAtEnd(playlistGrid)
            onMovementEnded:page.loadAtEnd(playlistGrid)
            Layout.fillWidth:true;Layout.fillHeight:true;visible:page.showPlaylists
            clip:true;model:page.searchCards?page.musicSource.searchResults:page.musicSource.playlists;reuseItems:true;cellWidth:(width-10)/Math.max(2,Math.floor((width-10)/180));cellHeight:cellWidth*(page.videoSource&&!page.searchCards?.75:1)+46;boundsBehavior:Flickable.StopAtBounds
            SmoothWheelHandler {flickable:playlistGrid}
            ScrollBar.vertical:ListScrollBar{}
            delegate:Item {
                id:card;required property var modelData;width:playlistGrid.cellWidth;height:playlistGrid.cellHeight
                objectName:"onlinePlaylist-"+modelData.id
                HoverHandler {id:cardHover}
                HighlightSurface {objectName:"onlinePlaylistBackground";width:parent.width-4;height:parent.height-8;radius:12;positionRevision:playlistGrid.contentY+card.x+card.y;hovered:cardHover.hovered}
                Artwork {id:cardCover;x:5;y:5;width:parent.width-14;height:width*(page.videoSource&&!page.searchCards?.75:1);videoCover:page.videoSource&&!page.searchCards;animated:false;cacheOnlineCover:card.modelData.kind!=="user";contentKey:page.provider+":"+(card.modelData.kind||"playlist")+":"+String(card.modelData.id);resolution:320;seed:card.modelData.name;file:card.modelData.cover||"";imageUrl:String(file).length?file:"image://relaxer/artwork?seed="+encodeURIComponent(seed)+"&resolution=256&radiusRatio=0"}
                UiText {x:5;y:cardCover.y+cardCover.height+5;width:parent.width-14;text:card.modelData.name;textFormat:Text.PlainText;basePixelSize:14;font.weight:Font.DemiBold;maximumLineCount:2;wrapMode:Text.Wrap;elide:Text.ElideRight}
                MouseArea {
                    anchors.fill:parent;cursorShape:Qt.PointingHandCursor
                    onClicked:{page.pinnedKey="";page.showPage(card.modelData.kind||"playlist",String(card.modelData.id))}
                }
            }
            EmptyState {
                objectName:"onlinePlaylistsEmptyState"
                anchors.centerIn:parent;width:Math.min(parent.width-24,400);visible:!playlistGrid.count&&!page.musicSource.busy
                heading:page.searchCards?"暂无搜索结果":page.videoSource?(page.activeView.kind==="mine"?"我的收藏夹":"搜索 B站视频或用户"):"暂无歌单"
                description:page.searchCards?"试试其他关键词或搜索分类。":page.videoSource?(page.activeView.kind==="mine"?(page.musicSource.loggedIn?"暂无收藏夹，可在 B站创建后刷新。":"登录 B站后可读取自己的收藏夹。"):"点击右上角搜索，查找视频或用户。"):"可切换平台，或登录查看你的云端歌单。"
            }
        }
        ListView {
            id:songs;objectName:"onlineSongs";
            onCountChanged:if(page.collectionQuery.trim().length)page.loadAtEnd(songs)
            onContentYChanged:page.loadAtEnd(songs)
            onAtYEndChanged:page.loadAtEnd(songs)
            onMovementEnded:page.loadAtEnd(songs)
            Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:60
            visible:!page.showPlaylists
            model:page.visibleSongs;clip:true;spacing:4;reuseItems:true;boundsBehavior:Flickable.StopAtBounds
            ScrollBar.vertical:ListScrollBar{}
            SmoothWheelHandler {flickable:songs}
            delegate:HighlightSurface {
                id:row;required property var modelData;required property int index
                readonly property bool current:playbackBridge.trackId===page.songTrackId(modelData)
                readonly property string songKey:page.provider+":"+String(modelData.id)
                readonly property bool favorite:{
                    libraryBridge.onlineFavoritesRevision
                    return libraryBridge.discoveryFavorite(page.provider,row.modelData)
                }
                selected:page.selectionMode?page.selectedIds.indexOf(String(modelData.id))>=0:page.selectedSongKey===songKey
                objectName:"onlineSong-"+modelData.id;width:songs.width-10;height:Math.ceil((modelData.reason.length?88:64)*Math.max(1,Theme.fontScale));radius:8
                positionRevision:songs.contentY+row.y;hovered:rowHover.hovered
                HoverHandler {id:rowHover}
                MouseArea {anchors.fill:parent;acceptedButtons:Qt.LeftButton|Qt.RightButton;onClicked:mouse=>{if(mouse.button===Qt.RightButton)page.openSongMenu(row.modelData,row.index,row,mouse.x,mouse.y);else{page.forceActiveFocus();page.chooseSong(row.modelData.id,row.index,mouse.modifiers)}};onDoubleClicked:mouse=>{if(mouse.button===Qt.LeftButton&&!page.selectionMode)page.musicSource.playSong(row.modelData.id)}}
                RowLayout {
                    anchors.fill:parent;anchors.margins:8;spacing:10
                    MorphCheckBox {
                        objectName:"onlineSelected-"+row.modelData.id;visible:page.selectionMode
                        Layout.preferredWidth:30;Layout.preferredHeight:30;checked:page.selectedIds.indexOf(String(row.modelData.id))>=0
                        Accessible.name:"选择 "+row.modelData.title;onClicked:page.chooseSong(row.modelData.id,row.index,Qt.NoModifier)
                    }
                    IconButton {
                        visible:!page.selectionMode
                        objectName:"onlinePlay-"+row.modelData.id;Layout.preferredWidth:30;Layout.preferredHeight:30;padding:3;horizontalPadding:3;verticalPadding:3
                        symbol:row.current&&playbackBridge.playing?"pause":"play";tip:row.current&&playbackBridge.playing?"暂停":"播放";showTooltip:true
                        onClicked:if(row.current)playbackBridge.toggle();else page.musicSource.playSong(row.modelData.id)
                    }
                    IconButton {
                        objectName:"onlineFavorite-"+row.modelData.id
                        visible:!page.selectionMode&&(settingsBridge.values.favoriteDisplayScopes&4)!==0
                        Layout.preferredWidth:28;Layout.preferredHeight:30;padding:4
                        symbol:row.favorite?"liked":"heart"
                        tint:row.favorite?Theme.accent:Theme.secondary
                        tip:row.favorite?"取消喜欢":"加入我喜欢";showTooltip:true
                        onClicked:libraryBridge.toggleDiscoveryFavorite(page.provider,row.modelData,page.songOrigin())
                    }
                    Artwork {
                        objectName:"onlineCover-"+row.modelData.id;contentKey:row.songKey
                        Layout.preferredWidth:page.videoSource?40*4/3:40;Layout.preferredHeight:40;videoCover:page.videoSource;resolution:128;animated:false
                        file:row.modelData.thumbnail||row.modelData.cover||"";seed:row.modelData.title
                        imageUrl:String(file).length?file:"image://relaxer/artwork?seed="+encodeURIComponent(seed)+"&resolution=128&radiusRatio=0"
                    }
                    ColumnLayout {
                        Layout.fillWidth:true;spacing:3
                        UiText {text:row.modelData.title;Layout.fillWidth:true;basePixelSize:15;font.weight:Font.DemiBold;color:row.current?Theme.accent:Theme.text;elide:Text.ElideRight;textFormat:Text.PlainText}
                        RowLayout {
                            Layout.fillWidth:true;spacing:5
                            Rectangle {objectName:"onlineVip-"+row.modelData.id;visible:!!row.modelData.requiresSubscription&&settingsBridge.values.showSongVip;implicitWidth:27;implicitHeight:15;radius:3;color:"transparent";border.color:"#c69545"
                                UiText {anchors.centerIn:parent;text:"VIP";basePixelSize:10;font.weight:Font.DemiBold;color:"#c69545"}
                                HoverHandler {id:vipHover}
                                HintBubble {visible:vipHover.hovered;text:"平台标记为需会员或单独购买，具体以账号权限为准"}
                            }
                            UiText {text:row.modelData.artist+(row.modelData.album.length?" · "+row.modelData.album:"")+(row.current&&playbackBridge.matchedLocalTrackId!=="0"?" · 已关联本地":"");Layout.fillWidth:true;basePixelSize:12;color:Theme.secondary;elide:Text.ElideRight;textFormat:Text.PlainText}
                        }
                        UiText {text:row.modelData.reason;visible:text.length>0;Layout.fillWidth:true;basePixelSize:12;color:Theme.secondary;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight;textFormat:Text.PlainText}
                    }
                    RowLayout {
                        objectName:"onlineSongActions-"+row.modelData.id;spacing:2;visible:settingsBridge.values.discoveryActions.length>0
                        Repeater {
                            model:page.videoSource?settingsBridge.values.discoveryActions.filter(action=>action!=="download"):settingsBridge.values.discoveryActions
                            Loader {
                                required property string modelData
                                Layout.preferredWidth:36;Layout.preferredHeight:36
                                sourceComponent:modelData==="download"?downloadAction:modelData==="next"?nextAction:moreAction
                                Component {id:downloadAction;OnlineDownloadButton {objectName:"downloadOnline-"+row.modelData.id;provider:page.provider;song:row.modelData;quality:page.musicSource.quality}}
                                Component {id:nextAction;NextSongButton {
                                    objectName:"nextOnline-"+row.modelData.id;songKey:row.songKey;enabled:!row.current
                                    onRequested:if(page.musicSource.batchSongs([String(row.modelData.id)],"next"))confirm()
                                }}
                                Component {id:moreAction;IconButton {objectName:"moreOnline-"+row.modelData.id;symbol:"more";tip:"更多操作";showTooltip:true;onClicked:page.openSongMenu(row.modelData,row.index,this,0,height)}}
                            }
                        }
                    }
                }
            }
            EmptyState {anchors.centerIn:parent;width:Math.min(parent.width-24,400);visible:!songs.count;heading:page.collectionQuery.trim().length?"没有匹配的内容":page.videoSource?"发现喜欢的视频":"发现下一首喜欢的歌";description:page.collectionQuery.trim().length?(page.showCatalogProgress?"正在查找后续内容…":"试试其他关键词，或清空本列表搜索。"):"搜索在线歌曲，或登录"+page.platformName+"读取推荐和歌单。"}
        }
    }
    RoundMenu {
        id:songMenu;objectName:"onlineSongMenu";parent:page.Overlay.overlay;width:190
        RoundMenuItem {objectName:"onlineContextPlay";text:page.contextIsCurrent?(playbackBridge.playing?"暂停":"继续播放"):page.contextRows().length>1?"播放所选":"播放";onTriggered:if(page.contextIsCurrent)playbackBridge.toggle();else page.contextAction("play")}
        RoundMenuItem {text:"随机播放";onTriggered:page.contextAction("shuffle")}
        RoundMenuItem {objectName:"onlineContextNext";text:"下一首播放";onTriggered:page.contextAction("next")}
        RoundMenuItem {text:"追加播放队列";onTriggered:page.contextAction("append")}
        RoundMenuItem {objectName:"onlineContextAddPlaylist";text:"加入自建歌单…";onTriggered:page.choosePlaylistFor(page.contextRows())}
        RoundMenuItem {objectName:"onlineContextDownload";visible:!page.videoSource;text:page.contextRows().length>1?"批量下载":"下载";onTriggered:page.contextAction("download")}
        RoundMenuItem {text:"复制歌曲信息";onTriggered:page.contextAction("copy")}
        RoundMenuItem {objectName:"onlineSongEqualizer";text:"歌曲均衡器…";visible:page.contextRows().length===1;onTriggered:equalizerBridge.requestOnlineSong(page.provider,page.contextSong)}
        MenuSeparator {}
        RoundMenuItem {objectName:"onlineContextMultiSelect";text:"多选";onTriggered:page.beginSelection()}
        RoundMenuItem {visible:page.selectionMode;text:"全选";onTriggered:page.selectAll(false)}
        RoundMenuItem {visible:page.selectionMode;text:"反选";onTriggered:page.selectAll(true)}
        RoundMenuItem {visible:page.selectionMode;text:"结束多选";onTriggered:{page.selectionMode=false;page.clearSelection()}}
    }
    RoundMenu {
        id:batchMenu;objectName:"onlineBatchMenu";parent:page.Overlay.overlay;width:190
        RoundMenuItem {text:"播放所选";enabled:page.selectedIds.length>0;onTriggered:page.actOnSelection("play")}
        RoundMenuItem {text:"随机播放所选";enabled:page.selectedIds.length>0;onTriggered:page.actOnSelection("shuffle")}
        RoundMenuItem {text:"批量下一首";enabled:page.selectedIds.length>0;onTriggered:page.actOnSelection("next")}
        RoundMenuItem {text:"追加播放队列";enabled:page.selectedIds.length>0;onTriggered:page.actOnSelection("append")}
        RoundMenuItem {objectName:"onlineBatchAddPlaylist";text:"加入自建歌单…";enabled:page.selectedIds.length>0;onTriggered:page.choosePlaylistFor(page.selectedSongs())}
        RoundMenuItem {text:"批量下载";visible:!page.videoSource;enabled:page.selectedIds.length>0;onTriggered:page.actOnSelection("download")}
        RoundMenuItem {text:"复制歌曲信息";enabled:page.selectedIds.length>0;onTriggered:page.actOnSelection("copy")}
        MenuSeparator {}
        RoundMenuItem {text:"反选";onTriggered:page.selectAll(true)}
        RoundMenuItem {text:"清空选择";onTriggered:page.clearSelection()}
    }
    ConfirmDialog {
        id:addPlaylistDialog
        objectName:"onlineAddPlaylistDialog"
        parent:page.Overlay.overlay
        anchors.centerIn:parent
        width:Math.min(420,parent?parent.width-28:420)
        modal:true
        title:"加入自建歌单"
        standardButtons:Dialog.Cancel
        property string provider:""
        property var entries:[]
        property var origin:({})
        property string errorText:""
        readonly property var destinations:{
            const rows=[]
            for(let i=0;i<playlistModel.count;i++){
                const row=playlistModel.get(i)
                if(!row.cloud)rows.push(row)
            }
            return rows
        }
        function complete(ok){
            if(ok){page.batchStatus="已加入歌单";close()}
            else errorText=libraryBridge.error
        }
        function createAndAdd(){
            if(!newDiscoveryPlaylistName.text.trim().length)return
            complete(libraryBridge.createPlaylistWithDiscoveryTracks(
                newDiscoveryPlaylistName.text,provider,entries,origin))
        }
        onAboutToShow:errorText=""
        onClosed:entries=[]
        contentItem:ColumnLayout {
            spacing:8
            ListView {
                Layout.fillWidth:true
                Layout.preferredHeight:Math.min(count*Theme.controlHeight,Theme.controlHeight*5)
                clip:true
                model:addPlaylistDialog.destinations
                visible:count>0
                ScrollBar.vertical:ScrollBar{}
                delegate:ShellButton {
                    required property var modelData
                    objectName:"onlineAddPlaylist-"+modelData.playlistKey
                    width:ListView.view.width
                    height:Theme.controlHeight
                    text:modelData.name
                    flat:true
                    onClicked:addPlaylistDialog.complete(libraryBridge.addDiscoveryTracks(
                        modelData.playlistKey,addPlaylistDialog.provider,addPlaylistDialog.entries,addPlaylistDialog.origin))
                }
            }
            RowLayout {
                Layout.fillWidth:true
                EntryField {
                    id:newDiscoveryPlaylistName
                    objectName:"onlineNewPlaylistName"
                    Layout.fillWidth:true
                    Layout.minimumWidth:0
                    placeholderText:"新歌单名称"
                    maximumLength:120
                    onAccepted:addPlaylistDialog.createAndAdd()
                }
                ShellButton {
                    objectName:"onlineCreatePlaylistAndAdd"
                    text:"新建并加入"
                    enabled:newDiscoveryPlaylistName.text.trim().length>0
                    onClicked:addPlaylistDialog.createAndAdd()
                }
            }
            UiText {
                Layout.fillWidth:true
                visible:text.length>0
                text:addPlaylistDialog.errorText
                textFormat:Text.PlainText
                color:Theme.accent
                wrapMode:Text.Wrap
            }
        }
    }
    DownloadList {id:downloadList;parent:downloadsButton}
    TextMetrics {id:accountMenuText;font:accountMenu.font;text:page.videoSource?"视频画质上限":"音乐音质"}
    TextMetrics {id:qualityMenuText;font:qualityMenu.font;text:page.videoSource?"1080P 高码率":"Hi-Res"}
    TextMetrics {id:sourceMenuText;font:sourceMenu.font;text:"网易云音乐"}
    RoundMenu {
        id:accountMenu;objectName:"onlineAccountMenu";parent:page.Overlay.overlay
        width:Math.ceil(accountMenuText.width)+48;padding:4;cascade:true
        delegate:RoundMenuItem {
            id:accountEntry;objectName:subMenu===qualityMenu?"onlineQualityEntry":"onlineSourceEntry";implicitHeight:32;leftPadding:28;rightPadding:10
            LayoutMirroring.enabled:false
            arrow:Symbol {objectName:accountEntry.subMenu===qualityMenu?"onlineQualityArrow":"onlineSourceArrow";visible:!!accountEntry.subMenu;name:"back";width:14;height:14;x:8;y:(accountEntry.height-height)/2;color:Theme.secondary}
        }
        enter:Transition {NumberAnimation {target:accountMenu;property:"expansion";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:140;easing.type:Easing.OutCubic}}
        exit:Transition {NumberAnimation {target:accountMenu;property:"expansion";to:0;duration:settingsBridge.values.reduceMotion?0:90;easing.type:Easing.InCubic}}
        RoundMenu {
            id:sourceMenu;objectName:"onlineSourceMenu";width:Math.ceil(sourceMenuText.width)+46;padding:4;title:"平台来源";cascade:true
            enter:Transition {NumberAnimation {target:sourceMenu;property:"expansion";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:140;easing.type:Easing.OutCubic}}
            exit:Transition {NumberAnimation {target:sourceMenu;property:"expansion";to:0;duration:settingsBridge.values.reduceMotion?0:90;easing.type:Easing.InCubic}}
            RoundMenuItem {objectName:"onlineSource-netease";text:"网易云音乐";checkable:true;checked:page.sourceIndex===0;implicitHeight:32;leftPadding:28;rightPadding:10;onTriggered:page.chooseSource(0)}
            RoundMenuItem {objectName:"onlineSource-qishui";text:"汽水音乐";checkable:true;checked:page.sourceIndex===2;implicitHeight:32;leftPadding:28;rightPadding:10;onTriggered:page.chooseSource(2)}
            RoundMenuItem {objectName:"onlineSource-bilibili";text:"哔哩哔哩";checkable:true;checked:page.sourceIndex===4;implicitHeight:32;leftPadding:28;rightPadding:10;onTriggered:page.chooseSource(4)}
            RoundMenuItem {objectName:"onlineSource-kugou";text:"酷狗音乐";checkable:true;checked:page.sourceIndex===3;implicitHeight:32;leftPadding:28;rightPadding:10;onTriggered:page.chooseSource(3)}
            RoundMenuItem {objectName:"onlineSource-qq";text:"QQ 音乐";checkable:true;checked:page.qqSelected;implicitHeight:32;leftPadding:28;rightPadding:10;onTriggered:page.chooseSource(1)}
        }
        RoundMenu {
            id:qualityMenu;objectName:"onlineQualityMenu";width:Math.ceil(qualityMenuText.width)+46;padding:4;title:page.videoSource?"视频画质上限":"音乐音质";cascade:true
            enter:Transition {NumberAnimation {target:qualityMenu;property:"expansion";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:140;easing.type:Easing.OutCubic}}
            exit:Transition {NumberAnimation {target:qualityMenu;property:"expansion";to:0;duration:settingsBridge.values.reduceMotion?0:90;easing.type:Easing.InCubic}}
            Instantiator {
                model:page.qualityKeys
                delegate:RoundMenuItem {
                    required property string modelData;required property int index
                    objectName:"onlineQuality-"+modelData;enabled:!page.videoSource||settingsBridge.values.onlineVideoMode===0;text:page.qualityLabels[index];checkable:true;checked:page.musicSource.quality===modelData;implicitHeight:32;leftPadding:28;rightPadding:10
                    onTriggered:page.musicSource.quality=modelData
                }
                onObjectAdded:(index,object)=>qualityMenu.insertItem(index,object)
                onObjectRemoved:(index,object)=>qualityMenu.removeItem(object)
            }
            MenuSeparator {visible:page.videoSource}
            RoundMenuItem {visible:page.videoSource;text:"视频音质";enabled:false}
            Instantiator {
                model:page.videoSource?["standard","high","hires"]:[]
                delegate:RoundMenuItem {
                    required property string modelData;required property int index
                    objectName:"bilibiliAudio-"+modelData;text:["标准","高音质","Hi-Res"][index]
                    checkable:true;checked:bilibili.audioQuality===modelData;onTriggered:bilibili.audioQuality=modelData
                }
                onObjectAdded:(index,object)=>qualityMenu.insertItem(qualityMenu.count,object)
                onObjectRemoved:(index,object)=>qualityMenu.removeItem(object)
            }
        }
        MenuSeparator {padding:0;topPadding:3;bottomPadding:3;contentItem:Rectangle {implicitHeight:1;color:Theme.border}}
        RoundMenuItem {objectName:"onlinePlatformEqualizer";text:"此平台均衡器…";implicitHeight:32;leftPadding:10;rightPadding:10;onTriggered:equalizerBridge.requestPlatform(page.provider)}
        RoundMenuItem {objectName:"onlineLogout";implicitHeight:32;leftPadding:10;rightPadding:10;text:page.musicSource.loggedIn?"退出账号":"扫码登录";destructive:page.musicSource.loggedIn;onTriggered:if(page.musicSource.loggedIn)page.musicSource.logout();else page.startLogin()}
    }
    ConfirmDialog {
        id:login;objectName:"neteaseLoginDialog";parent:page.Overlay.overlay;width:Math.min(360,parent.width-32);anchors.centerIn:parent
        title:(page.loginSource===onlineMusic?"网易云音乐":page.loginSource===qqMusic?"QQ 音乐":page.loginSource===sodaMusic?"汽水音乐":page.loginSource===bilibili?"哔哩哔哩":"酷狗音乐")+"扫码登录";standardButtons:Dialog.Cancel
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
    Connections {target:page.loginSource;function onLoginCompleted(){if(login.visible)login.close()}}
    Connections {target:page.musicSource;function onSongsChanged(){const valid=page.musicSource.songs.map(song=>String(song.id));page.selectedIds=page.selectedIds.filter(id=>valid.indexOf(id)>=0);if(page.musicSource.songs.length>0)page.showPlaylists=false}function onChanged(){page.syncPinnedKey()}}
    Connections {target:playlistModel;function onCountChanged(){page.syncPinnedKey()}}
    Connections {target:page.Window.window;function onActiveChanged(){if(!page.Window.window.active)page.closeAccountMenu()}}
}
