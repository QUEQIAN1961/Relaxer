import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
ConfirmDialog{
    id:dialog;objectName:"playlistEditor";property string listKey:"0"
    function create(){listKey="0";nameInput.text="新歌单";open()}
    function edit(key,name){listKey=key;nameInput.text=name;open()}
    signal created(string key)
    width:400;anchors.centerIn:parent;modal:true
    title:listKey==="0"?"创建歌单":"编辑歌单"
    standardButtons:Dialog.Save|Dialog.Cancel
    contentItem:ColumnLayout{
        UiText {text:"歌单名称";color:Theme.secondary}
        EntryField {id:nameInput;objectName:"playlistNameInput";Layout.fillWidth:true;maximumLength:120;selectByMouse:true;onAccepted:dialog.accept()}
        ShellButton{text:"删除此歌单…";visible:dialog.listKey!=="0";onClicked:deletion.open()}
    }
    onAccepted:{if(listKey==="0"){const key=libraryBridge.createPlaylist(nameInput.text);if(key!=="0")created(key)}else libraryBridge.renamePlaylist(listKey,nameInput.text)}
    ConfirmDialog{
        id:deletion;parent:dialog.parent;width:400;anchors.centerIn:parent;modal:true;title:"删除歌单";standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:Label{text:"仅删除歌单及关联，音乐仍保留在曲库。";wrapMode:Text.Wrap}
        onAccepted:{libraryBridge.deletePlaylist(dialog.listKey);dialog.close()}
    }
}
