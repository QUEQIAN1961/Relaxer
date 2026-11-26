import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../theme"

IconButton {
    id: control
    property bool popupOnHover: Theme.compact
    property Item backdropSource
    property Item secondaryBackdrop
    readonly property bool popupVisible:volumePopup.visible
    symbol:playbackBridge.muted?"volumeMuted":playbackBridge.volume===0?"volumeZero":playbackBridge.volume<34?"volumeLow":playbackBridge.volume<67?"volumeMedium":"volumeHigh"
    text:playbackBridge.muted?"取消静音":"静音"
    opacity:volumePopup.visible?0:1
    onClicked:playbackBridge.toggleMute()
    property point hoverOrigin:Qt.point(0,0)
    property bool towardPanel:true
    function acceptsHoverPoint(point){
        function inside(item){return item.contains(item.mapFromItem(hoverArea,point.x,point.y))}
        if(inside(popupMuteButton)){hoverOrigin=point;towardPanel=true;return true}
        if(inside(volumePanel)){hoverOrigin=point;towardPanel=false;return true}
        const target=towardPanel?volumePanel:popupMuteButton
        const left=target.mapToItem(hoverArea,0,towardPanel?target.height:0)
        const right=Qt.point(left.x+target.width,left.y)
        function side(a,b){return (b.x-a.x)*(point.y-a.y)-(b.y-a.y)*(point.x-a.x)}
        const a=side(hoverOrigin,left),b=side(left,right),c=side(right,hoverOrigin)
        return (a>=0&&b>=0&&c>=0)||(a<=0&&b<=0&&c<=0)
    }
    Connections {
        target:uiIntegration
        function onPointerMoved(position){if(volumePopup.opened&&!volumeSlider.pressed&&!control.acceptsHoverPoint(hoverArea.mapFromGlobal(position)))volumePopup.close()}
    }
    function closePopup(){volumePopup.close();devicePopup.close()}
    function openDevices(){
        volumePopup.close()
        const window=control.Window.window
        if(!window)return
        const p=control.mapToItem(window.contentItem,0,0)
        devicePopup.x=Math.max(12,Math.min(p.x+control.width-devicePopup.width,window.width-devicePopup.width-12))-p.x
        devicePopup.y=-Math.min(devicePopup.height+8,p.y-8)
        devicePopup.open()
    }
    TapHandler {acceptedButtons:Qt.RightButton;onTapped:control.openDevices()}
    function openVolume(){
        if(devicePopup.visible)return
        const center=mapToGlobal(Qt.point(width/2,0))
        const half=Math.min(center.x-Screen.virtualX,Screen.virtualX+Screen.width-center.x)
        volumePopup.width=Math.min(102,Math.max(36,Math.floor(2*half)))
        volumePopup.open()
    }
    onHoveredChanged:if(hovered&&popupOnHover)openVolume()
    onPopupOnHoverChanged:if(!popupOnHover)closePopup()
    onVisibleChanged:if(!visible)closePopup()
    Popup {
        id:volumePopup;objectName:"compactVolumePopup";popupType:Popup.Window
        x:(control.width-width)/2;y:-50;width:102;height:50+control.height;padding:0
        margins:0;modal:false;dim:false;focus:false
        closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
        onOpened:{control.hoverOrigin=Qt.point(width/2,50+control.height/2);control.towardPanel=true}
        // Pointer intent across the gap is checked against the safe triangle.
        background:Rectangle {color:Qt.rgba(0,0,0,1/255)}
        contentItem:Item {
            id:hoverArea;objectName:"compactVolumeHoverArea"
            HoverHandler {id:volumeHover;objectName:"compactVolumeHoverHandler";onHoveredChanged:if(!hovered&&volumePopup.opened&&!volumeSlider.pressed)volumePopup.close()}
            Rectangle {
                id:volumePanel;objectName:"compactVolumePanel";width:parent.width;height:42;radius:10;color:Theme.card;border.color:Theme.border
                PlaybackSlider {
                    id:volumeSlider;objectName:"compactVolumeSlider";anchors.centerIn:parent;width:parent.width-24
                    from:0;to:100;stepSize:1;value:playbackBridge.volume
                    onMoved:playbackBridge.setVolume(Math.round(value));Accessible.name:"音量"
                }
            }
            IconButton {
                id:popupMuteButton;objectName:"compactVolumeMuteButton";x:(parent.width-width)/2;y:50;width:control.width;height:control.height;padding:control.padding
                symbol:control.symbol;text:control.text;onClicked:playbackBridge.toggleMute()
                TapHandler {acceptedButtons:Qt.RightButton;onTapped:control.openDevices()}
            }
        }
    }
    Popup {
        id:devicePopup;objectName:"audioOutputPopup"
        popupType:control.Window.window&&control.Window.window.miniMode?Popup.Window:Popup.Item
        width:Math.min(360,control.Window.window?control.Window.window.width-24:360)
        height:Math.min(340,Math.max(94,48+playbackBridge.audioOutputs.length*40),control.Window.window?control.Window.window.height-48:340)
        padding:0;margins:8;focus:true;modal:false;dim:false
        Component.onCompleted:uiIntegration.registerDismissOnlyPopup(devicePopup)
        closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
        enter:Transition {NumberAnimation{property:"opacity";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:160;easing.type:Easing.OutCubic}}
        exit:Transition {NumberAnimation{property:"opacity";to:0;duration:settingsBridge.values.reduceMotion?0:120;easing.type:Easing.InCubic}}
        background:GlassSurface {
            objectName:"audioOutputGlass";radius:12;liquid:true;active:devicePopup.visible
            backdrop:control.backdropSource;secondaryBackdrop:control.secondaryBackdrop
            secondaryOffset:control.secondaryBackdrop&&control.backdropSource?control.secondaryBackdrop.mapToItem(control.backdropSource,0,0):Qt.point(0,0)
            sampleRect:{
                const origin=control.backdropSource?control.mapToItem(control.backdropSource,devicePopup.x,devicePopup.y):Qt.point(0,0)
                return Qt.rect(origin.x,origin.y,devicePopup.width,devicePopup.height)
            }
            effectName:"audioOutputBackdropBlur"
        }
        contentItem:Item {
            UiText {x:14;y:8;width:parent.width-28;height:28;text:"音频输出设备";color:Theme.text;font.weight:Font.DemiBold;verticalAlignment:Text.AlignVCenter}
            ListView {
                id:deviceList;objectName:"audioOutputList";x:6;y:40;width:parent.width-12;height:parent.height-46;clip:true
                model:playbackBridge.audioOutputs
                SmoothWheelHandler {flickable:deviceList}
                ScrollBar.vertical:ListScrollBar{}
                delegate:RoundMenuItem {
                    required property var modelData
                    required property int index
                    objectName:"audioOutputOption-"+index
                    width:deviceList.width;implicitHeight:40;text:modelData.name
                    checkable:true;checked:modelData.selected;enabled:modelData.available
                    contentItem:UiText {text:modelData.name;font:parent.font;color:parent.enabled?Theme.text:Theme.disabled;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter}
                    onTriggered:if(playbackBridge.selectAudioOutput(modelData.id))devicePopup.close()
                    HintBubble {visible:parent.hovered;text:modelData.name}
                }
            }
        }
    }
}
