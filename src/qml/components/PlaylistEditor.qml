import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../theme"
ConfirmDialog{
    id:dialog;objectName:"playlistEditor";property string listKey:"0"
    property string iconMark:"music"
    property string originalName:""
    property bool cloud:false
    property bool textMark:false
    property url savedCover:""
    property url pendingCover:""
    readonly property string selectedMarker:textMark?"text:"+(markInput.text.trim()||Array.from(nameInput.text.trim()||originalName)[0]||"歌"):"icon:"+iconMark
    readonly property var iconNames:({
        "music":"音乐","headphones":"耳机","disc-3":"唱片","radio":"电台","mic-vocal":"麦克风","star":"星星","heart":"爱心","sun":"太阳",
        "moon":"月亮","cloud-rain":"雨天","coffee":"咖啡","book-open":"阅读","leaf":"树叶","flame":"火焰","mountain":"山峰","gamepad-2":"游戏",
        "bell":"铃铛","bookmark":"书签","briefcase":"公文包","camera":"相机","clock":"时钟","compass":"指南针","cube":"方块","diamond":"钻石",
        "flower":"花朵","globe":"地球","graduation-cap":"学士帽","home":"房屋","key":"钥匙","laptop":"电脑","lightbulb":"灯泡","mail":"信封",
        "map-pin":"位置","palette":"调色盘","paw-print":"爪印","plane":"飞机","rocket":"火箭","shield":"盾牌","smile":"笑脸","sparkles":"闪光",
        "trophy":"奖杯","umbrella":"雨伞","video":"摄像机","waves":"波浪","wine":"酒杯","wrench":"扳手","zap":"闪电","balloon":"气球"
    })
    function create(){listKey="0";cloud=false;originalName="";nameInput.text="";iconMark="music";textMark=false;markInput.text="";savedCover="";pendingCover="";open()}
    function edit(key,name){
        listKey=key;const details=libraryBridge.playlistDetailsFor(key);cloud=!!details.cloud;nameInput.text=cloud?details.remark:name;originalName=cloud?details.name:name
        const marker=libraryBridge.playlistMarkers[key]||"icon:music"
        textMark=marker.startsWith("text:");iconMark=textMark?"music":marker.slice(5);markInput.text=textMark?marker.slice(5):""
        savedCover=details.cover||"";pendingCover=""
        open()
    }
    signal created(string key)
    width:Math.min(480,parent.width-32);height:Math.min(implicitHeight,parent.height-32);anchors.centerIn:parent;modal:true
    title:listKey==="0"?"创建歌单":cloud?"编辑备注/标记":"编辑名称/图标/封面"
    standardButtons:Dialog.Save|Dialog.Cancel
    onOpened:{editorScroll.contentItem.contentY=0;nameInput.forceActiveFocus()}
    contentItem:ScrollView {
      id:editorScroll;contentWidth:availableWidth;implicitHeight:editorContent.implicitHeight;clip:true
      SmoothWheelHandler {flickable:editorScroll.contentItem as Flickable}
      ColumnLayout {
        id:editorContent;width:editorScroll.availableWidth
        spacing:12
        UiText {text:dialog.cloud?"歌单备注":"歌单名称";color:Theme.secondary}
        EntryField {id:nameInput;objectName:"playlistNameInput";Layout.fillWidth:true;maximumLength:120;placeholderText:dialog.cloud?"留空显示平台歌单名称":dialog.listKey==="0"?"新歌单":"";selectByMouse:true;onAccepted:dialog.accept()}
        UiText {visible:dialog.cloud;Layout.fillWidth:true;text:"平台名称："+dialog.originalName;textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:12;color:Theme.secondary}
        RowLayout {
            Layout.fillWidth:true;spacing:14
            Artwork {
                objectName:"playlistEditorCover";Layout.preferredWidth:64;Layout.preferredHeight:64;radius:10;resolution:160;animated:false
                file:String(dialog.pendingCover).length?dialog.pendingCover:dialog.savedCover
                seed:nameInput.text.trim()||"新歌单"
            }
            ColumnLayout {
                Layout.fillWidth:true;spacing:6
                UiText {text:"歌单封面";color:Theme.secondary}
                ShellButton {objectName:"playlistChooseCover";text:dialog.cloud?"使用平台封面":"选择封面…";enabled:!dialog.cloud;onClicked:coverPicker.open()}
            }
        }
        RowLayout {
            Layout.fillWidth:true;spacing:8
            UiText {text:"歌单图标";color:Theme.secondary;Layout.fillWidth:true}
            ShellButton {objectName:"playlistIconMarkMode";text:"图标";primary:!dialog.textMark;onClicked:dialog.textMark=false}
            ShellButton {objectName:"playlistTextMarkMode";text:"文字";primary:dialog.textMark;onClicked:dialog.textMark=true}
        }
        Flow {
            objectName:"playlistMarkerChoices";visible:!dialog.textMark;Layout.fillWidth:true;spacing:6
            Repeater {
                model:libraryBridge.playlistMarkerIcons
                delegate:AbstractButton {
                    id:markerOption
                    required property string modelData
                    objectName:"playlistMarker-"+modelData;width:36;height:36;padding:8;hoverEnabled:true
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
    }
    FileDialog {
        id:coverPicker;objectName:"playlistCoverPicker";title:"选择歌单封面"
        nameFilters:["图片 (*.png *.jpg *.jpeg *.webp *.bmp)"]
        onAccepted:dialog.pendingCover=selectedFile
    }
    onAccepted:{
        const creating=listKey==="0"
        const key=creating?libraryBridge.createPlaylist(nameInput.text):listKey
        if(key==="0")return
        if(cloud){if(!libraryBridge.setCloudPlaylistRemark(key,nameInput.text))return}
        else if(!creating&&nameInput.text!==originalName&&!libraryBridge.renamePlaylist(key,nameInput.text))return
        libraryBridge.setPlaylistMarker(key,selectedMarker)
        if(!cloud&&String(pendingCover).length)libraryBridge.setPlaylistCover(key,pendingCover)
        if(creating)created(key)
    }
}
