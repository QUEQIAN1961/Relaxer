import QtQuick
import "../theme"
Text {
    property string fontRegion: "rest"
    property real basePixelSize:15
    color: Theme.text
    font.family: {
        const saved = settingsBridge.values.uiFont
        return settingsBridge.fontFamilyFor(fontRegion) || "Microsoft YaHei UI"
    }
    font.pixelSize:Math.max(8,Math.round(basePixelSize*Theme.fontScale))
    font.weight: settingsBridge.values.mutedBold && color === Theme.secondary && (fontRegion === "rest" || fontRegion === "songs") ? Font.DemiBold : Font.Medium
}
