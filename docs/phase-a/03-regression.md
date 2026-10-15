# 已有测试与新增测试覆盖报告

## 1. 执行结果

| 执行阶段 | 结果 | 用时 |
| --- | --- | --- |
| 修改前，最新受验收源码重新配置与完整编译 | 成功 | 见 baseline-build.log |
| 修改前完整 CTest | 21/21 组通过 | 134.70 秒 |
| 三项新增测试后重新配置与完整编译 | 成功 | 见 phase-a-build.log |
| Phase A 最终完整 CTest | 21/21 组通过 | 137.48 秒 |
| Phase031 | 33 passed，0 failed，0 skipped | 最终约 11.25 秒 |
| 隔离 Runtime 验证 | 导入、数据库、GUI、Qt PCM 解码通过 | 见 standalone-runtime.log |
| 原运行环境保护 | 532 个文件及 data/runtime 文件集合未变 | 见验证汇总 |

CTest 总组数保持 21，因为新增断言加入原 `relaxer_core` 对应的 `tests/tests.cpp`，没有新注册测试组或修改 CMake。`src/`、`resources/`、原测试断言和既有测试筛选未修改。

离线环境：`QT_QPA_PLATFORM=offscreen`、`QT_MEDIA_BACKEND=ffmpeg`、`RELAXER_SKIP_AUDIO_DEVICE_TESTS=1`。所有测试输出、临时数据、应用数据和构建目录位于 `Relaxer/test/phase-a-20261015`。

## 2. 最终 QtTest 汇总

QtTest 的 passed 计数包含初始化、清理及数据行，不应直接解释为同等数量的独立用户流程。

| CTest 组 | passed | failed | skipped |
| --- | ---: | ---: | ---: |
| relaxer_core | 21 | 0 | 2 |
| relaxer_gui | 9 | 0 | 1 |
| relaxer_v11 | 18 | 0 | 0 |
| relaxer_v12 | 14 | 0 | 0 |
| relaxer_v13 | 18 | 0 | 0 |
| relaxer_popup | 7 | 0 | 0 |
| relaxer_motion | 4 | 0 | 0 |
| relaxer_v14 | 10 | 0 | 0 |
| relaxer_v15 | 7 | 0 | 0 |
| relaxer_v16 | 33 | 0 | 1 |
| relaxer_v19 | 27 | 0 | 0 |
| relaxer_v110 | 6 | 0 | 0 |
| relaxer_phase03 | 9 | 0 | 0 |
| relaxer_phase031 | 33 | 0 | 0 |
| relaxer_media_smoke | 11 | 0 | 0 |

其余六组原生测试 `relaxer_core_update/readonly_lock/client_frame/launcher_env/prepare/pacman_lock` 均通过。四个 QtTest 跳过项是两项真实播放/恢复测试、一项真实播放器条跳转测试，以及一个显式实网歌词对比。没有把跳过项记作已通过的音频或在线功能。

## 3. 行为覆盖矩阵

| 行为 | 已有实际用例/实现核对 | 覆盖评价与后续缺口 |
| --- | --- | --- |
| 播放、暂停、切歌、状态 | `tests.cpp::queueModes/shuffle/realPlaybackAndSeek/realResumeAndLoop`；`v19::playbackToggleAndContinuousVolumeMotion`；GUI 状态/布局测试 | 队列、状态展示和音量有离线覆盖；两项真实媒体播放和完整恢复在本轮跳过。用户此前实机验收作为单独证据，不替代新增自动化 |
| 曲库读写与重开 | `libraryAndPlaylists/reopenResume/lyricsLookupCacheAndPlaybackStatePersist`、V13 旧数据库迁移 | CRUD、统计、设置、旧库扩列已有；Phase A 增加组合跨重开约束 |
| 歌单保存与恢复 | `reorder`、V13 元数据持久化与播放数、V14 批量操作 | 增加成员去重、顺序、收藏、标签和简介一起重开验证；播放队列与保存歌单仍需分开验收 |
| 歌词搜索与重新匹配 | V15/V16/V19、Phase03/031：精确匹配、放宽查询、来源槽、预算、旧评分丢弃、缓存保留、手动重试 | 本地 HTTP 夹具稳定覆盖多种失败与候选状态；非对真实网站永久可用的证明 |
| 歌词筛选与当前选择 | V16 来源索引映射、切曲不串写；Phase031 缺词快照、显式指定歌曲、已有歌词分组 | 有离线界面及网络夹具覆盖；未来新前端应继续按 ID 验证，不依赖固定行号 |
| 原文与译文保存 | V110 双语附加、Phase031 行数不符逐行重试、失败保留原文、预览双列 | Phase A 增加真实文件保存后重开，以及原译提取一致性 |
| 歌词时间轴与延时 | 核心解析 offset、多时间戳；V16 偏移、同步分组和滚动；V19 5 秒调整 | Phase A 额外冻结文件 offset 与数据库实时延时互不覆盖的契约 |
| 翻译取消与旧回调 | Phase031 关闭确认、失败状态、重试、取消后的异步校验隔离 | 旧网络夹具与浏览器异步错误回调有覆盖；没有完整真实 CDP 断线、导航竞争和验证码恢复的离线夹具 |
| 明暗主题与设置 | V11 新库系统主题默认/保留旧值、V12 深色草稿和系统模式、V13 明暗外观、V19 进度色迁移 | 配置与显式调色板覆盖充分；真实 Windows 系统设置变化和不同 DPI 仍建议人工验收 |
| 设置长期兼容 | V11/V13/V19 历史设置与主题迁移 | 新增未知嵌套 JSON 设置跨重开，避免新接口丢弃未来键 |
| 通知与批处理 | `Library::deferChanges()` 被歌词窗口使用 | Phase A 新增嵌套延迟通知测试，验证数据即时可读且最终只通知一次；延迟通知不是 DB 事务 |
| 首次准备与编译 | 原生 prepare/launcher_env/core_update/pacman_lock/readonly_lock；实际完整 CMake/Ninja 编译 | 测试状态机、主题、环境、锁、进度；没有重新下载/安装一套 MSYS2 或执行系统升级 |
| 独立 Runtime | media_smoke 11 项；本轮独立部署副本运行 verify-runtime.ps1 | 验证了新构建在收集依赖下可导入、写库、截图及解码；没有改动当前产品 runtime |

## 4. Phase A 新增三项测试

位置：[tests/tests.cpp](C:/Users/email/Desktop/Relaxer/Relaxer-V0.2.0-dev/tests/tests.cpp)。全部使用 `QTemporaryDir` 和公开业务 API，不触碰用户库，不增加音乐文件到 Git。

1. `foundationPlaylistAndSettingsSurviveReopen`：插入三首元数据，歌单加入重复 ID 后去重、调整顺序，保存收藏、标签、简介、主题、音量、翻译设置和未知嵌套 JSON；销毁 Library 后重新打开验证全部保留。它覆盖未来 Settings/Playlist 门面拆分的组合兼容契约。
2. `foundationBilingualLyricsAndOffsetsSurviveReopen`：带 `[offset:120]` 的歌词附加译文并保存，重开后验证同时间戳的原译行、原文提取及数据库延时；改变数据库延时后验证原文件字节不变。计时双语不要求普通文本用的 `bilingual` 标记，不改变既有格式。
3. `foundationNestedChangeBatchPreservesImmediateReads`：两层延迟变更通知时写入曲目、歌单和收藏，验证查询立即反映数据，内层结束不通知，外层结束只通知一次；额外释放不会重复通知。

测试开发时曾错误假定计时双语也必须有 `relaxer-bilingual` 标记，首轮新增用例因此失败。核对 `attachTranslations/Lyrics::parse/LyricView::setLyrics` 后修正为现有的同时间戳契约；**没有为使测试通过而改动业务实现**。原有断言未删除或放宽。

## 5. 独立运行验证方式

将原 runtime 的部署依赖复制到本轮隔离目录，只在副本中换入 Phase A 新构建的 `Relaxer.exe`，然后运行现有 `scripts/verify-runtime.ps1`。验证目录为：

`C:\Users\email\Desktop\Relaxer\test\phase-a-20261015\standalone\runtime`

该流程的导入库、截图和 PCM 音频夹具全部位于测试目录。原 `data/runtime` 不参与写入。PCM 证明：至少 500ms 实际解码、正帧数、采样率和声道；它不是物理声卡发声验收。

现有验证脚本的 ready.version 历史硬编码问题已记入架构报告；没有把该字段当作产品版本变更，也没有替换原 ready 标记。

## 6. 在线服务隔离

`relaxer_browser_smoke` 仍为 `EXCLUDE_FROM_ALL`，没有 CTest 注册；V16 实网对比仍由 `RELAXER_LIVE_QA` 显式启用。本轮不重新调用百度/必应，也不把上一轮真实浏览器结果冒充本阶段重新实测。

上一轮交接已有可见 Edge 中两条不同英文取回与重复行复用的记录。本阶段保留该行为；网站改版、限流、验证出现和恢复仍须单独人工验收，不能绑到默认离线 CTest。

## 7. 后续重构前需要补充的测试

- 播放接口抽离前：可控媒体后端或独立硬件作业下的 play/pause/next/previous/seek/save/restore 状态序列，不能仅测按钮符号。
- 搜索服务抽离前：不创建 QDialog 的请求/评分/保存事件轨迹，同时保留现有 Widgets 交互测试。
- 翻译协调器抽离前：可控浏览器传输下的等待验证、恢复、取消、关闭浏览器、陈旧导航结果和整首不完整保存。
- 存储接口扩大前：损坏设置值的兼容策略、文件提交后 SQL 失败的恢复策略、schema 迁移重复打开、线程归属和备份恢复。
- 启动链路修改前：独立干净环境的首次准备和失败恢复；源码更新与已存在 runtime 的选择规则单独设计与验收。

这些缺口已进入 Phase B 各步的进入条件。本阶段没有为了快速得到“全覆盖”而重构已验收业务。

## 8. 重现命令与记录

```powershell
. 'C:\Users\email\Desktop\Relaxer\test\phase-a-20261015\environment.ps1'
& 'C:\Software\MSYS2\ucrt64\bin\cmake.exe' -S $env:RELAXER_SOURCE_ROOT -B $env:RELAXER_BUILD_DIR -G Ninja -DCMAKE_BUILD_TYPE=Release
& 'C:\Software\MSYS2\ucrt64\bin\cmake.exe' --build $env:RELAXER_BUILD_DIR --parallel 6
& 'C:\Software\MSYS2\ucrt64\bin\ctest.exe' --test-dir $env:RELAXER_BUILD_DIR --output-on-failure
```

正式体验播放器应使用普通启动环境，避免沿用 `offscreen` 测试设置。配置、编译、初始与最终 CTest、逐组 QtTest、隔离 runtime 日志均在本轮 `test/phase-a-20261015`；精简结果已纳入 `docs/phase-a/evidence`，原始临时日志未进 Git。
