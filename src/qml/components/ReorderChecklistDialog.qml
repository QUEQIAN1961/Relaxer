import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

ConfirmDialog {
    id: editor
    modal: true
    width: Math.min(420, parent.width - 24)
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    property var names:[]
    property var sourceRows:[]
    property var mandatoryIds:[]
    property string entryPrefix:"sidebar"
    property string saveButtonName:"saveSidebarSections"
    property string description:"拖动左侧三横线调整顺序，勾选右侧决定是否显示。"
    property var commit:rows=>false
    property bool dragging:false
    property bool settling:false
    property int dragFrom:-1
    property int dragTo:-1
    property int dragId:-1
    property real ghostY:0
    property string dragTitle:""
    property bool dragShown:false
    property bool cancelled:false
    function beginDrag(index,id,shown,y){
        if(settling)return
        dragFrom=index;dragTo=index;dragId=id;dragTitle=names[id];dragShown=shown;ghostY=y;dragging=true
    }
    function moveDrag(y){if(!dragging||settling)return;ghostY=Math.max(0,Math.min((sections.count-1)*48,y));dragTo=Math.max(0,Math.min(sections.count-1,Math.floor((ghostY+24)/48)))}
    function finishDrag(cancel){
        if(!dragging||settling)return
        cancelled=cancel;if(cancel)dragTo=dragFrom
        settling=true;dropMotion.from=ghostY;dropMotion.to=dragTo*48;dropMotion.restart()
    }
    function commitDrag(){
        if(!cancelled&&dragFrom!==dragTo)sections.move(dragFrom,dragTo,1)
        dragging=false;settling=false;dragFrom=-1;dragTo=-1;dragId=-1
    }
    NumberAnimation {id:dropMotion;target:editor;property:"ghostY";duration:settingsBridge.values.reduceMotion?0:180;easing.type:Easing.OutCubic;onFinished:editor.commitDrag()}
    ListModel { id: sections }
    function openEditor() {
        dropMotion.stop();dragging=false;settling=false
        sections.clear()
        const rows = editor.sourceRows
        for (let i=0;i<rows.length;++i) sections.append({sectionId:rows[i].id,shown:rows[i].visible})
        open()
    }
    function save() {
        const rows=[]
        for(let i=0;i<sections.count;++i) rows.push({id:sections.get(i).sectionId,visible:sections.get(i).shown})
        if(editor.commit(rows)) close()
    }
    contentItem: ColumnLayout {
        spacing: 12
        UiText {Layout.fillWidth:true;text:editor.description;wrapMode:Text.Wrap;color:Theme.secondary}
        ListView {
            id: list
            objectName:editor.entryPrefix+"SectionList"
            Layout.fillWidth:true;Layout.preferredHeight:Math.min(sections.count*48,editor.parent.height-220)
            model:sections;spacing:0;clip:true;interactive:contentHeight>height&&!editor.dragging
            delegate: Item {
                id: row
                required property int index
                required property int sectionId
                required property bool shown
                objectName:editor.entryPrefix+"Draft-"+sectionId
                width:list.width;height:48
                transform:Translate {
                    y:editor.dragging&&row.index!==editor.dragFrom ? editor.dragFrom<editor.dragTo&&row.index>editor.dragFrom&&row.index<=editor.dragTo?-48:editor.dragFrom>editor.dragTo&&row.index>=editor.dragTo&&row.index<editor.dragFrom?48:0:0
                    Behavior on y {enabled:editor.dragging;NumberAnimation {duration:settingsBridge.values.reduceMotion?0:160;easing.type:Easing.OutCubic}}
                }
                Rectangle {
                    id: panel
                    width:row.width;height:46;radius:8;color:handle.pressed?Theme.hover:Theme.card
                    opacity:editor.dragging&&row.sectionId===editor.dragId?0:1
                    RowLayout {
                        anchors.fill:parent;anchors.leftMargin:8;anchors.rightMargin:8;spacing:12
                        Item {
                            Layout.preferredWidth:32;Layout.fillHeight:true
                            UiText {anchors.centerIn:parent;text:"☰";color:Theme.secondary;basePixelSize:20}
                            MouseArea {
                                id:handle;objectName:editor.entryPrefix+"Drag-"+row.sectionId
                                anchors.fill:parent;cursorShape:pressed?Qt.ClosedHandCursor:Qt.OpenHandCursor
                                enabled:!editor.settling
                                property real grabOffset:0
                                onPressed:mouse=>{grabOffset=mouse.y;editor.beginDrag(row.index,row.sectionId,row.shown,row.y)}
                                onReleased:editor.finishDrag(false)
                                onCanceled:editor.finishDrag(true)
                                onPositionChanged:mouse=>{if(pressed&&editor.dragging)editor.moveDrag(mapToItem(list.contentItem,mouse.x,mouse.y).y-grabOffset)}
                            }
                        }
                        UiText {Layout.fillWidth:true;text:editor.names[row.sectionId];color:Theme.text}
                        MorphCheckBox {
                            objectName:editor.entryPrefix+"Shown-"+row.sectionId
                            checked:row.shown;enabled:editor.mandatoryIds.indexOf(row.sectionId)<0&&!editor.settling
                            Accessible.name:editor.names[row.sectionId]+"是否显示"
                            onToggled:sections.setProperty(row.index,"shown",checked)
                        }
                    }
                }
            }
        }
        Rectangle {
            objectName:editor.entryPrefix+"DragGhost";parent:list.contentItem;x:0;y:editor.ghostY;width:list.width;height:46;z:100;visible:editor.dragging
            radius:8;color:Theme.hover;border.color:Theme.accent
            RowLayout {anchors.fill:parent;anchors.leftMargin:8;anchors.rightMargin:8;spacing:12
                UiText {Layout.preferredWidth:32;text:"☰";horizontalAlignment:Text.AlignHCenter;color:Theme.secondary;basePixelSize:20}
                UiText {Layout.fillWidth:true;text:editor.dragTitle;color:Theme.text}
                MorphCheck {width:22;height:22;checked:editor.dragShown;animated:false}
            }
        }
        RowLayout {
            Layout.alignment:Qt.AlignRight
            ShellButton {text:"取消";onClicked:editor.close()}
            ShellButton {objectName:editor.saveButtonName;text:"保存";primary:true;enabled:!editor.dragging;onClicked:editor.save()}
        }
    }
}
