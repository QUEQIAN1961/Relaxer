import QtQuick
import QtQuick.Effects
import QtQuick.Window
import "../theme"

Item {
    id:glass
    property Item backdrop
    property Item secondaryBackdrop
    property point secondaryOffset:Qt.point(0,0)
    property rect sampleRect:Qt.rect(0,0,width,height)
    property bool active:true
    property bool liquid:false
    property real radius:14
    property real tintOpacity:Theme.dark?.62:.55
    property string effectName:"glassBackdropBlur"
    readonly property bool software:GraphicsInfo.api===GraphicsInfo.Software
    readonly property bool presentationActive:active&&visible&&(!Window.window||(Window.window.visible&&Window.window.visibility!==Window.Minimized))
    Loader {
        anchors.fill:parent
        active:glass.presentationActive&&glass.backdrop&&!glass.software
        sourceComponent:Item {
            Item {
                id:scenePatch;anchors.fill:parent
                ShaderEffectSource {anchors.fill:parent;sourceItem:glass.backdrop;sourceRect:glass.sampleRect;live:true}
                ShaderEffectSource {
                    anchors.fill:parent;visible:!!glass.secondaryBackdrop;sourceItem:glass.secondaryBackdrop;live:true
                    sourceRect:Qt.rect(glass.sampleRect.x-glass.secondaryOffset.x,glass.sampleRect.y-glass.secondaryOffset.y,glass.sampleRect.width,glass.sampleRect.height)
                }
            }
            ShaderEffectSource {
                id:texture;visible:false;sourceItem:scenePatch;hideSource:true;live:true
            }
            Rectangle {id:mask;anchors.fill:parent;radius:glass.radius;color:"white";layer.enabled:true;visible:false}
            MultiEffect {
                objectName:glass.effectName;anchors.fill:parent;source:texture
                blurEnabled:true;blurMax:32;blur:1;autoPaddingEnabled:false
                maskEnabled:true;maskSource:mask
            }
        }
    }
    Rectangle {
        anchors.fill:parent;radius:glass.radius
        color:Qt.rgba(Theme.background.r,Theme.background.g,Theme.background.b,glass.software?.96:glass.tintOpacity)
        border.color:Qt.rgba(Theme.border.r,Theme.border.g,Theme.border.b,.8);border.width:1
    }
    Loader {
        anchors.fill:parent;active:glass.liquid&&glass.presentationActive
        sourceComponent:Rectangle {
            radius:glass.radius
            gradient:Gradient {
                GradientStop{position:0;color:Qt.rgba(1,1,1,Theme.dark?.12:.32)}
                GradientStop{position:.42;color:"transparent"}
                GradientStop{position:1;color:Qt.rgba(1,1,1,Theme.dark?.03:.10)}
            }
            border.width:1;border.color:Qt.rgba(1,1,1,Theme.dark?.18:.65)
        }
    }
}
