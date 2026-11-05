import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
ConfirmDialog{
    id:dialog;objectName:"playlistEditor";property string listKey:"0"
    property string iconMark:"music"
    property string originalName:""
    property bool textMark:false
    readonly property string selectedMarker:textMark?"text:"+(markInput.text.trim()||Array.from(nameInput.text.trim())[0]||"歌"):"icon:"+iconMark
    readonly property var iconNames:({"music":"音乐","headphones":"耳机","disc-3":"唱片","radio":"电台","mic-vocal":"麦克风","star":"星星","heart":"爱心","sun":"太阳","moon":"月亮","cloud-rain":"雨天","coffee":"咖啡","book-open":"阅读","leaf":"树叶","flame":"火焰","mountain":"山峰","gamepad-2":"游戏"})
    function create(){listKey="0";nameInput.text="新歌单";iconMark="music";textMark=false;markInput.text="";open()}
    function edit(key,name){
        listKey=key;nameInput.text=name;originalName=name
        const marker=libraryBridge.playlistMarkers[key]||"icon:music"
        textMark=marker.startsWith("text:");iconMark=textMark?"music":marker.slice(5);markInput.text=textMark?marker.slice(5):""
        open()
    }
    signal created(string key)
    width:Math.min(400,parent.width-32);anchors.centerIn:parent;modal:true
    title:listKey==="0"?"创建歌单":"编辑歌单"
    standardButtons:Dialog.Save|Dialog.Cancel
    contentItem:ColumnLayout{
        spacing:12
        UiText {text:"歌单名称";color:Theme.secondary}
        EntryField {id:nameInput;objectName:"playlistNameInput";Layout.fillWidth:true;maximumLength:120;selectByMouse:true;onAccepted:dialog.accept()}
        RowLayout {
            Layout.fillWidth:true;spacing:8
            UiText {text:"歌单标记";color:Theme.secondary;Layout.fillWidth:true}
            ShellButton {objectName:"playlistIconMarkMode";text:"图标";primary:!dialog.textMark;onClicked:dialog.textMark=false}
            ShellButton {objectName:"playlistTextMarkMode";text:"文字";primary:dialog.textMark;onClicked:dialog.textMark=true}
        }
        Flow {
            objectName:"playlistMarkerChoices";visible:!dialog.textMark;Layout.fillWidth:true;spacing:8
            Repeater {
                model:libraryBridge.playlistMarkerIcons
                delegate:AbstractButton {
                    id:markerOption
                    required property string modelData
                    objectName:"playlistMarker-"+modelData;width:44;height:44;padding:10;hoverEnabled:true
                    Accessible.name:dialog.iconNames[modelData]
                    HintBubble {visible:markerOption.hovered;text:markerOption.Accessible.name}
                    contentItem:Symbol {name:"mark-"+markerOption.modelData;color:dialog.iconMark===markerOption.modelData?Theme.accent:Theme.text}
                    background:Rectangle {radius:8;color:dialog.iconMark===markerOption.modelData?Theme.selected:markerOption.hovered?Theme.hover:Theme.card;border.color:dialog.iconMark===markerOption.modelData?Theme.accent:Theme.border}
                    onClicked:dialog.iconMark=modelData
                }
            }
        }
        RowLayout {
            visible:dialog.textMark;Layout.fillWidth:true
            EntryField {id:markInput;objectName:"playlistMarkerText";Layout.fillWidth:true;maximumLength:1;placeholderText:"输入一个字，留空使用歌单首字"}
            UiText {objectName:"playlistMarkerTextPreview";text:dialog.selectedMarker.slice(5);textFormat:Text.PlainText;font.pixelSize:22;font.weight:Font.DemiBold;color:Theme.accent;Layout.preferredWidth:32;horizontalAlignment:Text.AlignHCenter}
        }
    }
    onAccepted:{
        if(listKey==="0"){
            const key=libraryBridge.createPlaylist(nameInput.text)
            if(key!=="0"){libraryBridge.setPlaylistMarker(key,selectedMarker);created(key)}
        }else if(nameInput.text===originalName||libraryBridge.renamePlaylist(listKey,nameInput.text))libraryBridge.setPlaylistMarker(listKey,selectedMarker)
    }
}
