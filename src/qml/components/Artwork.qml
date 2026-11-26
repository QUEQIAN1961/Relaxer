import QtQuick
import QtQuick.Window
import QtQuick.Effects
import "../theme"
Item {
    id:artwork
    property bool videoCover:false
    property real coverAspect:videoCover?4/3:1
    property url file
    property string seed:"Relaxer"
    property string revision:""
    property string contentKey:String(file)+"\n"+seed
    property string displayedContentKey:""
    property int resolution:640
    property bool animated:true
    property bool retainWhenHidden:false
    property bool cacheOnlineCover:true
    property int direction:0
    property real radius:Math.min(width,height)*Theme.artworkCornerRatio
    property url imageUrl:(String(file).startsWith("https://")||String(file).startsWith("http://"))?file:
        "image://relaxer/artwork?file="+encodeURIComponent(String(file))+"&seed="+encodeURIComponent(seed)+"&rev="+encodeURIComponent(revision)
        +"&resolution="+resolution+"&radiusRatio=0&aspect="+coverAspect
    property url displayedUrl
    property url outgoingUrl
    property int activeDirection:0
    property real transition:1
    property bool initialized:false
    property bool pendingTransition:false
    property int activeSlot:-1
    property int outgoingSlot:-1
    property int softwareLoadRevision:0
    readonly property bool software:GraphicsInfo.api===GraphicsInfo.Software
    readonly property bool windowActive:!Window.window||(Window.window.visible&&Window.window.visibility!==Window.Minimized)
    readonly property bool presentationActive:visible&&windowActive
    readonly property bool needsMask:!software&&radius>0
    readonly property bool reduceMotion:settingsBridge.values.reduceMotion
    readonly property bool contentCurrent:animated||contentKey===displayedContentKey
    readonly property bool remoteImage:String(imageUrl).startsWith("https://")||String(imageUrl).startsWith("http://")
    readonly property url requestUrl:cacheOnlineCover&&remoteImage?onlineArtwork.coverImageUrl(imageUrl):imageUrl
    readonly property int decodeWidth:Math.max(64,Math.min(960,Math.ceil(resolution*(Window.window?Window.window.devicePixelRatio:1))))
    readonly property int decodeHeight:Math.max(1,Math.ceil(decodeWidth/coverAspect))
    readonly property alias imageItem:pocket
    readonly property alias status:pocket.status
    implicitWidth:62;implicitHeight:62;clip:true
    function buffer(index){return index===0?bufferA:index===1?bufferB:null}
    function imageFor(value){const key=String(value);if(!key.length)return null;if(String(bufferA.source)===key&&bufferA.status===Image.Ready)return bufferA;if(String(bufferB.source)===key&&bufferB.status===Image.Ready)return bufferB;return String(bufferA.source)===key?bufferA:String(bufferB.source)===key?bufferB:null}
    function loaded(value){
        // Canvas.isImageLoaded() has no notify signal of its own. Bindings
        // using loaded() must also observe asynchronous Canvas completion.
        const image=imageFor(value)
        return !!image&&image.status===Image.Ready&&(!software||(softwareLoadRevision>=0&&softwareLoader.item&&softwareLoader.item.isImageLoaded(value)))
    }
    function paintFallback(){if(initialized&&presentationActive&&softwareLoader.item)softwareLoader.item.requestPaint()}
    function mayUpdateImage(){return windowActive&&(visible||retainWhenHidden)}
    function scheduleImageUpdate(){if(initialized&&mayUpdateImage())imageUpdate.restart()}
    function imageArrived(){
        if(!initialized)return
        if(!presentationActive){if(!visible&&!retainWhenHidden)suspendImages();else settleBackground();return}
        if(loaded(displayedUrl)){
            if(pendingTransition){if(!motion.running)motion.restart()}
            else retireOutgoing()
        }
        paintFallback()
    }
    function updateImage(){
        if(!initialized||!mayUpdateImage())return
        const next=String(requestUrl),changed=contentKey!==displayedContentKey
        if(next===String(displayedUrl)){
            if(changed&&!animated){motion.stop();pendingTransition=false;transition=1;retireOutgoing()}
            displayedContentKey=contentKey;imageArrived();return
        }
        const previousDisplay=displayedUrl,previousOutgoing=outgoingUrl
        // A recycled row must not borrow the previous row's picture. Retain
        // an old texture only for this same media item or an explicit player transition.
        const old=changed&&!animated?-1:loaded(displayedUrl)?activeSlot:loaded(outgoingUrl)?outgoingSlot:-1
        motion.stop()
        const oldValue=old>=0?buffer(old).source:""
        const nextSlot=old===0?1:0
        activeDirection=contentKey===displayedContentKey?0:direction
        displayedContentKey=contentKey;outgoingSlot=old;outgoingUrl=oldValue
        activeSlot=String(oldValue)===next?old:nextSlot;displayedUrl=next
        pendingTransition=animated&&!reduceMotion&&old>=0&&activeSlot!==old
        transition=pendingTransition?0:1
        // Keep the outgoing texture in its original slot. Changing size or
        // beginning another transition must not reload the visible old image.
        if(activeSlot!==old)buffer(activeSlot).source=next
        for(let i=0;i<2;++i)if(i!==activeSlot&&i!==outgoingSlot)buffer(i).source=""
        if(softwareLoader.item){
            for(const value of [previousDisplay,previousOutgoing])
                if(String(value).length&&String(value)!==next&&String(value)!==String(outgoingUrl))softwareLoader.item.unloadImage(value)
            softwareLoader.item.loadImage(next)
        }
        imageArrived()
    }
    function retireOutgoing(){
        const value=outgoingUrl
        if(outgoingSlot>=0&&outgoingSlot!==activeSlot)buffer(outgoingSlot).source=""
        outgoingSlot=-1;outgoingUrl=""
        if(softwareLoader.item&&String(value).length&&String(value)!==String(displayedUrl))softwareLoader.item.unloadImage(value)
        paintFallback()
    }
    function settleBackground(){
        // Keep the decoded current image and its Canvas/mask. A hidden window
        // need not animate, but returning must not restart from a placeholder.
        motion.stop();pendingTransition=false;transition=1
        if(loaded(displayedUrl))retireOutgoing()
    }
    function suspendImages(){
        motion.stop();pendingTransition=false;transition=1
        if(softwareLoader.item){
            if(String(displayedUrl).length)softwareLoader.item.unloadImage(displayedUrl)
            if(String(outgoingUrl).length)softwareLoader.item.unloadImage(outgoingUrl)
        }
        activeSlot=-1;outgoingSlot=-1;displayedUrl="";outgoingUrl="";bufferA.source="";bufferB.source=""
    }
    function progress(){return loaded(displayedUrl)?transition:0}
    function opacityFor(index){
        if(!contentCurrent)return 0
        const image=buffer(index);if(!image||!loaded(image.source))return 0
        if(index===activeSlot)return activeDirection===0?progress():1
        if(index===outgoingSlot)return activeDirection===0?1-progress():progress()<1?1:0
        return 0
    }
    function offsetFor(index){
        if(index===outgoingSlot&&activeDirection<0)return width*progress()
        if(index===activeSlot&&activeDirection>0)return width*(1-progress())
        return 0
    }
    onRequestUrlChanged:scheduleImageUpdate()
    onContentKeyChanged:{paintFallback();scheduleImageUpdate()}
    onPresentationActiveChanged:if(initialized){
        if(presentationActive){
            imageUpdate.stop();updateImage();paintFallback()
        }else if(!visible&&!retainWhenHidden){
            imageUpdate.stop();suspendImages()
        }else settleBackground()
    }
    onWindowActiveChanged:if(initialized){
        if(!windowActive)imageUpdate.stop()
        else if(!visible&&retainWhenHidden)scheduleImageUpdate()
    }
    onRetainWhenHiddenChanged:if(initialized&&!visible){
        if(retainWhenHidden)scheduleImageUpdate()
        else{imageUpdate.stop();suspendImages()}
    }
    onTransitionChanged:paintFallback()
    onWidthChanged:paintFallback()
    onHeightChanged:paintFallback()
    onRadiusChanged:paintFallback()
    onAnimatedChanged:if(initialized&&!animated){motion.stop();pendingTransition=false;transition=1;if(loaded(displayedUrl))retireOutgoing()}
    onReduceMotionChanged:if(initialized&&reduceMotion){motion.stop();pendingTransition=false;transition=1;if(loaded(displayedUrl))retireOutgoing()}
    onSoftwareChanged:if(initialized){suspendImages();scheduleImageUpdate()}
    Component.onCompleted:{initialized=true;updateImage()}
    Component.onDestruction:{initialized=false;imageUpdate.stop();motion.stop()}
    // A child timer cancels with a recycled/destroyed delegate. A raw callLater
    // closure can otherwise reach an Artwork whose QML methods are gone.
    Timer {id:imageUpdate;interval:0;onTriggered:if(artwork.initialized)artwork.updateImage()}
    Item {
        id:pocket
        readonly property int status:artwork.loaded(artwork.displayedUrl)?Image.Ready:Image.Loading
        readonly property size sourceSize:Qt.size(artwork.decodeWidth,artwork.decodeHeight)
        function isImageLoaded(value:url):bool{return artwork.loaded(value)}
        function requestPaint(){artwork.paintFallback()}
    }
    ContinuousCorner {
        objectName:"artworkLoadingPlaceholder";anchors.fill:parent;radius:Math.min(artwork.radius,width/2,height/2)
        visible:!artwork.animated&&artwork.presentationActive&&(!artwork.contentCurrent||(!artwork.loaded(artwork.displayedUrl)&&!artwork.loaded(artwork.outgoingUrl)))
        color:Theme.card
        Symbol {anchors.centerIn:parent;width:Math.min(24,artwork.width*.4);height:width;name:artwork.videoCover?"video":"music";color:Theme.secondary;opacity:.38}
    }
    Item {
        id:coverScene;anchors.fill:parent
        visible:artwork.visible&&!artwork.software&&(!artwork.needsMask||!!coverMask.item)
        // Apply the mask to the actual image layer, including recycled queue rows
        // and native popup windows. There is no separate unmasked image pass.
        layer.enabled:artwork.visible&&artwork.needsMask
        layer.smooth:true
        layer.effect:MultiEffect {
            maskEnabled:true;maskThresholdMin:.5;maskSpreadAtMin:1
            maskSource:coverMask.item;autoPaddingEnabled:false
        }
        Image {
            id:bufferA;width:parent.width;height:parent.height;x:artwork.offsetFor(0)
            opacity:artwork.opacityFor(0);z:artwork.outgoingSlot===0&&artwork.activeDirection<0?2:artwork.activeSlot===0?1:0
            asynchronous:true;cache:artwork.remoteImage;retainWhileLoading:true;smooth:true;mipmap:true
            sourceSize.width:artwork.decodeWidth;sourceSize.height:artwork.decodeHeight
            fillMode:artwork.videoCover?Image.PreserveAspectCrop:Image.Stretch
            onStatusChanged:if(status===Image.Ready)artwork.imageArrived()
        }
        Image {
            id:bufferB;width:parent.width;height:parent.height;x:artwork.offsetFor(1)
            opacity:artwork.opacityFor(1);z:artwork.outgoingSlot===1&&artwork.activeDirection<0?2:artwork.activeSlot===1?1:0
            asynchronous:true;cache:artwork.remoteImage;retainWhileLoading:true;smooth:true;mipmap:true
            sourceSize.width:artwork.decodeWidth;sourceSize.height:artwork.decodeHeight
            fillMode:artwork.videoCover?Image.PreserveAspectCrop:Image.Stretch
            onStatusChanged:if(status===Image.Ready)artwork.imageArrived()
        }
    }
    Loader {
        id:coverMask;anchors.fill:parent;active:artwork.needsMask&&(artwork.visible||artwork.retainWhenHidden)
        sourceComponent:ContinuousCorner {
            radius:artwork.radius;color:"white";visible:false
            layer.enabled:true;layer.smooth:true
            // Small queue covers need more than a logical-pixel mask to retain
            // the continuous corner and its antialiasing at desktop DPI scales.
            readonly property real maskScale:Math.max(2,artwork.Window.window?artwork.Window.window.devicePixelRatio:1)
            layer.textureSize:Qt.size(Math.max(1,Math.ceil(width*maskScale)),Math.max(1,Math.ceil(height*maskScale)))
        }
    }
    Loader {
        id:softwareLoader;readonly property real pixelRatio:Math.max(2,artwork.Window.window?artwork.Window.window.devicePixelRatio:1)
        width:artwork.width*pixelRatio;height:artwork.height*pixelRatio;scale:1/pixelRatio;transformOrigin:Item.TopLeft
        active:(artwork.visible||artwork.retainWhenHidden)&&artwork.software
        onLoaded:{if(String(artwork.displayedUrl).length)item.loadImage(artwork.displayedUrl);if(String(artwork.outgoingUrl).length)item.loadImage(artwork.outgoingUrl)}
        sourceComponent:Canvas {
            readonly property real pixelRatio:softwareLoader.pixelRatio
            renderStrategy:Canvas.Immediate
            onImageLoaded:{++artwork.softwareLoadRevision;artwork.imageArrived()}
            function drawCover(context,value,x){
                const info=artwork.imageFor(value);if(!info)return
                if(artwork.videoCover){
                    if(info.implicitWidth<=0||info.implicitHeight<=0)return
                    const factor=Math.max(width/info.implicitWidth,height/info.implicitHeight)
                    const w=info.implicitWidth*factor,h=info.implicitHeight*factor
                    context.drawImage(value,x+(width-w)/2,(height-h)/2,w,h)
                }else context.drawImage(value,x,0,width,height)
            }
            onPaint:{
                const c=getContext("2d");c.clearRect(0,0,width,height);if(width<=0||height<=0||!artwork.contentCurrent)return
                const r=Math.max(0,Math.min(artwork.radius*pixelRatio*1.6,width/2,height/2));c.save();c.beginPath();const u=r*.1830582617584078;c.moveTo(r,0);c.lineTo(width-r,0);c.bezierCurveTo(width-4*u,0,width-2*u,0,width-u,u);c.bezierCurveTo(width,2*u,width,4*u,width,r);c.lineTo(width,height-r);c.bezierCurveTo(width,height-4*u,width,height-2*u,width-u,height-u);c.bezierCurveTo(width-2*u,height,width-4*u,height,width-r,height);c.lineTo(r,height);c.bezierCurveTo(4*u,height,2*u,height,u,height-u);c.bezierCurveTo(0,height-2*u,0,height-4*u,0,height-r);c.lineTo(0,r);c.bezierCurveTo(0,4*u,0,2*u,u,u);c.bezierCurveTo(2*u,0,4*u,0,r,0);c.closePath();c.clip()
                const oldReady=String(artwork.outgoingUrl).length&&isImageLoaded(artwork.outgoingUrl)
                const ready=String(artwork.displayedUrl).length&&isImageLoaded(artwork.displayedUrl);const p=ready?artwork.transition:0
                if(artwork.activeDirection<0){if(ready)drawCover(c,artwork.displayedUrl,0);if(oldReady&&p<1)drawCover(c,artwork.outgoingUrl,width*p)}
                else if(artwork.activeDirection>0){if(oldReady&&p<1)drawCover(c,artwork.outgoingUrl,0);if(ready)drawCover(c,artwork.displayedUrl,width*(1-p))}
                else{if(oldReady&&p<1){c.globalAlpha=1-p;drawCover(c,artwork.outgoingUrl,0)}c.globalAlpha=ready?p:0;if(ready)drawCover(c,artwork.displayedUrl,0)}
                c.restore()
            }
        }
    }
    NumberAnimation {id:motion;target:artwork;property:"transition";from:0;to:1;duration:artwork.activeDirection===0?180:420;easing.type:Easing.OutCubic;onFinished:{artwork.pendingTransition=false;artwork.retireOutgoing()}}
}
