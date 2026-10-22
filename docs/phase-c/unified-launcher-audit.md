# 修改前的启动架构审计

审计时间：2026-10-22（UTC+8）；源码 `D:/Git/Relaxer/phase-c-c4c5-ui-parity`。
HEAD `848426e054538a8c760b444a5afb33e21da04697`，Tree `a06b330dc9b95ff23841be3ec76850f2c3bef56e`；工作树干净。

1. Launcher 源码是 `src/start-windows.cpp`，CMake 的 WIN32 RelaxerStart 目标静态链接原生系统库，无 Qt。
2. Launcher 选择便携/源码准备流程、清理环境、设置 RELAXER_DATA、重定向 startup.log 并创建子进程；runtime `src/main.cpp` 负责 QApplication、CLI、资料目录、锁和业务/UI 组装。
3. `scripts/deploy-phase-c.py` 实际生成 CMD：`@echo off` / `setlocal` / `"%~dp0runtime\Relaxer.exe" --ui=qml %*` / `exit /b %errorlevel%`，没有其他环境设置。
4. QML 由 runtime Relaxer.exe 通过 OptionalQmlRuntime 加载 RelaxerQmlUi.dll；launcher 不加载 Qt。
5. runtime `--ui=widgets|qml`，默认 widgets。没有 UI 选择环境变量；RELAXER_DATA 控制资料根目录。
6. 现 launcher 固定运行 runtime `--launch`（不转发任意参数），因此进入 Widgets；直接 runtime 不传 --ui 也进入 Widgets。
7. QML DLL 在 CLI 校验及隔离诊断之后，mkdir、QLockFile、Library.open、Playback 之前加载。
8. OptionalQmlRuntime 使用 QLibrary ResolveAllSymbolsHint/PreventUnloadHint，检查加载、factory 和 ABI；错误写 stderr 后 runtime 返回 11。launcher 重定向 stderr 到 startup.log，启动 2.5 秒内非零退出显示泛化错误且错误地返回 1；超过 2.5 秒脱离进程并返回 0。
9. 权威单实例锁是 runtime dataRoot/relaxer.lock（QLockFile，冲突 exit 3）；launcher 另有包根 startup.lock，仅供首次准备/启动串行化。
10. 候选版本和窗口标题在 src/phase-c-version.h；Win32 PE 在 resources/phase-c-start.rc 和 phase-c-relaxer.rc。旧正式 release-version.h / start.rc 不应修改；ready marker 1.5.0 是独立协议。
11. 仓库未发现 .lnk 或安装器生成配置依赖旧 CMD。start-Relaxer.cmd 和 scripts/prepare-windows-launch.cmd 都启动 RelaxerStart.exe；便携部署生成 READ-ME-FIRST.md 和兼容 CMD，package-phase-c.py / package tests 要求 CMD 保留。
12. 旧默认入口硬编码在 validate-phase-c-native.py 的 Widgets 断言/脱离进程说明、deploy-phase-c.py 的用户说明；历史 phase-c C1 文档描述旧架构。isolation 脚本直接测试 runtime/CMD。新文档应覆盖当前入口规则，历史记录保持原样。

最小设计：launcher 默认注入 --ui=qml；支持 --qml、--widgets 及 runtime --ui 规范，拒绝冲突；按 Windows argv 规则逐项转发。准备完成后释放 startup.lock，再等待子进程完整生命周期并返回真实退出码。普通 QML 缺组件显示友好提示，诊断调用不弹交互框。保留 runtime 默认值、DLL 隔离、CMD、资料目录和锁的业务行为。不会自动回退 Widgets。
