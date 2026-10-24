# Relaxer 0.2.1

0.2.1 带来 QML 主界面，整合音乐库、歌单、播放队列、歌词、展开/全屏播放、设置与导入入口，并保留 Widgets 兼容模式。

## 本版改进

- 完整包通过统一启动器默认进入 QML；部署就绪文件或 QML 组件缺失时保留拒绝和错误提示。
- 图片与调色盘的文字、进度条、底栏配色统一为同行排列，未启用时保留灰化预览并禁止编辑。
- 修复展开播放背景模糊，增加独立底栏模糊；四区控件使用 2×2 布局，旧外观配置保持兼容。
- 图片共享与小字加粗选项同行；外观工具栏与普通页面对齐；窗口三按钮采用等大点击区域和矢量图标。
- 保留未保存外观的退出保护，以及既有曲库、歌词、播放与保存流程。

## 使用与升级

完整解压 `Relaxer-V0.2.1-Win64.zip`，双击 `RelaxerStart.exe`。`RelaxerQml.cmd` 继续可用，`--widgets` 打开兼容界面。

升级前完全退出旧程序，备份整个 `data`，再将资料副本放到新启动器旁。不要让两版同时写入同一份数据。

## 构建来源与验收

Windows 下载使用提交 `eb3027eed47109ab300047e8fc2d0fbe0246081a`、Tree `f353fa2a2b0f4390adc3f310fbad3d62056f0c6b` 的已验收原始包。发布只提升发行状态与整理页面，未重新编译或更改运行包字节；包内沿用的候选名称或开发证明是历史构建记录。

本轮用户手动测试成功。Harness 的外观专项报告及补充报告确认五项外观改进、默认 QML 启动、两个入口、缺文件拒绝、旧配置兼容和未保存关闭取消通过。本轮没有重跑历史全量 CTest/QtTest、47 项启动矩阵、WASAPI 或长时间稳定性；专项通过不代表所有硬件、网络服务和使用场景都经过全面验证。

在线歌词、浏览器翻译和 AI 提供方依赖外部网络与服务。AI 请求使用用户配置且需明确确认。发行包不包含个人音乐、数据库或凭据。

## English

Version 0.2.1 introduces the QML interface while retaining Widgets compatibility. This release fixes launcher deployment readiness, unifies optional color controls, makes footer blur independent, restores adjustable expanded-player blur, improves the appearance-page layout, and redraws the window controls.

Extract the entire Win64 ZIP and run `RelaxerStart.exe`. Back up your data before upgrading. The published Windows ZIP preserves the exact tested build from `eb3027e`; only release documentation and publication status change. User testing and the independent Harness scoped review passed. Full historical regression, exhaustive hardware/online-service coverage, and long-duration playback were not repeated for this patch.
