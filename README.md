# Relaxer V0.2.2-GA

**简体中文** · [English](README.en.md)

Relaxer 是使用 C++17、Qt 6 和 QML 开发的 Windows 本地音乐播放器，提供音乐库、歌单、播放队列、滚动歌词和可自定义的外观。

本目录为 V0.2.2-GA 源码。Windows 便携包、完整应用源码与对应第三方源码见 [GitHub Releases](https://github.com/QUEQIAN1961/Relaxer/releases)，请使用同一版本的附件和校验文件。

![Relaxer 主界面](docs/images/relaxer-v0.2.1.png)

## 下载与使用

Windows x64 便携包名称为 `Relaxer-V0.2.2-GA-Win64.zip`。完整解压到新的可写文件夹，双击根目录 **`Relaxer.exe`**。运行包自带依赖，不需要 Qt、编译环境或源码准备入口。不要在压缩包预览中运行，也不要单独取出 EXE。

默认 QML 界面；兼容界面可运行 `Relaxer.exe --widgets`。资料默认位于同级 `data/`，也可用 `Relaxer.exe --data "绝对资料目录"` 指定。日常本地播放可离线使用；在线歌词和可选翻译需要网络，凭据与付费请求由用户选择和确认。

升级前完全退出旧版并备份整个 `data/`。在新目录完整解压，用资料副本先验证；不要混装 runtime，不要让两个版本同时写一份资料。程序不自动迁移、删除或合并旧资料。

## 本版本

汇总播放、歌词、歌单及便携启动的既有修复，正式说明见 [V0.2.2-GA 更新日志](docs/releases/v0.2.2-GA/RELEASE-NOTES.md)。历史日志和版本说明保留于 `resources/changelog.json` 与 `docs/releases/`。

|历史公开版本|下载与说明|
|---|---|
|0.2.1-GA|[Release](https://github.com/QUEQIAN1961/Relaxer/releases/tag/v0.2.1-GA)|
|0.2.1|[Release](https://github.com/QUEQIAN1961/Relaxer/releases/tag/v0.2.1)|
|0.2.0-GA|[Release](https://github.com/QUEQIAN1961/Relaxer/releases/tag/v0.2.0-GA)|
|0.2.0-rc2|[历史预发行版](https://github.com/QUEQIAN1961/Relaxer/releases/tag/v0.2.0-rc2)|

## 校验与源码

使用 `Get-FileHash .\Relaxer-V0.2.2-GA-Win64.zip -Algorithm SHA256` 与同批 `SHA256SUMS.txt` 对照。GitHub 自动 Source code 归档不是可运行播放器。

构建环境为 Windows、MSYS2 UCRT64、C++17、CMake/Ninja、版本匹配的 Qt 6 Core/Gui/Widgets/Sql/Multimedia/Svg/Network/Qml/Quick/QuickControls2；测试另需 Qt Test。使用发行提交的完整源：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build --parallel
```

Declarative 位于单独前缀时设置 `QT_ADDITIONAL_PACKAGES_PREFIX_PATH` 或相应 Qt6 包目录。应用、QML DLL 与根便携入口须从同一源码身份构建，普通包不包含 `RelaxerStart.exe`。构建与材料组装方法见 [BUILD-AND-SOURCE.md](docs/releases/v0.2.2-GA/BUILD-AND-SOURCE.md)。

项目自有代码采用 [GPL-3.0-only](LICENSE)。Qt、FFmpeg、OpenCC 和其他组件保留原授权和声明；见 [第三方说明](THIRD_PARTY_NOTICES.md)、运行包许可证目录及配套对应源码。软件许可不授权外部音乐、封面、歌词或服务内容。

反馈请附版本、Windows 环境和脱敏复现步骤；不要提交凭据、真实音乐或个人数据库。
