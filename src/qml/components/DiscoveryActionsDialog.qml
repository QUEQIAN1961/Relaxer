import QtQuick
ReorderChecklistDialog {
    objectName:"discoveryActionsDialog";title:"发现页歌曲快捷功能"
    names:["下载","下一首播放","更多操作"]
    entryPrefix:"discovery";saveButtonName:"saveDiscoveryActions"
    sourceRows:settingsBridge.values.discoveryActionLayout
    commit:rows=>settingsBridge.saveDiscoveryActionLayout(rows)
}
