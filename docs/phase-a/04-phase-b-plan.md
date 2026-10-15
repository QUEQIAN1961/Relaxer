# Phase B 分阶段重构实施方案（待审核）

本文件仅为实施方案。本阶段没有创建以下业务模块或接口，没有开始代码迁移。收到审核批准后才能执行 Phase B；Qt Widgets 继续作为正式 GUI，不同时引入 Qt Quick 或重写主界面。

## 1. 目标与约束

目标是独立的 C++ 业务核心、清晰的 Qt 对象接口，以及可替换的 UI 前端。这里的独立指不依赖 QWidget/具体窗口，并非强行去掉 QString、QObject 等已广泛使用的 Qt Core 类型。

优先复用已有实现：Playback、Library、Importer 和文本处理已不依赖 Widgets，不需要重新发明播放、数据库或文件格式。先切断无必要的编译和生命周期依赖，再移动编排逻辑。每步单独提交、单独验收，保持前后行为可比较。

保持以下外部契约：

- `1.10.0`、产品更新日志、数据库表/键、ID、相对路径、统计规则不变。
- 原 Widgets 入口、选择/确认行为、错误提示、候选顺序、翻译保存与取消行为不变。
- 普通歌词/计时歌词/双语及两种延时继续兼容；已有未知设置键原样保留。
- 现有 `relaxer_core/relaxer_ui` 对外链接入口和必要头文件可保留转接层，避免一次改完所有测试与调用方。
- 现有引导 EXE、许可证、首次准备和部署脚本继续可用；不把启动系统与核心拆分绑在同一提交。

## 2. 推荐模块边界

| 模块 | 包含 | 允许依赖 | 禁止/不承担 |
| --- | --- | --- | --- |
| domain | Track/Playlist/Session DTO、Queue、歌词文本/时间轴、匹配策略 | C++17、Qt Core | QWidget、SQL 连接、浏览器进程、具体播放器窗口 |
| storage | 现有 Library 的兼容门面，SQLite 实现，设置、歌单、歌词文件及图片存储 | Core、Sql；图片操作可留 Gui | 界面选择、弹窗、QML、任意线程共用单连接 |
| media | 现有 Playback、媒体/转码/MIDI 平台实现、工具定位 | Core、Multimedia、平台 WinMM；存储接口 | 控件、页面动画、歌词候选 UI |
| services | 导入编排、歌词搜索/排序/翻译协调器、主题持久化策略 | Core、Network、domain、storage、media | QWidget、直接调用窗口、QMessageBox |
| interfaces/qt | 面向前端的 QObject facade、状态 DTO、命令/通知；必要的列表适配 | Core/Qt 模型；服务接口 | 持有旧窗口或在此重新实现业务规则 |
| ui/widgets | 现有 Window、对话框、绘制、动画、选择与确认、系统托盘、本地快捷键绑定 | Widgets、Gui、Svg、上述 facade | 第二份数据库/播放队列/网络任务状态机 |
| platform/windows/bootstrap | 现有 Win32 启动器、准备系统和进程监督 | Win32、标准 C++、现有脚本 | 播放器服务和未来 QML 页面 |

不要为每个类都造一个抽象接口。先在需要生命周期隔离、可控测试或替代后端的位置增加小接口；其余保留具体值对象和现有实现。

## 3. 目录与构建建议

第一步优先拆 CMake 目标和头文件，不要求一次物理搬迁目录。稳定后可逐步形成如下结构，旧头文件保留短期转接：

```text
src/
  domain/              # model、queue、lyrics、lyric-text、lyric-match
  storage/             # library facade、sqlite、settings、lyric documents
  media/               # playback、transcode、midi backend
  services/            # import、lyrics-search、translation、appearance-store
  interfaces/qt/       # application services、QObject facades、list adapters
  ui/widgets/          # window、dialogs、widgets、motion、appearance page
  platform/windows/    # bootstrap、native frame、native shortcuts
tests/
  domain/ storage/ services/ widgets/ platform/
```

构建目标建议逐步引入 `relaxer_domain/relaxer_storage/relaxer_media/relaxer_services`；保留 `relaxer_core` 和 `relaxer_ui` 兼容链接层。新无界面服务测试只链接所需目标，防止静态 UI 库的 MOC 对象又把 Widgets 拉回去。静态资源初始化和 Win32 RC 编译路径保持原状。

## 4. 推荐接口与职责

以下是契约建议，不是已经实现的 API；具体名字可在单步设计时调整。

| 接口/对象 | 最小职责与兼容方式 |
| --- | --- |
| `SettingsStore` | get/set/setMany，保留 QJsonValue、现有单元素 JSON 编码与默认值；先委托 Library，暂不换数据库 |
| `TrackRepository/PlaylistRepository` | 按 ID 读取、查询快照和现有写入；只在协调器需要替身时抽象，禁止复制另一个内存权威库 |
| `LyricDocumentStore` | load/save 原文与译文、纯音乐、文件偏移与播放延时；先包住 Library 和 Window 的读文件路径，保持原字节语义 |
| `LyricMatchPolicy` | normalized/searchTrack/matchScore/confident/credible/rank 的值输入输出；旧 LyricsLookup 静态入口暂时转发 |
| `TranslationCoordinator : QObject` | start/cancel/resume + taskId/trackId/progress/needsAttention/completed；持有现有 BrowserTranslation，整首完成才保存 |
| `LyricsSearchCoordinator : QObject` | 显式查询输入、候选快照、请求预算/重试/评分/取消；只发结果和需确认事件，不读 QListWidget 行号 |
| `ImportCoordinator : QObject` | 管工作线程、取消、结果提交、计数与目标歌单；Window 只收集文件路径并显示进度 |
| `PlaybackFacade : QObject` | 可观察 current/position/duration/playing/seekable/mode/volume，调用现有 Playback；新增信号后逐步消除 UI 对 media() 的直接订阅 |
| `ApplicationServices` | 显式组装依赖及销毁顺序；初期可保留与旧窗口等价的生命周期，再单步移到入口级所有权 |

任务接口必须统一 taskId、取消终态与返回对象的线程归属；不要用 QLabel 文本驱动服务状态。选择版本、是否覆盖和是否停止的确认留在前端，决定通过明确命令传给服务。

Q_PROPERTY 或 QAbstractListModel 适配应服务于明确的状态契约；不在本阶段预写 QML 页面或把全部 DTO 改成动态 QVariantMap。

## 5. 小步实施顺序

| 步骤 | 改动边界 | 独立验证与退出条件 | 回退单位 |
| --- | --- | --- | --- |
| B0：冻结契约 | 复核本次标签、数据备份、接口事件表；补每步需要的缺口测试 | 原 21 组、新增契约及 runtime 副本都通过；无业务代码变化 | 独立准备提交 |
| B1：拆无 UI 的构建依赖 | 将 BrowserTranslation、lyric_remote 从 UI 编译目标移至 Core/Network 服务目标；不改行为、不重排 DOM 逻辑 | 新目标可在无 Widgets 的小测试程序链接；原 Widgets、翻译取消夹具和完整回归不变 | 仅构建边界提交 |
| B2：整理值对象与 Queue | 原样抽出 Queue/Mode 和文本、时间轴的目标边界；现有头文件保留兼容 include。已经独立的文本代码无必要不搬 | queueModes/shuffle/预测与实际下一首、双语/延时测试；禁止“顺手优化”随机算法 | Queue 与纯值模块提交 |
| B3：抽匹配策略 | 从 QDialog 抽值算法，保留静态转发及评分计数测试入口 | Phase03 的 comparator、计数、过期评分；V16/031 候选及自动保存行为完全一致；新增无 QDialog 单测 | 匹配策略提交 |
| B4：设置与主题存储边界 | SettingsStore 委托 Library；将 AppearanceStore 与 QWidget 页面分开声明/编译，图像处理仍可用 Gui | 新库/旧库主题、未知 JSON、进度色迁移、草稿保存取消全部通过；不改变键或缺省值 | 设置门面与 Store 拆分可各一提交 |
| B5：歌词文档读取门面 | 将 Window 的读文件/解析/纯音乐/延时查询封装，保存仍走原 Library；不改文件/DB 原子性 | Phase A 双语重开和两种 offset、V16 分组滚动、V19 延时、原文件字节对比通过 | 文档服务提交 |
| B6：导入协调器 | 把 Window 管线程/插库/目标歌单/计数的业务编排移到 QObject；确认和提示仍在 Widgets | 真实临时文件导入、重复导入、取消、目标歌单、销毁等待、封面修复；DB 仍在原线程提交 | 导入与封面可分两步 |
| B7：整首翻译协调器 | 复用 BrowserTranslation，将队列、进度和整首保存从窗口移出；UI 行与 taskId 映射留适配器 | 先补等待验证/恢复/导航竞争的可控传输测试，再验取消、换原文、关闭窗口、不完整不保存；最后独立实网人工验收 | 翻译协调器提交 |
| B8：歌词搜索协调器 | 分离查询计划、来源槽/预算、网络、评分、缓存、接受策略；窗口只提供输入与确认 | 先得到无对话框事件轨迹，再逐段迁移。Phase03/031、多来源预算、老回调、候选排序及手动选择全通过 | 查询计划、调度、保存策略多次小提交 |
| B9：播放前端接口与生命周期收口 | 包装现有 Playback，转发 seekable 等通知；迁移 UI 的保存定时职责及最终服务所有权 | 先补可控媒体状态序列或独立硬件作业，验证暂停/切歌/恢复/统计；Widget 功能不变。不能重建后端打断播放 | facade、保存职责、所有权分别提交 |

以上是顺序建议，不要求为了“完成架构图”而全部创建抽象层。B1/B2/B3 是低耦合入口；B7/B8 是最重的业务风险，必须由先前接口和测试支撑。B9 主要清理状态接口，当前播放实现本身无需重写。

## 6. 兼容和风险控制

- 每个提取步骤同时保留旧调用入口的薄转发，迁完调用方再考虑移除；不在同一提交改函数行为、用户界面和目录结构。
- `Queue::upcoming()` 的随机袋预生成是状态语义的一部分，不能为了 const 形式删掉。
- SQLite 连接继续留在当前线程；不要把 Library 指针直接交给新线程。导入结果的 DB 提交与 worker 并发分开。
- 不改播放统计阈值、seek 后回退计数、MIDI alias、转码缓存键、错误自动跳过和恢复暂停状态。
- 不把歌词文件 `[offset:]` 和实时播放延时合并；不把计时原译改成另一种文件格式。
- 保留限流/预算/来源交错、候选真实性和自动保存门槛；“搜索成功”不得等同“任意候选都可自动保存”。
- 关闭窗口目前会取消歌词任务并关闭独立浏览器；服务移到更长生命周期时要主动维持这个外部行为，不能意外后台继续写入。
- `QPointer`、token、generation、epoch 以及 QObject receiver 上下文要逐个迁移，不只复制任务结构体。
- 静态库拆分注意 AUTOMOC 的唯一归属、RCC 显式初始化和部署 DLL 闭包，避免链接通过但独立包无法运行。
- Qt Quick/QML、数据库 schema 改造、启动器源码检测、ready.version 修复各自另立变更，不与本阶段拆分混做。

## 7. 每步验收与回退

每步在新的本地分支或提交上执行，先记录起点哈希。验收必须包括：相关模块的无界面测试、原 Widgets 交互测试、完整离线 CTest、新构建的隔离 Runtime、旧数据副本重开，以及该步新增的取消/生命周期场景。

通过后标记该步标签，再进入下一步。用户界面、数据库和文件格式任何变化都要单独列出并批准，不能以“内部重构”为由忽略。

回退优先把该步之后的源码恢复到新目录，配套恢复对应构建产物；个人 data 不受 Git 管理，始终单独备份。没有 schema 修改时无需转换用户资料。保留原 accepted 和 Phase A 标签与 bundle，禁止通过清理忽略文件来“回退”。

## 8. 审核门槛

Phase A 的源码和四份报告先交审核。审核确认模块边界、步骤顺序及各步进入条件后，再新开 Phase B 工作。当前没有实际执行 B0—B9，没有启动 Phase C/D，也没有添加 Qt Quick 依赖。
