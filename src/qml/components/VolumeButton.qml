import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../theme"

IconButton {
    id: control
    property bool popupOnHover: Theme.compact
    readonly property bool popupVisible:volumePopup.visible
    symbol:playbackBridge.muted?"volumeMuted":playbackBridge.volume===0?"volumeZero":playbackBridge.volume<34?"volumeLow":playbackBridge.volume<67?"volumeMedium":"volumeHigh"
    text:playbackBridge.muted?"取消静音":"静音"
    opacity:volumePopup.visible?0:1
    onClicked:playbackBridge.toggleMute()
    function closePopup(){volumePopup.close()}
    function openVolume(){
        const center=mapToGlobal(Qt.point(width/2,0))
        const half=Math.min(center.x-Screen.virtualX,Screen.virtualX+Screen.width-center.x)
        volumePopup.width=Math.min(102,Math.max(36,Math.floor(2*half)))
        volumePopup.open()
    }
    onHoveredChanged:{
        if(hovered&&popupOnHover)openVolume()
        else if(volumePopup.opened)Qt.callLater(function(){
            if(!control.hovered&&!volumeHover.hovered)volumePopup.close()
        })
    }
    onPopupOnHoverChanged:if(!popupOnHover)closePopup()
    onVisibleChanged:if(!visible)closePopup()
    Popup {
        id:volumePopup;objectName:"compactVolumePopup";popupType:Popup.Window
        x:(control.width-width)/2;y:-50;width:102;height:50+control.height;padding:0
        margins:0;modal:false;dim:false;focus:false
        closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
        // Nonzero alpha keeps Windows input continuous through the visual gap.
        background:Rectangle {color:Qt.rgba(0,0,0,1/255)}
        contentItem:Item {
            objectName:"compactVolumeHoverArea"
            HoverHandler {id:volumeHover;objectName:"compactVolumeHoverHandler";onHoveredChanged:if(!hovered&&volumePopup.opened)volumePopup.close()}
            MouseArea {anchors.fill:parent;acceptedButtons:Qt.AllButtons;onWheel:wheel=>wheel.accepted=true}
            Rectangle {
                objectName:"compactVolumePanel";width:parent.width;height:42;radius:10;color:Theme.card;border.color:Theme.border
                PlaybackSlider {
                    objectName:"compactVolumeSlider";anchors.centerIn:parent;width:parent.width-24
                    from:0;to:100;stepSize:1;value:playbackBridge.volume
                    onMoved:playbackBridge.setVolume(Math.round(value));Accessible.name:"音量"
                }
            }
            IconButton {
                objectName:"compactVolumeMuteButton";x:(parent.width-width)/2;y:50;width:control.width;height:control.height;padding:control.padding
                symbol:control.symbol;text:control.text;onClicked:playbackBridge.toggleMute()
            }
        }
    }
}
