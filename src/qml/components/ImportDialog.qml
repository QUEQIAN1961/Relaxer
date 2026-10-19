import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../theme"
Popup{
    id:dialog
    objectName:"importDialog"
    width:240;padding:8
    function chooseFiles(){files.open()}
    function chooseFolder(){folder.open()}
    function openMenu(){x=Math.max(0,parent.width-width-34);y=110;open()}
    function submitFiles(urls){return importBridge.start(urls,Number(libraryBridge.playlistId)>0?libraryBridge.playlistId:"0")}
    background:Rectangle{color:Theme.background;radius:10;border.color:Theme.border}
    contentItem:ColumnLayout{
        ShellButton{text:"导入音乐文件…";symbol:"music";Layout.fillWidth:true;enabled:!importBridge.busy;onClicked:{dialog.close();files.open()}}
        ShellButton{text:"导入整个文件夹…";symbol:"folder";Layout.fillWidth:true;enabled:!importBridge.busy;onClicked:{dialog.close();folder.open()}}
    }
    FileDialog{id:files;title:"导入音乐文件";fileMode:FileDialog.OpenFiles;nameFilters:["音频 (*.mp3 *.flac *.wav *.ogg *.opus *.m4a *.aac *.mid *.midi)","所有文件 (*)"];onAccepted:dialog.submitFiles(selectedFiles)}
    FolderDialog{id:folder;title:"导入整个文件夹";onAccepted:dialog.submitFiles([selectedFolder])}
}
