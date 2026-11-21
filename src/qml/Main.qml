import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "components"
import "pages"
import "theme"
ApplicationWindow {
    id: root
    readonly property real captionHeight:32*.8
    readonly property real captionButtonWidth:42*.8
    property bool playerChromeVisible:true
    property point playerPointer:Qt.point(-1,-1)
    function revealPlayerChrome(){if(playerPresented){playerChromeVisible=true;playerChromeIdle.restart()}}
    onPlayerPresentedChanged:if(playerPresented)revealPlayerChrome()
    Binding {target:playbackBridge;property:"onlineVideoVisible";value:(root.playerPresented||root.videoExpandPending||(root.videoExpandEnabled&&!root.firstVideoStarted&&playbackBridge.video))&&!root.miniMode&&!root.miniSwitching&&root.visible&&root.visibility!==Window.Minimized}
    property bool videoExpandPending:false
    property bool firstVideoStarted:false
    property bool videoExpandEnabled:settingsBridge.values.autoExpandVideo
    onVideoExpandEnabledChanged:firstVideoStarted=false
    function presentVideoWhenPlaying(){
        if(playbackBridge.video&&playbackBridge.playing&&videoExpandEnabled&&!firstVideoStarted){
            videoExpandPending=true;firstVideoStarted=true;const key=playbackBridge.trackId
            Qt.callLater(function(){if(root.videoExpandEnabled&&playbackBridge.playing&&playbackBridge.video&&playbackBridge.trackId===key)root.showLyrics();root.videoExpandPending=false})
        }
    }
    component DeferredPage: Loader {
        id:deferred
        property bool requested:false
        property bool recyclable:false
        property var retainedState:null
        active:false
        onRequestedChanged:{if(requested){idleRelease.stop();active=true}else if(recyclable)idleRelease.restart()}
        onLoaded:if(retainedState&&item&&typeof item.restorePageState==="function")item.restorePageState(retainedState)
        Component.onCompleted:if(requested)active=true
        Timer {
            id:idleRelease;interval:45000
            onTriggered:{
                if(deferred.requested||!deferred.recyclable||!deferred.active)return
                if(deferred.item&&typeof deferred.item.canUnloadPage==="function"&&!deferred.item.canUnloadPage()){restart();return}
                deferred.retainedState=deferred.item&&typeof deferred.item.savePageState==="function"?deferred.item.savePageState():null
                deferred.active=false
            }
        }
    }
    objectName: "qmlMainWindow"
    visible: false
    width: 1240
    height: 790
    property bool miniMode:false
    property bool miniSwitching:false
    property bool miniClosePending:false
    readonly property int miniWidth:340
    readonly property int miniHeight:58
    property var miniRestoreGeometry:[0,0,1240,790]
    property var miniNormalGeometry:miniRestoreGeometry
    property int miniRestoreVisibility:Window.Windowed
    property var miniRestoreFocus:null
    property real miniTransitionProgress:0
    property var miniMotionFrom:[]
    property var miniMotionTo:[]
    property bool miniRestorePlacement:true
    property bool windowResizeActive:false
    property bool resizeMiniPinned:false
    readonly property bool miniResizeActive:windowResizeActive&&miniMode
    property int miniResizeEdges:0
    property point miniResizeCursor
    property point resizePointerInset
    property point resizeFixedCorner
    property point resizeLatestCursor
    property point miniTransitionCursor
    property var miniResizeGeometry:[]
    property real miniReturnProgress:0
    property var miniReturnFrom:[]
    property var miniReturnTo:[]
    property bool miniEnteredByShortcut:false
    property bool miniShortcutTransition:false
    readonly property bool skipMiniAnimation:settingsBridge.values.animationMode===1&&miniShortcutTransition
    property var miniFlightParts:[]
    readonly property bool miniFlightActive:miniSwitching&&!miniMode&&miniFlightParts.length>0
    minimumWidth:miniMode||miniSwitching?miniWidth:414;minimumHeight:miniMode||miniSwitching?miniHeight:540
    maximumWidth:miniMode&&!miniSwitching?miniWidth+24:16777215;maximumHeight:miniMode&&!miniSwitching?miniHeight+14:16777215
    property bool sidebarCondensed:false
    property bool playlistOrderEditing:false
    property bool playlistOrderDraftActive:false
    property real playlistOrderProgress:playlistOrderEditing?1:0
    Behavior on playlistOrderProgress {NumberAnimation{duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.InOutCubic}}
    ListModel {id:playlistOrderDraft}
    function beginPlaylistOrder() {
        playlistOrderExit.stop()
        if(!playlistOrderDraftActive){
            playlistOrderDraft.clear()
            for(let i=0;i<playlistModel.count;++i)playlistOrderDraft.append(playlistModel.get(i))
            playlistOrderDraftActive=true
        }
        playlistOrderEditing=true
    }
    function cancelPlaylistOrder() {
        playlistOrderEditing=false
        for(let i=0;i<playlistModel.count;++i){
            const key=playlistModel.get(i).playlistKey
            for(let row=i;row<playlistOrderDraft.count;++row)if(playlistOrderDraft.get(row).playlistKey===key){if(row!==i)playlistOrderDraft.move(row,i,1);break}
        }
        playlistOrderExit.restart()
    }
    function savePlaylistOrder() {
        const ids=[]
        for(let i=0;i<playlistOrderDraft.count;++i)ids.push(playlistOrderDraft.get(i).playlistKey)
        if(libraryBridge.setPlaylistOrder(ids)){playlistOrderEditing=false;playlistOrderExit.restart()}
    }
    Timer {id:playlistOrderExit;interval:settingsBridge.values.reduceMotion?0:360;onTriggered:{root.playlistOrderDraftActive=false;playlistOrderDraft.clear()}}
    property real sidebarCondenseProgress:sidebarCondensed?1:0
    Behavior on sidebarCondenseProgress {NumberAnimation{duration:settingsBridge.values.reduceMotion?0:280;easing.type:Easing.OutCubic}}
    property real sidebarDragWidth:-1
    readonly property real sidebarMaxWidth:Math.min(400,Math.max(160,normalShell.width-340))
    readonly property real sidebarExpandedWidth:Math.min(sidebarMaxWidth,Math.max(160,sidebarDragWidth>=0?sidebarDragWidth:Number(settingsBridge.values.sidebarWidth||205)))
    readonly property real sidebarBaseWidth:sidebarExpandedWidth+(84-sidebarExpandedWidth)*sidebarCondenseProgress
    readonly property real sidebarRowHeight:36
    readonly property real sidebarRowSpacing:5.5
    readonly property bool narrowLayout: (miniSwitching?miniNormalGeometry[2]:width) < 547
    property real narrowProgress:0
    NumberAnimation {id:narrowMotion;target:root;property:"narrowProgress";duration:320;easing.type:Easing.OutCubic}
    Binding { target: Theme; property: "compact"; value: root.narrowLayout }
    Binding { target: Theme; property: "layoutProgress"; value: root.narrowProgress }
    property bool windowPinned: false
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowSystemMenuHint | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint | (miniMode||miniSwitching||(windowResizeActive&&resizeMiniPinned)||windowPinned ? Qt.WindowStaysOnTopHint : 0)
    title: appBridge.windowTitle + " · QML"
    color: miniSwitching ? (Theme.dark&&!Theme.customHighlights ? "#19191e" : "white") : Theme.background
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
    readonly property bool playerPresented:lyricsOpen||playerProgress>0
    readonly property bool playerSettled:lyricsOpen&&playerProgress>=1
    readonly property bool backgrounded: !visible || visibility === Window.Minimized
    onBackgroundedChanged:if(backgrounded)Qt.callLater(root.suspendTransientPopups)
    function suspendTransientPopups(){
        if(!backgrounded)return
        narrowQueue.close();compactNavigationMenu.close();playerBar.closeTransientPopups()
        if(miniLoader.item)miniLoader.item.closeTransientPopups()
        const action=navigationAction;navigationAction=null
        snapshotWarmup.stop();pageOut.stop();pageIn.stop();clearPageSnapshot()
        pageSurface.opacity=1;pageSurface.y=0
        if(action){root.clearBulk();action()}
    }
    readonly property var appearancePage: appearanceLoader.item
    readonly property var lyricsWorkspace: lyricsWorkspaceLoader.item
    readonly property var nowPage:nowPlayingLoader.item
    function ensureNowPlaying(){nowPlayingLoader.active=true;return nowPage}
    function ensureImportDialog(){importDialogLoader.active=true;return importDialogLoader.item}
    function ensurePlaylistEditor(){playlistEditorLoader.active=true;return playlistEditorLoader.item}
    function ensureBatchInstrumental(){batchInstrumentalLoader.active=true;return batchInstrumentalLoader.item}
    readonly property bool workspaceOpened: lyricsWorkspace ? lyricsWorkspace.opened : false
    readonly property bool workspaceVisible: lyricsWorkspace ? lyricsWorkspace.visible : false
    function ensureAppearancePage() { appearanceLoader.active = true; return appearancePage }
    function ensureLyricsWorkspace() { lyricsWorkspaceLoader.active = true; return lyricsWorkspace }
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
    // Suspend ordinary overlays without closing their dialogs or edit drafts.
    Binding {target:root.popupLayer;property:"opacity";value:0;when:root.miniMode||root.miniSwitching;restoreMode:Binding.RestoreBindingOrValue}
    Binding {target:root.popupLayer;property:"enabled";value:false;when:root.miniMode||root.miniSwitching;restoreMode:Binding.RestoreBindingOrValue}
    Binding {target:root.popupLayer;property:"visible";value:false;when:root.miniMode||root.miniSwitching;restoreMode:Binding.RestoreBindingOrValue}
    Connections {
        target:root.popupLayer
        function onVisibleChanged(){if((root.miniMode||root.miniSwitching)&&root.popupLayer.visible)root.popupLayer.visible=false}
    }
    property string initialVideoPreviewId:""
    property real playerProgress: 0
    readonly property real playerReveal: playerProgress
    property bool upcomingAtLeft: false
    property bool upcomingRelocating:false
    property real upcomingTravel: 0
    property var coverFrom: ({x:0,y:0,width:62,height:62})
    property var coverTo: ({x:0,y:0,width:62,height:62})
    property bool coverTravelling: false
    property real coverProgress: 0
    property url flightFile: ""
    property string flightSeed: ""
    property int navigationRevision: 0
    property var pageGrab: null
    property bool snapshotAwaitingFrame:false
    property size snapshotSize:Qt.size(0,0)
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
        // The persistent feedback Item is non-modal. Its detail dialogs are
        // separate overlay children and retain their existing Escape priority.
        for (let index = 0; index < items.length; ++index)
            if (items[index] !== feedbackPanel && items[index].visible && items[index].opacity > 0 && items[index].width > 0 && items[index].height > 0) return true
        return false
    }
    function imageRect(item) { const p = item.mapToItem(root.contentItem,0,0); return {x:p.x,y:p.y,width:item.width,height:item.height} }
    function artworkRect(item) {
        const bounds = imageRect(item)
        return bounds
    }
    function textRect(item) {
        const bounds = imageRect(item)
        // A flight keeps its font fixed and scales the glyphs. Capture its
        // painted size when reversing midway, rather than the backing font.
        const scale = item.flightScale === undefined ? 1 : item.flightScale
        bounds.width *= scale; bounds.height *= scale
        const width = Math.min(bounds.width, (item.contentWidth + 2) * scale)
        if (item.horizontalAlignment === Text.AlignHCenter) bounds.x += (bounds.width - width) / 2
        bounds.width = Math.max(1, width)
        bounds.pixelSize = item.font.pixelSize * scale
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
    function clearBulk() { if(lyricsSearchBridge.batchRunning)lyricsSearchBridge.cancelInstrumentalBatch(); if (bulkOwner) bulkOwner.clearBulk(); bulkActive = false; bulkKeys = []; bulkOwner = null }
    function selectBulk(all) {
        if (!bulkOwner) return
        bulkOwner.setBulkKeys(all ? libraryBridge.visibleTrackKeys() : [])
    }
    function animatePlayer(open) {
        if (lyricsOpen === open && !playerMotion.running) return
        ensureNowPlaying()
        if(open)nowPage.forceActiveFocus()
        const start = artworkRect(coverTravelling ? flight : open ? playerBar.smallArtwork : nowPage.largeArtwork)
        const titleStart = metadataTravelling ? textRect(titleFlight) : textRect(open ? playerBar.metadataTitle : nowPage.metadataTitle)
        const artistStart = metadataTravelling ? textRect(artistFlight) : textRect(open ? playerBar.metadataArtist : nowPage.metadataArtist)
        lyricsOpen = open
        coverFrom = start; coverTo = artworkRect(open ? nowPage.largeArtwork : playerBar.smallArtwork)
        titleFrom = titleStart; titleTo = textRect(open ? nowPage.metadataTitle : playerBar.metadataTitle)
        artistFrom = artistStart; artistTo = textRect(open ? nowPage.metadataArtist : playerBar.metadataArtist)
        flightFile = playbackBridge.coverUrl; flightSeed = playbackBridge.hasTrack ? playbackBridge.title : "Relaxer"
        playerMotion.stop(); coverMotion.stop()
        if (settingsBridge.values.reduceMotion) { playerProgress = open ? 1 : 0; coverTravelling = false; metadataTravelling = false; return }
        coverProgress = 0; coverTravelling = true; metadataTravelling = true
        playerMotion.from = playerProgress; playerMotion.to = open ? 1 : 0
        playerMotion.duration = 420; coverMotion.duration=420; playerMotion.restart(); coverMotion.restart()
    }
    property bool widthSwitchActive:false
    property var widthSwitchGeometry:[]
    property real widthSwitchProgress:0
    readonly property bool widthSwitchRunning:widthSwitchMotion.running
    onWidthSwitchProgressChanged: {
        if(widthSwitchGeometry.length!==7)return
        const g=widthSwitchGeometry,w=g[3]+(g[4]-g[3])*widthSwitchProgress
        narrowProgress=g[5]+(g[6]-g[5])*widthSwitchProgress
        uiIntegration.setWindowGeometry(g[0]-w,g[1],w,g[2])
    }
    NumberAnimation {
        id:widthSwitchMotion;target:root;property:"widthSwitchProgress";from:0;to:1
        duration:360;easing.type:Easing.InOutCubic
        onFinished:{
            root.narrowProgress=root.widthSwitchGeometry[6]
            const g=root.widthSwitchGeometry
            uiIntegration.setWindowGeometry(g[0]-g[4],g[1],g[4],g[2])
            Qt.callLater(function(){root.widthSwitchActive=false;uiIntegration.endResizeAnchor()})
        }
    }
    function toggleNarrowWindow() {
        if(miniMode||miniSwitching||widthSwitchActive)return
        // showNormal() restores an unrelated saved rectangle. Retain both the
        // logical bounds and the physical anchor of the window the user sees.
        const original=[root.x+root.width,root.y,root.height,root.width,narrowProgress]
        uiIntegration.beginResizeAnchor(Qt.LeftEdge)
        function applySize(){
            const right=original[0],top=original[1],keepHeight=original[2],initialWidth=original[3]
            const nextWidth=initialWidth<547?Math.max(547,Number(settingsBridge.values.wideWindowWidth||1240)):root.minimumWidth
            if(initialWidth>=547)settingsBridge.setPreference("wideWindowWidth",Math.round(initialWidth))
            narrowMotion.stop();playerBar.closeTransientPopups();upcoming.panelExpanded=false
            widthSwitchGeometry=[right,top,keepHeight,initialWidth,nextWidth,original[4],nextWidth<547?1:0]
            widthSwitchActive=true
            widthSwitchProgress=0
            if(settingsBridge.values.reduceMotion){widthSwitchProgress=1;widthSwitchActive=false;uiIntegration.endResizeAnchor()}
            else widthSwitchMotion.restart()
        }
        if(visibility===Window.Maximized||visibility===Window.FullScreen){
            showNormal()
            uiIntegration.setWindowGeometry(original[0]-original[3],original[1],original[3],original[2])
            Qt.callLater(applySize)
        }
        else applySize()
    }
    function relocateUpcoming(left) {
        root.upcomingRelocating=true
        upcomingOut.stop(); upcomingIn.stop()
        if (settingsBridge.values.reduceMotion || root.narrowLayout) { upcomingAtLeft=left; upcomingTravel=0; root.upcomingRelocating=false;return }
        if (upcomingAtLeft===left) { upcomingIn.from=upcomingTravel; upcomingIn.restart() }
        else { upcomingOut.from=upcomingTravel; upcomingOut.restart() }
    }
    onLyricsOpenChanged: {relocateUpcoming(lyricsOpen);if(lyricsOpen)compactNavigationMenu.close()}
    function showLyrics() { animatePlayer(true) }
    function feedbackPointerPressed(x,y) { feedbackPanel.pointerPressed(x,y) }
    function closeLyrics() { if (fullScreenMode) toggleFullscreen(); animatePlayer(false) }
    function toggleFullscreen() {
        const revision = ++fullscreenRevision
        if (fullScreenMode) {
            if (wasMaximized) showMaximized()
            else { showNormal(); const b=savedGeometry; Qt.callLater(function(){if(revision!==fullscreenRevision||fullScreenMode)return;root.x=b[0];root.y=b[1];root.width=b[2];root.height=b[3]}) }
        } else { savedGeometry=[x,y,width,height]; wasMaximized=visibility===Window.Maximized; showLyrics(); showFullScreen() }
    }
    function changePage(action) {
        if (appBridge.page === 3 && settingsSection === "appearance" && !ensureAppearancePage().confirmLeave()) {
            pendingPageAction = action
            return
        }
        pendingPageAction = null
        const revision=++navigationRevision
        snapshotWarmup.stop(); navigationAction=null
        pageOut.stop(); pageIn.stop(); pageSurface.opacity=1; pageSurface.y=0
        clearPageSnapshot()
        if (lyricsOpen) animatePlayer(false)
        if (settingsBridge.values.reduceMotion || root.backgrounded || !pageSurface.visible) { root.clearBulk();action(); return }
        // Capture the painted background as well as the controls. Capturing
        // only the transparent pageSurface produces a black backdrop when
        // its snapshot is composited by the software renderer.
        const capturedSize=Qt.size(paper.width,paper.height)
        const captured=paper.grabToImage(function(grab) {
            if(revision!==navigationRevision)return
            if(root.backgrounded||paper.width!==capturedSize.width||paper.height!==capturedSize.height){root.clearBulk();action();return}
            pageGrab=grab;snapshotSize=capturedSize;snapshotRevision=revision;navigationAction=action
            oldPage.opacity=.001; oldPage.scale=1; oldPage.source=grab.url
            if(oldPage.status===Image.Ready) warmSnapshot()
        })
        if(!captured){root.clearBulk();action()}
    }
    function clearPageSnapshot(){snapshotAwaitingFrame=false;snapshotWarmup.stop();oldPage.visible=false;oldPage.source="";pageGrab=null}
    function warmSnapshot(){
        if(snapshotRevision!==navigationRevision||!navigationAction)return
        oldPage.visible=true;snapshotAwaitingFrame=true;snapshotWarmup.restart()
    }
    function presentPageSnapshot(revision,animate){
        if(!snapshotAwaitingFrame||revision!==snapshotRevision||revision!==navigationRevision||!navigationAction)return
        snapshotAwaitingFrame=false;snapshotWarmup.stop()
        const action=navigationAction;navigationAction=null;root.clearBulk()
        if(!animate||backgrounded||settingsBridge.values.reduceMotion||paper.width!==snapshotSize.width||paper.height!==snapshotSize.height){action();pageSurface.opacity=1;pageSurface.y=0;clearPageSnapshot();return}
        oldPage.opacity=1;action();pageSurface.opacity=.001;pageSurface.y=-7;pageOut.restart();pageIn.restart()
    }
    Connections {target:root;function onFrameSwapped(){if(root.snapshotAwaitingFrame){const revision=root.snapshotRevision;Qt.callLater(function(){root.presentPageSnapshot(revision,true)})}}}
    function requestQuit() { exitRequested=true; root.close() }
    function setMiniGeometry(b){uiIntegration.setWindowGeometry(b[0],b[1],b[2],b[3])}
    function settleMiniLayout(){
        narrowMotion.stop()
        narrowProgress=narrowLayout?1:0
        playerBar.settleMiniLayout()
    }
    function animateMiniGeometry(from,to,entering){
        miniMotionFrom=from;miniMotionTo=to
        miniTween.from=entering?0:1;miniTween.to=entering?1:0
        miniTween.duration=settingsBridge.values.reduceMotion||skipMiniAnimation?0:300
        miniMotion.restart()
    }
    onMiniTransitionProgressChanged:{
        if(!miniSwitching||miniMotionFrom.length!==4)return
        const p=miniMode?miniTransitionProgress:1-miniTransitionProgress,a=miniMotionFrom,b=miniMotionTo
        setMiniGeometry([a[0]+(b[0]-a[0])*p,a[1]+(b[1]-a[1])*p,a[2]+(b[2]-a[2])*p,a[3]+(b[3]-a[3])*p])
    }
    SequentialAnimation {
        id:miniMotion;objectName:"miniGeometryMotion"
        // Present the destination's first scene before native resize starts.
        PauseAnimation {duration:settingsBridge.values.reduceMotion||root.skipMiniAnimation?0:16}
        NumberAnimation {id:miniTween;target:root;property:"miniTransitionProgress";easing.type:Easing.InOutCubic}
        onFinished:root.finishMiniTransition()
    }
    function finishMiniTransition(){
        setMiniGeometry(miniMotionTo)
        if(!miniMode&&miniRestorePlacement){
            if(miniRestoreVisibility===Window.Maximized)showMaximized()
            else if(miniRestoreVisibility===Window.FullScreen)showFullScreen()
            uiIntegration.restoreMiniWindowPlacement()
        }
        miniSwitching=false
        miniFlightParts=[]
        miniShortcutTransition=false
        if(windowResizeActive){
            miniResizeCursor=miniTransitionCursor;miniResizeGeometry=[x,y,width,height]
            Qt.callLater(function(){if(root.windowResizeActive)root.updateWindowResize(root.resizeLatestCursor)})
        }else uiIntegration.endResizeAnchor()
        if(!miniMode){
            windowSave.restart()
            if(appearancePage)appearancePage.retainDraftWhenHidden=false
            if(miniRestoreFocus)miniRestoreFocus.forceActiveFocus()
        }
        if(miniClosePending){
            if(miniMode)Qt.callLater(root.leaveMiniPlayer)
            else{miniClosePending=false;Qt.callLater(root.close)}
        }
    }
    function enterMiniPlayer(){enterMiniFromEdge(0)}
    function enterMiniFromEdge(edge,byShortcut){
        if(miniMode||miniSwitching)return
        if(appearancePage)appearancePage.retainDraftWhenHidden=true
        miniRestoreGeometry=[x,y,width,height];miniRestoreVisibility=visibility;miniRestoreFocus=activeFocusItem
        miniNormalGeometry=miniRestoreGeometry;miniRestorePlacement=true
        miniEnteredByShortcut=!!byShortcut;miniShortcutTransition=!!byShortcut;miniTransitionCursor=resizeLatestCursor
        if(windowResizeActive)resizeMiniPinned=true
        miniSwitching=true;windowSave.stop()
        settleMiniLayout()
        uiIntegration.saveMiniWindowPlacement()
        narrowQueue.close();compactNavigationMenu.close();playerBar.closeTransientPopups()
        playerMotion.stop();coverMotion.stop();playerProgress=lyricsOpen?1:0;coverTravelling=false;metadataTravelling=false
        if(visibility!==Window.Windowed)showNormal()
        miniMode=true
        const b=miniRestoreGeometry
        let targetX=edge&Qt.LeftEdge?b[0]+b[2]-miniWidth:edge&Qt.RightEdge?b[0]:b[0]+(b[2]-miniWidth)/2
        let targetY=edge&Qt.TopEdge?b[1]+b[3]-miniHeight:b[1]
        const saved=settingsBridge.miniWindowPosition()
        if(byShortcut&&saved.x!==undefined&&saved.y!==undefined){
            const p=uiIntegration.clampWindowPosition(saved.x,saved.y,miniWidth,miniHeight);targetX=p.x;targetY=p.y
        }
        animateMiniGeometry([x,y,width,height],[targetX,targetY,miniWidth,miniHeight],true)
    }
    function leaveMiniPlayer(byShortcut){
        if(!miniMode||miniSwitching)return
        miniShortcutTransition=!!byShortcut
        restoreFromMini(miniRestoreGeometry,true)
    }
    function restoreFromMini(geometry,originalPlacement){
        const parts=!settingsBridge.values.reduceMotion&&!skipMiniAnimation&&miniLoader.item?miniLoader.item.transitionParts():[]
        miniElasticReturn.stop();miniTransitionCursor=resizeLatestCursor
        if(originalPlacement&&!windowResizeActive)uiIntegration.endResizeAnchor()
        if(miniLoader.item)miniLoader.item.closeTransientPopups()
        miniNormalGeometry=geometry;miniRestorePlacement=originalPlacement
        const from=[x,y,width,height]
        miniSwitching=true;miniMode=false
        settleMiniLayout()
        const flight=[]
        for(const part of parts){
            let item=part.key==="cover"?(lyricsOpen?nowPage.largeArtwork:playerBar.smallArtwork)
                :part.key==="title"?(lyricsOpen?nowPage.metadataTitle:playerBar.metadataTitle)
                :part.key==="artist"?(lyricsOpen?nowPage.metadataArtist:playerBar.metadataArtist):playerBar.miniTarget(part.key)
            let bounds=part.kind==="text"?textRect(item):imageRect(item)
            if(part.key==="queue"&&!narrowLayout){bounds=imageRect(upcoming);bounds.x+=12;bounds.y+=8;bounds.width=18;bounds.height=18}
            part.to={x:geometry[0]+bounds.x,y:geometry[1]+bounds.y,width:bounds.width,height:bounds.height}
            part.toPixelSize=part.kind==="text"?item.font.pixelSize:0
            part.toFamily=part.kind==="text"?item.font.family:""
            part.toWeight=part.kind==="text"?item.font.weight:0
            part.toPadding=part.kind==="icon"?(part.key==="queue"&&!narrowLayout?0:item.padding):0
            flight.push(part)
        }
        miniFlightParts=flight
        animateMiniGeometry(from,geometry,false)
    }
    function beginWindowResize(edges,cursor){
        if(miniSwitching)return false
        const vertical=!!(edges&(Qt.TopEdge|Qt.BottomEdge)),corner=vertical&&!!(edges&(Qt.LeftEdge|Qt.RightEdge))
        if(!miniMode&&!corner&&!(narrowLayout&&vertical))return false
        miniElasticReturn.stop()
        uiIntegration.beginResizeAnchor(edges)
        resizeMiniPinned=miniMode
        if(miniLoader.item)miniLoader.item.closeTransientPopups()
        miniResizeEdges=edges;miniResizeCursor=cursor;resizeLatestCursor=cursor;miniResizeGeometry=[x,y,width,height];windowResizeActive=true
        resizePointerInset=Qt.point(cursor.x-(edges&Qt.RightEdge?x+width:x),cursor.y-(edges&Qt.BottomEdge?y+height:y))
        resizeFixedCorner=Qt.point(edges&Qt.LeftEdge?x+width:edges&Qt.RightEdge?x:x+width/2,edges&Qt.TopEdge?y+height:y)
        return true
    }
    function updateWindowResize(cursor){
        if(!windowResizeActive)return
        resizeLatestCursor=cursor
        if(miniSwitching)return
        const b=miniResizeGeometry,edge=miniResizeEdges,dx=cursor.x-miniResizeCursor.x,dy=cursor.y-miniResizeCursor.y
        const left=!!(edge&Qt.LeftEdge),top=!!(edge&Qt.TopEdge)
        if(!miniMode){
            const right=!!(edge&Qt.RightEdge)
            const requestedWidth=b[2]+(left?-dx:right?dx:0),requestedHeight=b[3]+(top?-dy:dy)
            const w=Math.max(414,Math.round(requestedWidth)),h=Math.max(540,Math.round(requestedHeight))
            setMiniGeometry([left?b[0]+b[2]-w:b[0],top?b[1]+b[3]-h:b[1],w,h])
            const inwardX=left?dx:right?-dx:0
            if(requestedHeight<=476&&(w<547||inwardX>=64))enterMiniFromEdge(edge)
            return
        }
        const horizontal=!!(edge&(Qt.LeftEdge|Qt.RightEdge)),vertical=!!(edge&(Qt.TopEdge|Qt.BottomEdge))
        const requestedWidth=horizontal?Math.round((cursor.x-resizePointerInset.x-resizeFixedCorner.x)*(left?-1:1)):414
        const requestedHeight=vertical?Math.round((cursor.y-resizePointerInset.y-resizeFixedCorner.y)*(top?-1:1)):540
        if(requestedWidth>=(vertical?414:560)&&requestedHeight>=540){
            const w=requestedWidth,h=requestedHeight
            const targetX=left?resizeFixedCorner.x-w:horizontal?resizeFixedCorner.x:resizeFixedCorner.x-w/2
            const targetY=top?resizeFixedCorner.y-h:resizeFixedCorner.y
            restoreFromMini([targetX,targetY,w,h],false)
            return
        }
        const outwardX=horizontal?Math.max(0,requestedWidth-miniWidth):0,outwardY=vertical?Math.max(0,requestedHeight-miniHeight):0
        const w=miniWidth+Math.round(24*outwardX/(outwardX+80)),h=miniHeight+Math.round(14*outwardY/(outwardY+80))
        setMiniGeometry([left?resizeFixedCorner.x-w:horizontal?resizeFixedCorner.x:resizeFixedCorner.x-w/2,top?resizeFixedCorner.y-h:resizeFixedCorner.y,w,h])
    }
    function endWindowResize(){
        if(!windowResizeActive)return
        windowResizeActive=false
        if(miniSwitching)return
        if(!miniMode){uiIntegration.endResizeAnchor();return}
        miniReturnFrom=[x,y,width,height]
        const edge=miniResizeEdges
        miniReturnTo=[edge&Qt.LeftEdge?resizeFixedCorner.x-miniWidth:edge&Qt.RightEdge?resizeFixedCorner.x:resizeFixedCorner.x-miniWidth/2,edge&Qt.TopEdge?resizeFixedCorner.y-miniHeight:resizeFixedCorner.y,miniWidth,miniHeight]
        miniElasticReturn.restart()
    }
    onMiniReturnProgressChanged:{
        if(miniReturnFrom.length!==4)return
        const a=miniReturnFrom,b=miniReturnTo,p=miniReturnProgress
        setMiniGeometry([a[0]+(b[0]-a[0])*p,a[1]+(b[1]-a[1])*p,a[2]+(b[2]-a[2])*p,a[3]+(b[3]-a[3])*p])
    }
    NumberAnimation {
        id:miniElasticReturn;target:root;property:"miniReturnProgress";from:0;to:1
        duration:settingsBridge.values.reduceMotion?0:180;easing.type:Easing.OutCubic
        onFinished:{root.setMiniGeometry(root.miniReturnTo);uiIntegration.endResizeAnchor()}
    }
    function toggleMiniPlayer(){
        if(miniSwitching)return
        if(miniMode){if(miniEnteredByShortcut)settingsBridge.saveMiniWindowPosition(x,y);leaveMiniPlayer(true)}
        else enterMiniFromEdge(0,true)
    }
    readonly property var sidebarSections: settingsBridge.values.sidebarSections.filter(row => (row.visible || row.id === 3&&settingsBridge.values.settingsEntryMode===0)&&!(row.id===3&&settingsBridge.values.settingsEntryMode===1)&&!(row.id===6&&settingsBridge.values.historyEntryMode===1))
    readonly property var sectionNames: ["本地音乐","发现","个人","设置","我喜欢","歌单","历史记录"]
    readonly property var sectionMarkers: ["music","mark-compass","person","settings","heart","playlist","history"]
    function sectionSelected(id) {
        return id===6?appBridge.page===5:id===5 ? appBridge.page===1 : id===4 ? appBridge.page===0&&libraryBridge.playlistId==="-1" : id===0 ? appBridge.page===0&&libraryBridge.playlistId==="0" : id===1 ? appBridge.page===4 : appBridge.page===id
    }
    function navigateOriginal(index) {
        const same = index===6?appBridge.page===5:index === 0 ? appBridge.page === 0 && libraryBridge.playlistId === "0"
                   : index === 4 ? appBridge.page === 0 && libraryBridge.playlistId === "-1"
                   : index===5 ? appBridge.page===1 : index===1 ? appBridge.page===4 : appBridge.page === index && (index !== 3 || settingsSection === "settings")
        if (same) { if (lyricsOpen) animatePlayer(false); return }
        changePage(function() {
            settingsSection="settings"
            if (index===0) { libraryBridge.openPlaylist("0"); appBridge.navigate(0) }
            else if(index===5) appBridge.navigate(1)
            else if(index===1) appBridge.navigate(4)
            else if(index===2) { libraryBridge.openPlaylist("-2"); appBridge.navigate(2) }
            else if(index===3) appBridge.navigate(3)
            else if(index===6) appBridge.navigate(5)
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
        ensureLyricsWorkspace().open()
    }
    readonly property url playerPreviewBackdropUrl: {
        if(root.miniMode || !settingsBridge.values.playerCoverBackground)return ""
        const thumbnail=String(onlineArtwork.thumbnailCover)
        const file=thumbnail.length?thumbnail:String(playbackBridge.coverUrl)
        if(!file.length || file.startsWith("http"))return ""
        return "image://relaxer/blur?file="+encodeURIComponent(file)+"&resolution=96&seed="+encodeURIComponent(playbackBridge.title)+"&blur="+Number(settingsBridge.palette.blurExpanded||0)
    }
    Image {id:playerBackdropWarmup;objectName:"playerBackdropWarmup";visible:false;source:root.playerPreviewBackdropUrl;sourceSize.width:96;sourceSize.height:96;asynchronous:true}
    function backgroundUrl(file,blur){return root.miniMode&&!root.miniSwitching ? "" : String(file||"").length?"image://relaxer/background?file="+encodeURIComponent(String(file))+"&blur="+Number(blur||0):""}
    function applySettings() { Theme.fontScale=Number(settingsBridge.values.fontSize||15)/15;Theme.fontFamily=root.restFontFamily; if(settingsBridge.values.reduceMotion)relocateUpcoming(lyricsOpen); Theme.dark=settingsBridge.palette.dark; Theme.custom=settingsBridge.palette.custom ? settingsBridge.palette : ({}); Theme.customHighlights=!!settingsBridge.palette.custom }
    Component.onCompleted: { initialVideoPreviewId=playbackBridge.video&&!playbackBridge.playing?playbackBridge.trackId:"";width=settingsBridge.windowBounds.width;height=settingsBridge.windowBounds.height;applySettings() }
    Connections {
        target: lyricsSearchBridge
        function onLocalImportRequested(key) {
            if (!root.workspaceVisible || lyricsSearchBridge.targetTrackKey !== key) root.openLyricsTools([key], false)
            if (root.workspaceVisible && lyricsSearchBridge.targetTrackKey === key) lyricsWorkspace.beginLocalImport()
        }
    }
    Connections { target: settingsBridge; function onSettingsChanged(){root.applySettings()} }
    Connections {target:uiIntegration;function onMiniPlayerToggleRequested(){root.toggleMiniPlayer()}}
    Connections { target: importBridge; function onReadyToClose(){if(root.awaitingImportClose){root.allowClose=true;Qt.callLater(root.close)}} }
    Connections { target: appBridge; function onPageChanged(){if(root.lyricsOpen)root.animatePlayer(false)} }
    Connections { target: playbackBridge; function onTransitionStarted(direction){if(nowPage)nowPage.trackDirection=direction} }
    Connections {
        target: upcomingModel
        function onAboutToActivate(row,key,cover) {
            if(root.miniMode||!root.lyricsOpen||settingsBridge.values.reduceMotion)return
            const origin=upcoming.coverItem(row)
            if(!origin)return
            root.coverFrom=root.artworkRect(origin);root.coverTo=root.artworkRect(nowPage.largeArtwork)
            root.flightFile=cover;root.flightSeed=key;root.coverProgress=0;root.coverTravelling=true
            coverMotion.duration=460;coverMotion.restart()
        }
    }
    Shortcut { sequence: "Escape"; enabled: !root.miniMode&&!root.workspaceVisible && !root.popupOpen; onActivated: {if(root.bulkActive)root.clearBulk();else if(root.fullScreenMode)root.toggleFullscreen();else if(root.lyricsOpen)root.animatePlayer(false);else upcoming.panelExpanded=false} }
    Shortcut {sequence:"Ctrl+O";onActivated:{root.leaveMiniPlayer();root.ensureImportDialog().chooseFiles()}}
    Shortcut {sequence:"Ctrl+Shift+O";onActivated:{root.leaveMiniPlayer();root.ensureImportDialog().chooseFolder()}}
    Shortcut {sequence:"Ctrl+F";enabled:!root.workspaceVisible;onActivated:{root.leaveMiniPlayer();root.navigateOriginal(0);searchFocus.restart()}}
    Timer{id:searchFocus;interval:260;onTriggered:localPage.focusSearch()}
    onClosing: close => {
        if(root.miniMode||root.miniSwitching){close.accepted=false;root.miniClosePending=true;if(root.miniMode)root.leaveMiniPlayer();return}
        feedbackBridge.setClosing(true)
        if(playbackBridge.nonInteractiveExit()){
            playbackBridge.requestExit(1)
            if(importBridge.unfinished){close.accepted=false;root.awaitingImportClose=true;if(!importBridge.closing)importBridge.beginShutdown()}
            return
        }
        if (settingsBridge.appearanceDirty) {
            close.accepted=false
            pendingAppearanceClose=true
            pendingPageAction=null
            ensureAppearancePage().confirmLeave()
            feedbackBridge.setClosing(false)
            return
        }
        if (!allowClose && (importBridge.busy || translationBridge.unfinished || lyricsSearchBridge.busy)) { close.accepted=false; closeConfirm.open() }
        else if(!allowClose && !exitRequested && settingsBridge.values.closeToTray && uiIntegration.trayAvailable){close.accepted=false;root.hide();uiIntegration.showTray()}
        else if(!playbackBridge.requestExit()){close.accepted=false;exitSaveFailure.open()}
        else if(importBridge.unfinished){close.accepted=false;root.awaitingImportClose=true;if(!importBridge.closing)importBridge.beginShutdown()}
        if(!close.accepted)feedbackBridge.setClosing(false)
        else lyricsSearchBridge.cancelInstrumentalBatch()
    }
    ConfirmDialog {
        id:exitSaveFailure;parent:root.popupLayer;objectName:"exitSaveFailureDialog";anchors.centerIn:parent
        width:Math.min(520,root.width-40);modal:true;closePolicy:Popup.NoAutoClose;title:"退出前保存失败"
        contentItem:Label{text:"播放状态未能保存。请选择后续操作。\n"+playbackBridge.exitError;wrapMode:Text.Wrap}
        footer:DialogButtonBox {
            background: null
            ShellButton {objectName:"exitSaveRetry";text:"重试";onClicked:{exitSaveFailure.close();root.close()}}
            ShellButton {objectName:"exitSaveAnyway";text:"仍然退出";onClicked:{playbackBridge.requestExit(1);exitSaveFailure.close();root.close()}}
            ShellButton {objectName:"exitSaveCancel";text:"取消退出";onClicked:{playbackBridge.requestExit(2);root.allowClose=false;root.exitRequested=false;root.awaitingImportClose=false;exitSaveFailure.close()}}
        }
    }
    onWidthChanged: windowSave.restart()
    onHeightChanged: windowSave.restart()
    Timer { id: windowSave; interval: 350; onTriggered: if(!root.miniMode&&!root.miniSwitching&&root.visibility!==Window.FullScreen && root.visibility!==Window.Minimized)settingsBridge.saveWindowState(root.width,root.height,root.visibility===Window.Maximized) }
    // Queue only task/page side effects. Start the layout animation while the
    // resize is still requesting a frame, including on the threaded renderer.
    onNarrowLayoutChanged: {
        narrowQueue.close();compactNavigationMenu.close();narrowMotion.stop()
        if(narrowLayout&&workspaceVisible&&!miniMode&&!miniSwitching)narrowWorkspaceExit.restart()
        if(widthSwitchActive)return
        if(miniSwitching||miniMode||settingsBridge.values.reduceMotion)narrowProgress=narrowLayout?1:0
        else{narrowMotion.from=narrowProgress;narrowMotion.to=narrowLayout?1:0;narrowMotion.restart()}
    }
    Timer { id:narrowWorkspaceExit;interval:0;onTriggered:root.closeNarrowWorkspace() }
    function closeNarrowWorkspace() {
        if(narrowLayout&&workspaceVisible&&!miniMode&&!miniSwitching){
            lyricsSearchBridge.stop();lyricsSearchBridge.cancelInstrumentalBatch();lyricsSearchBridge.cancelFileOffset()
            translationBridge.cancelAll()
            if(batchInstrumentalLoader.item)batchInstrumentalLoader.item.close()
            lyricsWorkspace.closeForNarrow()
            root.clearBulk();libraryBridge.openPlaylist("0");appBridge.navigate(0)
        }
    }
    Loader {
        id:miniLoader;anchors.fill:parent;z:200;active:root.miniMode||root.miniSwitching
        opacity:root.miniFlightActive&&root.miniTransitionProgress<1?0:root.miniTransitionProgress;enabled:root.miniMode&&!root.miniSwitching
        sourceComponent:MiniPlayer {
            modeTransitionActive:root.miniSwitching
            onRestoreRequested:Qt.callLater(root.leaveMiniPlayer)
        }
        onLoaded:item.forceActiveFocus()
    }
    Item {
        id:normalShell;objectName:"normalShell"
        width:root.miniMode||root.miniSwitching?root.miniNormalGeometry[2]:Math.max(414,root.width)
        height:root.miniMode||root.miniSwitching?root.miniNormalGeometry[3]:root.height
        visible:!root.miniMode||root.miniSwitching;enabled:!root.miniMode&&!root.miniSwitching
        // A tiny initial opacity prepares the hidden destination's images and
        // nodes in the warm-up frame, behind the still-visible mini player.
        opacity:root.miniSwitching&&!root.miniMode&&root.miniTransitionProgress===1?.001:1-root.miniTransitionProgress
        // Geometry stays fixed throughout the flight; only its GPU transform
        // changes. A new full-window frozen texture was unnecessary here.
        transform:Scale {xScale:root.miniSwitching&&root.miniMode?root.width/normalShell.width:1;yScale:root.miniSwitching&&root.miniMode?root.height/normalShell.height:1}
        // Keep an opaque background beneath every normal-view frame.
        Rectangle {anchors.fill:parent;color:Theme.background}
    Item {
        id: contentHost
        visible: !root.workspaceOpened
        objectName: "contentHost"
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        anchors.bottom: importPanel.visible ? importPanel.top : root.bulkActive ? bulkBar.top : playerBar.top
        Rectangle {
            id: sidebar; objectName: "sidebar"; width:root.sidebarBaseWidth*(1-root.narrowProgress); opacity:1-root.narrowProgress; enabled:!root.narrowLayout&&!root.playerSettled; height: parent.height; color: Theme.panel;clip:true
            Item {id:sidebarBackdrop;anchors.fill:parent
                Rectangle {anchors.fill:parent;color:Theme.panel}
            Image { objectName:"sidebarBackground"; x:0;y:0;width:settingsBridge.palette.shareLR?normalShell.width:parent.width;height:settingsBridge.palette.shareLR&&settingsBridge.palette.shareFooter?normalShell.height:parent.height;source:root.backgroundUrl(settingsBridge.palette.leftImage,settingsBridge.palette.blurLeft); fillMode: Image.PreserveAspectCrop; asynchronous: true; visible: source.toString().length>0 }
            }
            Binding {target:Theme;property:"sidebarBackdrop";value:sidebarBackdrop}
            ColumnLayout {
                x:18; y:27; width:parent.width-36; height:Math.max(0,parent.height-y); spacing:14
                Item {
                    Layout.fillWidth: true; Layout.preferredHeight: 32
                    Item {
                        id:sidebarBrand;objectName:"sidebarBrand"
                        width:(41+brandCaption.implicitWidth)*(1-root.sidebarCondenseProgress)+30*root.sidebarCondenseProgress
                        height:Math.max(32,brandCaption.implicitHeight)
                        scale:Math.min(1,Math.max(0,parent.width)/width);transformOrigin:Item.TopLeft
                        x:(parent.width-width*scale)/2;y:(parent.height-height*scale)/2
                        AbstractButton {
                            id:sidebarLogo;objectName:"sidebarLogo";x:0;width:30;height:30;padding:0;hoverEnabled:true;enabled:!root.lyricsOpen&&!root.playlistOrderEditing
                            text:root.sidebarCondensed?"展开侧栏":"收起侧栏";Accessible.name:text
                            HintBubble { visible:sidebarLogo.hovered; text:sidebarLogo.text }
                            background:null
                            contentItem:Image {source:"qrc:/icon.svg";fillMode:Image.PreserveAspectFit;smooth:true;mipmap:true;sourceSize:Qt.size(Math.ceil(sidebarLogo.width*Math.max(2,root.devicePixelRatio)),Math.ceil(sidebarLogo.height*Math.max(2,root.devicePixelRatio)))}
                            onClicked:root.sidebarCondensed=!root.sidebarCondensed
                        }
                        UiText {id:brandCaption;objectName:"sidebarBrandCaption";opacity:1-root.sidebarCondenseProgress;visible:opacity>0;x:41;width:implicitWidth;anchors.verticalCenter:parent.verticalCenter; text:"Relaxer"; basePixelSize:23; font.weight:Font.DemiBold; color:Theme.text }
                        MouseArea {objectName:"brandFallbackArea";anchors.fill:parent;acceptedButtons:Qt.RightButton;onClicked:mouse=>{if(root.sidebarSections.length===0)brandMenu.popup(sidebarBrand,mouse.x,mouse.y)}}
                        MouseArea { x:sidebarLogo.x+sidebarLogo.width; width:Math.max(0,parent.width-x);height:parent.height;onPressed:root.startSystemMove() }
                    }
                }
                Item { Layout.preferredHeight: 2 }
                ListView {
                    id: sidebarLists;objectName:"playlistNavigation"; Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:0
                    currentIndex:-1
                    Component.onCompleted:Qt.callLater(function(){sidebarLists.forceLayout();sidebarLists.positionViewAtBeginning()})
                    SmoothWheelHandler { flickable: sidebarLists }
                    headerPositioning:ListView.InlineHeader
                    header:Column {
                        width:sidebarLists.width;spacing:14;bottomPadding:14
                        Item {
                            width:parent.width;height:26
                            visible:root.sidebarSections.length>0
                            RowLayout {anchors.fill:parent;spacing:4;opacity:1-root.sidebarCondenseProgress;visible:opacity>0
                                UiText {id:libraryCaption;Layout.fillWidth:true;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter;text:"音乐资料库";color:Theme.text;basePixelSize:14;font.weight:Font.DemiBold}
                                IconButton {objectName:"sidebarHistoryButton";visible:settingsBridge.values.historyEntryMode===1;implicitWidth:28;implicitHeight:28;padding:4;symbol:"history";tip:"历史记录";showTooltip:true;onClicked:root.navigateOriginal(6)}
                                IconButton {objectName:"sidebarSettingsButton";visible:settingsBridge.values.settingsEntryMode===1;implicitWidth:28;implicitHeight:28;padding:4;symbol:"settings";tip:"设置";showTooltip:true;onClicked:root.navigateOriginal(3)}
                            }
                            Rectangle {objectName:"librarySectionDivider";visible:opacity>0;anchors.centerIn:parent;width:20;height:2;radius:1;color:Theme.secondary;opacity:.5*root.sidebarCondenseProgress}
                        }
                        Column {
                            width:parent.width;spacing:root.sidebarRowSpacing
                            Repeater {
                                model:root.sidebarSections
                                NavigationButton {
                                    required property int index; required property var modelData
                                    objectName:"navigation"+modelData.id; width:parent.width;height:root.sidebarRowHeight; text:root.sectionNames[modelData.id];compact:root.sidebarCondensed
                                    marker:root.sectionMarkers[modelData.id]
                                    selected:!root.lyricsOpen && root.sectionSelected(modelData.id)
                                    onClicked:root.navigateOriginal(modelData.id)
                                }
                            }
                        }
                        Item {
                            width:parent.width;height:28
                            Rectangle {objectName:"playlistSectionDivider";visible:opacity>0;anchors.centerIn:parent;width:20;height:2;radius:1;color:Theme.secondary;opacity:.5*root.sidebarCondenseProgress}
                            RowLayout {
                                opacity:1-root.sidebarCondenseProgress;visible:opacity>0;enabled:!root.sidebarCondensed;anchors.fill:parent;spacing:4
                                UiText { text:"我的歌单"; color:Theme.text; basePixelSize: 14;font.weight:Font.DemiBold; Layout.fillWidth:true;Layout.fillHeight:true;verticalAlignment:Text.AlignVCenter }
                                IconButton {
                                    objectName:root.playlistOrderEditing?"savePlaylistOrder":"sidebarPlaylistOptions";implicitWidth:28;implicitHeight:28;padding:4
                                    text:root.playlistOrderEditing?"保存排序":"歌单管理";showTooltip:true
                                    contentItem:Item {
                                        Symbol{anchors.fill:parent;name:"more";opacity:1-root.playlistOrderProgress;scale:1-.2*root.playlistOrderProgress}
                                        Symbol{anchors.fill:parent;name:"check";opacity:root.playlistOrderProgress;scale:.8+.2*root.playlistOrderProgress}
                                    }
                                    onClicked:if(root.playlistOrderEditing)root.savePlaylistOrder();else playlistNavigationOptions.popup(this,0,height)
                                }
                                IconButton {
                                    objectName:root.playlistOrderEditing?"cancelPlaylistOrder":"sidebarCreatePlaylist";implicitWidth:28;implicitHeight:28;padding:4
                                    text:root.playlistOrderEditing?"取消排序":"创建歌单";showTooltip:true
                                    contentItem:Symbol {objectName:"sidebarCreateRotation";name:"plus";rotation:45*root.playlistOrderProgress}
                                    onClicked:if(root.playlistOrderEditing)root.cancelPlaylistOrder();else root.ensurePlaylistEditor().create()
                                }
                            }
                        }
                    }
                    model:root.playlistOrderDraftActive?playlistOrderDraft:playlistModel;clip:true;spacing:root.sidebarRowSpacing
                    move:Transition {NumberAnimation{properties:"y";duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.InOutCubic}}
                    moveDisplaced:Transition {NumberAnimation{properties:"y";duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.InOutCubic}}
                    ScrollBar.vertical:ScrollBar {
                        objectName:"sidebarScrollBar";parent:sidebar;width:10;z:2
                        x:sidebar.width-width-4;y:sidebarLists.parent.y+sidebarLists.y;height:sidebarLists.height
                    }
                    delegate:PlaylistNavigationRow {
                        objectName:"sidebarPlaylist-"+playlistKey
                        width:sidebarLists.width;height:root.sidebarRowHeight;compact:root.sidebarCondensed
                        listView:sidebarLists;orderModel:playlistOrderDraft;sorting:root.playlistOrderEditing
                        selected:!root.lyricsOpen&&appBridge.page===0&&libraryBridge.playlistId===playlistKey
                        onActivated:root.openPlaylist(playlistKey)
                    }
                }
            }
        }
        MouseArea {
            id:sidebarResize;objectName:"sidebarResizeHandle"
            x:sidebar.width-width/2;y:0;width:8;height:parent.height;z:4
            enabled:sidebar.enabled&&!root.sidebarCondensed&&!root.lyricsOpen
            visible:enabled;hoverEnabled:true;cursorShape:Qt.SplitHCursor;acceptedButtons:Qt.LeftButton
            property real startX:0
            property real startWidth:205
            Accessible.name:"调整侧栏宽度"
            onPressed:mouse=>{startX=mapToGlobal(mouse.x,mouse.y).x;startWidth=root.sidebarExpandedWidth;root.sidebarDragWidth=startWidth}
            onPositionChanged:mouse=>{if(pressed)root.sidebarDragWidth=Math.round(Math.max(160,Math.min(root.sidebarMaxWidth,startWidth+mapToGlobal(mouse.x,mouse.y).x-startX)))}
            onReleased:{settingsBridge.setPreference("sidebarWidth",Math.round(root.sidebarExpandedWidth));root.sidebarDragWidth=-1}
            onCanceled:root.sidebarDragWidth=-1
        }
        Item {
            id: paper; objectName:"paperSurface"; x:sidebar.width;width:parent.width-x;height:parent.height;clip:true;enabled:!root.playerSettled
            Item {id:pageBackdrop;anchors.fill:parent
            Rectangle{anchors.fill:parent;color:Theme.background}
            Image{objectName:"pageBackground";x:settingsBridge.palette.shareLR||settingsBridge.palette.shareFooter?-paper.x:0;y:0;width:settingsBridge.palette.shareLR||settingsBridge.palette.shareFooter?normalShell.width:parent.width;height:settingsBridge.palette.shareFooter?normalShell.height:parent.height;source:root.backgroundUrl(settingsBridge.palette.rightImage,settingsBridge.palette.blurRight);fillMode:Image.PreserveAspectCrop;asynchronous:true;visible:source.toString().length>0}
            }
            Binding {target:Theme;property:"pageBackdrop";value:pageBackdrop}
            Item {
                id: pageSurface;objectName:"pageSurface";width:parent.width;height:parent.height
                StackLayout {
                    id: pages;objectName:"pageStack";anchors.fill:parent;anchors.topMargin:Math.max(0,Theme.pageContentOffset-(32-root.captionHeight));currentIndex:appBridge.page
                    LocalPage { id:localPage;backdropSource:paper;onBulkChanged:(active,keys)=>root.updateBulk(localPage,active,keys);onLyricsRequested:(keys,missing)=>root.openLyricsTools(keys,missing);onImportRequested:anchor=>root.ensureImportDialog().openMenu(anchor);onEditRequested:(key,name)=>root.ensurePlaylistEditor().edit(key,name) }
                    DeferredPage {
                        id: playlistsLoader
                        requested: appBridge.page === 1
                        sourceComponent: PlaylistsPage { onOpenRequested:key=>root.openPlaylist(key);onEditRequested:(key,name)=>root.ensurePlaylistEditor().edit(key,name);onCreateRequested:root.ensurePlaylistEditor().create() }
                    }
                    DeferredPage {
                        id: profileLoader
                        requested: appBridge.page === 2
                        sourceComponent: ProfilePage { id:profilePage;onBulkChanged:(active,keys)=>root.updateBulk(profilePage,active,keys);onLyricsRequested:keys=>root.openLyricsTools(keys,false) }
                    }
                    Item {
                        StackLayout {
                            anchors.fill:parent
                            currentIndex:root.settingsSection==="ai"?1:root.settingsSection==="appearance"?2:root.settingsSection==="log"?3:root.settingsSection==="platforms"?4:0
                            DeferredPage {
                                id: settingsLoader;recyclable:true
                                requested: appBridge.page === 3 && root.settingsSection === "settings"
                                sourceComponent: Item {
                                    function savePageState(){return settingsPage.savePageState()}
                                    function restorePageState(state){settingsPage.restorePageState(state)}
                                    function canUnloadPage(){return settingsPage.canUnloadPage()}
                                    SettingsPage { id:settingsPage;onVersionLogRequested:root.changePage(function(){root.settingsSection="log"});anchors.fill:parent;anchors.leftMargin:Theme.pageMargin;anchors.rightMargin:Theme.pageMargin;anchors.topMargin:28;anchors.bottomMargin:0;onAiRequested:root.changePage(function(){root.settingsSection="ai"});onPlatformsRequested:root.changePage(function(){root.settingsSection="platforms"});onAppearanceRequested:{settingsBridge.beginAppearance();root.changePage(function(){root.settingsSection="appearance"})} } }
                            }
                            DeferredPage {
                                id: aiLoader;recyclable:true
                                requested: appBridge.page === 3 && root.settingsSection === "ai"
                                sourceComponent: ScrollView {
                                    id:aiScroll;objectName:"aiSettingsScroll";contentWidth:availableWidth;clip:true
                                    function canUnloadPage(){return aiPage.canUnloadPage()}
                                    function savePageState(){return {subpage:aiPage.subpage,y:contentItem?contentItem.contentY:0}}
                                    function restorePageState(state){aiPage.subpage=state.subpage||"home";Qt.callLater(function(){if(aiScroll.contentItem)aiScroll.contentItem.contentY=Math.max(0,Math.min(Number(state.y||0),aiScroll.contentItem.contentHeight-aiScroll.contentItem.height))})}
                                    SmoothWheelHandler { flickable: aiScroll.contentItem as Flickable }
                                    leftPadding:34;rightPadding:34;topPadding:28;bottomPadding:16
                                    AiPage {
                                        id:aiPage;width:aiScroll.availableWidth
                                        onBackRequested:root.changePage(function(){root.settingsSection="settings"})
                                        onSubpageRequested:section=>root.changePage(function(){aiPage.subpage=section;aiScroll.contentItem.contentY=0})
                                    }
                                }
                            }
                            DeferredPage {
                                id: appearanceLoader
                                requested: appBridge.page === 3 && root.settingsSection === "appearance"
                                sourceComponent: AppearancePage {
                                    id:appearancePage
                                    onBackRequested:{
                                        if(root.pendingAppearanceClose){
                                            root.pendingAppearanceClose=false
                                            // Resume through the existing task, playback-save and import guards.
                                            Qt.callLater(root.requestQuit)
                                            return
                                        }
                                        const action=root.pendingPageAction;root.pendingPageAction=null
                                        root.changePage(function(){root.settingsSection="settings";if(action)action()})
                                    }
                                    onLeaveRejected:{
                                        root.pendingPageAction=null
                                        if(root.pendingAppearanceClose){root.pendingAppearanceClose=false;root.exitRequested=false}
                                    }
                                }
                            }
                            DeferredPage {
                                id: logLoader;recyclable:true
                                requested: appBridge.page === 3 && root.settingsSection === "log"
                                sourceComponent: VersionLogPage{onBackRequested:root.changePage(function(){root.settingsSection="settings"})}
                            }
                            DeferredPage {
                                requested:appBridge.page===3&&root.settingsSection==="platforms"
                                sourceComponent:ScrollView {
                                    id:platformScroll;objectName:"platformSettingsScroll";contentWidth:availableWidth;clip:true
                                    leftPadding:Theme.pageMargin;rightPadding:Theme.pageMargin;topPadding:28;bottomPadding:16
                                    SmoothWheelHandler {flickable:platformScroll.contentItem as Flickable}
                                    PlatformAccounts {width:platformScroll.availableWidth;onBackRequested:root.changePage(function(){root.settingsSection="settings"})}
                                }
                            }
                        }
                    }
                    DeferredPage {
                        requested:appBridge.page===4
                        sourceComponent:DiscoverPage {backdropSource:paper}
                    }
                    DeferredPage {requested:appBridge.page===5;recyclable:true;sourceComponent:HistoryPage {backdropSource:paper}}

                }
            }
            Image { id:oldPage;anchors.fill:parent;visible:false;fillMode:Image.Stretch;cache:false;asynchronous:false;onStatusChanged:if(status===Image.Ready)root.warmSnapshot() }
        }
        // Keep the main page painted for the wipe, while stopping input across
        // the whole covered surface from the first opening frame to the last closing frame.
        MouseArea {
            objectName:"expandedPlayerInputBarrier";anchors.fill:parent;visible:root.playerPresented
            acceptedButtons:Qt.AllButtons;hoverEnabled:true
            onWheel:wheel=>{wheel.accepted=true}
        }
        Item {
            id: playerClip;objectName:"playerBackdropClip"
            x:0;y:contentHost.height*(1-root.playerReveal);width:contentHost.width;height:contentHost.height*root.playerReveal
            clip:true;visible:root.playerProgress>0
            Loader {
                id:nowPlayingLoader;active:false;x:0;y:-playerClip.y;width:contentHost.width;height:contentHost.height
                sourceComponent:LyricsPage {
                    objectName:"nowPlayingPage"
                    previewBackdrop:root.playerPreviewBackdropUrl
                    detailsOpacity:root.lyricsOpen ? Math.max(0,Math.min(1,(root.playerProgress-.52)/.48)) : Math.max(0,root.playerProgress)
                    artworkTravelling:root.coverTravelling||root.miniFlightActive;metadataTravelling:root.metadataTravelling||root.miniFlightActive;fullScreen:root.fullScreenMode
                    onCollapseRequested:root.closeLyrics()
                    onFullscreenRequested:root.toggleFullscreen()
                    onTranslationRequested:root.openLyricsTools([playbackBridge.trackId],false)
                }
            }
            // Fading controls must not receive clicks before they reach their
            // final position, or while they are leaving the screen.
            MouseArea {
                objectName:"playerTransitionInputBarrier";anchors.fill:parent;visible:playerMotion.running
                acceptedButtons:Qt.AllButtons
                onWheel:wheel=>{wheel.accepted=true}
            }
        }
    }
    Connections {target:playbackBridge;function onTrackChanged(){if(root.initialVideoPreviewId!==playbackBridge.trackId)root.initialVideoPreviewId="";if(playbackBridge.video)nowPlayingLoader.active=true;root.presentVideoWhenPlaying()}function onPlayingChanged(){root.presentVideoWhenPlaying()}function onCapabilitiesChanged(){root.presentVideoWhenPlaying()}}
    PlayerBar {
        id:playerBar;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:implicitHeight
        popupBackdrop:contentHost
        visible:!root.bulkActive&&!root.workspaceOpened;expanded:root.lyricsOpen;expansion:root.playerProgress;artworkTravelling:root.coverTravelling||root.miniFlightActive;miniControlsTravelling:root.miniFlightActive
        wideLayoutWidth:root.widthSwitchActive?Math.max(root.widthSwitchGeometry[3],root.widthSwitchGeometry[4]):width
        compactLayoutWidth:root.widthSwitchActive?Math.min(root.widthSwitchGeometry[3],root.widthSwitchGeometry[4]):width
        backgroundCanvasHeight: normalShell.height; backgroundOffsetY: playerBar.y
        onExpandRequested:root.animatePlayer(!root.lyricsOpen)
        onNarrowRequested:root.toggleNarrowWindow()
        onQueueRequested: anchor => {
            if(narrowQueue.visible){narrowQueue.close();return}
            const p=anchor.mapToItem(root.contentItem,0,0)
            narrowQueue.x=Math.max(8,Math.min(root.width-narrowQueue.width-8,p.x))-p.x
            narrowQueue.y=Math.max(34,p.y-narrowQueue.height-10)-p.y
            narrowQueue.open()
        }
    }

    Rectangle {
        id:bulkBar;objectName:"bulkBar";anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:bulkActions.implicitHeight+20;visible:root.bulkActive&&!root.workspaceOpened;color:Theme.card;z:3
        ColumnLayout {
            id:bulkActions
            anchors.fill:parent;anchors.leftMargin:22;anchors.rightMargin:22;anchors.topMargin:10;anchors.bottomMargin:10;spacing:4
            Flow {
                Layout.fillWidth:true;  spacing:8
                UiText {objectName:"bulkCount";text:"已选 "+root.bulkKeys.length+" 首歌曲"+(lyricsSearchBridge.batchRunning?" · 剩余 "+lyricsSearchBridge.batchResult.pending:"");color:Theme.text;basePixelSize: 15}
                ShellButton{objectName:"bulkSelectAll";text:"全选";flat:true;onClicked:root.selectBulk(true)}
                ShellButton{objectName:"bulkClear";text:"取消选择";flat:true;onClicked:root.selectBulk(false)}
                ShellButton{objectName:"bulkLyrics";text:"查找歌词";flat:true;enabled:root.bulkKeys.length>0;onClicked:root.openLyricsTools(root.bulkKeys,false)}
                ShellButton{objectName:"bulkInstrumental";text:lyricsSearchBridge.batchRunning?"取消纯音乐处理":"标记纯音乐…";flat:true;enabled:root.bulkKeys.length>0;onClicked:{if(lyricsSearchBridge.batchRunning)lyricsSearchBridge.cancelInstrumentalBatch();else root.ensureBatchInstrumental().review(root.bulkKeys.slice())}}
                ShellButton{objectName:"bulkExit";text:"退出多选";flat:true;onClicked:root.clearBulk()}
            }
            Flow {
                Layout.fillWidth:true;  spacing:8
                ShellButton{objectName:"bulkDelete";text:"删除";enabled:root.bulkKeys.length>0&&root.bulkOwner===localPage&&root.bulkScope==="0";onClicked:{root.bulkActionKeys=root.bulkKeys.slice();bulkDelete.open()}}
                ShellButton{objectName:"bulkUnlike";text:"取消喜欢";enabled:root.bulkKeys.length>0;onClicked:libraryBridge.applyFavorite(root.bulkKeys,false)}
                ShellButton{objectName:"bulkLike";text:"全部喜欢";enabled:root.bulkKeys.length>0;onClicked:libraryBridge.applyFavorite(root.bulkKeys,true)}
                ShellButton{id:bulkAdd;objectName:"bulkAddPlaylist";text:"添加至歌单";enabled:root.bulkKeys.length>0;onClicked:bulkAddMenu.popup(bulkAdd,0,-bulkAddMenu.expandedHeight)}
                ShellButton{objectName:"bulkNewPlaylist";text:"新建歌单并加入";enabled:root.bulkKeys.length>0;onClicked:{root.bulkActionKeys=root.bulkKeys.slice();bulkNew.open()}}
                ShellButton{objectName:"bulkRemovePlaylist";text:"从歌单移除";enabled:root.bulkKeys.length>0&&Number(root.bulkScope)>0;onClicked:libraryBridge.removeTracksFromPlaylist(root.bulkKeys)}
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
    ConfirmDialog {id:bulkNew;objectName:"bulkNewPlaylistDialog";parent:root.popupLayer;anchors.centerIn:parent;title:"新建歌单并加入";width:420;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel;contentItem:EntryField {id:bulkName;objectName:"bulkNewPlaylistName";placeholderText:"歌单名称"} onAccepted:{const id=libraryBridge.createPlaylist(bulkName.text);if(id!=="0")libraryBridge.addTracksToPlaylist(id,root.bulkActionKeys)}}
    Rectangle {
        id:importPanel;objectName:"importPanel";anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:root.bulkActive?bulkBar.top:playerBar.top
        property bool observedTask:false
        property bool completionVisible:false
        property string completionText:""
        readonly property string shownStatus:importBridge.busy?importBridge.status:completionText
        visible:(importBridge.busy||completionVisible)&&!root.workspaceOpened
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
            UiText {id:importStatus;objectName:"importStatusText";Layout.fillWidth:true;text:importPanel.shownStatus;color:Theme.secondary;basePixelSize:14;textFormat:Text.PlainText;wrapMode:Text.Wrap;maximumLineCount:5;elide:Text.ElideRight}
            ProgressBar{objectName:"importProgress";visible:importBridge.busy;Layout.preferredWidth:Math.min(230,root.width*.22);from:0;to:Math.max(1,importBridge.total);value:importBridge.current;indeterminate:importBridge.total===0}
            UiText { objectName:"importProgressCount"; visible:importBridge.busy&&importBridge.total>0; text:importBridge.current+"/"+importBridge.total; color:Theme.text; basePixelSize:14 }
            ShellButton{objectName:"importResultDetails";visible:!importBridge.busy;text:"详情";implicitHeight:28;onClicked:feedbackPanel.openDetails({message:importPanel.completionText,details:importBridge.result.details})}
            ShellButton{visible:importBridge.busy;text:"取消导入";implicitHeight:28;onClicked:importBridge.cancel()}
        }
    }
    MouseArea {
        objectName:"upcomingOutsidePress";parent:root.contentItem;anchors.fill:parent;z:3
        enabled:upcoming.panelExpanded&&!root.narrowLayout&&!root.workspaceOpened&&!root.miniMode&&!root.miniSwitching
        acceptedButtons:Qt.AllButtons
        onPressed:mouse=>{
            if(!upcoming.contains(upcoming.mapFromItem(this,mouse.x,mouse.y)))upcoming.panelExpanded=false
            // Let the same press reach the button or list beneath this observer.
            mouse.accepted=false
        }
    }
    Item {
        objectName:"upcomingWindowClip";anchors.fill:parent;clip:true;z:2;visible:!root.workspaceOpened
        UpcomingPanel {
            id:upcoming;objectName:"nextQueuePanel"
            backdropSource:contentHost
            secondaryBackdrop:playerBar;secondaryOffset:Qt.point(playerBar.x-contentHost.x,playerBar.y-contentHost.y)
            backdropRect:Qt.rect(x,y,width,height)
            expandedPlayer:root.upcomingAtLeft
            readonly property real edgeMargin:root.upcomingAtLeft?8:playerBar.coverMargin
            dockedHeight:Math.max(0,playerBar.height-2*edgeMargin)
            readonly property real inlineDockX:playerBar.x+playerBar.wideVolumeIconX-inlineIconLeft
            readonly property real inlineDockWidth:Math.max(0,playerBar.wideLayoutWidth-playerBar.coverMargin-inlineDockX)
            width:root.upcomingAtLeft?Math.min(implicitWidth,Math.max(0,playerBar.wideCenterX-edgeMargin-2*playerBar.coverMargin))
                :inlinePreview?inlineDockWidth:Math.max(inlineDockWidth,Math.min(implicitWidth,Math.max(0,playerBar.wideRightSpace-24)))
            x:root.upcomingAtLeft?edgeMargin+(playerBar.x+playerBar.queueControlX-edgeMargin)*root.narrowProgress:normalShell.width-playerBar.coverMargin-width+(root.widthSwitchActive?(width+playerBar.coverMargin)*root.narrowProgress:0)
            y:root.upcomingAtLeft
                ?(normalShell.height-edgeMargin-height)*(1-root.narrowProgress)+(playerBar.y+playerBar.queueControlY)*root.narrowProgress+(height+edgeMargin)*root.upcomingTravel
                :normalShell.height-7-height+(height+7)*root.upcomingTravel+(root.widthSwitchActive?0:(height+7)*root.narrowProgress)
            opacity:1-root.narrowProgress
            transform:Scale {
                origin.x:0;origin.y:0
                xScale:root.upcomingAtLeft?1+(36/upcoming.width-1)*root.narrowProgress:1
                yScale:root.upcomingAtLeft?1+(36/upcoming.height-1)*root.narrowProgress:1
            }
            relocating:root.upcomingRelocating||root.widthSwitchActive||root.narrowLayout
            enabled:!relocating&&!root.narrowLayout
            visible:!root.bulkActive
        }
    }
    NumberAnimation {
        id:upcomingOut;target:root;property:"upcomingTravel";to:1;duration:210;easing.type:Easing.OutCubic
        onFinished:{root.upcomingAtLeft=root.lyricsOpen;upcomingIn.from=1;upcomingIn.restart()}
    }
    NumberAnimation {id:upcomingIn;target:root;property:"upcomingTravel";to:0;duration:210;easing.type:Easing.OutCubic;onFinished:root.upcomingRelocating=false}
    Artwork {
        id:flight;objectName:"sharedArtworkFlight";z:4;visible:root.coverTravelling&&!root.workspaceOpened
        file:root.flightFile;seed:root.flightSeed;resolution:768;videoCover:playbackBridge.video
        coverAspect:playbackBridge.video?16/9:1
        x:root.coverFrom.x+(root.coverTo.x-root.coverFrom.x)*root.coverProgress
        y:root.coverFrom.y+(root.coverTo.y-root.coverFrom.y)*root.coverProgress
        width:root.coverFrom.width+(root.coverTo.width-root.coverFrom.width)*root.coverProgress
        height:root.coverFrom.height+(root.coverTo.height-root.coverFrom.height)*root.coverProgress
        MouseArea {anchors.fill:parent;acceptedButtons:Qt.AllButtons;onWheel:wheel=>{wheel.accepted=true}}
    }

    UiText {
        id:titleFlight;objectName:"sharedTitleFlight";z:4;visible:root.metadataTravelling&&!root.workspaceOpened
        // font.pixelSize is integral. Keep glyph layout at the larger endpoint
        // size and interpolate a real-valued transform instead of reshaping it.
        readonly property real flightScale:(root.titleFrom.pixelSize+(root.titleTo.pixelSize-root.titleFrom.pixelSize)*root.coverProgress)/font.pixelSize
        opacity:root.narrowLayout&&!playerBar.metadataTitle.visible?(root.lyricsOpen?root.coverProgress:1-root.coverProgress):1
        text:playbackBridge.hasTrack?playbackBridge.title:"Relaxer";textFormat:Text.PlainText;elide:Text.ElideRight;color:Theme.text
        x:root.titleFrom.x+(root.titleTo.x-root.titleFrom.x)*root.coverProgress
        y:root.titleFrom.y+(root.titleTo.y-root.titleFrom.y)*root.coverProgress
        width:(root.titleFrom.width+(root.titleTo.width-root.titleFrom.width)*root.coverProgress)/flightScale
        clip:true
        height:(root.titleFrom.height+(root.titleTo.height-root.titleFrom.height)*root.coverProgress)/flightScale
        basePixelSize:Math.max(1,Math.ceil(Math.max(root.titleFrom.pixelSize,root.titleTo.pixelSize)))/Theme.fontScale
        font.family:root.titleTo.family;font.weight:root.titleTo.weight
        transform:Scale {xScale:titleFlight.flightScale;yScale:titleFlight.flightScale}
        MouseArea {anchors.fill:parent;acceptedButtons:Qt.AllButtons;onWheel:wheel=>{wheel.accepted=true}}
    }
    UiText {
        id:artistFlight;objectName:"sharedArtistFlight";z:4;visible:root.metadataTravelling&&!root.workspaceOpened
        readonly property real flightScale:(root.artistFrom.pixelSize+(root.artistTo.pixelSize-root.artistFrom.pixelSize)*root.coverProgress)/font.pixelSize
        opacity:root.narrowLayout&&!playerBar.metadataArtist.visible?(root.lyricsOpen?root.coverProgress:1-root.coverProgress):1
        text:playbackBridge.hasTrack?playbackBridge.artist:"选一首歌，开始放松。";textFormat:Text.PlainText;elide:Text.ElideRight;color:Theme.secondary
        x:root.artistFrom.x+(root.artistTo.x-root.artistFrom.x)*root.coverProgress
        y:root.artistFrom.y+(root.artistTo.y-root.artistFrom.y)*root.coverProgress
        width:(root.artistFrom.width+(root.artistTo.width-root.artistFrom.width)*root.coverProgress)/flightScale
        clip:true
        height:(root.artistFrom.height+(root.artistTo.height-root.artistFrom.height)*root.coverProgress)/flightScale
        basePixelSize:Math.max(1,Math.ceil(Math.max(root.artistFrom.pixelSize,root.artistTo.pixelSize)))/Theme.fontScale
        font.family:root.artistTo.family;font.weight:root.artistTo.weight
        transform:Scale {xScale:artistFlight.flightScale;yScale:artistFlight.flightScale}
        MouseArea {anchors.fill:parent;acceptedButtons:Qt.AllButtons;onWheel:wheel=>{wheel.accepted=true}}
    }
    DropArea{anchors.fill:parent;onDropped:drop=>{if(drop.hasUrls&&!importBridge.busy){importBridge.start(drop.urls,Number(libraryBridge.playlistId)>0?libraryBridge.playlistId:"0");drop.acceptProposedAction()}}}
    HoverHandler {parent:root.contentItem;onPointChanged:{const p=point.scenePosition;if(p.x!==root.playerPointer.x||p.y!==root.playerPointer.y){root.playerPointer=p;root.revealPlayerChrome()}}}
    Timer {
        id:playerChromeIdle;interval:3000
        onTriggered:if(playerCollapse.hovered||videoPartsEntry.hovered||localVideoLyricsSearch.hovered||(root.nowPage&&(root.nowPage.partsVisible||root.nowPage.chromeHovered)))restart();else root.playerChromeVisible=false
    }
    IconButton {
        id:playerCollapse;objectName:"compactPlayerCollapse";x:0;y:0;z:8;width:root.captionButtonWidth;height:root.captionHeight;padding:6
        visible:Theme.compact&&root.playerPresented&&!root.miniMode&&!root.miniSwitching
        opacity:root.playerChromeVisible?1:0;enabled:opacity>0&&!playerMotion.running
        symbol:"collapse";tip:"收起播放";showTooltip:true;onClicked:root.closeLyrics()
        Behavior on opacity {NumberAnimation {duration:settingsBridge.values.reduceMotion?0:180}}
    }
    IconButton {
        id:videoPartsEntry;objectName:"compactVideoPartsEntry";x:titleBar.x-width-4;y:0;z:8;width:root.captionButtonWidth;height:root.captionHeight;padding:6
        readonly property var parts:{const key=playbackBridge.trackId;return bilibili.videoParts}
        visible:Theme.compact&&root.playerPresented&&!root.miniMode&&!root.miniSwitching&&bilibili.currentVideo
        opacity:root.playerChromeVisible?1:0;enabled:opacity>0&&!playerMotion.running&&parts.length>1
        symbol:"parts";tip:parts.length?"选集 · P"+bilibili.currentPart+"/"+parts.length:bilibili.partsLoading?"正在读取分P":"暂无可用分P信息";showTooltip:true
        onClicked:root.ensureNowPlaying().toggleParts()
        Behavior on opacity {NumberAnimation {duration:settingsBridge.values.reduceMotion?0:180}}
    }
    IconButton {
        id:localVideoLyricsSearch;objectName:"compactVideoLyricsSearch";x:titleBar.x-width-4;y:0;z:8;width:root.captionButtonWidth;height:root.captionHeight;padding:6
        visible:Theme.compact&&root.playerPresented&&playbackBridge.video&&!playbackBridge.onlineActive&&settingsBridge.values.localVideoMode===0&&!root.miniMode&&!root.miniSwitching
        opacity:root.playerChromeVisible?1:0;enabled:opacity>0;symbol:"search";tip:"查找本地视频歌词";showTooltip:true
        onClicked:root.ensureNowPlaying().translationRequested()
    }
    MouseArea{objectName:"windowDragArea";z:5;x:root.playerPresented?root.captionButtonWidth:root.narrowLayout?54:sidebar.width;y:0;width:Math.max(0,root.width-x-titleBar.width-(videoPartsEntry.visible?videoPartsEntry.width+4:localVideoLyricsSearch.visible?localVideoLyricsSearch.width+4:0));height:root.captionHeight;onPressed:root.startSystemMove();onDoubleClicked:root.visibility===Window.Maximized?root.showNormal():root.showMaximized()}
    Row {
        id:titleBar;objectName:"titleBar";anchors.top:parent.top;anchors.right:parent.right;z:7;height:root.captionHeight
        AbstractButton {
            id:windowPin;objectName:"windowPinButton"
            width:root.captionButtonWidth;height:root.captionHeight;hoverEnabled:true;checkable:true;checked:root.windowPinned
            Accessible.name:root.windowPinned?"取消置顶":"置顶窗口"
            HintBubble {visible:windowPin.hovered;text:windowPin.Accessible.name}
            background:Rectangle {color:windowPin.down?Theme.pressed:windowPin.hovered?Theme.hover:"transparent"}
            contentItem:Item {
                Symbol {
                    objectName:"windowPinIcon";anchors.centerIn:parent;width:18*.8;height:18*.8
                    name:"pin";color:root.windowPinned?Theme.accent:Theme.text
                    rotation:root.windowPinned?0:45
                    Behavior on rotation {NumberAnimation{duration:settingsBridge.values.reduceMotion?0:200;easing.type:Easing.OutCubic}}
                }
            }
            onClicked:root.windowPinned=!root.windowPinned
        }
        Repeater{
            model:["最小化","最大化","关闭"]
            delegate:AbstractButton{
                id: windowControl
                required property int index;required property string modelData
                objectName: "windowControl" + index
                width:root.captionButtonWidth;height:root.captionHeight;hoverEnabled:true
                Accessible.name: index===1 && root.visibility===Window.Maximized ? "还原窗口" : modelData
                HintBubble { visible:windowControl.hovered; text:windowControl.Accessible.name }
                background:Rectangle{color:windowControl.down?(index===2?Qt.darker(Theme.accent,1.15):Theme.pressed):windowControl.hovered?(index===2?Theme.accent:Theme.hover):"transparent"}
                contentItem: Item {
                    Canvas {
                        anchors.centerIn: parent; width:14; height:14;scale:.8
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
    IconButton {
        id:compactNavigation;objectName:"compactNavigation";x:0;y:0;z:7;width:42;height:32;padding:9
        visible:root.narrowProgress>0&&!root.lyricsOpen;opacity:root.narrowProgress;text:"导航"
        contentItem:Item { Repeater{model:3;Rectangle{required property int index;x:(parent.width-width)/2;y:(parent.height-12)/2+index*5;width:16;height:2;radius:1;color:Theme.text}} }
        onClicked:{if(compactNavigationMenu.visible)compactNavigationMenu.close();else compactNavigationMenu.open()}
    }
    RoundMenu {id:brandMenu;objectName:"brandFallbackMenu";parent:root.Overlay.overlay;width:160
        RoundMenuItem {objectName:"fallbackHistory";text:"历史记录";onTriggered:{compactNavigationMenu.close();root.navigateOriginal(6)}}
        RoundMenuItem {objectName:"fallbackSettings";text:"设置";onTriggered:{compactNavigationMenu.close();root.navigateOriginal(3)}}
    }
    Popup {
        id:compactNavigationMenu;objectName:"compactNavigationPanel";parent:compactNavigation;popupType:Popup.Item
        x:8;y:34;width:Math.min(168,root.width-32)
        height:Math.min(compactNavigationContent.implicitHeight+topPadding+bottomPadding,normalShell.height-playerBar.height-52)
        padding:14;modal:false;dim:false
        property real slideProgress:0
        onAboutToShow:Qt.callLater(function(){compactPlaylists.forceLayout();compactPlaylists.positionViewAtBeginning()})
        closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutsideParent
        enter:Transition {NumberAnimation{target:compactNavigationMenu;property:"slideProgress";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:280;easing.type:Easing.OutCubic}}
        exit:Transition {NumberAnimation{target:compactNavigationMenu;property:"slideProgress";to:0;duration:settingsBridge.values.reduceMotion?0:220;easing.type:Easing.InCubic}}
        background:Rectangle {
            radius:14;color:Theme.panel;border.color:Theme.border
            transform:Translate{x:-(compactNavigationMenu.width+8)*(1-compactNavigationMenu.slideProgress)}
        }
        contentItem:ColumnLayout {
            id:compactNavigationContent
            spacing:10
            transform:Translate{x:-(compactNavigationMenu.width+8)*(1-compactNavigationMenu.slideProgress)}
            UiText {text:"Relaxer";basePixelSize:23;font.weight:Font.DemiBold;color:Theme.text;Layout.preferredHeight:34;Layout.fillWidth:true
                MouseArea {anchors.fill:parent;acceptedButtons:Qt.RightButton;onClicked:mouse=>{if(root.sidebarSections.length===0)brandMenu.popup(parent,mouse.x,mouse.y)}}
            }
            ListView {
                id:compactPlaylists;objectName:"compactPlaylistNavigation"
                currentIndex:-1
                Component.onCompleted:Qt.callLater(function(){compactPlaylists.forceLayout();compactPlaylists.positionViewAtBeginning()})
                SmoothWheelHandler { flickable: compactPlaylists }
                Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:0
                Layout.preferredHeight:count*root.sidebarRowHeight+Math.max(0,count-1)*spacing+(headerItem?headerItem.height:0)
                headerPositioning:ListView.InlineHeader
                header:Column {
                    width:compactPlaylists.width;spacing:10;bottomPadding:10
                    RowLayout {width:parent.width;height:28;visible:root.sidebarSections.length>0;spacing:2
                        UiText {text:"音乐资料库";color:Theme.secondary;basePixelSize:13;Layout.fillWidth:true;elide:Text.ElideRight}
                        IconButton {objectName:"compactHistoryButton";visible:settingsBridge.values.historyEntryMode===1;implicitWidth:28;implicitHeight:28;padding:4;symbol:"history";tip:"历史记录";onClicked:{compactNavigationMenu.close();root.navigateOriginal(6)}}
                        IconButton {objectName:"compactSettingsButton";visible:settingsBridge.values.settingsEntryMode===1;implicitWidth:28;implicitHeight:28;padding:4;symbol:"settings";tip:"设置";onClicked:{compactNavigationMenu.close();root.navigateOriginal(3)}}
                    }
                    Column {
                        width:parent.width;spacing:root.sidebarRowSpacing
                        Repeater {
                            model:root.sidebarSections
                            NavigationButton {
                                required property int index;required property var modelData
                                objectName:"compactNavigate-"+modelData.id;width:parent.width;height:root.sidebarRowHeight;text:root.sectionNames[modelData.id]
                                marker:root.sectionMarkers[modelData.id]
                                selected:root.sectionSelected(modelData.id)
                                onClicked:{compactNavigationMenu.close();root.navigateOriginal(modelData.id)}
                            }
                        }
                    }
                    Rectangle{width:parent.width;height:1;color:Theme.border}
                    RowLayout {
                        width:parent.width;height:26;spacing:4
                        UiText {text:"我的歌单";color:Theme.secondary;basePixelSize:13;Layout.fillWidth:true}
                        IconButton {
                            objectName:root.playlistOrderEditing?"compactSavePlaylistOrder":"compactPlaylistOptions";width:26;height:26;padding:5
                            text:root.playlistOrderEditing?"保存排序":"歌单管理";showTooltip:true
                            contentItem:Item {
                                Symbol{anchors.fill:parent;name:"more";opacity:1-root.playlistOrderProgress;scale:1-.2*root.playlistOrderProgress}
                                Symbol{anchors.fill:parent;name:"check";opacity:root.playlistOrderProgress;scale:.8+.2*root.playlistOrderProgress}
                            }
                            onClicked:if(root.playlistOrderEditing)root.savePlaylistOrder();else playlistNavigationOptions.popup(this,0,height)
                        }
                        IconButton {
                            objectName:root.playlistOrderEditing?"compactCancelPlaylistOrder":"compactCreatePlaylist";width:26;height:26;padding:5
                            text:root.playlistOrderEditing?"取消排序":"创建歌单";showTooltip:true
                            contentItem:Symbol {objectName:"compactCreateRotation";name:"plus";rotation:45*root.playlistOrderProgress}
                            onClicked:if(root.playlistOrderEditing)root.cancelPlaylistOrder();else{compactNavigationMenu.close();root.ensurePlaylistEditor().create()}
                        }
                    }
                }
                clip:true;model:root.playlistOrderDraftActive?playlistOrderDraft:playlistModel;spacing:root.sidebarRowSpacing
                move:Transition {NumberAnimation{properties:"y";duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.InOutCubic}}
                moveDisplaced:Transition {NumberAnimation{properties:"y";duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.InOutCubic}}
                ScrollBar.vertical:ScrollBar {
                    objectName:"compactSidebarScrollBar";parent:compactNavigationMenu.background;width:10;z:2
                    x:parent.width-width-4;y:compactNavigationContent.y+compactPlaylists.y;height:compactPlaylists.height
                }
                delegate:PlaylistNavigationRow {
                    objectName:"compactSidebarPlaylist-"+playlistKey
                    width:compactPlaylists.width;height:root.sidebarRowHeight;listView:compactPlaylists;orderModel:playlistOrderDraft;sorting:root.playlistOrderEditing
                    selected:appBridge.page===0&&libraryBridge.playlistId===playlistKey
                    onActivated:{compactNavigationMenu.close();root.openPlaylist(playlistKey)}
                }
            }
        }
    }
    Popup {
        id:narrowQueue;objectName:"narrowQueuePopup";parent:playerBar.queueControl;popupType:Popup.Item
        width:Math.min(440,root.width-24);height:Math.min(42+Math.max(1,Math.min(5,upcomingModel.count))*56,normalShell.height-playerBar.height-44);padding:0
        property real reveal:0
        readonly property point anchorPoint:Qt.point(playerBar.queueControl.width/2-x,playerBar.queueControl.height/2-y)
        closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutsideParent
        enter:Transition { NumberAnimation{target:narrowQueue;property:"reveal";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:280;easing.type:Easing.OutCubic} }
        exit:Transition { NumberAnimation{target:narrowQueue;property:"reveal";to:0;duration:settingsBridge.values.reduceMotion?0:180;easing.type:Easing.InCubic} }
        background:null
        contentItem:Item {
            objectName:"narrowQueueSurface";opacity:narrowQueue.reveal
            transform:Scale {origin.x:narrowQueue.anchorPoint.x;origin.y:narrowQueue.anchorPoint.y;xScale:narrowQueue.reveal;yScale:narrowQueue.reveal}
            GlassSurface {
                objectName:"queueGlassSurface";anchors.fill:parent;radius:12;backdrop:contentHost;active:narrowQueue.visible;effectName:"queueBackdropBlur"
                secondaryBackdrop:playerBar;secondaryOffset:Qt.point(playerBar.x-contentHost.x,playerBar.y-contentHost.y)
                sampleRect:Qt.rect(playerBar.x+playerBar.queueControlX+narrowQueue.x,playerBar.y+playerBar.queueControlY+narrowQueue.y,narrowQueue.width,narrowQueue.height)
            }
            UpcomingPanel {anchors.fill:parent;popupPresentation:true;expandedPlayer:true;onActivated:narrowQueue.close();onDismissRequested:narrowQueue.close()}
        }
    }
    NumberAnimation { id:playerMotion;target:root;property:"playerProgress";duration:420;easing.type:Easing.OutCubic }
    NumberAnimation { id:coverMotion;target:root;property:"coverProgress";from:0;to:1;duration:420;easing.type:Easing.OutCubic;onFinished:{root.coverTravelling=false;root.metadataTravelling=false;duration=420} }
    ParallelAnimation {
        id:pageOut
        NumberAnimation{target:oldPage;property:"opacity";to:0;duration:84;easing.type:Easing.OutCubic}
        NumberAnimation{target:oldPage;property:"scale";to:.99;duration:84;easing.type:Easing.OutCubic}
        onFinished:root.clearPageSnapshot()
    }
    ParallelAnimation {
        id:pageIn
        NumberAnimation{target:pageSurface;property:"opacity";to:1;duration:136;easing.type:Easing.OutCubic}
        NumberAnimation{target:pageSurface;property:"y";to:0;duration:136;easing.type:Easing.OutCubic}
        onFinished:root.clearPageSnapshot()
    }
    Timer {
        id:snapshotWarmup;interval:250
        // Unexposed/occluded windows may not swap a frame. Do not leave a
        // navigation action waiting or start a stale geometry animation.
        onTriggered:root.presentPageSnapshot(root.snapshotRevision,false)
    }
    Loader {
        id:importDialogLoader;active:false
        sourceComponent:ImportDialog{parent:root.popupLayer}
    }
    Loader {
        id:playlistEditorLoader;active:false
        sourceComponent:PlaylistEditor{parent:root.popupLayer;onCreated:key=>root.openPlaylist(key)}
    }
    RoundMenu {
        id:playlistNavigationOptions;objectName:"playlistNavigationOptions";parent:root.popupLayer;width:Math.min(144,root.width-32)
        RoundMenuItem {objectName:"adjustPlaylistOrder";text:"调整排序";onTriggered:{root.beginPlaylistOrder();if(root.narrowLayout)compactNavigationMenu.open()}}
    }
    Loader {
        id: lyricsWorkspaceLoader; anchors.fill: parent; z: 6; active: false
        sourceComponent: LyricsWorkspace { onBatchInstrumentalRequested: keys => root.ensureBatchInstrumental().review(keys) }
    }
    Loader {
        id:batchInstrumentalLoader;active:false
        sourceComponent:BatchInstrumentalDialog{parent:root.popupLayer}
    }
    FeedbackPanel{id:feedbackPanel;parent:root.popupLayer;z:100;suspended:root.popupOpen;bottomInset:playerBar.visible?playerBar.height+edgeMargin:edgeMargin}
    Toast{id:toast;anchors.horizontalCenter:parent.horizontalCenter;anchors.bottom:playerBar.top;anchors.bottomMargin:16;z:20}
    ConfirmDialog {
        id:closeConfirm;parent:root.popupLayer;objectName:"closeTasksConfirmation";anchors.centerIn:parent;width:Math.min(480,root.width-40);modal:true
        title:"结束当前任务并退出？";standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:Label{text:(lyricsSearchBridge.busy?"还有歌词尚未查找完毕，确认退出将中断查找。\n":"")+"退出将取消尚未完成的导入和翻译，并放弃未保存的翻译预览。已经保存的结果会保留；已发送的 AI 请求可能仍产生费用。";wrapMode:Text.Wrap}
        onRejected:root.exitRequested=false
        onAccepted:{lyricsSearchBridge.stop();root.allowClose=true;root.close()}
    }
    }
    Repeater {
        model:root.miniFlightParts
        delegate:Item {
            id:flightPart
            required property var modelData
            readonly property real progress:1-root.miniTransitionProgress
            function blend(a,b){return a+(b-a)*progress}
            objectName:"miniHandoff-"+modelData.key;z:250;visible:root.miniFlightActive;opacity:modelData.alpha
            x:blend(modelData.from.x,modelData.to.x)-root.x;y:blend(modelData.from.y,modelData.to.y)-root.y
            width:blend(modelData.from.width,modelData.to.width);height:blend(modelData.from.height,modelData.to.height)
            Loader {
                anchors.fill:parent;active:modelData.kind==="art"
                sourceComponent:Artwork {videoCover:playbackBridge.video;coverAspect:playbackBridge.video?(root.lyricsOpen?16/9:4/3):1;file:playbackBridge.coverUrl;revision:playbackBridge.artworkRevision;seed:playbackBridge.title;resolution:320;animated:false}
            }
            UiText {
                id:flightText
                objectName:"miniHandoffText-"+modelData.key;visible:modelData.kind==="text"
                // font.pixelSize is an integer: animating it stepped through
                // just a few glyph sizes and reshaped text during the flight.
                readonly property real flightScale:modelData.kind==="text"?parent.blend(modelData.pixelSize,modelData.toPixelSize)/modelData.toPixelSize:1
                width:parent.width/flightScale;height:parent.height/flightScale
                transform:Scale{xScale:flightText.flightScale;yScale:flightText.flightScale}
                text:modelData.text;textFormat:Text.PlainText;elide:Text.ElideRight;color:modelData.color
                font.family:modelData.kind==="text"?modelData.toFamily:Theme.fontFamily;basePixelSize:modelData.kind==="text"?modelData.toPixelSize/Theme.fontScale:14;font.weight:modelData.kind==="text"?modelData.toWeight:Font.Normal
            }
            Rectangle {
                anchors.fill:parent;visible:modelData.kind==="icon";color:modelData.filled?Theme.accent:"transparent";radius:modelData.filled?width/2:0
                Item {
                    anchors.fill:parent;anchors.margins:parent.parent.blend(modelData.padding,modelData.toPadding)
                    Symbol {anchors.fill:parent;visible:modelData.key!=="queue";name:modelData.symbol;color:modelData.filled?Theme.accentText:modelData.color}
                    Repeater {model:modelData.key==="queue"?3:0;Rectangle{required property int index;x:0;y:parent.height*(.2+index*.25);width:parent.width*(.33+index*.25);height:2;radius:1;color:Theme.text}}
                }
            }
        }
    }
    Item {
        anchors.fill:parent;z:300
        Repeater {
            model:[Qt.LeftEdge,Qt.RightEdge,Qt.TopEdge,Qt.BottomEdge,Qt.TopEdge|Qt.LeftEdge,Qt.TopEdge|Qt.RightEdge,Qt.BottomEdge|Qt.LeftEdge,Qt.BottomEdge|Qt.RightEdge]
            delegate:MouseArea {
                required property int modelData
                objectName:(root.miniMode?"miniResizeEdge-":"windowResizeEdge-")+modelData
                readonly property bool compactResize:root.windowResizeActive&&root.miniResizeEdges===modelData
                preventStealing:true;enabled:root.visibility===Window.Windowed&&(!root.miniSwitching||compactResize)
                x:modelData&Qt.RightEdge?root.width-width:0;y:modelData&Qt.BottomEdge?root.height-height:0
                width:modelData&(Qt.LeftEdge|Qt.RightEdge)?6:root.width
                height:modelData&(Qt.TopEdge|Qt.BottomEdge)?6:root.height
                cursorShape:modelData===(Qt.LeftEdge|Qt.TopEdge)||modelData===(Qt.RightEdge|Qt.BottomEdge)?Qt.SizeFDiagCursor:modelData===(Qt.RightEdge|Qt.TopEdge)||modelData===(Qt.LeftEdge|Qt.BottomEdge)?Qt.SizeBDiagCursor:modelData&(Qt.LeftEdge|Qt.RightEdge)?Qt.SizeHorCursor:Qt.SizeVerCursor
                onPressed:mouse=>{if(root.miniSwitching){mouse.accepted=false;return}if(!root.beginWindowResize(modelData,uiIntegration.pointerPosition()))root.startSystemResize(modelData)}
                onPositionChanged:mouse=>{if(pressed)root.updateWindowResize(uiIntegration.pointerPosition())}
                onReleased:root.endWindowResize()
                onCanceled:root.endWindowResize()
            }
        }
    }
}
