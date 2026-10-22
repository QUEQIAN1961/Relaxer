# Relaxer V0.2.1

本正式版本号候选从 Harness R18 已通过的 Final-RC 提交 `1f1624bb123f1bbd1ab1d034beda57bd8a3e28cc` 收敛，只更新版本与发布资料，播放器业务逻辑和 QML 布局保持不变。状态：FINAL RELEASE CANDIDATE — PENDING HARNESS APPROVAL。未创建 v0.2.1 标签，未推送或对外发布。

便携包完整解压后，双击根目录 `RelaxerStart.exe` 直接打开 QML 新界面。`RelaxerStart.exe --widgets` 打开兼容界面，`--qml` 显式选择新版；`RelaxerQml.cmd` 保留兼容。两套界面共享包根 data、数据库和锁；切换前关闭另一实例。使用与升级说明见 `RELEASE-NOTES-V0.2.1.md`。

## 开发构建

Windows MSYS2 UCRT64、C++17、CMake、Ninja、Qt 6 和 FFmpeg；Declarative 前缀须与基础 Qt 匹配。配置 Release 并使用全新构建目录，例如：

```powershell
cmake -S . -B build-v021-final -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Software/MSYS2/ucrt64 -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH=D:/path/to/quick/ucrt64
cmake --build build-v021-final --parallel 8
ctest --test-dir build-v021-final --output-on-failure
```

源码中的历史准备工具及预编译 bootstrap 仅用于源码环境恢复；正式便携包的三个产品 PE 必须由本候选 Commit 重新构建，不能直接复制源码根的历史 EXE。`relaxer-ready.json` 的 version=1.5.0 是保留的启动握手协议，displayVersion 才是产品显示版本。历史版本日志和旧报告保留原名。

主 EXE 不直接导入 QtQuick/QtQml，QML 模块按需加载；Widgets 可独立于 QML 依赖启动。默认启动 QML，正常退出码 0；锁冲突 3、QML 组件缺失 11、非法或冲突参数 64。

开发自动验证使用隔离资料库、合成媒体与 Mock；不读取私人曲库或真实 API Key。最终交付和独立复核边界以随包报告为准。

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
