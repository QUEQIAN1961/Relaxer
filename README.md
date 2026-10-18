# Relaxer V0.2.1-dev-c1 · Phase C / C0 + C1 开发交付

本分支从正式 `v0.2.0` / `43203314012f509a2f83e4558b9206001b2e5f75` 起步。C1 不等于 V0.2.1 正式发布；未创建正式 V0.2.1 标签。默认仍为完整 Widgets，`runtime/Relaxer.exe --ui=qml`（便携包内也可用 `RelaxerQml.cmd`）打开 QML Foundation。两种界面互斥，沿用同一 dataRoot、资料锁、Library 和 Playback。

QML 已实现原生窗口、导航、主题/组件、真实曲库数量与播放状态、已恢复曲目的播放/暂停。进度和音量只读。列表、歌单、歌词、完整设置和 AI 入口尚未迁移，界面明确标注。文档见 `docs/phase-c/`，本轮仅 C0/C1。

构建仍用 MSYS2 UCRT64 / C++17 / CMake / Ninja / Qt 6；GUI 增加官方 Qml、Quick、QuickControls2（MSYS2 `mingw-w64-ucrt-x86_64-qt6-declarative`）。可用 `-DRELAXER_ENABLE_QML=OFF` 单独构建 Widgets。四种服务配置在 Quick 查找前返回，不需要 Widgets/Quick。

本地隔离 overlay 构建示例（prefix 必须是同版本 Qt）：

```powershell
cmake -S . -B build-c1 -G Ninja -DCMAKE_BUILD_TYPE=Release -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH=D:/path/to/quick/ucrt64
cmake --build build-c1 --parallel 8
# overlay 模式运行测试前设置 PATH 和 QML_IMPORT_PATH；正常完整 Qt 安装无需 overlay。
ctest --test-dir build-c1 --output-on-failure
python scripts/deploy-phase-c.py --source . --build build-c1 --qt-prefix C:/Software/MSYS2/ucrt64 --quick-prefix D:/path/to/quick/ucrt64 --destination D:/path/to/fresh-package
```

可重复观测：`Relaxer.exe --data <隔离目录> --ui=qml --diagnostics <JSON> --quit-after 4000`。`quit-after` 仅供显式资料目录的诊断；正式回归另用原生关闭验证。部署脚本只收集运行时，必须通过系统 PATH 的原生验证后才可视为已验证便携包。

以下为保留的 V0.2.0 使用说明与验收背景，其发布结论仅适用于原稳定提交。

---

# V0.2.0 历史说明

Windows 本地音乐播放器，基于 C++17、Qt 6 Widgets、SQLite 和 FFmpeg。正式数值版本为 0.2.0，窗口标题为 Relaxer V0.2.0。

## 使用与升级

推荐使用 `Relaxer-V0.2.0-Win64-Portable.zip`。完整解压到新目录，双击根目录 `RelaxerStart.exe`；便携包自带运行依赖，日常本地播放可以离线使用。

升级前完全退出旧版并备份整个 `data`，将副本复制到新目录，与启动器同级。不要在旧版仍运行时复制数据库。曲目身份、歌单、歌词、人工翻译分类和设置继续沿用，不需要重新导入。详见 `V0.2.0-Upgrade-Guide.md`。

源码包供开发和恢复使用，不包含便携 runtime。源码根目录保留已重建的原生启动器和首次准备脚本；首次源码准备需要 Windows MSYS2 UCRT64 开发环境。直接使用请选便携包。

## 文档

- `V0.2.0-ReleaseNotes.md`：Phase B 完整发布说明。
- `V0.2.0-Manual.md`：播放、歌词与 AI 翻译操作。
- `V0.2.0-Known-Issues.md`：继承 Harness R10 的 22 项 P3。
- `V0.2.0-RC2-ReleaseNotes.md`、`V0.2.0-RC2-Known-Issues.md`：冻结的历史 RC2 资料。

本版从 RC2 提交 `2540a6729a8a0fff06fe1f41d0a2d7bcc703b6a9` 制作。RC2 的 Harness R10 技术验收 PASS，用户人工验收 6/6 PASS。正式转换仅更新版本身份与发布元数据，业务逻辑和数据库 schema 保持不变。

## 开发构建

使用 Windows MSYS2 UCRT64、CMake、Ninja 与 Qt 6 Core/Gui/Widgets/Sql/Multimedia/Svg/Test/Network。`CMakeLists.txt` 的项目版本为 0.2.0。可运行 `scripts/build-windows-ucrt64.sh`；服务独立构建开关保留。

无界面测试通过 `RELAXER_SKIP_AUDIO_DEVICE_TESTS=1` 保留历史设备跳过；真实声卡听音与外部歌词站点测试需单独人工执行。AI 自动测试只使用 Mock，不读取凭据或发送真实付费请求。

`relaxer-ready.json` 的 `version=1.5.0` 是既有启动握手协议标记，不是产品版本，本次不变更。
