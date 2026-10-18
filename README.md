# Relaxer V0.2.1-dev-c2c3 · QML 音乐库与播放控制

本批次从 Harness R13 PASS 的 C1 Fix1 `a4fbda6d5919e5d53771db394d5f94f16f0810b5` 起步。当前状态 **C2+C3 DEVELOPER-VERIFIED — AWAITING HARNESS**；本版是开发交付，未进入 C4/C5/C6，未创建正式 v0.2.1 标签或公开发布。

默认 `RelaxerStart.exe` 使用完整 Widgets。`RelaxerQml.cmd` 或 `runtime/Relaxer.exe --ui=qml` 使用 QML；切换界面前先关闭另一入口。两种模式沿用同一 dataRoot、relaxer.lock、SQLite schema、PlaybackFacade、Queue 和会话设置。不要在运行中复制真实资料库。

QML 已接入真实本地曲目、文字筛选、排序、单选/双击播放、收藏、歌单创建/重命名/删除/切换及成员管理。播放栏支持实际播放暂停、上下曲、Seek、音量、静音与四种既有模式；队列是核心的只读投影，可以选择播放或移除。队列按原始顺序显示，随机下一曲由既有核心决定；不另建 QML 播放器或队列状态机。

使用步骤：先在 Widgets 导入音乐，再关闭并打开 QML。从音乐库/歌单双击或点播放，会以当前筛选排序后的列表建立既有核心队列。单击只选择，不改变正在播放曲目。筛选隐藏选中项时保留其 ID，隐藏项操作禁用；删除曲目后清除失效选择。歌单内上移/下移要求清空筛选、恢复原顺序。删除歌单仅删除关联；确认“移除引用”会移除歌曲引用、歌单关联和统计，音乐文件保留。完整导入管理、歌词、AI、高级设置继续在 Widgets 使用。

曲目角色 trackId 保持 qint64，另提供十进制 trackKey 供 QML 命令传递，避免 JavaScript 数值精度损失。模型只读，不另开数据库连接。Library::trackCount 使用同一连接的 COUNT，不物化全库。粗粒度 Library::changed 合并后刷新快照，播放进度不刷新列表。静音调用原音量接口，最后可听音量只存于已有 settings 表的 qml.lastAudibleVolume UI 偏好，不新增 schema。

Quick 仍按需加载于 `RelaxerQmlUi.dll`。主 EXE 不导入 Quick/QML；显式 QML 缺模块或依赖时返回 11，在创建 dataRoot、锁和数据库前失败。模块代码保留至进程结束，以保护 Qt 全局类型缓存；每次会话按 engine → model/bridge → facade → Library 释放。private ABI 只用于同包相同 Qt/编译器构建。

## 构建与测试

Windows MSYS2 UCRT64、C++17、CMake/Ninja、匹配 Qt 6 Core/Gui/Widgets/Sql/Multimedia/Svg/Test/Network/Qml/Quick/QuickControls2。

```powershell
cmake -S . -B build-c2c3 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Software/MSYS2/ucrt64 -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH=D:/path/to/quick/ucrt64
cmake --build build-c2c3 --parallel 8
ctest --test-dir build-c2c3 --output-on-failure
```

CTest 自动设置所选 Declarative 的真实 imports，无需手工设置 QML_IMPORT_PATH。Qt Base/Declarative 必须版本匹配。开发机 Qt moc 的完整中文 build 输出路径限制仍存在；空格 build、中文运行/data/import 路径分别验证，不把工具限制冒充通过。

专用测试包括 library model、QML C2+C3 和真实 Widgets/QML parity；旧用例完整保留。原生性能探针 `relaxer_c2c3_performance_probe` 按需构建，先 --prepare 创建新合成夹具，再用新进程测量；拒绝无合成标记的已有资料库。帧响应指标不代表真实 FPS 或物理听音结果。

## 部署与打包

先用 scripts/deploy-phase-c.py 组装全新候选，再以 scripts/validate-phase-c-native.py 和继承的 validate-c1-fix1-isolation.py 验证。用候选的 binary-hash-bound verification-proof 重新部署到另一全新 shipping 目录，最后 scripts/package-phase-c.py 生成 ZIP。候选运行过的目录保留证据，禁止直接打包；打包器拒绝日志、数据库、媒体、缓存和凭据，并验证核心三二进制哈希与包内清单。--data 和 RELAXER_DATA 优先级不变。

详细边界见 docs/phase-c/c2c3.md；批次末仅一次完整交付，开发日志与性能结果在 Evidence，报告不冒充独立 Harness 验收。

以下为原稳定版历史说明，其发布结论仅适用于对应稳定提交。

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
