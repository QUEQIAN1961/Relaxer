Relaxer V0.2.2-RC6 — Windows x64 PRIVATE TEST / TEST-UNVERIFIED

完整解压到新文件夹，双击根目录 Relaxer.exe 启动 QML 应用。
兼容界面：Relaxer.exe --widgets，或 RelaxerWidgets.cmd。
RelaxerQml.cmd 仍可使用。先正常退出当前界面，再切换入口。
不要单独移动 EXE；保留 runtime 与 PORTABLE-SHA256SUMS.txt。

RelaxerStart.exe 是源码准备启动器，不是本包的应用入口。
首次准备正向流程仍未验收；本包不含 relaxer-ready.json。
CMD 或应用成功启动不会生成准备标记，不要手工创建标记。

本包不含个人音乐、资料库、AI 凭据。默认在本包 data 下创建新资料。
可通过 --data 指定独立测试目录；不要覆盖旧版或使用个人 SQLite 做回归。
ART-01 仍为 UNRESOLVED — NOT REPRODUCED，未宣称修复。
等待独立验收；七项 GA 门仍开放，不是 GA，不得公开分发。
