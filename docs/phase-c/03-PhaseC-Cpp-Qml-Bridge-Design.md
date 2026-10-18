# C++ / QML 桥设计

## 状态与命令

AppBridge 借用当前唯一 Library，提供只读开发版本、曲库数量、有限页面索引。导航是 UI 状态，不保存到业务数据库。数量从既有 Library 查询计算，Library.changed 到桥的刷新应合并，播放定时持久化不能引发每帧重复刷新。C1 不导出 SQLite、凭据或服务内部指针。

PlaybackBridge 借用唯一 PlaybackFacade，提供 title、artist、hasTrack、playing、position、duration、volume、available 和错误文本等明确 Q_PROPERTY。只在值变化时发相应 NOTIFY。命令 toggle() 仅转发既有 facade.toggle，不创建队列/媒体播放器；没有 current 时禁用。快照到达时读当前 authoritative snapshot，避免 queued 旧值把较新状态覆盖。UI 不以 JS 预测播放状态。

## 所有权

入口：QApplication → root lock → 栈上 Library.open → 互斥 Window 或 QmlShell。QmlShell 内创建 PlaybackFacade、AppBridge、PlaybackBridge、QQmlApplicationEngine。先设置 CppOwnership / context properties，再加载 qrc Main.qml。引擎最后创建、最先销毁，使 QML 对象先释放借用引用，然后销毁桥，接着 facade，最后外部 Library。

不向 QML 暴露可析构 C++ 服务；QPointer guards 允许被借用服务先销毁时把界面置 unavailable、清空快照，命令安全无效。connect 必须带接收 context，延迟任务必须带 QObject context；C1 不建后台任务。所有访问必须在 owner GUI 线程，构造断言线程一致。

## 关闭与失败

ApplicationWindow close → engine.quit / lastWindowClosed → app event loop 返回；QmlShell.shutdown 幂等转发 facade.shutdown，停计时器、保存 session/queue、停止 MIDI/Qt 后端。aboutToQuit 也连接关闭，析构再作安全网；保存失败必须可观察并影响进程退出码。加载失败先销毁部分 QML，再 shutdown，返回非零。

界面模式参数在创建 dataRoot/Library 前验证；未知模式/页面及不支持参数组合明确报错，不能在输入错误后创建用户数据。隔离诊断仍在 UI 初始化前处理。既有数据选择算法原样保留。两个实例共用原有 lock，UI 模式不参与资料身份。

## 验证与未来扩展

专项 QtTest 使用临时 SQLite 和合成媒体；测试 NOTIFY 去重、外部更新、桥/服务两种销毁顺序、queued 通知后销毁、QML 绑定、无第二 engine/connection、资源/组件/theme、导航和真实 facade 状态；进程测试验证默认 Widgets、显式 QML、非法输入、关闭、数据内容保存和 bundle 恢复。四服务链接审计增加 Quick 检查。

C2 可增加 QAbstractListModel，roles 必须是 title/artist/album/duration 等稳定类型；id 用受控值，写操作仍通过业务服务；不能把 QVariantMap 作为通用所有业务包。设置桥继续借用 SettingsStore。歌词/AI 迁移前必须完整设计任务 identity、取消、consent、预览保护和文件快照冲突。
