# Relaxer V0.2.1 — Launcher Deployment Fix

请将新的 Win64 Candidate ZIP **完整解压到一个新文件夹**，然后双击
`RelaxerStart.exe`，默认进入 QML 播放器。`RelaxerQml.cmd` 仍可使用。
Widgets 兼容模式继续使用 `RelaxerStart.exe --widgets`；显式 QML 为 `--qml`。
不要把新 EXE 单独放进旧包，不要自行创建 `relaxer-ready.json`。

原候选的播放器和 Qt 文件完整，但交付脚本把 `verified=false`、未经启动器
验证的暂存目录直接打成 ZIP，绕过了正式打包器。因而缺少
`runtime/relaxer-ready.json`，触发 `launch-plan.h` 中的部署就绪检查，退出码为 1。
CMD 直接运行播放器，不检查该标记，所以同目录下可以成功启动。
这不是中文、空格或 runtime 路径识别错误。现在保留原检查，并在错误提示中列出实际缺失路径。

此次为 V0.2.1 部署补丁：播放器和 QML DLL 沿用基线原始二进制；仅重编译原生
启动器，新增受验证证明约束的 `scripts/package-launcher-fix.py`。
该工具从未运行过的原候选副本重组完整文件，核对来源、全部原始输入的 SHA256、
新启动器、源码身份以及定向原生验证证明，然后生成标记并调用严格的
`scripts/package-phase-c.py`；不能用通用 ZIP 函数代替正式打包器。

开发验证范围：默认 QML 启动器、兼容 CMD、缺少 QML DLL 的拒绝、中文和空格路径、
最终 ZIP 重新解压启动。测试全部使用隔离资料目录和 Windows/包内依赖。
验证用副本中的临时握手标记只服务于测试，不能直接打包或作为发行证明。
发行标记仅表示本轮匹配文件的开发验证，未声称新增音频验收或 Harness 验收。
逐项命令、结果、源码/二进制来源和最终 ZIP 哈希见交付根目录的报告与证据。

版本仍为 0.2.1；播放功能、SQLite schema、单实例控制、启动参数和子进程退出码
转发规则保持原实现。测试使用显式 `--data`，不触碰真实曲库或配置。
用户正常启动继续沿用原有资料目录选择规则。

状态：**DEV VERIFIED / PENDING HARNESS**。新 Commit 不再具有 `7ea7506` 的冻结身份。
DeepSeek Harness 需核对新 Commit/Tree、哈希、最终解压包的两个入口和缺失文件拒绝，
并自行给出专项独立验收结论。没有修改 27 项历史关闭记录或历史报告。
未执行全量 CTest/QtTest、47 项矩阵、ManualUX13/ExitGuard/WASAPI 重复验收，
未 Tag、Push、Merge 或 Release。
