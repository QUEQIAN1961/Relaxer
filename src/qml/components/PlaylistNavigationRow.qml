import QtQuick
import QtQuick.Controls
import "../theme"
Item {
    id: row
    required property int index
    required property string playlistKey
    required property string name
    required property bool cloud
    required property ListView listView
    required property var orderModel
    property bool sorting:false
    property real sortingReveal:0
    property bool initialized:false
    Behavior on sortingReveal {NumberAnimation{duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.InOutCubic}}
    onSortingChanged:if(initialized)sortingReveal=sorting?1:0
    Component.onCompleted:{initialized=true;sortingReveal=sorting?1:0}
    property bool compact:false
    property bool selected:false
    signal activated()
    height:36;clip:true
    readonly property string savedMarker:libraryBridge.playlistMarkers[playlistKey]||"icon:music"
    readonly property string marker:"mark-"+(savedMarker.startsWith("icon:")?savedMarker.slice(5):"music")
    readonly property string textMarker:savedMarker.startsWith("text:")?savedMarker.slice(5):""
    function reorderAt(point) {
        const local=listView.contentItem.mapFromItem(null,point.x,point.y)
        const target=Math.max(0,Math.min(orderModel.count-1,Math.floor(local.y/(height+listView.spacing))))
        if(index!==target)orderModel.move(index,target,1)
    }
    Rectangle {anchors.fill:parent;radius:8;color:Theme.hover;visible:reorderDrag.active}
    Item {
        objectName:"playlistOrderHandle-"+row.playlistKey
        x:-22*(1-row.sortingReveal);width:22;height:parent.height;visible:row.sortingReveal>0;opacity:row.sortingReveal
        Column {
            anchors.centerIn:parent;spacing:3
            Repeater {model:3;Rectangle {width:13;height:1.5;radius:.75;color:Theme.secondary}}
        }
        HoverHandler {cursorShape:reorderDrag.active?Qt.ClosedHandCursor:Qt.OpenHandCursor}
        DragHandler {
            id:reorderDrag;objectName:"playlistOrderDrag-"+row.playlistKey
            target:null;enabled:row.sorting
            onCentroidChanged:if(active)row.reorderAt(centroid.scenePosition)
            onActiveChanged:if(active)row.reorderAt(centroid.scenePosition)
        }
        Timer {
            interval:32;repeat:true;running:reorderDrag.active
            onTriggered:{
                const point=reorderDrag.centroid.scenePosition
                const local=row.listView.mapFromItem(null,point.x,point.y)
                const step=local.y<24?-8:local.y>row.listView.height-24?8:0
                if(step){row.listView.contentY=Math.max(0,Math.min(Math.max(0,row.listView.contentHeight-row.listView.height),row.listView.contentY+step));row.reorderAt(point)}
            }
        }
    }
    NavigationButton {
        x:22*row.sortingReveal;width:parent.width-x;height:parent.height
        compact:row.compact;compactWidth:48-x;text:row.name;selected:row.selected;cloud:row.cloud
        marker:row.marker;textMarker:row.textMarker
        onClicked:if(!row.sorting)row.activated()
    }
}
