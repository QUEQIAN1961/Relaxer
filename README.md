# Relaxer

**简体中文** · [English](README.en.md)

为本地音乐留一个安静的位置。Relaxer 是使用 C++17、Qt 6 和 QML 开发的 Windows 本地音乐播放器，提供音乐库、歌单、队列、滚动歌词和可自定义的外观。

[下载最新版](https://github.com/QUEQIAN1961/Relaxer/releases/tag/v0.2.1-GA) · [全部版本](https://github.com/QUEQIAN1961/Relaxer/releases) · [反馈问题](https://github.com/QUEQIAN1961/Relaxer/issues)

![Relaxer 0.2.1 主界面](docs/images/relaxer-v0.2.1.png)

## 下载

当前提供 **Windows x64 便携版**，运行包自带依赖，无需安装 Qt 或开发环境。

| 版本 | 界面 | Windows 下载 | 源码 | 发布说明 |
| --- | --- | --- | --- | --- |
| **0.2.1-GA · 推荐** | 默认 QML，保留 Widgets 兼容模式 | [Win64 ZIP](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.1-GA/Relaxer-V0.2.1-GA-Win64.zip) | [Source ZIP](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.1-GA/Relaxer-V0.2.1-GA-Source.zip) | [发布说明](docs/releases/v0.2.1-GA/RELEASE-NOTES-V0.2.1-GA.md) |
| 0.2.1 · 历史稳定版 | 默认 QML，保留 Widgets 兼容模式 | [Win64 ZIP](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.1/Relaxer-V0.2.1-Win64.zip) | [Source ZIP](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.1/Relaxer-V0.2.1-Source.zip) | [发布说明](docs/releases/v0.2.1/RELEASE-NOTES-V0.2.1.md) |
| 0.2.0-GA · 历史稳定版 | Widgets | [Win64 Portable ZIP](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.0-GA/Relaxer-V0.2.0-GA-Win64.zip) | [Source ZIP](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.0-GA/Relaxer-V0.2.0-GA-Source.zip) | [发布说明](docs/releases/v0.2.0-GA/V0.2.0-GA-ReleaseNotes.md) |
| 0.2.0-rc2 · 历史预发行版 | Widgets | [Win64 ZIP](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.0-rc2/Relaxer-V0.2.0-rc2-win64.zip) | [Source ZIP](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.0-rc2/Relaxer-V0.2.0-rc2-foundation-src.zip) | [发布说明](docs/releases/v0.2.0-rc2/V0.2.0-RC2-ReleaseNotes.md) |

以上四个发行条目包含三个稳定版本和历史 RC2 预发行版，版本文档集中于 [`docs/releases/`](docs/releases/)，当前程序源码位于 `src/`。

各 Release 均附 `SHA256SUMS.txt`。GitHub 自动生成的 Source code 归档是源码，不是可直接运行的 Windows 播放器。

## 开始使用

1. 下载所需版本的 Win64 ZIP，**完整解压到一个新的可写文件夹**。
2. 双击解压目录中的 **`RelaxerStart.exe`**，然后导入本地音乐。
3. 保留整个目录结构；不要直接在压缩包内运行或单独取出 EXE。

0.2.1-GA 默认打开 QML 界面；`RelaxerQml.cmd` 也可使用。兼容界面可运行：

```powershell
.\RelaxerStart.exe --widgets
```

日常本地播放可以离线使用。在线歌词和翻译需要网络，AI 翻译需要自行配置提供方；付费请求仍需用户明确确认。

0.2.1-GA 修复歌单重复创建，加入大封面与平滑收起的歌单页头，并移除展开播放页多余的全屏按钮。详见 [发布说明](docs/releases/v0.2.1-GA/RELEASE-NOTES-V0.2.1-GA.md)。

## 功能

- 本地音乐导入、音乐库、歌单、收藏、播放队列与播放状态保存。
- 滚动歌词、双语显示、歌词偏移，以及歌词搜索、预览和手动导入。
- 展开播放与全屏播放，保留 Widgets 兼容界面。
- 调色盘和图片外观、字体设置、文字与进度条配色，以及左侧、右侧、底栏、展开播放的模糊调整。
- 可选的歌词翻译流程；使用自己的提供方配置与凭据。

## 从 0.2.0-GA 或 0.2.1 升级

完全退出旧程序，备份整个 `data` 目录，再将**资料副本**放到新版本启动器旁的 `data`。保留原备份，不要让两个版本同时写入同一份资料。曲库、歌单、歌词与设置沿用既有存储；升级说明见 [0.2.1-GA 发布说明](docs/releases/v0.2.1-GA/RELEASE-NOTES-V0.2.1-GA.md)。既有重复歌单保留，由用户自行管理。

## 校验下载

```powershell
Get-FileHash .\Relaxer-V0.2.1-GA-Win64.zip -Algorithm SHA256
```

将结果与同一 Release 的 `SHA256SUMS.txt` 对照。

## 从源码构建

环境：Windows、MSYS2 UCRT64、C++17、CMake 3.22+、Ninja，以及版本匹配的 Qt 6 Core/Gui/Widgets/Sql/Multimedia/Svg/Network/Qml/Quick/QuickControls2；构建测试还需 Qt Test。

```powershell
git clone https://github.com/QUEQIAN1961/Relaxer.git
cd Relaxer
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build --target Relaxer RelaxerStart --parallel
```

若 Declarative 安装在单独前缀，配置时加入 `-DQT_ADDITIONAL_PACKAGES_PREFIX_PATH=<对应前缀>`。依赖和部署脚本见 `scripts/`。源码根的引导启动器用于从源码准备环境；直接使用请选择 Release 的完整 Win64 包。复现已发布版本时，请先切换到对应发行标签再构建。

## 发布与验证

0.2.1-GA 整合用户已手动验收的歌单修复，再以 GA 标识重新构建，并完成开发方专项回归与原生包验证。本次 GA 补丁没有新增的 Harness 独立验收结论。

历史 0.2.1 的 Windows 下载来自已通过用户测试和 Harness 专项独立验收的构建，构建提交为 `eb3027eed47109ab300047e8fc2d0fbe0246081a`。0.2.0-GA 保留其历史稳定构建；两版的来源、哈希和验证范围见各自 Release。

在线服务可用性、所有硬件/编码组合和长时间稳定性没有获得全面保证。反馈问题时请附版本、Windows 环境、复现步骤和脱敏日志；不要提交 API Key、个人数据库或音乐文件。

## 许可证与致谢

项目由作者拥有权利的代码采用 **GPL-3.0-only**，全文见 [LICENSE](LICENSE)，项目介绍见 [ABOUT.md](ABOUT.md)。Qt、FFmpeg、OpenCC、编译器运行库及其他第三方组件保留各自许可证和版权声明，详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 及发行包中的许可目录。
