import QtQuick
import QtQuick.Window
import "../theme"

Item {
    id: header
    objectName: "playlistHeader"
    property bool firstFramePresented:false
    Connections {
        target:header.Window.window
        function onFrameSwapped(){if(header.visible)header.firstFramePresented=true}
    }
    property var details: ({})
    property string playlistName: ""
    property bool hasTracks: false
    property bool reorderEnabled: false
    readonly property bool compactLayout:Theme.compact||width<620
    property real layoutProgress:compactLayout?1:0
    Behavior on layoutProgress {enabled:header.firstFramePresented&&header.visible;NumberAnimation{duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.OutCubic}}
    function reflow(wide,narrow){return wide+(narrow-wide)*layoutProgress}
    property bool collapsed: false
    property real collapseProgress: collapsed ? 1 : 0
    readonly property real compactSize: Theme.controlHeight * 2 + 6
    readonly property real coverSize:reflow(blend(160,compactSize),width<340?48:80)
    readonly property real buttonWidth: Math.max(playActions.implicitWidth, editActions.implicitWidth)
    readonly property real buttonLeft:reflow(blend(180,compactSize+20),width<340?64:100)
    readonly property real rowStep: (160 - Math.min(40, descriptionMeasure.implicitHeight)) / 3
    readonly property bool metadataDocked:collapseProgress>=.5
    readonly property real metadataExit:Math.min(1,collapseProgress*2)
    readonly property real metadataEnter:Math.max(0,collapseProgress*2-1)
    readonly property real expandedMetadataTop:rowStep*2
    readonly property real expandedMetadataHeight:160-expandedMetadataTop
    signal playRequested()
    signal shuffleRequested()
    signal playModeRequested(int mode)
    signal optionsRequested(var anchor)
    signal moveRequested(int direction)
    implicitHeight:Math.max(coverSize,compactLayout?78:0)
    function blend(expanded, compact) { return expanded + (compact - expanded) * collapseProgress }
    function metadataRowX(contentWidth,availableWidth) {
        const aligned=Math.max(0,availableWidth-contentWidth)*(metadataDocked?1:layoutProgress)
        return aligned+(metadataDocked?contentWidth*(1-metadataEnter)*(1-layoutProgress):0)
    }
    Behavior on collapseProgress {enabled:header.firstFramePresented&&header.visible;
        NumberAnimation { duration: settingsBridge.values.reduceMotion ? 0 : 320; easing.type: Easing.InOutCubic }
    }
    Artwork {
        objectName: "playlistCover"
        width: header.coverSize; height: width; radius: width * .1
        resolution: 384; animated: false
        file: header.details.cover || ""; seed: header.playlistName
        Accessible.name: "歌单封面"
    }
    Row {
        id: playActions; objectName: "playlistPlayActions"
        x: header.buttonLeft; spacing: 10
        ShellButton {id:playAllButton;objectName: "playlistPlayAll"; text:header.width<340?"播放":"播放全部"; symbol: "play"; primary: true; enabled: header.hasTracks; onClicked: header.playRequested() }
        PlayModeButton {objectName:"playlistMode";width:Math.min(160,Math.max(112,header.width-header.buttonLeft-playAllButton.width-10));enabled:header.hasTracks;onPlayRequested:mode=>header.playModeRequested(mode)}
    }
    Row {
        id: editActions; objectName: "playlistEditActions"
        x: header.buttonLeft; y:header.reflow(header.blend(header.rowStep,Theme.controlHeight+6),42); spacing: 10
        ShellButton {id:optionsButton;objectName: "playlistOptions"; text: "歌单选项"; onClicked:header.optionsRequested(this) }
        ShellButton { objectName: "playlistUp";visible:!header.compactLayout; text: "上移"; enabled: header.reorderEnabled; onClicked: header.moveRequested(-1) }
        ShellButton { objectName: "playlistDown";visible:!header.compactLayout; text: "下移"; enabled: header.reorderEnabled; onClicked: header.moveRequested(1) }
    }
    UiText {
        id: descriptionMeasure; visible: false
        width: Math.max(1, header.width - 180 - descriptionLink.implicitWidth - 8)
        text: header.details.description || "未设置"
        font.pixelSize: 13; wrapMode: Text.Wrap; maximumLineCount: 2
    }
    Item {
        id:metadataViewport;objectName:"playlistMetadataViewport";clip:true
        x:header.reflow(header.metadataDocked?header.compactSize+20+header.buttonWidth+28:180,header.buttonLeft+optionsButton.width+12)
        y:header.reflow(header.metadataDocked?0:header.expandedMetadataTop,42)
        width:Math.max(0,header.width-x)
        height:Math.max(0,Math.min(header.height-y,header.reflow(header.metadataDocked?header.compactSize:header.expandedMetadataHeight,Theme.controlHeight)))
        Item {
            anchors.fill:parent
            transform:Translate {y:header.metadataDocked?0:-header.expandedMetadataHeight*header.metadataExit*(1-header.layoutProgress)}
            Item {
                id: statistics; objectName: "playlistStatisticsRow"
                x:0;y:0;width:parent.width;height:header.reflow(header.metadataDocked?Theme.controlHeight:22,Theme.controlHeight)
                Row {
                    id: statisticsContent;objectName:"playlistStatisticsContent"
                    x:header.metadataRowX(width,statistics.width)
                    anchors.verticalCenter: parent.verticalCenter; spacing: 8
                    UiText {
                        id: counts; objectName: "playlistCounts"
                        width: Math.min(implicitWidth, Math.max(0, statistics.width - tagsLink.width - 16))
                        text:(header.details.trackCount||0)+(header.compactLayout&&header.width<400?" 首 · ":" 首歌曲 · ")+(header.details.plays||0)+(header.compactLayout&&header.width<400?" 次":" 次播放")
                        color: Theme.secondary; font.pixelSize: 13; elide: Text.ElideRight
                    }
                    UiText {
                        id: tagsLink; text: "标签"; color: Theme.secondary; font.pixelSize: 13
                    }
                    UiText {
                        width: Math.min(implicitWidth, Math.max(0, statistics.width - counts.width - tagsLink.width - 16))
                        text: (header.details.tags || []).length ? header.details.tags.join("、") : "未设置"
                        color: Theme.secondary; font.pixelSize: 13; elide: Text.ElideRight
                    }
                }
            }
            Item {
                id: description; objectName: "playlistDescriptionRow";visible:!header.compactLayout
                x:0;y:header.metadataDocked?42:header.rowStep;width:parent.width
                height:header.metadataDocked?Theme.controlHeight:Math.min(40,descriptionMeasure.implicitHeight)
                Row {
                    objectName:"playlistDescriptionContent";x:header.metadataRowX(width,description.width)
                    anchors.verticalCenter:parent.verticalCenter;spacing:8
                    UiText { id: descriptionLink; text: "简介"; color: Theme.secondary; font.pixelSize: 13 }
                    UiText {
                        width: Math.min(implicitWidth, Math.max(0, description.width - descriptionLink.width - 8))
                        text: header.details.description || "未设置"
                        color: Theme.secondary; font.pixelSize: 13
                        wrapMode: header.metadataDocked ? Text.NoWrap : Text.Wrap
                        maximumLineCount: 2; elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
