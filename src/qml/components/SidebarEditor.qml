import QtQuick
ReorderChecklistDialog {
    objectName:"sidebarEditor";title:"音乐资料库板块"
    names:["本地音乐","发现","个人","设置","我喜欢","歌单","历史记录"]
    function included(row){return !(row.id===3&&settingsBridge.values.settingsEntryMode===1)&&!(row.id===6&&settingsBridge.values.historyEntryMode===1)}
    sourceRows:settingsBridge.values.sidebarSections.filter(included).map(row=>row.id===3?{id:3,visible:true}:row)
    mandatoryIds:settingsBridge.values.settingsEntryMode===0?[3]:[]
    description:"拖动左侧三横线调整顺序，勾选右侧决定是否显示。按钮入口不在此列表中。"
    commit:rows=>{let index=0;return settingsBridge.saveSidebarSections(settingsBridge.values.sidebarSections.map(row=>included(row)?rows[index++]:row))}
}
