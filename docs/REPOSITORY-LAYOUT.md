# 源码与发行文件

GitHub 仓库和 Source.zip 面向开发者；Windows 用户包面向运行播放器，两者不混装。

- `src/`、`resources/`：应用源代码、QML 和资源。
- `tests/`、`scripts/tests/`：可重复使用的测试源代码，不是运行测试产生的日志或图片。
- `scripts/`：构建、依赖准备、打包和维护工具。
- `docs/releases/`：正式发行说明与构建说明。私人候选阶段的调查、门禁、交接和原始测试记录不进入当前源码归档。
- `docs/third-party/`、`licenses/`、根目录许可与致谢：第三方权利、来源、改编和对应源码材料。基础依赖的历史来源记录在 `docs/third-party/base-runtime/`。
- `docs/maintainers/`：可复用的维护者检查清单。

源码目录不提供预编译的 `RelaxerStart.exe`、`RelaxerLockCheck.exe` 或旧桌面快捷方式。Windows 用户完整解压发布包后运行根目录 `Relaxer.exe`；开发者使用同一源码构建需要的目标。

开发运行：设置 `RELAXER_BUILD_DIR` 为构建目录、`RELAXER_QT_PREFIX` 为匹配的 Qt/MSYS2 前缀后，调用 `scripts/prepare-windows-launch.cmd`。该入口直接启动已构建的应用，不触发源码准备或系统包更新。CMake 仍保留旧部署工具的源码目标，以支持维护历史部署流程；需要维护历史部署流程时，从当前源码构建辅助工具并按该流程部署；锁检查器仍要求自身位于对应工程目录，不放宽项目和缓存归属检查。生成的 EXE 被 Git 忽略，不进入源码归档。

构建输出、测试样本、截图、日志、用户 data 和机器缓存均放在源码外或已忽略的目录，不进入 Git/Source.zip。旧工程记录可从相应历史提交和发行源归档获取，历史事实不因目录整理而改写。

Windows 包保留启动校验、实际需要的 Qt/QML 模块、音色与第三方通知。生成脚本和原始字典等只用于复现的材料保留在 Source.zip。运行工具与播放器共享哈希一致的 DLL，工具所需的不同版本库仍保留；每次打包重新生成身份和完整性清单。
