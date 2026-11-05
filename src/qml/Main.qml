import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "components"
import "pages"
import "theme"
ApplicationWindow {
    id: root
    component DeferredPage: Loader {
        property bool requested: false
        active: false
        onRequestedChanged: if (requested) active = true
        Component.onCompleted: if (requested) active = true
    }
    objectName: "qmlMainWindow"
    visible: true
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
    readonly property real sidebarBaseWidth:205-121*sidebarCondenseProgress
    readonly property bool narrowLayout: (miniSwitching?miniNormalGeometry[2]:width) < 547
    property real narrowProgress:0
    NumberAnimation {id:narrowMotion;target:root;property:"narrowProgress";duration:320;easing.type:Easing.OutCubic}
    Binding { target: Theme; property: "compact"; value: root.narrowLayout }
    Binding { target: Theme; property: "layoutProgress"; value: root.narrowProgress }
    property bool windowPinned: false
    flags: Qt.Window | Qt.FramelessWindowHint | (miniMode||miniSwitching||(windowResizeActive&&resizeMiniPinned)||windowPinned ? Qt.WindowStaysOnTopHint : 0)
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
    readonly property bool playerPresented:lyricsOpen||playerProgress>0
    readonly property bool playerSettled:lyricsOpen&&playerProgress>=1
    readonly property bool backgrounded: !visible || visibility === Window.Minimized
    onBackgroundedChanged:if(backgrounded)Qt.callLater(root.suspendTransientPopups)
    function suspendTransientPopups(){
        if(!backgrounded)return
        narrowQueue.close();compactNavigationMenu.close();playerBar.closeTransientPopups()
        if(miniLoader.item)miniLoader.item.closeTransientPopups()
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
        // The persistent feedback Item is non-modal. Its detail dialogs are
        // separate overlay children and retain their existing Escape priority.
        for (let index = 0; index < items.length; ++index)
            if (items[index] !== feedbackPanel && items[index].visible && items[index].opacity > 0 && items[index].width > 0 && items[index].height > 0) return true
        return false
    }
    function imageRect(item) { const p = item.mapToItem(root.contentItem,0,0); return {x:p.x,y:p.y,width:item.width,height:item.height} }
    function textRect(item) {
        const bounds = imageRect(item)
        const width = Math.min(bounds.width, item.contentWidth + 2)
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
    function clearBulk() { if(lyricsSearchBridge.batchRunning)lyricsSearchBridge.cancelInstrumentalBatch(); if (bulkOwner) bulkOwner.clearBulk(); bulkActive = false; bulkKeys = []; bulkOwner = null }
    function selectBulk(all) {
        if (!bulkOwner) return
        bulkOwner.setBulkKeys(all ? libraryBridge.visibleTrackKeys() : [])
    }
    function animatePlayer(open) {
        if (lyricsOpen === open && !playerMotion.running) return
        ensureNowPlaying()
        if(open)nowPage.forceActiveFocus()
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
        if (settingsBridge.values.reduceMotion || root.narrowLayout) { upcomingAtLeft=left; upcomingTravel=0; return }
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
    function setMiniGeometry(b){uiIntegration.setWindowGeometry(b[0],b[1],b[2],b[3])}
    function settleMiniLayout(){
        narrowMotion.stop()
        narrowProgress=narrowLayout?1:0
        playerBar.settleMiniLayout()
    }
    function animateMiniGeometry(from,to,entering){
        miniMotionFrom=from;miniMotionTo=to
        miniMotion.from=entering?0:1;miniMotion.to=entering?1:0
        miniMotion.duration=settingsBridge.values.reduceMotion?0:300
        miniMotion.restart()
    }
    onMiniTransitionProgressChanged:{
        if(!miniSwitching||miniMotionFrom.length!==4)return
        const p=miniMode?miniTransitionProgress:1-miniTransitionProgress,a=miniMotionFrom,b=miniMotionTo
        setMiniGeometry([a[0]+(b[0]-a[0])*p,a[1]+(b[1]-a[1])*p,a[2]+(b[2]-a[2])*p,a[3]+(b[3]-a[3])*p])
    }
    NumberAnimation {id:miniMotion;objectName:"miniGeometryMotion";target:root;property:"miniTransitionProgress";easing.type:Easing.InOutCubic;onFinished:root.finishMiniTransition()}
    function finishMiniTransition(){
        setMiniGeometry(miniMotionTo)
        if(!miniMode&&miniRestorePlacement){
            if(miniRestoreVisibility===Window.Maximized)showMaximized()
            else if(miniRestoreVisibility===Window.FullScreen)showFullScreen()
            uiIntegration.restoreMiniWindowPlacement()
        }
        miniSwitching=false
        miniFlightParts=[]
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
        miniEnteredByShortcut=!!byShortcut;miniTransitionCursor=resizeLatestCursor
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
    function leaveMiniPlayer(){
        if(!miniMode||miniSwitching)return
        restoreFromMini(miniRestoreGeometry,true)
    }
    function restoreFromMini(geometry,originalPlacement){
        const parts=miniLoader.item?miniLoader.item.transitionParts():[]
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
            part.toRadius=part.kind==="art"?item.radius:0
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
        if(miniMode){if(miniEnteredByShortcut)settingsBridge.saveMiniWindowPosition(x,y);leaveMiniPlayer()}
        else enterMiniFromEdge(0,true)
    }
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
        ensureLyricsWorkspace().open()
    }
    function backgroundUrl(file,blur){return root.backgrounded||(root.miniMode&&!root.miniSwitching) ? "" : String(file||"").length?"image://relaxer/background?file="+encodeURIComponent(String(file))+"&blur="+Number(blur||0):""}
    function applySettings() { Theme.fontFamily=root.restFontFamily; if(settingsBridge.values.reduceMotion)relocateUpcoming(lyricsOpen); Theme.dark=settingsBridge.palette.dark; Theme.custom=settingsBridge.palette.custom ? settingsBridge.palette : ({}) }
    Component.onCompleted: { width=settingsBridge.windowBounds.width;height=settingsBridge.windowBounds.height;applySettings(); if(settingsBridge.windowBounds.maximized)showMaximized() }
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
            root.coverFrom=root.imageRect(origin);root.coverTo=root.imageRect(nowPage.largeArtwork)
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
    onNarrowLayoutChanged: {
        narrowQueue.close();compactNavigationMenu.close();narrowMotion.stop()
        if(narrowLayout&&workspaceVisible&&!miniMode&&!miniSwitching){
            lyricsSearchBridge.stop();lyricsSearchBridge.cancelInstrumentalBatch();lyricsSearchBridge.cancelFileOffset()
            translationBridge.cancelAll()
            if(batchInstrumentalLoader.item)batchInstrumentalLoader.item.close()
            lyricsWorkspace.closeForNarrow()
            root.clearBulk();libraryBridge.openPlaylist("0");appBridge.navigate(0)
        }
        if(miniSwitching||miniMode||settingsBridge.values.reduceMotion)narrowProgress=narrowLayout?1:0
        else{narrowMotion.from=narrowProgress;narrowMotion.to=narrowLayout?1:0;narrowMotion.restart()}
    }
    Loader {
        id:miniLoader;anchors.fill:parent;z:200;active:root.miniMode||root.miniSwitching
        opacity:root.miniFlightActive?0:root.miniTransitionProgress;enabled:root.miniMode&&!root.miniSwitching
        sourceComponent:MiniPlayer {
            onRestoreRequested:Qt.callLater(root.leaveMiniPlayer)
        }
        onLoaded:item.forceActiveFocus()
    }
    Item {
        id:normalShell;objectName:"normalShell"
        width:root.miniMode||root.miniSwitching?root.miniNormalGeometry[2]:Math.max(414,root.width)
        height:root.miniMode||root.miniSwitching?root.miniNormalGeometry[3]:root.height
        visible:!root.miniMode||root.miniSwitching;enabled:!root.miniMode&&!root.miniSwitching
        opacity:1-root.miniTransitionProgress
        // Its layout is already frozen at the normal geometry. Cache that
        // presentation only for the flight instead of repainting the entire
        // page while the native swapchain and its scale change every frame.
        layer.enabled:root.miniSwitching
        layer.live:false
        layer.smooth:true
        transform:Scale {xScale:root.miniSwitching&&root.miniMode?root.width/normalShell.width:1;yScale:root.miniSwitching&&root.miniMode?root.height/normalShell.height:1}
    Item {
        id: contentHost
        visible: !root.workspaceOpened
        objectName: "contentHost"
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        anchors.bottom: importPanel.visible ? importPanel.top : root.bulkActive ? bulkBar.top : playerBar.top
        Rectangle {
            id: sidebar; objectName: "sidebar"; width:root.sidebarBaseWidth*(1-root.narrowProgress); opacity:1-root.narrowProgress; enabled:!root.narrowLayout&&!root.playerSettled; height: parent.height; color: Theme.panel;clip:true
            Image { objectName:"sidebarBackground"; x:0;y:0;width:settingsBridge.palette.shareLR?normalShell.width:parent.width;height:settingsBridge.palette.shareLR&&settingsBridge.palette.shareFooter?normalShell.height:parent.height;source:root.backgroundUrl(settingsBridge.palette.leftImage,settingsBridge.palette.blurLeft); fillMode: Image.PreserveAspectCrop; asynchronous: true; visible: source.toString().length>0 }
            ColumnLayout {
                x:18; y:27; width:169-121*root.sidebarCondenseProgress; height:parent.height-47; spacing:14
                Item {
                    Layout.fillWidth: true; Layout.preferredHeight: 32
                    AbstractButton {
                        id:sidebarLogo;objectName:"sidebarLogo";x:8+root.sidebarCondenseProgress;width:30;height:30;padding:0;hoverEnabled:true;enabled:!root.lyricsOpen&&!root.playlistOrderEditing
                        text:root.sidebarCondensed?"展开侧栏":"收起侧栏";Accessible.name:text
                        HintBubble { visible:sidebarLogo.hovered; text:sidebarLogo.text }
                        background:null
                        contentItem:Image {source:"qrc:/icon.svg";fillMode:Image.PreserveAspectFit}
                        onClicked:root.sidebarCondensed=!root.sidebarCondensed
                    }
                    UiText {opacity:1-root.sidebarCondenseProgress;visible:opacity>0;x:49;width:Math.max(0,parent.width-x);elide:Text.ElideRight; anchors.verticalCenter:parent.verticalCenter; text:"Relaxer"; font.pixelSize:23; font.weight:Font.DemiBold; color:Theme.text }
                    MouseArea { x:sidebarLogo.x+sidebarLogo.width; width:parent.width-x;height:parent.height;onPressed:root.startSystemMove() }
                }
                Item { Layout.preferredHeight: 2 }
                Item {
                    Layout.fillWidth:true;Layout.preferredHeight:libraryCaption.implicitHeight
                    UiText {id:libraryCaption;opacity:1-root.sidebarCondenseProgress;visible:opacity>0;text:"音乐资料库";color:Theme.text;font.pixelSize:14;font.weight:Font.DemiBold}
                    Rectangle {objectName:"librarySectionDivider";visible:opacity>0;anchors.centerIn:parent;width:20;height:2;radius:1;color:Theme.secondary;opacity:.5*root.sidebarCondenseProgress}
                }
                Column {
                    Layout.fillWidth:true; spacing:8
                    Repeater {
                        model:["本地音乐","歌单","个人","设置","我喜欢"]
                        NavigationButton {
                            required property int index; required property string modelData
                            objectName:"navigation"+index; width:parent.width; text:modelData;compact:root.sidebarCondensed
                            marker:["music","playlist","person","settings","heart"][index]
                            selected:!root.lyricsOpen && (index===4 ? appBridge.page===0&&libraryBridge.playlistId==="-1" : index===0 ? appBridge.page===0&&libraryBridge.playlistId==="0" : appBridge.page===index)
                            onClicked:root.navigateOriginal(index)
                        }
                    }
                }
                Item {
                    Layout.fillWidth:true;Layout.preferredHeight:26
                    Rectangle {objectName:"playlistSectionDivider";visible:opacity>0;anchors.centerIn:parent;width:20;height:2;radius:1;color:Theme.secondary;opacity:.5*root.sidebarCondenseProgress}
                    RowLayout {
                        opacity:1-root.sidebarCondenseProgress;visible:opacity>0;enabled:!root.sidebarCondensed;anchors.fill:parent;spacing:4
                        UiText { text:"我的歌单"; color:Theme.text; font.pixelSize: 14;font.weight:Font.DemiBold; Layout.fillWidth:true }
                        IconButton {
                            objectName:root.playlistOrderEditing?"savePlaylistOrder":"sidebarPlaylistOptions";width:26;height:26;padding:5
                            text:root.playlistOrderEditing?"保存排序":"歌单管理";showTooltip:true
                            contentItem:Item {
                                Symbol{anchors.fill:parent;name:"more";opacity:1-root.playlistOrderProgress;scale:1-.2*root.playlistOrderProgress}
                                Symbol{anchors.fill:parent;name:"check";opacity:root.playlistOrderProgress;scale:.8+.2*root.playlistOrderProgress}
                            }
                            onClicked:if(root.playlistOrderEditing)root.savePlaylistOrder();else playlistNavigationOptions.popup(this,0,height)
                        }
                        IconButton {
                            objectName:root.playlistOrderEditing?"cancelPlaylistOrder":"sidebarCreatePlaylist";width:26;height:26;padding:5
                            text:root.playlistOrderEditing?"取消排序":"创建歌单";showTooltip:true
                            contentItem:Symbol {objectName:"sidebarCreateRotation";name:"plus";rotation:45*root.playlistOrderProgress}
                            onClicked:if(root.playlistOrderEditing)root.cancelPlaylistOrder();else root.ensurePlaylistEditor().create()
                        }
                    }
                }
                ListView {
                    id: sidebarLists;objectName:"playlistNavigation"; Layout.fillWidth:true;Layout.fillHeight:true
                    model:root.playlistOrderDraftActive?playlistOrderDraft:playlistModel;clip:true;spacing:3
                    move:Transition {NumberAnimation{properties:"y";duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.InOutCubic}}
                    moveDisplaced:Transition {NumberAnimation{properties:"y";duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.InOutCubic}}
                    ScrollBar.vertical:ScrollBar{}
                    delegate:PlaylistNavigationRow {
                        objectName:"sidebarPlaylist-"+playlistKey
                        width:sidebarLists.width;compact:root.sidebarCondensed
                        listView:sidebarLists;orderModel:playlistOrderDraft;sorting:root.playlistOrderEditing
                        selected:!root.lyricsOpen&&appBridge.page===0&&libraryBridge.playlistId===playlistKey
                        onActivated:root.openPlaylist(playlistKey)
                    }
                }
            }
        }
        Item {
            id: paper; objectName:"paperSurface"; x:sidebar.width;width:parent.width-x;height:parent.height;clip:true;enabled:!root.playerSettled
            Rectangle{anchors.fill:parent;color:Theme.background}
            Image{objectName:"pageBackground";x:settingsBridge.palette.shareLR||settingsBridge.palette.shareFooter?-paper.x:0;y:0;width:settingsBridge.palette.shareLR||settingsBridge.palette.shareFooter?normalShell.width:parent.width;height:settingsBridge.palette.shareFooter?normalShell.height:parent.height;source:root.backgroundUrl(settingsBridge.palette.rightImage,settingsBridge.palette.blurRight);fillMode:Image.PreserveAspectCrop;asynchronous:true;visible:source.toString().length>0}
            Item {
                id: pageSurface;objectName:"pageSurface";width:parent.width;height:parent.height
                StackLayout {
                    id: pages;objectName:"pageStack";anchors.fill:parent;anchors.topMargin:Theme.pageContentOffset;currentIndex:appBridge.page
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
                            currentIndex:root.settingsSection==="ai"?1:root.settingsSection==="appearance"?2:root.settingsSection==="log"?3:0
                            DeferredPage {
                                id: settingsLoader
                                requested: appBridge.page === 3 && root.settingsSection === "settings"
                                sourceComponent: Item { SettingsPage { id:settingsPage;onVersionLogRequested:root.changePage(function(){root.settingsSection="log"});anchors.fill:parent;anchors.leftMargin:Theme.pageMargin;anchors.rightMargin:Theme.pageMargin;anchors.topMargin:28;anchors.bottomMargin:0;onAiRequested:root.changePage(function(){root.settingsSection="ai"});onAppearanceRequested:{settingsBridge.beginAppearance();root.changePage(function(){root.settingsSection="appearance"})} } }
                            }
                            DeferredPage {
                                id: aiLoader
                                requested: appBridge.page === 3 && root.settingsSection === "ai"
                                sourceComponent: ScrollView {
                                    id:aiScroll;objectName:"aiSettingsScroll";contentWidth:availableWidth;clip:true
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
                                id: logLoader
                                requested: appBridge.page === 3 && root.settingsSection === "log"
                                sourceComponent: VersionLogPage{onBackRequested:root.changePage(function(){root.settingsSection="settings"})}
                            }
                        }
                    }
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
    PlayerBar {
        id:playerBar;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:98
        visible:!root.bulkActive&&!root.workspaceOpened;expanded:root.lyricsOpen;artworkTravelling:root.coverTravelling||root.miniFlightActive;miniControlsTravelling:root.miniFlightActive
        backgroundCanvasHeight: normalShell.height; backgroundOffsetY: playerBar.y
        onExpandRequested:root.animatePlayer(!root.lyricsOpen)
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
                UiText {objectName:"bulkCount";text:"已选 "+root.bulkKeys.length+" 首歌曲"+(lyricsSearchBridge.batchRunning?" · 剩余 "+lyricsSearchBridge.batchResult.pending:"");color:Theme.text;font.pixelSize: 15}
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
            UiText {id:importStatus;objectName:"importStatusText";Layout.fillWidth:true;text:importPanel.shownStatus;color:Theme.secondary;font.pixelSize:14;textFormat:Text.PlainText;wrapMode:Text.Wrap;maximumLineCount:5;elide:Text.ElideRight}
            ProgressBar{objectName:"importProgress";visible:importBridge.busy;Layout.preferredWidth:Math.min(230,root.width*.22);from:0;to:Math.max(1,importBridge.total);value:importBridge.current;indeterminate:importBridge.total===0}
            UiText { objectName:"importProgressCount"; visible:importBridge.busy&&importBridge.total>0; text:importBridge.current+"/"+importBridge.total; color:Theme.text; font.pixelSize:14 }
            ShellButton{objectName:"importResultDetails";visible:!importBridge.busy;text:"详情";implicitHeight:28;onClicked:feedbackPanel.openDetails({message:importPanel.completionText,details:importBridge.result.details})}
            ShellButton{visible:importBridge.busy;text:"取消导入";implicitHeight:28;onClicked:importBridge.cancel()}
        }
    }
    TapHandler {
        gesturePolicy:TapHandler.DragThreshold
        onPressedChanged:if(pressed&&upcoming.panelExpanded&&!upcoming.contains(upcoming.mapFromItem(root.contentItem,point.position.x,point.position.y)))upcoming.panelExpanded=false
    }
    Item {
        objectName:"upcomingWindowClip";anchors.fill:parent;clip:true;z:2;visible:!root.workspaceOpened
        UpcomingPanel {
            id:upcoming;objectName:"nextQueuePanel"
            backdropSource:contentHost
            secondaryBackdrop:playerBar;secondaryOffset:Qt.point(playerBar.x-contentHost.x,playerBar.y-contentHost.y)
            backdropRect:Qt.rect(x,y,width,height)
            expandedPlayer:root.upcomingAtLeft
            width:root.upcomingAtLeft?Math.min(implicitWidth,Math.max(160,playerBar.wideCenterX-24))
                :Math.min(implicitWidth,Math.max(0,playerBar.wideRightSpace-24))
            x:root.upcomingAtLeft?8+(playerBar.x+playerBar.queueControlX-8)*root.narrowProgress:normalShell.width-playerBar.coverMargin-width
            y:root.upcomingAtLeft
                ?(normalShell.height-4-height)*(1-root.narrowProgress)+(playerBar.y+playerBar.queueControlY)*root.narrowProgress+(height+4)*root.upcomingTravel
                :normalShell.height-playerBar.coverMargin-height+(height+playerBar.coverMargin)*root.upcomingTravel+(height+playerBar.coverMargin)*root.narrowProgress
            opacity:1-root.narrowProgress
            transform:Scale {
                origin.x:0;origin.y:0
                xScale:root.upcomingAtLeft?1+(36/upcoming.width-1)*root.narrowProgress:1
                yScale:root.upcomingAtLeft?1+(36/upcoming.height-1)*root.narrowProgress:1
            }
            relocating:upcomingOut.running||upcomingIn.running
            enabled:!relocating&&!root.narrowLayout
            visible:!root.bulkActive
        }
    }
    NumberAnimation {
        id:upcomingOut;target:root;property:"upcomingTravel";to:1;duration:210;easing.type:Easing.OutCubic
        onFinished:{root.upcomingAtLeft=root.lyricsOpen;upcomingIn.from=1;upcomingIn.restart()}
    }
    NumberAnimation {id:upcomingIn;target:root;property:"upcomingTravel";to:0;duration:210;easing.type:Easing.OutCubic}
    Artwork {
        id:flight;objectName:"sharedArtworkFlight";z:4;visible:root.coverTravelling&&!root.workspaceOpened
        file:root.flightFile;seed:root.flightSeed;resolution:768
        x:root.coverFrom.x+(root.coverTo.x-root.coverFrom.x)*root.coverProgress
        y:root.coverFrom.y+(root.coverTo.y-root.coverFrom.y)*root.coverProgress
        width:root.coverFrom.width+(root.coverTo.width-root.coverFrom.width)*root.coverProgress
        height:root.coverFrom.height+(root.coverTo.height-root.coverFrom.height)*root.coverProgress
        MouseArea {anchors.fill:parent;acceptedButtons:Qt.AllButtons;onWheel:wheel=>{wheel.accepted=true}}
    }

    UiText {
        id:titleFlight;objectName:"sharedTitleFlight";z:4;visible:root.metadataTravelling&&!root.workspaceOpened
        opacity:root.narrowLayout&&!playerBar.metadataTitle.visible?(root.lyricsOpen?root.coverProgress:1-root.coverProgress):1
        text:playbackBridge.hasTrack?playbackBridge.title:"Relaxer";textFormat:Text.PlainText;elide:Text.ElideNone;color:Theme.text
        x:root.titleFrom.x+(root.titleTo.x-root.titleFrom.x)*root.coverProgress
        y:root.titleFrom.y+(root.titleTo.y-root.titleFrom.y)*root.coverProgress
        width:!root.lyricsOpen&&root.narrowLayout?root.titleFrom.width+(root.titleTo.width-root.titleFrom.width)*root.coverProgress:Math.max(root.titleFrom.width+(root.titleTo.width-root.titleFrom.width)*root.coverProgress,titleExpandedMetrics.advanceWidth<=(root.lyricsOpen?root.titleTo.width:root.titleFrom.width)?titleFlightMetrics.advanceWidth+2:0)
        clip:true
        height:root.titleFrom.height+(root.titleTo.height-root.titleFrom.height)*root.coverProgress
        font.pixelSize:root.titleFrom.pixelSize+(root.titleTo.pixelSize-root.titleFrom.pixelSize)*root.coverProgress
        font.family:root.titleTo.family;font.weight:root.titleTo.weight
        MouseArea {anchors.fill:parent;acceptedButtons:Qt.AllButtons;onWheel:wheel=>{wheel.accepted=true}}
    }
    UiText {
        id:artistFlight;objectName:"sharedArtistFlight";z:4;visible:root.metadataTravelling&&!root.workspaceOpened
        opacity:root.narrowLayout&&!playerBar.metadataArtist.visible?(root.lyricsOpen?root.coverProgress:1-root.coverProgress):1
        text:playbackBridge.hasTrack?playbackBridge.artist:"选一首歌，开始放松。";textFormat:Text.PlainText;elide:Text.ElideNone;color:Theme.secondary
        x:root.artistFrom.x+(root.artistTo.x-root.artistFrom.x)*root.coverProgress
        y:root.artistFrom.y+(root.artistTo.y-root.artistFrom.y)*root.coverProgress
        width:!root.lyricsOpen&&root.narrowLayout?root.artistFrom.width+(root.artistTo.width-root.artistFrom.width)*root.coverProgress:Math.max(root.artistFrom.width+(root.artistTo.width-root.artistFrom.width)*root.coverProgress,artistExpandedMetrics.advanceWidth<=(root.lyricsOpen?root.artistTo.width:root.artistFrom.width)?artistFlightMetrics.advanceWidth+2:0)
        clip:true
        height:root.artistFrom.height+(root.artistTo.height-root.artistFrom.height)*root.coverProgress
        font.pixelSize:root.artistFrom.pixelSize+(root.artistTo.pixelSize-root.artistFrom.pixelSize)*root.coverProgress
        font.family:root.artistTo.family
        MouseArea {anchors.fill:parent;acceptedButtons:Qt.AllButtons;onWheel:wheel=>{wheel.accepted=true}}
    }
    // Measure the animated font independently of the clipped Text item's width.
    TextMetrics { id:titleFlightMetrics; text:titleFlight.text; font:titleFlight.font }
    TextMetrics { id:artistFlightMetrics; text:artistFlight.text; font:artistFlight.font }
    TextMetrics {
        id:titleExpandedMetrics; text:titleFlight.text
        font.family:root.lyricsOpen?root.titleTo.family:root.titleFrom.family
        font.pixelSize:root.lyricsOpen?root.titleTo.pixelSize:root.titleFrom.pixelSize
        font.weight:root.lyricsOpen?root.titleTo.weight:root.titleFrom.weight
    }
    TextMetrics {
        id:artistExpandedMetrics; text:artistFlight.text
        font.family:root.lyricsOpen?root.artistTo.family:root.artistFrom.family
        font.pixelSize:root.lyricsOpen?root.artistTo.pixelSize:root.artistFrom.pixelSize
        font.weight:root.lyricsOpen?root.artistTo.weight:root.artistFrom.weight
    }
    DropArea{anchors.fill:parent;onDropped:drop=>{if(drop.hasUrls&&!importBridge.busy){importBridge.start(drop.urls,Number(libraryBridge.playlistId)>0?libraryBridge.playlistId:"0");drop.acceptProposedAction()}}}
    MouseArea{objectName:"windowDragArea";z:5;x:root.narrowLayout?54:root.playerPresented?0:sidebar.width;y:0;width:Math.max(0,root.width-x-titleBar.width);height:26;onPressed:root.startSystemMove();onDoubleClicked:root.visibility===Window.Maximized?root.showNormal():root.showMaximized()}
    Row {
        id:titleBar;objectName:"titleBar";anchors.top:parent.top;anchors.right:parent.right;z:7;height:32
        AbstractButton {
            id:windowPin;objectName:"windowPinButton"
            width:42;height:32;hoverEnabled:true;checkable:true;checked:root.windowPinned
            Accessible.name:root.windowPinned?"取消置顶":"置顶窗口"
            HintBubble {visible:windowPin.hovered;text:windowPin.Accessible.name}
            background:Rectangle {color:windowPin.down?Theme.pressed:windowPin.hovered?Theme.hover:"transparent"}
            contentItem:Item {
                Symbol {
                    objectName:"windowPinIcon";anchors.centerIn:parent;width:18;height:18
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
                width:42;height:32;hoverEnabled:true
                Accessible.name: index===1 && root.visibility===Window.Maximized ? "还原窗口" : modelData
                HintBubble { visible:windowControl.hovered; text:windowControl.Accessible.name }
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
    IconButton {
        id:compactNavigation;objectName:"compactNavigation";x:0;y:0;z:7;width:42;height:32;padding:9
        visible:root.narrowProgress>0&&!root.lyricsOpen;opacity:root.narrowProgress;text:"导航"
        contentItem:Item { Repeater{model:3;Rectangle{required property int index;x:(parent.width-width)/2;y:(parent.height-12)/2+index*5;width:16;height:2;radius:1;color:Theme.text}} }
        onClicked:{if(compactNavigationMenu.visible)compactNavigationMenu.close();else compactNavigationMenu.open()}
    }
    Popup {
        id:compactNavigationMenu;objectName:"compactNavigationPanel";parent:compactNavigation;popupType:Popup.Item
        x:8;y:34;width:Math.min(168,root.width-32)
        height:Math.min(compactNavigationContent.implicitHeight+topPadding+bottomPadding,normalShell.height-playerBar.height-52)
        padding:14;modal:false;dim:false
        property real slideProgress:0
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
            UiText {text:"Relaxer";font.pixelSize:23;font.weight:Font.DemiBold;color:Theme.text;Layout.preferredHeight:34}
            UiText {text:"音乐资料库";color:Theme.secondary;font.pixelSize:13}
            Column {
                Layout.fillWidth:true;spacing:5
                Repeater {
                    model:["本地音乐","歌单","个人","设置","我喜欢"]
                    NavigationButton {
                        required property int index;required property string modelData
                        objectName:"compactNavigate-"+index;width:parent.width;text:modelData
                        marker:["music","playlist","person","settings","heart"][index]
                        selected:index===4?appBridge.page===0&&libraryBridge.playlistId==="-1":index===0?appBridge.page===0&&libraryBridge.playlistId==="0":appBridge.page===index
                        onClicked:{compactNavigationMenu.close();root.navigateOriginal(index)}
                    }
                }
            }
            Rectangle{Layout.fillWidth:true;height:1;color:Theme.border}
            RowLayout {
                Layout.fillWidth:true;spacing:4
                UiText {text:"我的歌单";color:Theme.secondary;font.pixelSize:13;Layout.fillWidth:true}
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
            ListView {
                id:compactPlaylists;objectName:"compactPlaylistNavigation"
                Layout.fillWidth:true;Layout.fillHeight:true
                Layout.preferredHeight:count*36+Math.max(0,count-1)*spacing
                Layout.minimumHeight:0
                clip:true;model:root.playlistOrderDraftActive?playlistOrderDraft:playlistModel;spacing:4
                move:Transition {NumberAnimation{properties:"y";duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.InOutCubic}}
                moveDisplaced:Transition {NumberAnimation{properties:"y";duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.InOutCubic}}
                ScrollBar.vertical:ScrollBar{}
                delegate:PlaylistNavigationRow {
                    objectName:"compactSidebarPlaylist-"+playlistKey
                    width:compactPlaylists.width;listView:compactPlaylists;orderModel:playlistOrderDraft;sorting:root.playlistOrderEditing
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
                sourceComponent:Artwork {file:playbackBridge.coverUrl;revision:playbackBridge.artworkRevision;seed:playbackBridge.title;radius:flightPart.blend(8,modelData.toRadius);resolution:320;animated:false}
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
                font.family:modelData.kind==="text"?modelData.toFamily:Theme.fontFamily;font.pixelSize:modelData.kind==="text"?modelData.toPixelSize:14;font.weight:modelData.kind==="text"?modelData.toWeight:Font.Normal
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
