# Relaxer 0.2.1-GA

## 中文

本次正式增量版基于 0.2.1，整合用户已实测通过的歌单修复。版本标识为 **0.2.1-GA**；此前所有版本的标签和附件保留原样。

### 修复与调整

- 修复一次确认新建歌单却创建两份的问题：移除公共确认框重复连接的确认/取消信号。覆盖侧栏、歌单页、批量新建并加入、歌曲菜单新建并加入；既有重复歌单保留，允许用户主动分两次建立同名歌单。
- 歌单页头采用 160×160 大封面和四行布局。向下滚动时，封面平滑缩至 78×78，与两行按钮总高一致；统计、标签和简介移动到右侧。220ms 动画支持中途反向，并遵循减少动态效果设置。
- 移除展开播放页右上角多余的全屏按钮，保留收起、底栏展开/收起及原有窗口还原行为。
- 程序标题、版本显示和 Windows 文件属性统一增加 GA 后缀。CMake 版本与 Windows 固定数字版本仍为 0.2.1 / 0.2.1.0；启动就绪协议仍为 1.5.0。

### 下载与使用

下载 `Relaxer-V0.2.1-GA-Win64.zip`，完整解压至新的可写目录，双击 `RelaxerStart.exe`。`RelaxerQml.cmd` 保留；Widgets 兼容模式使用 `RelaxerStart.exe --widgets`。

升级前完全退出旧版并备份整个 data 目录，再将资料副本放到新启动器旁。不要让两个程序同时写同一份资料。既有重复歌单不会自动删除或合并。

### 来源与验证范围

功能修复提交：`40b7435c11eeeb153fda7bfc0e4c3393d0504124`，用户已手动测试并接受。GA 源码与构建身份、包内文件清单、最终 ZIP 原生验证记录随发行附件提供。

开发方验证包括歌单专项的 14 个功能场景（QtTest 含初始化/清理共 16 项）、相关界面回归，以及 GA 版本显示和启动包核验。原生检查使用独立资料目录、中文与空格路径、包内依赖和 Windows PATH；重新解压 ZIP 后核对所有载荷哈希，并验证默认启动器、兼容 CMD 与缺失 QML 组件的失败行为。

本次增量没有新的独立 Harness 验收结论，也未重新执行全部历史测试、完整 47 项启动矩阵、WASAPI 或长期播放测试。具体运行结果以 `VERIFICATION.json` 为准。原 0.2.1 的独立验收结论属于原版本。

项目代码采用 GPL-3.0-only；第三方组件保留各自许可证。发行包附许可文件与第三方声明。

## English

This stable update builds on 0.2.1 and integrates playlist fixes manually tested and accepted by the user. Its version is **0.2.1-GA**. All earlier tags and release attachments are preserved.

### Changes

- Fix creating two playlists from a single confirmation by removing duplicate confirmation/cancellation signal connections. Covers the sidebar, playlist page, bulk create-and-add, and track-menu create-and-add. Existing duplicates remain, and deliberately creating the same name twice is supported.
- Restore a 160×160 playlist cover and a four-row header. Scrolling smoothly shrinks the cover to 78×78, matching the two button rows, while statistics, tags, and description move right. The 220ms animation supports reversal and reduced motion.
- Remove the extra fullscreen button at the top right of the expanded player. Collapse, footer expansion/collapse, and existing window restoration remain available.
- Add the GA suffix to application titles, displayed versions, and Windows file properties. CMake and fixed numeric Windows versions remain 0.2.1 / 0.2.1.0; the readiness protocol stays at 1.5.0.

### Download and upgrade

Extract the complete `Relaxer-V0.2.1-GA-Win64.zip` into a new writable folder and run `RelaxerStart.exe`. `RelaxerQml.cmd` remains available; use `RelaxerStart.exe --widgets` for compatibility mode.

Exit the previous version, back up the full data folder, and place a copy beside the new launcher. Do not let two applications write to the same data folder. Existing duplicate playlists are not automatically removed or merged.

### Provenance and verification

The functional fixes are commit `40b7435c11eeeb153fda7bfc0e4c3393d0504124`, manually accepted by the user. Release attachments provide the GA build identity, payload hashes, and verification results.

Developer checks cover 14 playlist scenarios (16 QtTest passes including setup/cleanup), related UI regressions, GA version identity, and native package startup. Native checks use isolated data, Chinese/spaced paths, bundled dependencies, and a Windows-only system PATH. The final ZIP is extracted, all payload hashes are checked, and launcher/CMD startup plus missing-QML failure behavior are verified.

This patch has no new independent Harness verdict. The full historical suite, complete 47-case startup matrix, WASAPI, and long-duration playback were not rerun. See `VERIFICATION.json` for actual results. The original 0.2.1 Harness verdict applies to that historical build.

Relaxer-authored code is GPL-3.0-only; dependencies retain their own licenses. License files and third-party notices are included.
