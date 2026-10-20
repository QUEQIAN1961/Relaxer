import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"
ColumnLayout {
    objectName: "playingPage"
    property string selectedQueueKey: "0"
    UiText { text: "播放队列"; font.pixelSize: Theme.headingSize; font.weight: Font.DemiBold; color: Theme.text }
    UiText { text: queueModel.count + " 首歌曲 · " + ["顺序播放", "列表循环", "单曲循环", "随机播放"][playbackBridge.mode]; color: Theme.secondary }
    TrackList {
        Layout.fillWidth: true; Layout.fillHeight: true
        trackModel: queueModel; selectedKey: selectedQueueKey; currentKey: playbackBridge.trackId
        emptyHeading: "播放队列为空"
        emptyDescription: "从本地音乐或歌单开始播放，音乐会出现在这里。"
        onSelected: key => selectedQueueKey = key
        onPlayRequested: key => playbackBridge.playQueued(key)
        onFavoriteRequested: key => libraryBridge.toggleFavorite(key)
    }
    RowLayout {
        ShellButton { text: "播放选中"; enabled: queueModel.indexForKey(selectedQueueKey) >= 0; onClicked: playbackBridge.playQueued(selectedQueueKey) }
        ShellButton { text: "移出队列"; enabled: queueModel.indexForKey(selectedQueueKey) >= 0; onClicked: playbackBridge.removeFromQueue(selectedQueueKey) }
        UiText { Layout.fillWidth: true; text: "随机模式会随机选择下一首；此处保留原队列顺序。"; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 14 }
    }
    Connections { target: queueModel; function onCountChanged() { if (queueModel.indexForKey(selectedQueueKey) < 0) selectedQueueKey = "0" } }
}
