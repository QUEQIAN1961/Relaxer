pragma Singleton
import QtQuick
QtObject {
    objectName: "themeTokens"
    property bool dark: false
    readonly property color background: dark ? "#17191e" : "#f5f4f1"
    readonly property color panel: dark ? "#22252c" : "#ffffff"
    readonly property color text: dark ? "#f2f2f5" : "#292b35"
    readonly property color secondary: dark ? "#b0b5c2" : "#707683"
    readonly property color accent: dark ? "#fb8198" : "#b62e50"
    readonly property color accentText: dark ? "#251318" : "#ffffff"
    readonly property color border: dark ? "#353a45" : "#e5e5e8"
    readonly property color hover: dark ? "#30353f" : "#eeedf0"
    readonly property color pressed: dark ? "#424957" : "#dfdce3"
    readonly property color disabled: dark ? "#636977" : "#a1a5af"
    readonly property color selected: dark ? "#432833" : "#f9e7ec"
    readonly property int radius: 14
    readonly property int spacing: 16
    readonly property int fontSize: 14
    readonly property int headingSize: 28
}
