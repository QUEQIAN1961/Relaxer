# Relaxer 0.2.0-GA

## 中文

Windows x64 的 Widgets 稳定版，版本标识统一为 **0.2.0-GA**，命名格式与 0.2.1-GA 一致。

### 修复与调整

- 将程序版本、窗口标题、Windows 文件属性、发行标签和压缩包统一为 0.2.0-GA。
- 保持原稳定版的音乐库、歌单、收藏、队列、歌词搜索/预览、双语歌词与可选 AI 翻译行为。
- 资料目录、SQLite schema 和启动就绪协议 1.5.0 保持兼容；不包含 0.2.1 系列的 QML 界面。
- CMake 和 Windows 固定数字版本仍为 0.2.0 / 0.2.0.0，文字版本增加 GA 后缀。

### 下载与使用

下载 `Relaxer-V0.2.0-GA-Win64.zip`，完整解压到新的可写目录，双击 `RelaxerStart.exe`。
源码下载 `Relaxer-V0.2.0-GA-Source.zip`；以同页 `SHA256SUMS.txt` 校验文件。
升级前退出旧程序并备份整个 data，将资料副本放到新启动器旁，不要让两版同时写入同一份资料。

### 来源与验证范围

基于原稳定提交 `43203314012f509a2f83e4558b9206001b2e5f75`，只调整版本身份、许可与发布资料，重新构建程序和启动器。
原 RC2 Harness R10 及原稳定版用户验收为历史结论；本次 GA 命名构建另行检查窗口版本、启动器、独立资料目录和最终 ZIP 文件哈希，不冒充新的独立 Harness 或听音验收。
本轮没有重新执行全部历史测试、WASAPI、长期播放或真实付费请求。具体记录见 `VERIFICATION.json`。
项目代码采用 GPL-3.0-only；第三方组件保留各自许可证。

## English

The stable Widgets release for Windows x64 is now consistently named **0.2.0-GA**, following the same format as 0.2.1-GA.

### Changes

- Align application version, window title, Windows file properties, release tag, and archive names with 0.2.0-GA.
- Preserve existing library, playlists, favorites, queue, lyrics, and optional AI translation behavior.
- Keep the data directory, SQLite schema, and readiness protocol 1.5.0 compatible. This version uses Widgets.
- CMake and fixed numeric Windows versions remain 0.2.0 / 0.2.0.0; displayed versions carry the GA suffix.

### Download and upgrade

Extract `Relaxer-V0.2.0-GA-Win64.zip` into a new writable folder and run `RelaxerStart.exe`.
Source is provided as `Relaxer-V0.2.0-GA-Source.zip`; verify downloads with `SHA256SUMS.txt`.
Exit the previous version and back up data before upgrading. Do not run two versions against the same data folder.

### Provenance and verification

Based on stable commit `43203314012f509a2f83e4558b9206001b2e5f75`, with version identity, license, and release-document changes only. The application and launcher are rebuilt.
Historical RC2 Harness R10 and stable user acceptance remain historical findings. GA checks cover version identity, startup, isolated data, and final archive hashes; this is not a new independent Harness or listening verdict.
The full historical suite, WASAPI, long-duration playback, and paid requests were not rerun. See `VERIFICATION.json`.
Relaxer-authored code is GPL-3.0-only; dependencies retain their own licenses.
