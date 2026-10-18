# C0：V0.2.0 架构调查

调查对象是正式提交 `43203314012f509a2f83e4558b9206001b2e5f75`，Tree `6892dccb401ec6ca71cdfc71d668cafa3bf17858`，annotated tag `v0.2.0` 对象 `060623c5ffddf81eff6e18e50f30314f04ef8ae9`。中央 bare 仓库缺少此对象，已从正式 Complete-History.bundle 恢复 release 分支和原标签，随后建立独立 checkout / `phase-c/v0.2.1-qml-foundation`。没有使用 RC 替代正式起点。

## 入口与所有权

- `src/start-windows.cpp`：原生 Windows 启动器，选择 portable 或 first-start 构建路径；设置运行时 PATH、插件环境和 `RELAXER_DATA=<package>/data`；启动 `runtime/Relaxer.exe --launch`，等待并记录退出码。它不转发任意命令行，C1 的 QML 选项应直接传给播放器，附独立便捷启动脚本。
- `src/main.cpp`：QApplication / Fusion / 应用身份；先处理隔离音频诊断，然后解析 dataRoot、创建目录、取得 `relaxer.lock`，创建一个栈上 Library 并 open。`--data` > `RELAXER_DATA` > executable 相邻 data（runtime/build* 先上移一级）。同一资料目录的第二实例被 QLockFile 拒绝，不能同时由两个界面编辑。
- 普通启动创建 `Window(&lib)`；`src/window.cpp` 的 Window 构造唯一 PlaybackFacade、ImportCoordinator、AiRuntime 和 Widgets 树。构造末尾 refresh、restore 队列、延迟修复旧封面。PlaybackFacade 内部拥有唯一 Playback；Queue 是 Playback 的值成员。
- `resources/resources.qrc` 由 Window 中 Q_INIT_RESOURCE 显式保留。QML 模式不能依赖一个隐藏 Window 来初始化资源。
- Window closeEvent：保存窗口大小；按设置可关闭到托盘；保护未保存外观预览；导入未结束则等待 shutdownReady 再关闭；然后关闭导入与播放。析构重复 shutdown 是幂等安全网。LyricsLookup 析构/关闭承担搜索、翻译任务及未保存预览保护；它们不应在 C1 被隐藏创建。

## 实际 Widgets 表面

| 表面 | 源码 / 类型 | 依赖及事件 / 更新 |
|---|---|---|
| 主窗口、侧栏、页面容器 | window.h/.cpp / Window、QListWidget、QStackedWidget | Library.changed → refresh；statsChanged 合并延迟刷新；导航直接调用 showLibrary/showPlaylists/showProfile/showSettings/selectScope |
| 顶部标题与无边框控制 | chrome.h/.cpp / TitleBar、CaptionButton；client-frame-windows.h | Qt 窗口事件、Windows 原生边框、最大化/还原/关闭 |
| 本地音乐、收藏、搜索、筛选 | Window::buildLibrary；widgets.h/.cpp / SongTable、SongDelegate；v14widgets / FilterButton | Library.tracks/recent、Library.changed；双击 → facade.play；菜单、批量栏、过滤条件刷新整表 |
| 歌单导航/卡片/详情 | Window::buildPlaylists、playlistMenu；playlist-ui / PlaylistHeader、Tags/DescriptionDialog | Library.playlists 与 CRUD；排序/收藏/移除/描述/标签写回 Library |
| 底部控制、展开详情 | Window::buildPlayerBar / buildNowPlaying；CircleButton、Slider、Cover、Backdrop | facade.trackChanged/stateChanged/positionChanged/seekableChanged；进度绑定、歌词时钟、播放模式/音量；NextQueuePanel 和 ArtworkFlight/PageMotion 负责队列与动画 |
| 播放大页面 | Window::setPlayerPage/showNowPlaying/closeNowPlaying、motion.cpp | 是同一 Window 内展开布局/动画，没有第二个播放器，也没有独立顶层全屏播放器类 |
| 歌词显示及工具 | widgets / LyricView；lyric-tools / LyricTools、FrostedLyricScroll | LyricsDocumentReader、位置同步、延时、双语显示、手动滚动 |
| 搜索与翻译窗口 | lyricslookup.h/.cpp / LyricsLookup (QDialog) | LyricsSearchCoordinator、TranslationCoordinator；候选 revision/generation、分类筛选、预览保存/丢弃确认；异步结果后更新各行 |
| 设置与关于 | Window::buildSettings/buildVersionLog；changelog.cpp / ChangeLog | 设置卡片内含关于，没有独立 AboutWindow；SQLite settings；音频/快捷键/字体/外观/AI 设置入口 |
| 外观 | appearance / AppearancePage、Surface；appearance-image；storage/appearance-store | 状态/草稿/预览/confirmLeave；持久化、自定义图片与进度推荐色 |
| AI 设置 | ai/ai-settings-page / AiSettingsPage | AiRuntime、CredentialStore、modelCatalogFinished；请求统一协调，不能从 QML 直接网络调用 |
| 弹窗/菜单/快捷键/托盘 | RoundedMenu、QFileDialog、QInputDialog、QMessageBox、歌单对话框；shortcuts / PlayerShortcuts | 局部 QAction、Ctrl+O/Ctrl+Shift+O/Ctrl+F、媒体键、Windows 全局快捷键；托盘仍由 Window 所有 |

## 可复用服务与线程

| 模块 | 已有接口及边界 | C1 决定 |
|---|---|---|
| PlaybackFacade | snapshot/current/position/duration/playing/volume/queue；play/toggle/seek/restore/save/shutdown；snapshotChanged 是有 context 的 queued publication | 借用对象，投影为明确 Q_PROPERTY；不导出 backend 给 QML，不复制 Queue |
| Playback / Queue | Qt 音频 + Windows MIDI transport；自动下一首、随机循环、session 统计、4 秒持久化 | 源码不变，不能引入 QML MediaPlayer |
| Library | tracks/playlists/track、settings、sessions、歌词与分类；每实例 UUID SQLite connection | 单实例/单次 open，所有调用留在 GUI 线程；C1 只读取 count，不提前执行 C2 列表功能 |
| SettingsStore / AppearanceStore | 借用 Library，无独立连接；事务和通知仍在 Library | C1 不另存设置；只提供独立基础视觉 tokens，不覆盖正式外观 |
| ImportCoordinator | start/cancel/shutdown；worker 输入值对象和 paths，GUI 线程提交；结果 barrier + thread exit 才 finished | 保留 Widgets；QML 不创建导入 worker |
| LyricsSearchCoordinator | start/stop/loadCached/candidates/shutdown；网络在 owner loop，排序在 QThreadPool；hash/generation/epoch/revision 丢弃过期结果 | 保留 Widgets；后续专门 typed model，不能直接把整个候选 JSON 搬给界面 |
| TranslationCoordinator | prepare 不发请求，start 是显式同意边界；save 校验歌曲和文件快照；CompletedPendingSave 不是 terminal | C1 不开放入口；C5 必须等价保护同意与预览 |
| AiRequestCoordinator / AiRuntime | provider/model/credential 检查，task id、超时、取消；queued provider 回调与 QPointer | 完整保留，不读真实 key，不发付费请求 |

Library、PlaybackFacade、Playback、桥和 QML engine 均属于 UI 线程。网络回调以 QObject context 自动断连；工作线程不拿 SQLite 对象。歌词排序只传值快照并检查有效性。新桥同样需要 QPointer、context-bound connect 和销毁顺序测试。没有理由在 C1 重写这些业务模块。

## 数据身份

`Library::open` 在既有 root 中创建 Music/Artwork/Lyrics/Profile，打开 `relaxer.sqlite`，QSQLITE、foreign_keys、WAL、busy_timeout=5000；已有兼容 ALTER 逻辑仍原样保留，C1 不加 schema。曲目 id 和受管相对 file path 不变。

settings 是同一 SQLite 的 JSON 编码表，而不是新增 QSettings 数据目录。queue/current/position/activeSession、volume/mode、窗口、字体、外观、AI 配置均经既有接口保存。歌词实体在 Lyrics，候选缓存为 lyrics_lookup 表，延时/人工分类在 lyrics_playback；分类编码继续由 Library 控制。封面在 Artwork，用户资料 Profile，字体 Fonts，自定义主题图经 AppearanceStore 的 imagePath/已有复制规则引用。凭据存储继续由 AiRuntime/CredentialStore 管理，浏览器后端有专用 browser-translation 状态；C1 不读取或启动它。

测试只用 QTemporaryDir / 独立工作目录的合成媒体和数据库。真实用户目录、正式源码/包、正式提交/标签均不作为写入测试目标。
