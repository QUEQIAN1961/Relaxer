Option Explicit
Dim shell, fso, root, link
Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
root = fso.GetParentFolderName(WScript.ScriptFullName)
Set link = shell.CreateShortcut(shell.SpecialFolders("Desktop") & "\Relaxer.lnk")
link.TargetPath = root & "\RelaxerStart.exe"
link.WorkingDirectory = root
link.IconLocation = root & "\resources\icon.ico,0"
link.Description = "Relaxer - Local Music Player"
link.Save
MsgBox "Relaxer desktop shortcut created.", 64, "Relaxer"
