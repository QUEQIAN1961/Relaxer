import QtQuick
import QtQuick.Layouts
import "../components"
import "../theme"
ColumnLayout {
    objectName: "localPage"
    spacing: 24
    Text { text: "本地音乐"; font.pixelSize: Theme.headingSize; font.weight: Font.DemiBold; color: Theme.text }
    Text {
        objectName: "librarySummary"
        text: appBridge.trackCount + " 首歌曲 · 来自当前资料库"
        color: Theme.secondary; font.pixelSize: Theme.fontSize
    }
    Rectangle {
        objectName: "libraryListContainer"
        Layout.fillWidth: true; Layout.fillHeight: true
        color: Theme.panel; radius: Theme.radius; border.color: Theme.border
        EmptyState {
            anchors.centerIn: parent
            width: Math.min(parent.width - 64, 480)
            heading: appBridge.trackCount === 0 ? "音乐，从这里开始" : "曲库已连接"
            description: appBridge.trackCount === 0
                ? "当前资料库暂无歌曲。\nC1 已建立列表容器；导入与曲目列表将在后续阶段迁移。\n现在可用默认 Widgets 模式管理音乐。"
                : "已读取真实曲库数量。曲目列表计划在 C2 接入。\n导入、搜索、收藏与歌单管理请使用默认 Widgets 模式。"
        }
    }
}
