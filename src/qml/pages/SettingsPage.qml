import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../theme"
ColumnLayout {
    id: page
    objectName: "settingsPage"
    Layout.minimumWidth: 0; Layout.minimumHeight: 0
    property int section: 0
    spacing: 12
    Text { text: "设置与工具"; font.pixelSize: 28; font.weight: Font.Bold; color: Theme.text }
    TabBar {
        Layout.fillWidth: true; currentIndex: page.section; onCurrentIndexChanged: page.section = currentIndex
        TabButton { text: "基础设置" }
        TabButton { text: "音乐导入" }
        TabButton { text: "歌词 / 翻译 / AI" }
    }
    StackLayout {
        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 0; Layout.minimumHeight: 0; currentIndex: page.section
        ScrollView {
            id: basics; contentWidth: availableWidth; Layout.minimumWidth: 0; Layout.minimumHeight: 0
            ColumnLayout {
                width: basics.availableWidth; spacing: 16
                Text { Layout.fillWidth: true; text: "外观与播放偏好"; color: Theme.text; font.pixelSize: 22; font.weight: Font.DemiBold }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "主题"; color: Theme.secondary }
                    ComboBox { objectName: "qmlThemeSetting"; Layout.fillWidth: true; model: ["浅色", "深色", "跟随系统", "已有自定义外观"]; currentIndex: settingsBridge.draft.theme; onActivated: settingsBridge.edit("theme", currentIndex) }
                    Text { text: "统计阈值"; color: Theme.secondary }
                    SpinBox { objectName: "qmlThresholdSetting"; from: 70; to: 100; value: settingsBridge.draft.threshold; editable: true; onValueModified: settingsBridge.edit("threshold", value) }
                }
                Text { Layout.fillWidth: true; text: "整轮实际播放达到所设比例后计次，沿用原播放统计规则。"; color: Theme.secondary; wrapMode: Text.Wrap; font.pixelSize: 12 }
                CheckBox { text: "显示歌词译文"; checked: settingsBridge.draft.showLyricTranslation; onClicked: settingsBridge.edit("showLyricTranslation", checked) }
                CheckBox { text: "展开播放使用封面背景"; checked: settingsBridge.draft.playerCoverBackground; onClicked: settingsBridge.edit("playerCoverBackground", checked) }
                CheckBox { objectName: "reduceMotionSetting"; text: "减少动画"; checked: settingsBridge.draft.reduceMotion; onClicked: settingsBridge.edit("reduceMotion", checked) }
                TextField { Layout.fillWidth: true; text: settingsBridge.draft.lyricColor; placeholderText: "歌词颜色，例如 #f08198；留空跟随主题"; onTextEdited: settingsBridge.edit("lyricColor", text) }
                RowLayout {
                    ShellButton { objectName: "settingsSaveButton"; text: "保存设置"; primary: true; onClicked: settingsBridge.save() }
                    ShellButton { objectName: "settingsCancelButton"; text: "取消修改"; onClicked: settingsBridge.resetDraft() }
                }
                Text { Layout.fillWidth: true; text: settingsBridge.status; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.accent }
                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
                Text { text: "已保存外观"; color: Theme.text; font.pixelSize: 20 }
                RowLayout {
                    Layout.fillWidth: true
                    ComboBox { id: appearance; Layout.fillWidth: true; model: settingsBridge.appearances; textRole: "name"; valueRole: "id" }
                    ShellButton { text: "应用"; enabled: appearance.currentIndex >= 0; onClicked: settingsBridge.useAppearance(appearance.currentValue) }
                }
                TextField { id: paletteName; Layout.fillWidth: true; placeholderText: "新配色名称" }
                RowLayout {
                    Layout.fillWidth: true
                    TextField { id: background; Layout.fillWidth: true; text: "#f5f4f1"; placeholderText: "背景颜色" }
                    TextField { id: panel; Layout.fillWidth: true; text: "#ffffff"; placeholderText: "面板颜色" }
                    TextField { id: accent; Layout.fillWidth: true; text: "#b62e50"; placeholderText: "强调颜色" }
                }
                ShellButton { text: "保存并应用配色"; onClicked: settingsBridge.savePalette(paletteName.text, background.text, panel.text, accent.text) }
                Text { Layout.fillWidth: true; text: "资料目录：" + settingsBridge.dataRoot; textFormat: Text.PlainText; color: Theme.secondary; wrapMode: Text.Wrap; font.pixelSize: 12 }
                Text { Layout.fillWidth: true; text: "图片外观编辑、自定义字体、快捷键编辑与托盘行为仍可在 Widgets 中配置。此处保留其已有配置。"; color: Theme.secondary; wrapMode: Text.Wrap; font.pixelSize: 12 }
                Item { Layout.preferredHeight: 16 }
            }
        }
        ImportPage {}
        TranslationPage {}
    }
}
