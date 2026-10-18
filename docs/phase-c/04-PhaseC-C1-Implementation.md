# C1 实际实现

交付身份：V0.2.1-dev-c1，Phase C 的 C0 + C1 开发节点；不是 V0.2.1 正式版。没有执行 C2—C6。默认启动继续完整 Widgets，显式 `--ui=qml` 进入 QML Foundation。

## 工程与组件

`CMakeLists.txt` 保持 C++17。`RELAXER_ENABLE_QML` 默认 ON，GUI 查找 Qml/Quick/QuickControls2；可 OFF 构建 Widgets。四服务配置在此之前 return。新增 relaxer_qml 静态库、relaxer_qml_tests 和 EXCLUDE_FROM_ALL 的导航测量探针。没有第三方 UI 框架、WebView、浏览器、第二播放器或第二数据库。

`src/qml/qml.qrc` 显式嵌入 Main、theme、components、pages；C++ Q_INIT_RESOURCE 独立保留 QML 与旧图标资源。没有依赖隐藏 Window。主窗口使用官方 ApplicationWindow/Controls Basic，1180×760，最小 900×600，原生边框负责最大化、还原和关闭，Qt 自身处理 DPI。

Theme 是本地 singleton：background/panel/text/secondary/accent/accentText/border、radius/spacing/fontSize/headingSize、hover/pressed/disabled/selected，支持仅本次运行的浅/深色预览。ShellButton、NavigationButton、EmptyState、PlayerBar 可复用。按钮含 accessible name、焦点、hover/pressed/disabled 状态。

| 页面/区域 | 已接入 | 保留范围 |
|---|---|---|
| 本地音乐 | 真实 Library 曲目数量、空库状态、基础列表容器 | 完整曲目列表/搜索/导入/收藏留待 C2 |
| 歌单 | 导航与明确未迁移说明 | 管理/排序/标签仍在 Widgets |
| 正在播放 | 真实歌曲/歌手，底部真实播放状态 | 歌词、队列、详情动画仍未迁移 |
| 设置 | 迁移状态说明、基础视觉预览 | 正式设置/外观/AI 留在 Widgets |
| 底栏 | current/artist/playing/position/duration/volume；对已恢复曲目转发播放暂停 | 进度与音量明确只读；无曲目禁用播放按钮 |

桥见 `src/ui-bridge/`。AppBridge 只保留页索引和展示用曲目数量；PlaybackBridge 只保留 facade 快照的展示投影，不保存 Queue。所有状态真实来源仍是既有 C++。QML 没有 SQLite 查询、业务匹配算法或 AI 请求入口。

## 启动与诊断

- `RelaxerStart.exe`：默认 Widgets；`RelaxerQml.cmd`：直接运行 runtime/Relaxer.exe --ui=qml。旧原生启动器不转发任意参数，所以独立提供便捷入口。
- `--data`、RELAXER_DATA、默认 dataRoot 的优先级与原算法不变。两种 UI 共用原 QLockFile，不能同时打开同一资料目录。
- `--diagnostics <JSON> --data <隔离目录> --quit-after <1..60000>`：可重复启动观测；退出前实际调用 shutdown。诊断记录一条数据库连接/一个 Playback。非法 UI/页面/参数组合在建立资料目录前拒绝。
- 退出码：64 明确用法错误，3 资料锁，2 数据库，7 截图，8 最终保存失败，9 诊断写出失败，10 QML 创建失败。Qt parser 自身的未知选项返回非零并给出文本。
- 开发身份通过新 phase-c-version.h / 独立 Windows .rc 资源展示；旧正式资源文件与 release-version.h 不变。main 注入开发标题，Window 仅三处版本展示适配；关于/标题都显示 dev-c1。直接构造旧 Window 的冻结测试仍使用原身份回退。

实际便携验证使用官方 qmlimportscanner 的 32 条导入记录和 262 个复制依赖（含辅助工具的独立闭包）。便携目录包含完整 Qt QML/Quick/Controls 运行库、模块、平台/SQLite/FFmpeg 插件、许可证与工具。Qt 开发环境本身没有被安装或改写；缺少的 Declarative 仅在隔离 overlay 中展开。
