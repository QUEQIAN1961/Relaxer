import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Layouts
import "../theme"

Popup {
    id:panel
    objectName:"videoPartsPanel"
    property var rows:[]
    property int currentPart:1
    property real reveal:0
    property real panelTop:76
    property Item backdropSource:parent
    signal partRequested(int page)
    readonly property real exitBottom:parent.Window.window?parent.Window.window.height-parent.mapToItem(parent.Window.window.contentItem,0,0).y:parent.height
    readonly property var filteredRows:rows.filter(row=>!search.text.trim().length||String(row.part||"").toLowerCase().indexOf(search.text.trim().toLowerCase())>=0||String(row.page).indexOf(search.text.trim())>=0)
    popupType:Popup.Item
    width:Theme.compact?parent.width-2*Theme.pageMargin:Math.min(320,parent.width*.4)
    height:Theme.compact?Math.min(520,parent.height*.72):Math.max(100,parent.height-panelTop-12)
    x:parent.width-Theme.pageMargin-width+(Theme.compact?0:(width+Theme.pageMargin)*(1-reveal))
    y:(Theme.compact?parent.height-height-12:panelTop)+(Theme.compact?(exitBottom-(parent.height-height-12))*(1-reveal):0)
    opacity:1
    margins:-1
    padding:16;focus:true;modal:false;dim:Theme.compact
    closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutsideParent
    onAboutToShow:{search.text="";Qt.callLater(function(){parts.forceLayout();const index=panel.filteredRows.findIndex(row=>Number(row.page)===panel.currentPart);if(index>=0)parts.positionViewAtIndex(index,ListView.Contain)})}
    enter:Transition {NumberAnimation {target:panel;property:"reveal";from:0;to:1;duration:settingsBridge.values.reduceMotion?0:320;easing.type:Easing.OutCubic}}
    exit:Transition {NumberAnimation {target:panel;property:"reveal";to:0;duration:settingsBridge.values.reduceMotion?0:240;easing.type:Easing.InOutCubic}}
    // Popup.Item is rendered in the window overlay, outside backdropSource;
    // sampling the player therefore cannot feed this panel back into itself.
    background:GlassSurface {
        objectName:"videoPartsGlassSurface";radius:12;liquid:true;active:panel.visible
        backdrop:panel.backdropSource;effectName:"videoPartsBackdropBlur"
        sampleRect:{
            const origin=panel.backdropSource&&panel.parent?panel.parent.mapToItem(panel.backdropSource,panel.x,panel.y):Qt.point(0,0)
            return Qt.rect(origin.x,origin.y,panel.width,panel.height)
        }
    }
    contentItem:ColumnLayout {
        spacing:12
        RowLayout {
            Layout.fillWidth:true;spacing:8
            UiText {text:"选集";basePixelSize:18;font.weight:Font.DemiBold;color:Theme.text}
            UiText {text:panel.rows.length+"P";color:Theme.secondary;Layout.fillWidth:true}
            IconButton {objectName:"closeVideoParts";symbol:"close";tip:"关闭选集";showTooltip:true;Layout.preferredWidth:30;Layout.preferredHeight:30;onClicked:panel.close()}
        }
        EntryField {id:search;objectName:"videoPartsSearch";Layout.fillWidth:true;placeholderText:"搜索分P";selectByMouse:true}
        ListView {
            id:parts;objectName:"videoPartsList";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:6
            model:panel.filteredRows;boundsBehavior:Flickable.StopAtBounds
            ScrollBar.vertical:ListScrollBar{}
            SmoothWheelHandler {flickable:parts}
            delegate:Rectangle {
                id:partRow;required property var modelData
                objectName:"videoPart-"+modelData.page
                readonly property bool current:Number(modelData.page)===panel.currentPart
                width:parts.width-10;height:Math.max(58,title.implicitHeight+26);radius:8
                color:current?Theme.selected:partHover.hovered?Theme.hover:"transparent"
                border.width:current?1:0;border.color:Theme.accent
                RowLayout {
                    anchors.fill:parent;anchors.margins:10;spacing:10
                    UiText {text:"P"+modelData.page;color:current?Theme.accent:Theme.secondary;basePixelSize:13;Layout.alignment:Qt.AlignVCenter;Layout.preferredWidth:34}
                    UiText {id:title;text:modelData.part||"未命名分P";textFormat:Text.PlainText;Layout.fillWidth:true;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight;color:current?Theme.accent:Theme.text;basePixelSize:14}
                    UiText {text:Math.floor(Number(modelData.duration||0)/60)+":"+String(Number(modelData.duration||0)%60).padStart(2,"0");color:Theme.secondary;basePixelSize:12}
                }
                MouseArea {anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:panel.partRequested(Number(partRow.modelData.page))}
                HoverHandler {id:partHover}
                HintBubble {visible:partHover.hovered;text:modelData.part||"未命名分P"}
            }
            EmptyState {anchors.centerIn:parent;width:parent.width;visible:!parts.count;heading:"没有匹配的分P";description:"试试其他标题或P序号。"}
        }
        RowLayout {
            Layout.fillWidth:true;spacing:8
            ShellButton {objectName:"previousVideoPart";text:"上一P";Layout.fillWidth:true;enabled:panel.currentPart>1;onClicked:panel.partRequested(panel.currentPart-1)}
            ShellButton {objectName:"nextVideoPart";text:"下一P";Layout.fillWidth:true;enabled:panel.currentPart<panel.rows.length;onClicked:panel.partRequested(panel.currentPart+1)}
        }
    }
}
