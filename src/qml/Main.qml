import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "components"
import "pages"
import "theme"
ApplicationWindow {
    id: root
    objectName: "qmlMainWindow"
    visible: true
    width: 1240
    height: 790
    minimumWidth: 1000; minimumHeight: 660
    flags: Qt.Window | Qt.FramelessWindowHint
    title: appBridge.windowTitle + " · QML"
    color: Theme.background
    readonly property string restFontFamily: { const saved = settingsBridge.values.uiFont; return settingsBridge.fontFamilyFor("rest") || "Microsoft YaHei UI" }
    readonly property string footerFontFamily: { const saved = settingsBridge.values.uiFont; return settingsBridge.fontFamilyFor("footer") || root.restFontFamily }
    font.family: restFontFamily
    font.pixelSize: Theme.fontSize
    font.weight: Font.Medium
    palette.window: Theme.background; palette.base: Theme.background
    palette.windowText: Theme.text; palette.text: Theme.text; palette.buttonText: Theme.text
    palette.placeholderText: Theme.secondary; palette.button: Theme.card
    palette.highlight: Theme.accent; palette.highlightedText: "white"
    palette.disabled.text: Theme.disabled; palette.disabled.windowText: Theme.disabled
    property int currentPage: appBridge.page
    readonly property QtObject designTokens: Theme
    property color themeAccent: Theme.accent
    property bool lyricsOpen: false
    readonly property bool fullScreenMode: visibility === Window.FullScreen
    property var savedGeometry: [0,0,1240,790]
    property bool wasMaximized: false
    property int fullscreenRevision: 0
    property bool allowClose: false
    property bool exitRequested: false
    property var navigationAction: null
    property int snapshotRevision: 0
    property bool awaitingImportClose: false
    readonly property Item popupLayer: contentItem.Overlay.overlay
    property real playerProgress: 0
    readonly property real playerReveal: playerProgress
    property bool upcomingAtLeft: false
    property real upcomingTravel: 0
    property var coverFrom: ({x:0,y:0,width:62,height:62})
    property var coverTo: ({x:0,y:0,width:62,height:62})
    property bool coverTravelling: false
    property real coverProgress: 0
    property url flightFile: ""
    property string flightSeed: ""
    property int navigationRevision: 0
    property var pageGrab: null
    property string settingsSection: "settings"
    property var pendingPageAction: null
    property bool pendingAppearanceClose: false
    property bool bulkActive: false
    property var bulkKeys: []
    property var bulkOwner: null
    property string bulkScope: "0"
    property var bulkActionKeys: []
    property bool metadataTravelling: false
    property var titleFrom: ({x:0,y:0,width:1,height:1,pixelSize:13,family:"Microsoft YaHei UI",weight:400})
    property var titleTo: titleFrom
    property var artistFrom: titleFrom
    property var artistTo: titleFrom
    readonly property bool popupOpen: {
        const items = popupLayer ? popupLayer.children : []
        for (let index = 0; index < items.length; ++index)
            if (items[index].visible && items[index].opacity > 0 && items[index].width > 0 && items[index].height > 0) return true
        return false
    }
    function imageRect(item) { const p = item.mapToItem(root.contentItem,0,0); return {x:p.x,y:p.y,width:item.width,height:item.height} }
    function textRect(item) {
        const bounds = imageRect(item)
        const width = Math.min(bounds.width, item.contentWidth)
        if (item.horizontalAlignment === Text.AlignHCenter) bounds.x += (bounds.width - width) / 2
        bounds.width = Math.max(1, width)
        bounds.pixelSize = item.font.pixelSize
        bounds.family = item.font.family
        bounds.weight = item.font.weight
        return bounds
    }
    function updateBulk(owner, active, keys) {
        if (!active && bulkOwner !== owner) return
        if (active && bulkOwner && bulkOwner !== owner) bulkOwner.clearBulk()
        bulkOwner = active ? owner : null
        bulkActive = active
        bulkKeys = active ? keys.slice() : []
        bulkScope = active && owner === localPage ? libraryBridge.playlistId : "0"
    }
    function clearBulk() { if (bulkOwner) bulkOwner.clearBulk(); bulkActive = false; bulkKeys = []; bulkOwner = null }
    function selectBulk(all) {
        if (!bulkOwner) return
        bulkOwner.setBulkKeys(all ? libraryBridge.visibleTrackKeys() : [])
    }
    function animatePlayer(open) {
        if (lyricsOpen === open && !playerMotion.running) return
        const start = coverTravelling ? ({x:flight.x,y:flight.y,width:flight.width,height:flight.height}) : imageRect(open ? playerBar.smallArtwork : nowPage.largeArtwork)
        const titleStart = metadataTravelling ? textRect(titleFlight) : textRect(open ? playerBar.metadataTitle : nowPage.metadataTitle)
        const artistStart = metadataTravelling ? textRect(artistFlight) : textRect(open ? playerBar.metadataArtist : nowPage.metadataArtist)
        lyricsOpen = open
        coverFrom = start; coverTo = imageRect(open ? nowPage.largeArtwork : playerBar.smallArtwork)
        titleFrom = titleStart; titleTo = textRect(open ? nowPage.metadataTitle : playerBar.metadataTitle)
        artistFrom = artistStart; artistTo = textRect(open ? nowPage.metadataArtist : playerBar.metadataArtist)
        flightFile = playbackBridge.coverUrl; flightSeed = playbackBridge.hasTrack ? playbackBridge.title : "Relaxer"
        playerMotion.stop(); coverMotion.stop()
        if (settingsBridge.values.reduceMotion) { playerProgress = open ? 1 : 0; coverTravelling = false; metadataTravelling = false; return }
        coverProgress = 0; coverTravelling = true; metadataTravelling = true
        playerMotion.from = playerProgress; playerMotion.to = open ? 1 : 0
        playerMotion.duration = 420; coverMotion.duration=420; playerMotion.restart(); coverMotion.restart()
    }
    function relocateUpcoming(left) {
        upcomingOut.stop(); upcomingIn.stop()
        if (settingsBridge.values.reduceMotion) { upcomingAtLeft=left; upcomingTravel=0; return }
        if (upcomingAtLeft===left) { upcomingIn.from=upcomingTravel; upcomingIn.restart() }
        else { upcomingOut.from=upcomingTravel; upcomingOut.restart() }
    }
    onLyricsOpenChanged: relocateUpcoming(lyricsOpen)
    function showLyrics() { animatePlayer(true) }
    function closeLyrics() { if (fullScreenMode) toggleFullscreen(); animatePlayer(false) }
    function toggleFullscreen() {
        const revision = ++fullscreenRevision
        if (fullScreenMode) {
            if (wasMaximized) showMaximized()
            else { showNormal(); const b=savedGeometry; Qt.callLater(function(){if(revision!==fullscreenRevision||fullScreenMode)return;root.x=b[0];root.y=b[1];root.width=b[2];root.height=b[3]}) }
        } else { savedGeometry=[x,y,width,height]; wasMaximized=visibility===Window.Maximized; showLyrics(); showFullScreen() }
    }
    function changePage(action) {
        if (appBridge.page === 3 && settingsSection === "appearance" && !appearancePage.confirmLeave()) {
            pendingPageAction = action
            return
        }
        pendingPageAction = null
        const revision=++navigationRevision
        snapshotWarmup.stop(); navigationAction=null; oldPage.visible=false
        pageOut.stop(); pageIn.stop(); pageSurface.opacity=1; pageSurface.y=0
        if (lyricsOpen) animatePlayer(false)
        if (settingsBridge.values.reduceMotion || !pageSurface.visible) { root.clearBulk();action(); return }
        // Capture the painted background as well as the controls. Capturing
        // only the transparent pageSurface produces a black backdrop when
        // its snapshot is composited by the software renderer.
        paper.grabToImage(function(grab) {
            if (revision!==navigationRevision) return
            pageGrab=grab; snapshotRevision=revision; navigationAction=action
            oldPage.opacity=.001; oldPage.scale=1; oldPage.source=grab.url
            if(oldPage.status===Image.Ready) warmSnapshot()
        })
    }
    function warmSnapshot() {
        if(snapshotRevision!==navigationRevision || !navigationAction)return
        oldPage.visible=true
        snapshotWarmup.restart()
    }
    function requestQuit() { exitRequested=true; root.close() }
    function navigateOriginal(index) {
        const same = index === 0 ? appBridge.page === 0 && libraryBridge.playlistId === "0"
                   : index === 4 ? appBridge.page === 0 && libraryBridge.playlistId === "-1"
                   : appBridge.page === index && (index !== 3 || settingsSection === "settings")
        if (same) { if (lyricsOpen) animatePlayer(false); return }
        changePage(function() {
            settingsSection="settings"
            if (index===0) { libraryBridge.openPlaylist("0"); appBridge.navigate(0) }
            else if(index===1) appBridge.navigate(1)
            else if(index===2) { libraryBridge.openPlaylist("-2"); appBridge.navigate(2) }
            else if(index===3) appBridge.navigate(3)
            else { libraryBridge.openPlaylist("-1"); appBridge.navigate(0) }
        })
    }
    function openPlaylist(key) {
        if (appBridge.page === 0 && libraryBridge.playlistId === key) {
            if (lyricsOpen) animatePlayer(false)
            return
        }
        changePage(function(){if(libraryBridge.openPlaylist(key))appBridge.navigate(0)})
    }
    function openLyricsTools(keys,missingOnly) {
        if (!keys || !keys.length) return
        if (!lyricsSearchBridge.setTargetTracks(keys,!!missingOnly,true)) { toast.show("没有需要处理的歌曲"); return }
        if (!lyricsSearchBridge.targetTracks.length) { toast.show("没有需要处理的歌曲"); return }
        const target=lyricsSearchBridge.targetTracks[0].trackKey
        lyricsSearchBridge.setTargetTrack(target); translationBridge.setTargetTrack(target)
        lyricsWorkspace.open()
    }
    function backgroundUrl(file,blur){return String(file||"").length?"image://relaxer/background?file="+encodeURIComponent(String(file))+"&blur="+Number(blur||0):""}
    function applySettings() { Theme.fontFamily=root.restFontFamily; if(settingsBridge.values.reduceMotion)relocateUpcoming(lyricsOpen); Theme.dark=settingsBridge.palette.dark; Theme.custom=settingsBridge.palette.custom ? settingsBridge.palette : ({}) }
    Component.onCompleted: { width=settingsBridge.windowBounds.width;height=settingsBridge.windowBounds.height;applySettings(); if(settingsBridge.windowBounds.maximized)showMaximized() }
    Connections { target: settingsBridge; function onSettingsChanged(){root.applySettings()} }
    Connections { target: importBridge; function onReadyToClose(){if(root.awaitingImportClose){root.allowClose=true;Qt.callLater(root.close)}} }
    Connections { target: appBridge; function onPageChanged(){if(root.lyricsOpen)root.animatePlayer(false)} }
    Connections { target: playbackBridge; function onTransitionStarted(direction){nowPage.trackDirection=direction} }
    Connections {
        target: upcomingModel
        function onAboutToActivate(row,key,cover) {
            if(!root.lyricsOpen||settingsBridge.values.reduceMotion)return
            const origin=upcoming.coverItem(row)
            if(!origin)return
            root.coverFrom=root.imageRect(origin);root.coverTo=root.imageRect(nowPage.largeArtwork)
            root.flightFile=cover;root.flightSeed=key;root.coverProgress=0;root.coverTravelling=true
            coverMotion.duration=460;coverMotion.restart()
        }
    }
    Shortcut { sequence: "Escape"; enabled: !lyricsWorkspace.visible && !root.popupOpen; onActivated: {if(root.bulkActive)root.clearBulk();else if(root.fullScreenMode)root.toggleFullscreen();else if(root.lyricsOpen)root.animatePlayer(false);else upcoming.panelExpanded=false} }
    Shortcut {sequence:"Ctrl+O";onActivated:importDialog.chooseFiles()}
    Shortcut {sequence:"Ctrl+Shift+O";onActivated:importDialog.chooseFolder()}
    Shortcut {sequence:"Ctrl+F";enabled:!lyricsWorkspace.visible;onActivated:{root.navigateOriginal(0);searchFocus.restart()}}
    Timer{id:searchFocus;interval:260;onTriggered:localPage.focusSearch()}
    onClosing: close => {
        if(playbackBridge.nonInteractiveExit()){playbackBridge.requestExit(1);return}
        if (settingsBridge.appearanceDirty) {
            close.accepted=false
            pendingAppearanceClose=true
            pendingPageAction=null
            appearancePage.confirmLeave()
            return
        }
        if (!allowClose && (importBridge.busy || translationBridge.unfinished || lyricsSearchBridge.busy)) { close.accepted=false; closeConfirm.open() }
        else if(!allowClose && !exitRequested && settingsBridge.values.closeToTray && uiIntegration.trayAvailable){close.accepted=false;root.hide();uiIntegration.showTray()}
        else if(!playbackBridge.requestExit()){close.accepted=false;exitSaveFailure.open()}
        else if(importBridge.busy){close.accepted=false;root.awaitingImportClose=true;if(!importBridge.closing)importBridge.beginShutdown()}
    }
    ConfirmDialog {
        id:exitSaveFailure;parent:root.popupLayer;objectName:"exitSaveFailureDialog";anchors.centerIn:parent
        width:Math.min(520,root.width-40);modal:true;closePolicy:Popup.NoAutoClose;title:"退出前保存失败"
        contentItem:Label{text:"播放状态未能保存。请选择后续操作。\n"+playbackBridge.exitError;wrapMode:Text.Wrap}
        footer:DialogButtonBox {
            ShellButton {objectName:"exitSaveRetry";text:"重试";onClicked:{exitSaveFailure.close();root.close()}}
            ShellButton {objectName:"exitSaveAnyway";text:"仍然退出";onClicked:{playbackBridge.requestExit(1);exitSaveFailure.close();root.close()}}
            ShellButton {objectName:"exitSaveCancel";text:"取消退出";onClicked:{playbackBridge.requestExit(2);root.allowClose=false;root.exitRequested=false;root.awaitingImportClose=false;exitSaveFailure.close()}}
        }
    }
    onWidthChanged: windowSave.restart()
    onHeightChanged: windowSave.restart()
    Timer { id: windowSave; interval: 350; onTriggered: if(root.visibility!==Window.FullScreen && root.visibility!==Window.Minimized)settingsBridge.saveWindowState(root.width,root.height,root.visibility===Window.Maximized) }
    Item {
        id: contentHost
        visible: !lyricsWorkspace.visible
        objectName: "contentHost"
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        anchors.bottom: importPanel.visible ? importPanel.top : playerBar.top
        Rectangle {
            id: sidebar; objectName: "sidebar"; width: 205; height: parent.height; color: Theme.panel;clip:true
            Image { objectName:"sidebarBackground"; x:0;y:0;width:settingsBridge.palette.shareLR?root.width:parent.width;height:settingsBridge.palette.shareLR&&settingsBridge.palette.shareFooter?root.height:parent.height;source:root.backgroundUrl(settingsBridge.palette.leftImage,settingsBridge.palette.blurLeft); fillMode: Image.PreserveAspectCrop; asynchronous: true; visible: source.toString().length>0 }
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 18; anchors.topMargin: 27; anchors.bottomMargin: 20; spacing: 14
                Item {
                    Layout.fillWidth: true; Layout.preferredHeight: 32
                    Image { x:8; width:30; height:30; source:"qrc:/icon.svg" }
                    UiText { x:49; anchors.verticalCenter:parent.verticalCenter; text:"Relaxer"; font.pixelSize:23; font.weight:Font.DemiBold; color:Theme.text }
                    MouseArea { anchors.fill:parent; onPressed:root.startSystemMove() }
                }
                Item { Layout.preferredHeight: 2 }
                UiText { text:"音乐资料库"; color:Theme.text; font.pixelSize: 14;font.weight:Font.DemiBold }
                Column {
                    Layout.fillWidth:true; spacing:8
                    Repeater {
                        model:["本地音乐","歌单","个人","设置","我喜欢"]
                        NavigationButton {
                            required property int index; required property string modelData
                            objectName:"navigation"+index; width:parent.width; text:modelData
                            marker:["music","playlist","person","settings","heart"][index]
                            selected:!root.lyricsOpen && (index===4 ? appBridge.page===0&&libraryBridge.playlistId==="-1" : index===0 ? appBridge.page===0&&libraryBridge.playlistId==="0" : appBridge.page===index)
                            onClicked:root.navigateOriginal(index)
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth:true
                    UiText { text:"我的歌单"; color:Theme.text; font.pixelSize: 14;font.weight:Font.DemiBold; Layout.fillWidth:true }
                    IconButton { width:26;height:26; symbol:"plus"; tip:"创建歌单"; onClicked:playlistEditor.create() }
                }
                ListView {
                    id: sidebarLists; objectName:"playlistNavigation"; Layout.fillWidth:true;Layout.fillHeight:true
                    model:playlistModel;clip:true;spacing:3
                    ScrollBar.vertical:ScrollBar{}
                    delegate:AbstractButton {
                        required property string playlistKey; required property string name
                        width:sidebarLists.width; height:34; hoverEnabled:true
                        background:Rectangle{radius:7;color:!root.lyricsOpen&&appBridge.page===0&&libraryBridge.playlistId===playlistKey?Theme.selected:parent.hovered?Theme.hover:"transparent"}
                        contentItem:UiText {text:name;color:Theme.text;font.pixelSize: 15;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter;leftPadding:12}
                        onClicked:root.openPlaylist(playlistKey)
                        TapHandler{acceptedButtons:Qt.RightButton;onTapped:{playlistEditor.edit(parent.playlistKey,parent.name)}}
                    }
                }
            }
        }
        Item {
            id: paper; objectName:"paperSurface"; x:205;width:parent.width-205;height:parent.height;clip:true
            Rectangle{anchors.fill:parent;color:Theme.background}
            Image{objectName:"pageBackground";x:settingsBridge.palette.shareLR||settingsBridge.palette.shareFooter?-205:0;y:0;width:settingsBridge.palette.shareLR||settingsBridge.palette.shareFooter?root.width:parent.width;height:settingsBridge.palette.shareFooter?root.height:parent.height;source:root.backgroundUrl(settingsBridge.palette.rightImage,settingsBridge.palette.blurRight);fillMode:Image.PreserveAspectCrop;asynchronous:true;visible:source.toString().length>0}
            Item {
                id: pageSurface;objectName:"pageSurface";width:parent.width;height:parent.height
                StackLayout {
                    id: pages;objectName:"pageStack";anchors.fill:parent;anchors.topMargin:Theme.pageContentOffset;currentIndex:appBridge.page
                    LocalPage { id:localPage;onBulkChanged:(active,keys)=>root.updateBulk(localPage,active,keys);onLyricsRequested:(keys,missing)=>root.openLyricsTools(keys,missing);onImportRequested:anchor=>importDialog.openMenu(anchor) }
                    PlaylistsPage { onOpenRequested:key=>root.openPlaylist(key);onEditRequested:(key,name)=>playlistEditor.edit(key,name);onCreateRequested:playlistEditor.create() }
                    ProfilePage { id:profilePage;onBulkChanged:(active,keys)=>root.updateBulk(profilePage,active,keys);onLyricsRequested:keys=>root.openLyricsTools(keys,false) }
                    Item {
                        StackLayout {
                            anchors.fill:parent
                            currentIndex:root.settingsSection==="ai"?1:root.settingsSection==="appearance"?2:root.settingsSection==="log"?3:0
                            Item { SettingsPage { id:settingsPage;onVersionLogRequested:root.changePage(function(){root.settingsSection="log"});anchors.fill:parent;anchors.leftMargin:34;anchors.rightMargin:34;anchors.topMargin:28;anchors.bottomMargin:16;onAiRequested:root.changePage(function(){root.settingsSection="ai"});onAppearanceRequested:{settingsBridge.beginAppearance();root.changePage(function(){root.settingsSection="appearance"})} } }
                            ScrollView { id:aiScroll;contentWidth:availableWidth;clip:true;leftPadding:34;rightPadding:34;topPadding:28;bottomPadding:16;AiPage{width:aiScroll.availableWidth;onBackRequested:root.changePage(function(){root.settingsSection="settings"})} }
                            AppearancePage {
                                id:appearancePage
                                onBackRequested:{
                                    if(root.pendingAppearanceClose){
                                        root.pendingAppearanceClose=false
                                        // Resume through the existing task, playback-save and import guards.
                                        Qt.callLater(root.requestQuit)
                                        return
                                    }
                                    const action=root.pendingPageAction;root.pendingPageAction=null;root.settingsSection="settings";root.changePage(action||function(){root.settingsSection="settings"})
                                }
                                onLeaveRejected:{
                                    root.pendingPageAction=null
                                    if(root.pendingAppearanceClose){root.pendingAppearanceClose=false;root.exitRequested=false}
                                }
                            }
                            VersionLogPage{onBackRequested:root.changePage(function(){root.settingsSection="settings"})}
                        }
                    }
                }
            }
            Image { id:oldPage;anchors.fill:parent;visible:false;fillMode:Image.Stretch;cache:false;asynchronous:false;onStatusChanged:if(status===Image.Ready)root.warmSnapshot() }
        }
        Item {
            id: playerClip;objectName:"playerBackdropClip"
            x:0;y:contentHost.height*(1-root.playerReveal);width:contentHost.width;height:contentHost.height*root.playerReveal
            clip:true;visible:root.playerProgress>0
            LyricsPage {
                id:nowPage;objectName:"nowPlayingPage";x:0;y:-playerClip.y;width:contentHost.width;height:contentHost.height
                detailsOpacity:root.lyricsOpen ? Math.max(0,Math.min(1,(root.playerProgress-.52)/.48)) : Math.max(0,root.playerProgress)
                artworkTravelling:root.coverTravelling;metadataTravelling:root.metadataTravelling;fullScreen:root.fullScreenMode
                onCollapseRequested:root.closeLyrics()
                onFullscreenRequested:root.toggleFullscreen()
                onTranslationRequested:root.openLyricsTools([playbackBridge.trackId],false)
            }
        }
    }
    PlayerBar {
        id:playerBar;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:98
        visible:!root.bulkActive&&!lyricsWorkspace.visible;expanded:root.lyricsOpen;artworkTravelling:root.coverTravelling
        backgroundCanvasHeight: root.height; backgroundOffsetY: playerBar.y
        onExpandRequested:root.animatePlayer(!root.lyricsOpen)
    }

    Rectangle {
        objectName:"bulkBar";anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:98;visible:root.bulkActive&&!lyricsWorkspace.visible;color:Theme.card;z:3
        ColumnLayout {
            anchors.fill:parent;anchors.leftMargin:22;anchors.rightMargin:22;anchors.topMargin:10;anchors.bottomMargin:10;spacing:4
            RowLayout {
                Layout.fillWidth:true
                UiText {objectName:"bulkCount";text:"已选 "+root.bulkKeys.length+" 首歌曲";color:Theme.text;font.pixelSize: 15}
                ShellButton{objectName:"bulkSelectAll";text:"全选";flat:true;onClicked:root.selectBulk(true)}
                ShellButton{objectName:"bulkClear";text:"取消选择";flat:true;onClicked:root.selectBulk(false)}
                Item{Layout.fillWidth:true}
                ShellButton{objectName:"bulkLyrics";text:"查找歌词";flat:true;enabled:root.bulkKeys.length>0;onClicked:root.openLyricsTools(root.bulkKeys,false)}
                ShellButton{objectName:"bulkExit";text:"退出多选";flat:true;onClicked:root.clearBulk()}
            }
            RowLayout {
                Layout.fillWidth:true
                ShellButton{objectName:"bulkDelete";text:"删除";enabled:root.bulkKeys.length>0&&root.bulkOwner===localPage&&root.bulkScope==="0";onClicked:{root.bulkActionKeys=root.bulkKeys.slice();bulkDelete.open()}}
                ShellButton{objectName:"bulkUnlike";text:"取消喜欢";enabled:root.bulkKeys.length>0;onClicked:libraryBridge.applyFavorite(root.bulkKeys,false)}
                ShellButton{objectName:"bulkLike";text:"全部喜欢";enabled:root.bulkKeys.length>0;onClicked:libraryBridge.applyFavorite(root.bulkKeys,true)}
                ShellButton{id:bulkAdd;objectName:"bulkAddPlaylist";text:"添加至歌单";enabled:root.bulkKeys.length>0;onClicked:bulkAddMenu.popup(bulkAdd,0,-bulkAddMenu.height)}
                ShellButton{objectName:"bulkNewPlaylist";text:"新建歌单并加入";enabled:root.bulkKeys.length>0;onClicked:{root.bulkActionKeys=root.bulkKeys.slice();bulkNew.open()}}
                ShellButton{objectName:"bulkRemovePlaylist";text:"从歌单移除";enabled:root.bulkKeys.length>0&&Number(root.bulkScope)>0;onClicked:libraryBridge.removeTracksFromPlaylist(root.bulkKeys)}
                Item{Layout.fillWidth:true}
            }
        }
    }
    RoundMenu {
        id:bulkAddMenu;parent:root.popupLayer
        Instantiator {
            model:playlistModel
            delegate:RoundMenuItem {required property string name;required property string playlistKey;text:name;onTriggered:libraryBridge.addTracksToPlaylist(playlistKey,root.bulkKeys)}
            onObjectAdded:(index,object)=>bulkAddMenu.insertItem(index,object)
            onObjectRemoved:(index,object)=>bulkAddMenu.removeItem(object)
        }
    }
    ConfirmDialog{id:bulkDelete;parent:root.popupLayer;anchors.centerIn:parent;title:"从曲库移除";width:460;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel;contentItem:Label{text:"移除所选歌曲及歌单关联和统计，保留音乐文件。"} onAccepted:{libraryBridge.removeTracks(root.bulkActionKeys,false);root.clearBulk()}}
    ConfirmDialog {id:bulkNew;parent:root.popupLayer;anchors.centerIn:parent;title:"新建歌单并加入";width:420;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel;contentItem:EntryField {id:bulkName;placeholderText:"歌单名称"} onAccepted:{const id=libraryBridge.createPlaylist(bulkName.text);if(id!=="0")libraryBridge.addTracksToPlaylist(id,root.bulkActionKeys)}}
    Rectangle {
        id:importPanel;objectName:"importPanel";anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:playerBar.top
        property bool observedTask:false
        property bool completionVisible:false
        property string completionText:""
        readonly property string shownStatus:importBridge.busy?importBridge.status:completionText
        visible:(importBridge.busy||completionVisible)&&!lyricsWorkspace.visible
        height:Math.min(144,Math.max(44,importStatus.implicitHeight+16));color:Theme.panel;clip:true;z:3
        function updateTask(){
            if(importBridge.busy){
                observedTask=true;completionTimer.stop();completionVisible=false;completionText=""
            }else if(observedTask&&Object.keys(importBridge.result).length>0){
                observedTask=false;completionText=importBridge.status;completionVisible=true;completionTimer.restart()
            }
        }
        Connections{target:importBridge;function onChanged(){importPanel.updateTask()}}
        Timer{id:completionTimer;objectName:"importCompletionTimer";interval:4000;onTriggered:importPanel.completionVisible=false}
        RowLayout {
            anchors.fill:parent;anchors.leftMargin:22;anchors.rightMargin:22;spacing:16
            UiText {id:importStatus;objectName:"importStatusText";Layout.fillWidth:true;text:importPanel.shownStatus;color:Theme.secondary;font.pixelSize:14;textFormat:Text.PlainText;wrapMode:Text.Wrap;maximumLineCount:5;elide:Text.ElideRight}
            ProgressBar{objectName:"importProgress";visible:importBridge.busy;Layout.preferredWidth:Math.min(230,root.width*.22);from:0;to:Math.max(1,importBridge.total);value:importBridge.current;indeterminate:importBridge.total===0}
            ShellButton{visible:importBridge.busy;text:"取消导入";implicitHeight:28;onClicked:importBridge.cancel()}
        }
    }
    TapHandler {
        gesturePolicy:TapHandler.DragThreshold
        onPressedChanged:if(pressed&&upcoming.panelExpanded&&!upcoming.contains(upcoming.mapFromItem(root.contentItem,point.position.x,point.position.y)))upcoming.panelExpanded=false
    }
    Item {
        objectName:"upcomingWindowClip";anchors.fill:parent;clip:true;z:2;visible:!lyricsWorkspace.visible
        UpcomingPanel {
            id:upcoming;objectName:"nextQueuePanel"
            expandedPlayer:root.upcomingAtLeft;x:root.upcomingAtLeft?8:root.width-8-width
            y:root.height-4-height+(height+4)*root.upcomingTravel
            relocating:upcomingOut.running||upcomingIn.running
            enabled:!relocating
            visible:!root.bulkActive
        }
    }
    NumberAnimation {
        id:upcomingOut;target:root;property:"upcomingTravel";to:1;duration:210;easing.type:Easing.OutCubic
        onFinished:{root.upcomingAtLeft=root.lyricsOpen;upcomingIn.from=1;upcomingIn.restart()}
    }
    NumberAnimation {id:upcomingIn;target:root;property:"upcomingTravel";to:0;duration:210;easing.type:Easing.OutCubic}
    Artwork {
        id:flight;objectName:"sharedArtworkFlight";z:4;visible:root.coverTravelling&&!lyricsWorkspace.visible
        file:root.flightFile;seed:root.flightSeed;resolution:768
        x:root.coverFrom.x+(root.coverTo.x-root.coverFrom.x)*root.coverProgress
        y:root.coverFrom.y+(root.coverTo.y-root.coverFrom.y)*root.coverProgress
        width:root.coverFrom.width+(root.coverTo.width-root.coverFrom.width)*root.coverProgress
        height:root.coverFrom.height+(root.coverTo.height-root.coverFrom.height)*root.coverProgress
    }

    UiText {
        id:titleFlight;objectName:"sharedTitleFlight";z:4;visible:root.metadataTravelling&&!lyricsWorkspace.visible
        text:playbackBridge.hasTrack?playbackBridge.title:"Relaxer";textFormat:Text.PlainText;elide:Text.ElideRight;color:Theme.text
        x:root.titleFrom.x+(root.titleTo.x-root.titleFrom.x)*root.coverProgress
        y:root.titleFrom.y+(root.titleTo.y-root.titleFrom.y)*root.coverProgress
        width:root.titleFrom.width+(root.titleTo.width-root.titleFrom.width)*root.coverProgress
        height:root.titleFrom.height+(root.titleTo.height-root.titleFrom.height)*root.coverProgress
        font.pixelSize:root.titleFrom.pixelSize+(root.titleTo.pixelSize-root.titleFrom.pixelSize)*root.coverProgress
        font.family:root.titleTo.family;font.weight:root.titleTo.weight
    }
    UiText {
        id:artistFlight;objectName:"sharedArtistFlight";z:4;visible:root.metadataTravelling&&!lyricsWorkspace.visible
        text:playbackBridge.hasTrack?playbackBridge.artist:"选一首歌，开始放松。";textFormat:Text.PlainText;elide:Text.ElideRight;color:Theme.secondary
        x:root.artistFrom.x+(root.artistTo.x-root.artistFrom.x)*root.coverProgress
        y:root.artistFrom.y+(root.artistTo.y-root.artistFrom.y)*root.coverProgress
        width:root.artistFrom.width+(root.artistTo.width-root.artistFrom.width)*root.coverProgress
        height:root.artistFrom.height+(root.artistTo.height-root.artistFrom.height)*root.coverProgress
        font.pixelSize:root.artistFrom.pixelSize+(root.artistTo.pixelSize-root.artistFrom.pixelSize)*root.coverProgress
        font.family:root.artistTo.family
    }
    DropArea{anchors.fill:parent;onDropped:drop=>{if(drop.hasUrls&&!importBridge.busy){importBridge.start(drop.urls,Number(libraryBridge.playlistId)>0?libraryBridge.playlistId:"0");drop.acceptProposedAction()}}}
    MouseArea{z:5;x:205;y:0;width:Math.max(0,root.width-331);height:26;onPressed:root.startSystemMove();onDoubleClicked:root.visibility===Window.Maximized?root.showNormal():root.showMaximized()}
    Item {
        anchors.fill:parent;z:8
        Repeater {
            model:[Qt.LeftEdge,Qt.RightEdge,Qt.TopEdge,Qt.BottomEdge,Qt.TopEdge|Qt.LeftEdge,Qt.TopEdge|Qt.RightEdge,Qt.BottomEdge|Qt.LeftEdge,Qt.BottomEdge|Qt.RightEdge]
            delegate:MouseArea{
                required property int modelData
                enabled:root.visibility===Window.Windowed
                x:modelData&Qt.RightEdge?root.width-width:0;y:modelData&Qt.BottomEdge?root.height-height:0
                width:(modelData&(Qt.LeftEdge|Qt.RightEdge))?6:root.width
                height:(modelData&(Qt.TopEdge|Qt.BottomEdge))?6:root.height
                cursorShape:modelData===(Qt.LeftEdge|Qt.TopEdge)||modelData===(Qt.RightEdge|Qt.BottomEdge)?Qt.SizeFDiagCursor:modelData===(Qt.RightEdge|Qt.TopEdge)||modelData===(Qt.LeftEdge|Qt.BottomEdge)?Qt.SizeBDiagCursor:modelData&(Qt.LeftEdge|Qt.RightEdge)?Qt.SizeHorCursor:Qt.SizeVerCursor
                onPressed:root.startSystemResize(modelData)
            }
        }
    }
    Row {
        id:titleBar;objectName:"titleBar";anchors.top:parent.top;anchors.right:parent.right;z:7;height:32
        Repeater{
            model:["最小化","最大化","关闭"]
            delegate:AbstractButton{
                id: windowControl
                required property int index;required property string modelData
                objectName: "windowControl" + index
                width:42;height:32;hoverEnabled:true
                Accessible.name: index===1 && root.visibility===Window.Maximized ? "还原窗口" : modelData
                ToolTip.visible: hovered; ToolTip.text: Accessible.name
                background:Rectangle{color:windowControl.down?(index===2?Qt.darker(Theme.accent,1.15):Theme.pressed):windowControl.hovered?(index===2?Theme.accent:Theme.hover):"transparent"}
                contentItem: Item {
                    Canvas {
                        anchors.centerIn: parent; width:14; height:14
                        property color ink: windowControl.hovered && windowControl.index===2 ? "white" : Theme.text
                        property bool maximized: root.visibility===Window.Maximized
                        onInkChanged: requestPaint()
                        onMaximizedChanged: requestPaint()
                        onPaint: {
                            const ctx=getContext("2d");ctx.clearRect(0,0,width,height)
                            ctx.strokeStyle=ink;ctx.lineWidth=1.2;ctx.lineCap="square";ctx.beginPath()
                            if(windowControl.index===0){ctx.moveTo(1,10);ctx.lineTo(13,10)}
                            else if(windowControl.index===1){
                                if(maximized){ctx.moveTo(4,1);ctx.lineTo(13,1);ctx.lineTo(13,10);ctx.moveTo(1,4);ctx.rect(1,4,9,9)}
                                else ctx.rect(1,1,12,12)
                            }else{ctx.moveTo(2,2);ctx.lineTo(12,12);ctx.moveTo(12,2);ctx.lineTo(2,12)}
                            ctx.stroke()
                        }
                    }
                }
                onClicked:{if(index===0)root.showMinimized();else if(index===1){if(root.visibility===Window.Maximized)root.showNormal();else root.showMaximized()}else root.close()}
            }
        }
    }
    NumberAnimation { id:playerMotion;target:root;property:"playerProgress";duration:420;easing.type:Easing.OutCubic }
    NumberAnimation { id:coverMotion;target:root;property:"coverProgress";from:0;to:1;duration:420;easing.type:Easing.OutCubic;onFinished:{root.coverTravelling=false;root.metadataTravelling=false;duration=420} }
    ParallelAnimation {
        id:pageOut
        NumberAnimation{target:oldPage;property:"opacity";to:0;duration:84;easing.type:Easing.OutCubic}
        NumberAnimation{target:oldPage;property:"scale";to:.99;duration:84;easing.type:Easing.OutCubic}
        onFinished:{oldPage.visible=false}
    }
    ParallelAnimation {
        id:pageIn
        NumberAnimation{target:pageSurface;property:"opacity";to:1;duration:136;easing.type:Easing.OutCubic}
        NumberAnimation{target:pageSurface;property:"y";to:0;duration:136;easing.type:Easing.OutCubic}
        onFinished:{root.pageGrab=null;oldPage.visible=false;oldPage.source=""}
    }
    Timer {
        id:snapshotWarmup;interval:32
        onTriggered:{
            if(root.snapshotRevision!==root.navigationRevision||!root.navigationAction)return
            const action=root.navigationAction;root.navigationAction=null
            oldPage.opacity=1;root.clearBulk();action()
            pageSurface.opacity=.001;pageSurface.y=-7
            pageOut.restart();pageIn.restart()
        }
    }
    ImportDialog{id:importDialog;parent:root.popupLayer}
    PlaylistEditor{id:playlistEditor;parent:root.popupLayer;onCreated:key=>root.openPlaylist(key)}
    LyricsWorkspace{id:lyricsWorkspace;parent:root.contentItem;z:6}
    Toast{id:toast;anchors.horizontalCenter:parent.horizontalCenter;anchors.bottom:playerBar.top;anchors.bottomMargin:16;z:20}
    ConfirmDialog {
        id:closeConfirm;parent:root.popupLayer;objectName:"closeTasksConfirmation";anchors.centerIn:parent;width:Math.min(480,root.width-40);modal:true
        title:"结束当前任务并退出？";standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:Label{text:(lyricsSearchBridge.busy?"还有歌词尚未查找完毕，确认退出将中断查找。\n":"")+"退出将取消尚未完成的导入和翻译，并放弃未保存的翻译预览。已经保存的结果会保留；已发送的 AI 请求可能仍产生费用。";wrapMode:Text.Wrap}
        onRejected:root.exitRequested=false
        onAccepted:{lyricsSearchBridge.stop();root.allowClose=true;root.close()}
    }
}
