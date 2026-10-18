# 兼容性与安全边界

唯一稳定起点是正式 v0.2.0 / 43203314012f509a2f83e4558b9206001b2e5f75。正式 tag 对象和 release 分支保持原值，原交付目录没有覆盖。

## 复用证明

入口在同一 root / lock / Library.open 后互斥选择 Window 或 QmlShell。Widgets 仍由 Window 拥有 facade；QML owner 拥有一个 facade。没有同时创建两套完整服务。QtTest 对生产 owner 的 Playback/Facade 子对象和 SQLite connectionNames 进行断言；两个 UI 的独立进程诊断均为 Playback=1 / connection=1。模型/桥不打开连接，也不调用 Library.open。

与正式 Git 比较：Library、Playback、PlaybackFacade、Queue、MIDI、Importer/ImportCoordinator、歌词、匹配、TranslationCoordinator、AI、Settings/Appearance 核心文件字节不变。原有测试没有删改。Window.cpp 仅开发身份显示适配；主窗口构造、设置、菜单、预览、导入和 closeEvent 的业务流程均保留。

MP3/FLAC/WAV/MIDI 路径仍使用既有 Playback。MIDI 长路径修复没有改动；MIDI 17 项、媒体烟测、Queue/Facade、歌词/翻译/统计等旧完整测试重跑通过。自动化使用合成音频/Mock；没有新做物理听音、真实付费 API、一小时稳定性或真实网站验收，历史人工结论没有冒充本轮结论。

## 数据

dataRoot 算法原样保留，--data > RELAXER_DATA > executable 相邻 data（runtime/build* 上移）。没有第二 data、schema 变更、曲目重新编号、媒体重写、用户库扫描或复制。现有 Library.open 的历史升级逻辑保持原样，不增加升级步骤。设置、歌词缓存、延时和人工分类继续使用原来的 SQLite / Lyrics / Artwork / Profile / Fonts。

专项进程测试使用隔离合成数据库，验证显式 --data 优先于哨兵环境路径、没有意外 data 子目录、曲目/收藏/歌单/延时/人工分类/自定义设置保留、锁释放、正常退出。旧存储/歌词/翻译分类/集成测试提供更广的兼容验证。持久化正常会更新播放设置/session，测试不把预期保存当作数据库字节完全不变。

## 歌词与 AI

所有原 Widgets 入口保留。QML 没有 AI、歌词搜索或付费授权入口，没有构造 AiRuntime/搜索/翻译协调器或后台导入 worker。C5 前不会开启这些 UI；prepare/start、单曲门禁、未保存预览、hash/文件快照及跨曲保存保护没有旁路。没有使用真实 DeepSeek Key 或付费请求。

## 回退与退出

同一开发包普通启动就是 Widgets；关闭 QML 后用启动器即可返回。没有同进程热切换。另用无 Declarative overlay 的 Widgets-only 配置完整构建播放器/启动器 71/71；PE 导入没有 Quick/Qml，默认启动通过，显式 qml 请求在创建目录前以 64 拒绝。

QML engine 最先销毁，桥使用 CppOwnership / QPointer / context-bound connect。backend 生命周期通知解决了旧 facade 析构先 disconnect 自身信号的特殊情况。shutdown 幂等，停止媒体与计时器、保存队列/session；保存失败可观察并返回 8。原生 WM_CLOSE、启动器父子进程和 10 次测量均未发现所属残留进程。退出保存失败的交互式重试能力仍继承历史 P3，未声称已修复。
