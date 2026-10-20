pragma Singleton
import QtQuick
QtObject {
    objectName: "themeTokens"
    property bool dark: false
    property var custom: ({})
    readonly property color background: custom.background || (dark ? "#19191e" : "#ffffff")
    readonly property color panel: custom.panel || (dark ? "#222228" : "#f5f5f7")
    readonly property color text: custom.text || (dark ? "#f0edf1" : "#25252b")
    readonly property color secondary: custom.secondary || (dark ? "#a09ca6" : "#86818a")
    readonly property color accent: custom.accent || "#e5455f"
    readonly property color accentText: custom.accentText || "#ffffff"
    readonly property color sideText: custom.sideText || text
    readonly property color border: custom.border || (dark ? "#34343c" : "#e9e7eb")
    readonly property color hover: dark ? "#302e37" : "#eeedf1"
    readonly property color pressed: dark ? "#40353e" : "#e7e1e8"
    readonly property color disabled: dark ? "#6d6874" : "#b5afb9"
    readonly property color selected: dark ? "#49313a" : "#fbe6eb"
    readonly property color card: custom.card || (dark ? "#25252d" : "#f7f6f8")
    readonly property color footer: custom.footer || panel
    readonly property color progress: custom.progressFill || accent
    readonly property color progressTrack: custom.progressTrack || (dark ? "#45414a" : "#e6e1e7")
    readonly property int radius: 10
    readonly property int spacing: 16
    readonly property int fontSize: 15
    readonly property int controlHeight: 36
    readonly property int pageContentOffset: Math.round(controlHeight * 0.6)
    property string fontFamily: "Microsoft YaHei UI"
    readonly property int headingSize: 28
}
