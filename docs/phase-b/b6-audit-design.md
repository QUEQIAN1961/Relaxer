# B6-A：真实导入审计与实施契约

基准：b57420db1a585fb1236fb1cfd9af4f6c90ce029c，Tree 4bc87c9fa2e9716a7b13db1645d6a88be1393ee0。完整交付包、源码ZIP、SHA清单和Git的215个文件已逐项一致验证。B6工作树独立，旧分支与归档不变。

## 实际调用链与12项审计

| 问题 | 现状证据与结论 |
|---|---|
| 谁创建线程 | Window::importSources创建QThread(this)及无parent的Importer，moveToThread后started→run |
| 谁拥有对象 | Window持有QPointer线程/worker，QThread为Window子对象，finished→worker.deleteLater；Window析构cancel/quit/wait无期限 |
| 目标歌单 | importSources开始时复制m_scope>0?m_scope:0，结果lambda捕获target；不会读结束时UI选中歌单 |
| 重复判断 | Importer扫描用canonicalFilePath去重；importOne计算原始字节SHA-256并以hash命名文件；Library::insert按tracks.hash UNIQUE查找并返回已有id |
| 计数 | Window每条结果前后各读取完整tracks列表比较size，分别记新增/重复；媒体错误/insert返回0记失败；没有跳过统计 |
| 数据库提交 | Window所属线程中的queued imported lambda调用Library::insert和addToPlaylist。worker只持root/paths，不持Library |
| 迟到信号 | imported/progress由run内部最多8个QThreadPool线程发出，queued到Window；QObject context销毁会断开，但Window持线程并无界wait |
| 取消后写入 | 允许已处理/在途成功结果继续提交，保留部分成功；已启动文件返回“已取消”仍计失败；扫描和未启动文件静默跳过。不能改成取消即丢弃所有queued结果 |
| 重复完成 | 实际Importer::run仅一次finished(bool)，Window没有任务id/终态屏障；重构需加身份、工作完成与线程退出双重保护 |
| 封面依赖 | importOne可选readArtwork失败不使歌曲失败，*-v2.png路径不变；Library::insert保留manual封面及统计，重复导入可修正自动封面。独立ArtworkRepair经Window排队回主线程applyArtworkRepair |
| 可复用服务 | 有Importer和ArtworkRepair QObject、Library和现有Track/ImportResult/ArtworkResult，没有导入协调器 |
| UI/业务边界 | 文件选择、对话框、进度控件、toast、刷新属于Window；任务身份、线程、计数、提交与终态属于协调器 |

补充：Library::addToPlaylist在本线程使用既有事务；目标歌单删除后成员写入失败，但歌曲插入仍然有效。旧Window此时warning，新增/重复数不改成失败。B6保留计数，另报playlistFailures并在最终提示中展示错误。文件处理先落盘后SQL，SQL失败可能保留托管文件；不擅自删除共享hash文件或回滚已成功歌曲。

## 决定

1. 在relaxer_core新增ImportCoordinator : QObject，复用Importer/ArtworkRepair和同一Library。服务测试使用QCoreApplication，不创建Widgets；新增独立构建开关及二进制链接审计。
2. 请求包含sources、固定targetPlaylist和可选最大并发（默认沿用最多8个）。start返回单调taskId，拒绝忙碌/错误线程/无Library；没有第二套Track/Playlist。
3. 报告分别记录原始sourceCount、扫描得到的唯一可导入totalFiles、扫描忽略数、processed、added/duplicates/failed、未启动数和cancelled。旧三项计数定义不变；目录展开/未扫描完不能假装知道总文件数。ignored+unstarted为新增skipped观察值，不混入失败数。
4. 每个worker通过独立生命周期Session拥有QThread；线程结束才删除worker/thread/session。QEventLoopLocker维持异步清理所需事件循环，Coordinator析构仅取消并断开业务回调，不在UI线程wait。Window正常关闭先异步shutdown，全部导入/修复线程退出后再完成close。
5. 单任务权威只在Coordinator；Session仅负责底层线程与安全传输。结果使用任务id/job身份，工作结束后迟到结果忽略；只有worker完成且线程退出才发一次final。取消截止以worker终态发布的原子标记裁决，取消不能重标已完成任务。
6. SQL只在Library归属线程执行。给Library增加插入结果是否新增的薄接口，原insert签名及实现语义保留，避免每首歌曲两次全库加载；不改schema、id、目录或统计。
7. 媒体解析、命名、封面选择不重写。现有FFmpeg/ffprobe等待增加共享取消检查，使关闭可及时结束；保持原超时与错误语义。不强制terminate工作线程。
8. 进度通过共享邮箱和定时取最新值合并，常量级进度存储，单调显示，终态前刷新最后进度；文件结果仍逐个回归属线程提交。
9. 测试使用真实临时文件/SQLite，控制worker工厂和信号/信号量同步点验证竞态；不使用随机sleep作为唯一依据。保留全部692项有效基线及4历史skip，禁止改B5.1业务。

## 分阶段提交

- B6-A：本文、Importer/Library行为冻结测试，先在未改生产实现上执行。
- B6-B：独立协调器、生命周期Session、最小Importer/Library适配、无Widgets服务测试。
- B6-C：Window薄接入，移除旧线程/worker/计数权威；封面修复一起接入服务，保持交互。
- B6-D：有缺陷时最小补强；测试覆盖取消/完成顺序、跨任务、销毁、文件锁、数据库及目标歌单失败。
- B6-E：新Release配置与全量回归、独立Runtime、便携解压启动退出、独立Git恢复、逐文件ZIP与五份报告。

限制：底层操作系统文件I/O无法保证实时中断，不使用不安全线程terminate；正常关闭依靠异步会合而非UI阻塞。测试应验证迟到完成受控且资源最终回收。退出整个进程前应完成shutdown，不能把强制杀进程等同于安全关闭。
