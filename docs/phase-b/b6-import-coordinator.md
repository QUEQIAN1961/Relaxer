# Phase B6：ImportCoordinator 使用与维护

本轮基准为 b57420db1a585fb1236fb1cfd9af4f6c90ce029c，保留产品版本 1.10.0。只拆分导入编排及修复 Harness R4 追加范围，不开展 B7/QML，不迁移 schema。

## 生产调用链

Window 收集 paths 和开始时的 targetPlaylist → ImportCoordinator::start(ImportRequest) → ImportWorkerSession/QThread → Importer::run → 最多 8 个池 worker 的 importOne → queued ImportResult → Library::insertImportedTrack / addToPlaylist（Library 归属线程）→ 结果屏障及线程退出 → 唯一 ImportSummary → Window 刷新并显示结果。

Window 不再持有导入或封面修复 QThread、worker、导入计数；也没有 wait()。封面旧记录筛选和 applyArtworkRepair 由同一协调器安排，导入和修复仍可并行。

## 接口与计数

start 返回单调 taskId；0 表示请求被拒绝。请求包括 sources、固定歌单和最大并发（0 保持原 CPU/8 上限）。忙碌、关闭中、空来源、错误线程、无 Library 等拒绝新任务。

sourceCount 是原始来源个数，目录可展开。totalFiles 是扫描选中的 canonical 唯一支持文件数；ignoredPaths 是已扫描的无效、不支持或重复路径数。scanComplete=false 表示取消导致扫描范围未知，不凭空估计未扫描文件。processed 是收到并接受的文件结果，added/duplicates/failed 延续原版定义，unstarted=max(0,totalFiles-processed)，skipped=ignoredPaths+unstarted。目标歌单删除造成 playlistFailures，歌曲仍保留，原新增/重复计数不改为失败。fatalError 与单文件失败分开。

## 所有权与线程

Coordinator 属于调用方/Window，Library 通过 QPointer 引用，必须与服务同线程；不会移动或复制数据库连接。worker 只有 root、paths、Track 值和共享原子取消 token。Library 是唯一数据库入口，原 insert 签名转入同一插入实现，没有第二套去重规则。

Session 无业务状态权威，只负责线程和消息传输。线程完成时安排 worker 的 deferred deletion，随后 session 删除自己的 QThread。Session 持有 QEventLoopLocker，使异步清理可继续；Coordinator 析构只请求取消和断开业务回调。使用服务的宿主必须继续运行事件循环直至资源清理完成；正常关闭使用 shutdown/shutdownReady。

worker finished 的直接连接在 mutex 下裁决取消/完成先后，并只发布一次结果屏障；queued 业务结果在屏障前接受，屏障后及旧 taskId 回调忽略。run 返回或异常均安排线程退出，不能靠 worker 一定发 finished 才退出。最终信号同时要求屏障与 thread finished。公开信号中的调用方可以销毁 Coordinator，完成路径通过 QPointer 防止继续访问失效对象。

进度保存在常量大小的 mutex 邮箱，每 40ms 合并通知，终态前刷新一次，避免按高频 worker 事件排队。ffprobe/封面进程的等待加入取消检查，不强制终止 QThread。OS 文件 I/O 仍非硬实时可中断。

## R4 相关保护

歌词预览和应用候选共用 ReplacementContent；仅在原文、时间轴及元信息结构一致时保留本地双语。差异候选显示将写入的内容并明确提示移除旧译文。保存成功后才作废未保存 AI 结果。

Library 的普通/保留原文字节两条保存接口共用事务与文件备份实现。SQL UPDATE、纯音乐 UPSERT 或 COMMIT 失败均回滚数据库并恢复旧文件/移除新文件；恢复失败明确报错。文件与 SQLite 跨资源操作仍有进程崩溃窗口，并不宣称绝对原子。

当前歌词及实际目标文件按需记录原始字节 SHA-256 快照，确认前、确认后及临近 QSaveFile commit 再次检查。等长且恢复 mtime 的外部编辑、删除或不可读均阻止旧候选覆盖；显式刷新后才接受新快照。最后一次检查到 rename 仍存在很窄的外部竞争窗口。

筛选普通行状态变化仅更新单行，维护可见计数；明确切换或刷新仍为一次 O(N) 同步遍历。缓存用字段结构比较代替 JSON 序列化；应用保存使对应项失效，刷新强制重读全部，活动文件的保存依靠内容快照，而非 mtime 缓存做决定。其他未选文件的同大小/mtime 改动仍需显式刷新。

## 构建与测试入口

完整：cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release；cmake --build build --parallel 8；ctest --test-dir build --output-on-failure。

独立服务：增加 -DRELAXER_IMPORT_ONLY=ON，构建 relaxer_import_coordinator_tests、relaxer_importer_contract_tests、relaxer_lyrics_persistence_tests，再运行对应 CTest。链接审计实际检查 PE import table，拒绝 Widgets/Svg/Quick/Qml。独立配置与其他 ONLY 选项互斥。

测试用真实临时文件和 SQLite；故障注入、受控信号量、旧回调、双向取消竞态、对象销毁、异常、进度洪泛、窗口导入和异步关闭均有断言。AI 仅使用 loopback Mock 与假凭据。完整验收数量及交付身份以本轮外部最终报告和原始日志为准。
