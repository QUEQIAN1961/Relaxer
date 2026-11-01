import QtQuick
import "../theme"
Text {
    property string fontRegion: "rest"
    font.family: {
        const saved = settingsBridge.values.uiFont
        return settingsBridge.fontFamilyFor(fontRegion) || "Microsoft YaHei UI"
    }
    font.pixelSize: Theme.fontSize
    font.weight: settingsBridge.values.mutedBold && color === Theme.secondary && (fontRegion === "rest" || fontRegion === "songs") ? Font.DemiBold : Font.Medium
}
