import QtQuick
import QtQuick.Layouts
import "../theme"

Item {
    id:group
    property string title
    property Component headingAccessory
    readonly property real headingAreaHeight:Math.max(heading.height,headingExtra.height)
    default property alias rows: body.data
    readonly property bool stacked:width<620
    implicitHeight:stacked?headingAreaHeight+10+body.implicitHeight:Math.max(headingAreaHeight,body.implicitHeight)
    Layout.fillWidth:true
    Layout.minimumWidth:0
    UiText {
        id:heading;objectName:group.objectName+"Heading"
        width:group.stacked?group.width:Math.min(group.width*.32,Math.max(Math.min(140,group.width*.22)*.7,Math.ceil(titleMeasure.implicitWidth)+2))
        height:group.stacked?implicitHeight:Math.max(Theme.controlHeight,implicitHeight)
        text:group.title;color:Theme.secondary;basePixelSize:16;font.weight:Font.DemiBold
        wrapMode:Text.Wrap;verticalAlignment:Text.AlignVCenter
    }
    Loader {id:headingExtra;x:heading.x+Math.ceil(titleMeasure.implicitWidth)+4;anchors.verticalCenter:heading.verticalCenter;width:item?item.implicitWidth:0;height:item?item.implicitHeight:0;sourceComponent:group.headingAccessory}
    Text {id:titleMeasure;visible:false;text:group.title;font:heading.font;textFormat:Text.PlainText}
    ColumnLayout {
        id:body;objectName:group.objectName+"Body"
        readonly property bool stacked:group.stacked
        x:group.stacked?0:heading.width+24*.7;y:group.stacked?group.headingAreaHeight+10:0
        width:Math.max(0,group.width-x);height:implicitHeight;spacing:12
    }
}
