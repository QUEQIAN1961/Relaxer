import QtQuick
import QtQuick.Controls
import "../theme"

Button {
    id:control
    property string provider:""
    property var song:({})
    property string quality:"hires"
    readonly property var task:{onlineDownloads.taskRevision;return onlineDownloads.task(provider,song)}
    readonly property bool downloaded:{onlineDownloads.revision;return onlineDownloads.isDownloaded(provider,song)}
    readonly property bool pending:["queued","resolving","downloading","converting"].indexOf(task.state)>=0
    readonly property bool canDownload:!downloaded&&!pending
    implicitWidth:36;implicitHeight:36;padding:7;hoverEnabled:true
    Accessible.name:downloaded?"已下载":pending?"下载中":"下载"
    focusPolicy:canDownload?Qt.TabFocus:Qt.NoFocus
    onClicked:if(canDownload)onlineDownloads.enqueue(provider,song,quality)
    contentItem:DownloadGlyph {
        active:control.pending;complete:control.downloaded;progress:control.task.progress||0
        indeterminate:control.task.state==="resolving"||control.task.state==="converting"||control.task.state==="downloading"&&!(control.task.total>0)
    }
    background:Rectangle {radius:8;color:control.down||control.hovered?Qt.rgba(0.5,0.5,0.5,0.06):"transparent";border.width:control.activeFocus?1:0;border.color:Theme.accent}
    HintBubble {visible:control.hovered;text:control.Accessible.name}
}
