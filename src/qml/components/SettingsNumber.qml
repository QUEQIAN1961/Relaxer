import QtQuick
import QtQuick.Controls
import "../theme"
SpinBox {
    id:control
    property string suffix:" %"
    property string controlPrefix:"threshold"
    width:Math.min(180,parent.width);implicitHeight:Theme.controlHeight
    leftPadding:30;rightPadding:30;editable:true
    validator:RegularExpressionValidator {regularExpression:/\d*\s*(%|MB|px)?/}
    textFromValue:value=>value+suffix
    valueFromText:text=>Number(text.replace(suffix.trim(),"").trim())
    background:Rectangle {objectName:control.controlPrefix+"Frame";radius:8;color:Theme.card;border.color:control.activeFocus?Theme.accent:Theme.border}
    contentItem:TextInput {
        text:control.textFromValue(control.value,control.locale);font:control.font;color:Theme.text
        horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter
        readOnly:!control.editable;validator:control.validator;selectByMouse:true;inputMethodHints:Qt.ImhDigitsOnly
    }
    up.indicator:Rectangle {
        objectName:control.controlPrefix+"Increase";x:control.width-width;width:30;height:control.height;radius:8
        color:control.up.pressed?Theme.pressed:control.up.hovered?Theme.hover:"transparent"
        UiText {anchors.centerIn:parent;text:"+";basePixelSize:20}
    }
    down.indicator:Rectangle {
        objectName:control.controlPrefix+"Decrease";width:30;height:control.height;radius:8
        color:control.down.pressed?Theme.pressed:control.down.hovered?Theme.hover:"transparent"
        UiText {anchors.centerIn:parent;text:"−";basePixelSize:20}
    }
}
