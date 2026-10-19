import QtQuick
Item {
    id: artwork
    property url file
    property string seed: "Relaxer"
    property string revision: ""
    property int resolution: 640
    property bool animated: true
    property int direction: 0
    property real radius: Math.min(width, height) * 0.075
    property url imageUrl: "image://relaxer/artwork?file=" + encodeURIComponent(String(file))
        + "&seed=" + encodeURIComponent(seed) + "&rev=" + encodeURIComponent(revision)
        + "&resolution=" + resolution
        + "&radiusRatio=" + (Math.min(width, height) > 0 ? radius / Math.min(width, height) : 0.075)
    property url displayedUrl
    property url outgoingUrl
    property int activeDirection: 0
    property real transition: 1
    property bool initialized: false
    property bool pendingTransition: false
    readonly property bool reduceMotion: settingsBridge.values.reduceMotion
    readonly property alias imageItem: pocket
    implicitWidth: 62
    implicitHeight: 62
    clip: true
    function updateImage() {
        const next = String(imageUrl)
        if (next === String(displayedUrl)) return
        const previousDisplay = displayedUrl
        const previousOutgoing = outgoingUrl
        const visibleImage = pocket.isImageLoaded(displayedUrl) ? displayedUrl : outgoingUrl
        motion.stop()
        outgoingUrl = visibleImage
        activeDirection = direction
        displayedUrl = next
        if (String(previousDisplay).length && String(previousDisplay) !== String(outgoingUrl) && String(previousDisplay) !== next) pocket.unloadImage(previousDisplay)
        if (String(previousOutgoing).length && String(previousOutgoing) !== String(outgoingUrl) && String(previousOutgoing) !== next) pocket.unloadImage(previousOutgoing)
        pendingTransition = initialized && animated && !reduceMotion && String(outgoingUrl).length > 0
        transition = pendingTransition ? 0 : 1
        pocket.loadImage(displayedUrl)
        if (pocket.isImageLoaded(displayedUrl)) {
            if (pendingTransition) { if (!motion.running) motion.restart() }
            else retireOutgoing()
            pocket.requestPaint()
        }
    }
    function retireOutgoing() {
        if (String(outgoingUrl).length && String(outgoingUrl) !== String(displayedUrl)) pocket.unloadImage(outgoingUrl)
        outgoingUrl = ""
        pocket.requestPaint()
    }
    onImageUrlChanged: if (initialized) Qt.callLater(updateImage)
    onTransitionChanged: if (initialized) pocket.requestPaint()
    onWidthChanged: if (initialized) pocket.requestPaint()
    onHeightChanged: if (initialized) pocket.requestPaint()
    onRadiusChanged: if (initialized) pocket.requestPaint()
    onAnimatedChanged: if (initialized && !animated) { motion.stop(); pendingTransition = false; transition = 1; if (pocket.isImageLoaded(displayedUrl)) retireOutgoing() }
    onReduceMotionChanged: if (initialized && reduceMotion) { motion.stop(); pendingTransition = false; transition = 1; if (pocket.isImageLoaded(displayedUrl)) retireOutgoing() }
    Component.onCompleted: { initialized = true; updateImage() }
    Canvas {
        id: pocket
        anchors.fill: parent
        renderStrategy: Canvas.Immediate
        onImageLoaded: {
            if (isImageLoaded(artwork.displayedUrl)) {
                if (artwork.pendingTransition && !motion.running) motion.restart()
                else if (!artwork.pendingTransition) artwork.retireOutgoing()
            }
            requestPaint()
        }
        onPaint: {
            const context = getContext("2d")
            context.clearRect(0, 0, width, height)
            const radius = Math.min(artwork.radius, width / 2, height / 2)
            context.save()
            context.beginPath()
            context.moveTo(radius, 0)
            context.lineTo(width - radius, 0)
            context.quadraticCurveTo(width, 0, width, radius)
            context.lineTo(width, height - radius)
            context.quadraticCurveTo(width, height, width - radius, height)
            context.lineTo(radius, height)
            context.quadraticCurveTo(0, height, 0, height - radius)
            context.lineTo(0, radius)
            context.quadraticCurveTo(0, 0, radius, 0)
            context.closePath()
            context.clip()
            const oldReady = String(artwork.outgoingUrl).length && isImageLoaded(artwork.outgoingUrl)
            const nextReady = String(artwork.displayedUrl).length && isImageLoaded(artwork.displayedUrl)
            const progress = nextReady ? artwork.transition : 0
            if (artwork.activeDirection < 0) {
                if (nextReady) context.drawImage(artwork.displayedUrl, 0, 0, width, height)
                if (oldReady && progress < 1) context.drawImage(artwork.outgoingUrl, width * progress, 0, width, height)
            } else if (artwork.activeDirection > 0) {
                if (oldReady && progress < 1) context.drawImage(artwork.outgoingUrl, 0, 0, width, height)
                if (nextReady) context.drawImage(artwork.displayedUrl, width * (1 - progress), 0, width, height)
            } else {
                if (oldReady && progress < 1) { context.globalAlpha = 1 - progress; context.drawImage(artwork.outgoingUrl, 0, 0, width, height) }
                context.globalAlpha = nextReady ? progress : 0
                if (nextReady) context.drawImage(artwork.displayedUrl, 0, 0, width, height)
            }
            context.restore()
        }
    }
    NumberAnimation {
        id: motion
        target: artwork
        property: "transition"
        from: 0
        to: 1
        duration: artwork.activeDirection === 0 ? 180 : 420
        easing.type: Easing.OutCubic
        onFinished: { artwork.pendingTransition = false; artwork.retireOutgoing() }
    }
}
