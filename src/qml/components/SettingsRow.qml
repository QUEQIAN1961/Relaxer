import QtQuick
import QtQuick.Layouts
import "../theme"

Item {
    id:row
    property string label
    property string description:""
    property var optionLabels:[]
    readonly property bool optionsInline:!stacked||(optionLabels.length>0&&optionMeasure.implicitWidth<=controlFlow.width)
    default property alias controls: controlFlow.data
    readonly property bool stacked:parent&&parent.stacked!==undefined?parent.stacked:width<620
    readonly property real firstLineHeight:Math.max(labels.implicitHeight,Theme.controlHeight)
    implicitHeight:stacked?labels.implicitHeight+8+controlFlow.implicitHeight:Math.max(firstLineHeight,controlFlow.y+controlFlow.implicitHeight)
    Layout.fillWidth:true
    Layout.minimumWidth:0
    Column {
        id:labels;objectName:row.objectName+"Labels";width:row.stacked?row.width:Math.min(row.width*.45,Math.max(Math.min(220,row.width*.32)*.7,Math.ceil(labelMeasure.implicitWidth)+2));spacing:3
        y:row.stacked?0:(row.firstLineHeight-height)/2
        UiText {id:labelText;objectName:row.objectName+"LabelText";width:parent.width;text:row.label;wrapMode:Text.Wrap;color:Theme.text;basePixelSize:14}
        UiText {width:parent.width;visible:text.length>0;text:row.description;wrapMode:Text.Wrap;color:Theme.secondary;basePixelSize:12}
    }
    Text {id:labelMeasure;visible:false;text:row.label;font:labelText.font;textFormat:Text.PlainText}
    Row {
        // Keep the positioner active for natural widths without painting or
        // exposing its measurement controls to input/accessibility.
        id:optionMeasure;opacity:0;enabled:false;height:0;clip:true;spacing:8
        Repeater {model:row.optionLabels;ShellButton {required property string modelData;text:modelData;Accessible.ignored:true}}
    }
    Flow {
        id:controlFlow;objectName:row.objectName+"Controls";x:row.stacked?0:labels.width+24*.7;y:row.stacked?labels.implicitHeight+8:(row.firstLineHeight-Theme.controlHeight)/2
        width:Math.max(0,row.width-x);height:implicitHeight;spacing:8
    }
}
