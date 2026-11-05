import QtQuick
import QtQuick.Controls
import "../theme"
RoundMenu {
    id: menu
    objectName: "textEditingMenu"
    required property Item editor
    readonly property bool protectedText: editor.echoMode !== undefined && editor.echoMode !== TextInput.Normal
    RoundMenuItem { text:"撤销"; enabled:!menu.editor.readOnly&&menu.editor.canUndo; onTriggered:menu.editor.undo() }
    RoundMenuItem { text:"重做"; enabled:!menu.editor.readOnly&&menu.editor.canRedo; onTriggered:menu.editor.redo() }
    MenuSeparator { contentItem:Rectangle { implicitHeight:1; color:Theme.border } }
    RoundMenuItem { text:"剪切"; enabled:!menu.protectedText&&!menu.editor.readOnly&&menu.editor.selectedText.length>0; onTriggered:menu.editor.cut() }
    RoundMenuItem { text:"复制"; enabled:!menu.protectedText&&menu.editor.selectedText.length>0; onTriggered:menu.editor.copy() }
    RoundMenuItem { text:"粘贴"; enabled:!menu.editor.readOnly&&menu.editor.canPaste; onTriggered:menu.editor.paste() }
    RoundMenuItem { text:"删除"; enabled:!menu.editor.readOnly&&menu.editor.selectedText.length>0; onTriggered:menu.editor.remove(menu.editor.selectionStart,menu.editor.selectionEnd) }
    MenuSeparator { contentItem:Rectangle { implicitHeight:1; color:Theme.border } }
    RoundMenuItem { text:"全选"; enabled:menu.editor.length>0; onTriggered:menu.editor.selectAll() }
}
