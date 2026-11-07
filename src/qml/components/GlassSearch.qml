import QtQuick
import QtQuick.Controls
import "../theme"

Item {
    id:control
    required property Item backdrop
    property string query:""
    property string searchName:"library"
    property string searchLabel:"搜索音乐库"
    property string placeholderText:"搜索歌曲、歌手、专辑"
    readonly property bool opened:searchPopup.visible
    property point anchor:Qt.point(0,0)
    signal queryEdited(string value)
    signal accepted(string value)
    implicitWidth:36;implicitHeight:36
    function place() {
        anchor=searchButton.mapToItem(backdrop,0,0)
        searchPopup.expandedWidth=Math.min(280,Math.max(160,anchor.x+searchButton.width-8),backdrop.width-16)
    }
    function open() {place();searchPopup.open();Qt.callLater(function(){input.forceActiveFocus();input.selectAll()})}
    function close() {searchPopup.close()}
    onVisibleChanged:if(!visible)close()
    Connections {target:control.backdrop;function onWidthChanged(){if(control.opened)Qt.callLater(control.place)}}
    IconButton {
        id:searchButton;objectName:control.searchName+"SearchButton";anchors.fill:parent
        symbol:"search";text:control.searchLabel;showTooltip:true
        iconColor:control.query.length?Theme.accent:Theme.text
        onClicked:{if(searchPopup.visible)control.close();else control.open()}
    }
    Popup {
        id:searchPopup;objectName:control.searchName+"SearchPopup";parent:searchButton;popupType:Popup.Item
        property real reveal:0
        property real expandedWidth:280
        x:Math.max(8-control.anchor.x,searchButton.width-width);y:(searchButton.height-height)/2
        width:36+(expandedWidth-36)*reveal;height:42;padding:0
        modal:false;dim:false;focus:true
        closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutsideParent
        enter:Transition {NumberAnimation{target:searchPopup;property:"reveal";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:220;easing.type:Easing.OutCubic}}
        exit:Transition {NumberAnimation{target:searchPopup;property:"reveal";to:0;duration:settingsBridge.values.reduceMotion?0:100;easing.type:Easing.InCubic}}
        background:GlassSurface {
            objectName:"searchGlassSurface";backdrop:control.backdrop;active:searchPopup.visible;effectName:"searchBackdropBlur"
            tintOpacity:Theme.dark?.76:.68
            sampleRect:Qt.rect(control.anchor.x+searchPopup.x,control.anchor.y+searchPopup.y,searchPopup.width,searchPopup.height)
        }
        contentItem:Item {
            clip:true
            EntryField {
                id:input;objectName:control.searchName+"Search";anchors.fill:parent
                text:control.query;placeholderText:control.placeholderText
                leftPadding:40;rightPadding:clearButton.visible?36:12
                opacity:searchPopup.reveal
                background:Item{}
                onTextEdited:control.queryEdited(text)
                onAccepted:{control.accepted(text);control.close()}
                Accessible.name:control.searchLabel
            }
            Symbol {x:12;anchors.verticalCenter:parent.verticalCenter;width:18;height:18;name:"search";color:Theme.secondary;opacity:searchPopup.reveal}
            IconButton {
                id:clearButton;objectName:"clearLibrarySearch";anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter
                anchors.rightMargin:4;width:28;height:28;padding:7;visible:input.text.length>0;opacity:searchPopup.reveal
                symbol:"close";text:"清空搜索";onClicked:{control.queryEdited("");input.forceActiveFocus()}
            }
        }
    }
}
