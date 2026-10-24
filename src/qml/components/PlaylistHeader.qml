import QtQuick
import "../theme"

Item {
    id: header
    objectName: "playlistHeader"
    property var details: ({})
    property string playlistName: ""
    property bool hasTracks: false
    property bool reorderEnabled: false
    property bool collapsed: false
    property real collapseProgress: collapsed ? 1 : 0
    readonly property real compactSize: Theme.controlHeight * 2 + 6
    readonly property real coverSize: blend(160, compactSize)
    readonly property real buttonWidth: Math.max(playActions.implicitWidth, editActions.implicitWidth)
    readonly property real buttonLeft: blend(180, compactSize + 20)
    readonly property real metadataLeft: blend(180, compactSize + 20 + buttonWidth + 28)
    readonly property real rowStep: (160 - Math.min(40, descriptionMeasure.implicitHeight)) / 3
    signal playRequested()
    signal shuffleRequested()
    signal optionsRequested()
    signal moveRequested(int direction)
    signal metadataRequested()
    signal coverRequested()
    implicitHeight: coverSize
    function blend(expanded, compact) { return expanded + (compact - expanded) * collapseProgress }
    Behavior on collapseProgress {
        NumberAnimation { duration: settingsBridge.values.reduceMotion ? 0 : 220; easing.type: Easing.OutCubic }
    }
    Artwork {
        objectName: "playlistCover"
        width: header.coverSize; height: width; radius: width * .1
        resolution: 384; animated: false
        file: header.details.cover || ""; seed: header.playlistName
        TapHandler { onTapped: header.coverRequested() }
        Accessible.name: "更换歌单封面"
    }
    Row {
        id: playActions; objectName: "playlistPlayActions"
        x: header.buttonLeft; spacing: 10
        ShellButton { objectName: "playlistPlayAll"; text: "播放全部"; symbol: "play"; primary: true; enabled: header.hasTracks; onClicked: header.playRequested() }
        ShellButton { objectName: "playlistShuffle"; text: "随机播放"; symbol: "shuffle"; enabled: header.hasTracks; onClicked: header.shuffleRequested() }
    }
    Row {
        id: editActions; objectName: "playlistEditActions"
        x: header.buttonLeft; y: header.blend(header.rowStep, Theme.controlHeight + 6); spacing: 10
        ShellButton { objectName: "playlistOptions"; text: "歌单选项"; onClicked: header.optionsRequested() }
        ShellButton { objectName: "playlistUp"; text: "上移"; enabled: header.reorderEnabled; onClicked: header.moveRequested(-1) }
        ShellButton { objectName: "playlistDown"; text: "下移"; enabled: header.reorderEnabled; onClicked: header.moveRequested(1) }
    }
    Item {
        id: statistics; objectName: "playlistStatisticsRow"
        x: header.metadataLeft; y: header.blend(header.rowStep * 2, 0)
        width: Math.max(0, header.width - x); height: header.blend(22, Theme.controlHeight)
        Row {
            id: statisticsContent
            x: Math.max(0, statistics.width - width) * header.collapseProgress
            anchors.verticalCenter: parent.verticalCenter; spacing: 8
            UiText {
                id: counts; objectName: "playlistCounts"
                width: Math.min(implicitWidth, Math.max(0, statistics.width - tagsLink.width - 16))
                text: (header.details.trackCount || 0) + " 首歌曲 · " + (header.details.plays || 0) + " 次播放"
                color: Theme.secondary; font.pixelSize: 13; elide: Text.ElideRight
            }
            UiText {
                id: tagsLink; text: "标签"; color: Theme.secondary; font.pixelSize: 13
                TapHandler { onTapped: header.metadataRequested() }
                Accessible.name: "编辑歌单标签"
            }
            UiText {
                width: Math.min(implicitWidth, Math.max(0, statistics.width - counts.width - tagsLink.width - 16))
                text: (header.details.tags || []).length ? header.details.tags.join("、") : "未设置"
                color: Theme.secondary; font.pixelSize: 13; elide: Text.ElideRight
            }
        }
    }
    UiText {
        id: descriptionMeasure; visible: false
        width: Math.max(1, header.width - 180 - descriptionLink.implicitWidth - 8)
        text: header.details.description || "未设置"
        font.pixelSize: 13; wrapMode: Text.Wrap; maximumLineCount: 2
    }
    Item {
        id: description; objectName: "playlistDescriptionRow"
        x: header.metadataLeft; y: header.blend(header.rowStep * 3, Theme.controlHeight + 6)
        width: Math.max(0, header.width - x)
        height: header.blend(Math.min(40, descriptionMeasure.implicitHeight), Theme.controlHeight)
        Row {
            x: Math.max(0, description.width - width) * header.collapseProgress
            anchors.bottom: parent.bottom; spacing: 8
            UiText { id: descriptionLink; text: "简介"; color: Theme.secondary; font.pixelSize: 13 }
            UiText {
                width: Math.min(implicitWidth, Math.max(0, description.width - descriptionLink.width - 8))
                text: header.details.description || "未设置"
                color: Theme.secondary; font.pixelSize: 13
                wrapMode: header.collapseProgress > .001 ? Text.NoWrap : Text.Wrap
                maximumLineCount: 2; elide: Text.ElideRight
            }
        }
        TapHandler { onTapped: header.metadataRequested() }
        Accessible.name: "编辑歌单简介"
    }
}
