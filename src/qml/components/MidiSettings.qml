import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../theme"
SettingsGroup {
    id:group;title:"MIDI";objectName:"settingsMidiGroup"
    readonly property bool dialogVisible:fontDialog.visible||removeDialog.visible||helpDialog.visible
    headingAccessory:Component {
        Item {
            implicitWidth:20;implicitHeight:20
            ToolButton {
                objectName:"midiHelpQuestion";width:20;height:20;text:"?";font.pixelSize:14;focusPolicy:Qt.TabFocus
                Accessible.name:"MIDI 功能说明"
                background:Rectangle{radius:10;color:Theme.hover;border.color:Theme.border}
                onClicked:helpDialog.open()
                HintBubble{visible:parent.hovered;text:"MIDI 功能说明"}
            }
        }
    }
    SettingsRow {label:"音色库"
        ColumnLayout {
            width:parent.width;spacing:8
            Repeater {
                model:settingsBridge.midiSoundFonts
                delegate:Rectangle {
                    required property var modelData
                    Layout.fillWidth:true;implicitHeight:entryLayout.implicitHeight+16
                    objectName:"midiSoundFontCard-"+modelData.id
                    radius:8;color:modelData.selected?Theme.hover:Theme.card;border.width:1;border.color:Theme.border
                    ColumnLayout {
                        id:entryLayout;anchors{left:parent.left;right:parent.right;top:parent.top;margins:8} spacing:6
                        RowLayout {
                            Layout.fillWidth:true;spacing:6
                            UiText {Layout.fillWidth:true;Layout.minimumWidth:0;text:modelData.name;textFormat:Text.PlainText;color:Theme.text;basePixelSize:13;wrapMode:Text.Wrap}
                            Symbol {objectName:"midiSoundFontSelectedMark-"+modelData.id;Layout.preferredWidth:14;Layout.preferredHeight:14;visible:modelData.selected;name:"check";color:Theme.secondary;Accessible.ignored:true}
                            UiText {objectName:"midiSoundFontSelectedLabel-"+modelData.id;visible:modelData.selected;text:"已选择";basePixelSize:12;color:Theme.secondary}
                        }
                        UiText {Layout.fillWidth:true;text:modelData.format+" · "+(modelData.size/1048576).toFixed(1)+" MB"+(modelData.active?" · 本次启动配置":"")+(!modelData.available?" · 文件不可用":"");color:Theme.secondary;basePixelSize:12;wrapMode:Text.Wrap}
                        Flow {
                            Layout.fillWidth:true;spacing:8;visible:!modelData.selected||modelData.removable
                            ShellButton {objectName:"useMidiFont-"+modelData.id;text:modelData.selected?"已选择":"使用";visible:!modelData.selected;enabled:!settingsBridge.midiSoundFontBusy&&!modelData.selected&&modelData.available;onClicked:settingsBridge.useMidiSoundFont(modelData.id)}
                            ShellButton {objectName:"removeMidiFont-"+modelData.id;text:"移除";visible:modelData.removable;enabled:!settingsBridge.midiSoundFontBusy;onClicked:{removeDialog.fontId=modelData.id;removeDialog.fontName=modelData.name;removeDialog.open()}}
                        }
                    }
                }
            }
            ShellButton {objectName:"chooseMidiSoundFont";text:settingsBridge.midiSoundFontBusy?"正在导入…":"导入 SF2 / SF3…";enabled:!settingsBridge.midiSoundFontBusy;onClicked:fontDialog.open()}
            UiText {objectName:"midiSoundFontHint";Layout.fillWidth:true;visible:text.length>0;text:settingsBridge.midiSoundFontStatus.length?settingsBridge.midiSoundFontStatus:settingsBridge.midiSoundFontRestartRequired?"已保存，下次启动生效":"";textFormat:Text.PlainText;wrapMode:Text.Wrap;basePixelSize:12;color:Theme.secondary}
        }
    }
    SettingsRow {label:"混响"
        ToggleButton {objectName:"midiReverb";text:"启用";checked:settingsBridge.midiSynthesis.reverb;onClicked:settingsBridge.setMidiSynthesis("reverb",checked)}
        SettingsNumber {objectName:"midiReverbLevel";controlPrefix:"midiReverbLevel";enabled:settingsBridge.midiSynthesis.reverb;from:0;to:100;value:settingsBridge.midiSynthesis.reverbLevel;onValueModified:settingsBridge.setMidiSynthesis("reverbLevel",value)}
    }
    SettingsRow {label:"合唱"
        ToggleButton {objectName:"midiChorus";text:"启用";checked:settingsBridge.midiSynthesis.chorus;onClicked:settingsBridge.setMidiSynthesis("chorus",checked)}
        SettingsNumber {objectName:"midiChorusLevel";controlPrefix:"midiChorusLevel";enabled:settingsBridge.midiSynthesis.chorus;from:0;to:100;value:settingsBridge.midiSynthesis.chorusLevel;onValueModified:settingsBridge.setMidiSynthesis("chorusLevel",value)}
    }
    SettingsRow {label:"采样插值"
        ChoiceBox {objectName:"midiInterpolation";width:Math.min(240,parent.width);model:["四阶（默认）","Sinc"];currentIndex:settingsBridge.midiSynthesis.interpolation===7?1:0;onActivated:settingsBridge.setMidiSynthesis("interpolation",currentIndex===1?7:4)}
    }
    SettingsRow {label:"最大复音数"
        SettingsNumber {objectName:"midiPolyphony";controlPrefix:"midiPolyphony";suffix:"";from:128;to:2048;stepSize:128;value:settingsBridge.midiSynthesis.polyphony;onValueModified:settingsBridge.setMidiSynthesis("polyphony",value)}
    }
    SettingsRow {label:"声音设置"
        ShellButton {objectName:"resetMidiSynthesis";text:"恢复默认声音";onClicked:settingsBridge.resetMidiSynthesis()}
    }
    ConfirmDialog {
        id:helpDialog;objectName:"midiHelpDialog";parent:group.Overlay.overlay
        title:"MIDI 功能说明";modal:true;standardButtons:Dialog.Close
        width:Math.min(480,parent?parent.width-24:480)
        height:Math.min(implicitHeight,parent?parent.height-24:implicitHeight)
        anchors.centerIn:parent
        closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
        contentItem:ScrollView {
            id:helpScroll;implicitHeight:helpText.implicitHeight;contentWidth:availableWidth;contentHeight:helpText.implicitHeight;clip:true
            ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
            UiText {
                id:helpText;objectName:"midiHelpText";width:helpScroll.availableWidth;textFormat:Text.PlainText;wrapMode:Text.Wrap;basePixelSize:14;color:Theme.text
                text:"音色库：支持 SF2、SF3，决定 MIDI 乐器的声音。导入会保存应用副本，移除不会删除原文件；本次启动配置的音色需切换并重启后才能移除。\n\n混响：模拟空间感和声音尾音。\n\n合唱：叠加轻微差异，让声音更宽厚。\n\n采样插值：影响采样变调时的处理。四阶为默认；Sinc 使用更多 CPU，听感随音色而异。\n\n最大复音数：限制同时发声的数量。密集乐段可提高上限，也会增加 CPU 使用。\n\n恢复默认声音：重置 MIDI 声音参数，保留所选音色库。\n\n生效时间：音色库与声音参数均在下次启动生效，仅作用于 MIDI。"
            }
            SmoothWheelHandler{flickable:helpScroll.contentItem as Flickable}
        }
    }
    FileDialog {id:fontDialog;objectName:"midiSoundFontDialog";title:"导入 MIDI 音色库";fileMode:FileDialog.OpenFile;nameFilters:["SoundFont 音色库 (*.sf2 *.sf3)"];onAccepted:settingsBridge.selectMidiSoundFont(selectedFile)}
    ConfirmDialog {id:removeDialog;parent:group.Overlay.overlay;property string fontId;property string fontName;title:"移除音色库";standardButtons:Dialog.Ok|Dialog.Cancel
        implicitWidth:Math.min(400,parent?Math.max(0,parent.width-24):400)
        anchors.centerIn:parent
        contentItem:UiText {text:"移除“"+removeDialog.fontName+"”的应用副本？原始文件会保留。";textFormat:Text.PlainText;wrapMode:Text.Wrap;color:Theme.text}
        onAccepted:settingsBridge.removeMidiSoundFont(fontId)
    }
}
