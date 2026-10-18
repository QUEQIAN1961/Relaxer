# Relaxer V0.2.1-dev-c4c5 · 歌词、全屏、设置、导入与 AI 界面

本批次从 Harness R14 PASS 的 C2+C3 `f2242c9523ea0200d43ad6fbcba6ab6978dbe364` 起步。当前为 C4+C5 开发交付，独立验收状态以随包五份报告为准。未进入 C6，未创建正式 v0.2.1 标签或公开发布。

默认 `RelaxerStart.exe` 使用 Widgets；`RelaxerQml.cmd` 或 `runtime/Relaxer.exe --ui=qml` 使用 QML。切换前关闭另一入口。两者沿用同一 dataRoot、单实例锁、SQLite schema、Library、PlaybackFacade、Queue 和既有设置。请先备份资料库，再在副本上人工验收。

QML 保留 C2+C3 音乐库、筛选排序、收藏、歌单、真实播放控制和队列。本轮新增真实歌词页面：原有解析时间轴、双语对应、当前行 1.2 倍显示、手动滚动及回到当前、点击 Seek、每曲 ±5 秒精细延时、原歌词缓存及本地导入。全屏使用同一窗口，支持 Esc、窗口几何恢复、异步封面和适度动画；背景使用低透明度封面着色。减少动画可在设置中开启。

设置页复用 SettingsStore/AppearanceStore，提供主题、显示偏好、歌词颜色及原有颜色历史。导入页调用原 ImportCoordinator，支持文件/文件夹、目标歌单、取消、结果与即时模型刷新。AI 页使用原配置和凭据存储：打开页面不会读取 Key 或发起网络请求；显式查询模型目录才联网。歌词检索、翻译准备、一次性付费确认、预览、保存/放弃和浏览器翻译均经原协调器。预览不会自动保存；切歌后旧任务不能写入新歌。

高级图片外观编辑、字体/快捷键编辑和托盘操作仍由 Widgets 提供。真实付费服务与实际听音须由用户另行验收。自动测试只用隔离资料库、合成音频与 Mock/Fake，不读取真实凭据或调用收费 API。

## 构建与测试

Windows MSYS2 UCRT64、C++17、CMake/Ninja，以及版本匹配的 Qt 6 Core/Gui/Widgets/Sql/Multimedia/Svg/Test/Network/Qml/Quick/QuickControls2。

```powershell
cmake -S . -B build-c4c5 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Software/MSYS2/ucrt64 -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH=D:/path/to/quick/ucrt64
cmake --build build-c4c5 --parallel 8
ctest --test-dir build-c4c5 --output-on-failure
```

CTest 自动配置匹配 Declarative 的 imports；全量验证前清除外部 QML_IMPORT_PATH、QML2_IMPORT_PATH、QT_PLUGIN_PATH。完整中文 build 输出路径的 Qt moc 限制仍存在；中文运行、资料和导入路径另有隔离测试。

新增队列选择读取、歌词模型、C4C5 bridge/UI 与检索 bridge 专项。原生 `relaxer_c4c5_performance_probe` 按需构建，先用 --prepare 在新目录创建合成夹具，再以新进程测量。真实渲染帧响应与合成 adapter 信号耗时分别报告，不能视为 FPS 或物理听音结果。

## 架构与部署

Library::tracksByIds 仅在原连接按队列 ID 批量读取，保留队列次序、重复及缺失项规则。播放进度不刷新队列或歌词文档。歌词解析、导入、翻译、付费门禁和配置格式均复用原核心；QML 仅提供模型和受控命令。接口映射见 `docs/phase-c/c4c5-interface-map.md`。

主 EXE 不导入 Quick/QML；`RelaxerQmlUi.dll` 按需加载。显式 QML 缺模块/依赖时在创建 dataRoot 前返回 11。模块代码保留到进程结束，单次会话按 engine、bridge/services、facade、Library 顺序释放。关闭时处理未完成任务/预览，等待原导入线程退出屏障。

`scripts/deploy-phase-c.py` 创建全新候选，继承隔离矩阵和 `scripts/validate-phase-c-native.py` 验证后，以绑定实际二进制哈希的 proof 重新部署到全新 shipping 目录。`scripts/package-phase-c.py` 拒绝日志、数据库、媒体、缓存及凭据，输出包内清单。运行过的候选不用于交付。--data / RELAXER_DATA 优先级保持不变。

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
