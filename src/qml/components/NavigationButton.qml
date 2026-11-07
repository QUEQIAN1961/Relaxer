import QtQuick
import QtQuick.Controls
import "../theme"
Button {
    id: control
    required property string marker
    property string textMarker: ""
    property bool cloud:false
    property bool selected: false
    property bool compact: false
    property real compactWidth: 48
    property real compactProgress:compact?1:0
    Behavior on compactProgress {NumberAnimation{duration:settingsBridge.values.reduceMotion?0:280;easing.type:Easing.OutCubic}}
    implicitHeight: 36
    implicitWidth: 145
    leftPadding: 10
    rightPadding: 10
    topPadding: 7
    bottomPadding: 7
    hoverEnabled: true
    HintBubble { visible:control.compact&&control.hovered; text:control.text }
    Accessible.name: text
    font.pixelSize: Theme.fontSize
    // Interpolate toward the final compact center, not the current center of
    // a shrinking button, which would send the icon out and then back again.
    readonly property real markerOffset:(compactWidth-leftPadding-rightPadding-20)/2*compactProgress
    contentItem: Item {
        implicitWidth: 30 + caption.implicitWidth
        implicitHeight: 22
        Symbol {
            visible:!control.textMarker.length
            width:20;height:20;x:control.markerOffset;anchors.verticalCenter:parent.verticalCenter
            name: control.marker
            color: !control.enabled ? Theme.disabled : control.selected ? Theme.accent : Theme.text
        }
        UiText {
            visible:control.textMarker.length>0
            x:control.markerOffset;width:20;height:22;anchors.verticalCenter:parent.verticalCenter
            text:control.textMarker;textFormat:Text.PlainText;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;font.pixelSize:18;font.weight:Font.DemiBold
            color:!control.enabled?Theme.disabled:control.selected?Theme.accent:Theme.text
        }
        UiText {
            id:caption;objectName:"navigationCaption";opacity:1-control.compactProgress;visible:opacity>0
            x:30-8*control.compactProgress; width: Math.max(0, parent.width - x-(control.cloud?23:0)); anchors.verticalCenter: parent.verticalCenter
            text: control.text; textFormat: Text.PlainText; elide: Text.ElideRight
            font: control.font
            color: !control.enabled ? Theme.disabled : control.selected ? Theme.accent : Theme.text
        }
        Symbol {objectName:"cloudPlaylistMarker";visible:control.cloud;opacity:1-control.compactProgress;anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;width:16;height:16;name:"cloud";color:Theme.secondary}
    }
    background: HighlightSurface {
        radius: 8
        positionRevision:control.x+control.y+control.compactProgress;backdrop:Theme.sidebarBackdrop;pressed:control.down;selected:control.selected;hovered:control.hovered
        border.color: control.activeFocus ? Theme.accent : "transparent"
    }
}
