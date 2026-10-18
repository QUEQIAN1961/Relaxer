# 构建与测试

环境：Windows 10 10.0.19045，MSYS2 UCRT64，GCC 16.2.0，Qt 6.11.2，CMake 4.4.4，Ninja 1.13.2，C++17 / Release / FFmpeg。源码指纹：`ca736b647b44f890e6d0048bb93de199769a3a22519ace32d347041b8e826725`。

已安装 Base/Multimedia/Svg，没有完整 Declarative。官方包 mingw-w64-ucrt-x86_64-qt6-declarative-6.11.2-1，SHA256 `e6f303088cf9c33c45404c3c7331fd40c785248ae5eba1fbd97d4b6daf506335`；与仓库元数据一致，签名为 MSYS2 Christoph Reiter 开发 key，gpg Good signature。只解压到工作目录，不修改已验收 Qt。overlay 必须通过 QT_ADDITIONAL_PACKAGES_PREFIX_PATH 配置，运行设置 PATH/QML_IMPORT_PATH。

| 项目 | 结果 |
|---|---|
| 最终 Release 全目标重建 | 282/282；另导航观测目标 4/4 |
| CTest | 57/57，0 失败，271.53 秒 |
| 原有 QtTest 42 套件 | 1002 passed / 0 failed / 4 historical skipped |
| 新 C1 QtTest | 20 passed / 0 failed / 0 skipped |
| 合计 QtTest 43 套件 | 1022 / 0 / 4 |
| 部署原生旧 14 套件 | 404 / 0 / 1 |
| 部署原生 C1 | 20 / 0 / 0 |
| 部署原生合计 | 424 / 0 / 1（是全量测试子集，不累加） |
| 四服务独立配置 | Playback 9/9；Search 19/19；Translation 17/17；Import 11/11 |
| Widgets-only | 播放器/启动器 71/71；无 Quick/Qml PE 导入；默认启动/退出通过 |

最终构建目录在开发过程中曾用于迭代；源码指纹变化后所有 282 步均重新执行，不冒称完全未使用的配置缓存。最终编译输入与冻结源码指纹一致。最终 bundle 恢复与 Source ZIP 的逐文件验证另在交付校验中记录。

历史四项 skip：core.realPlaybackAndSeek、core.realResumeAndLoop、gui.playerBarStableAndRealSeek（自动会话未测试声卡），v16.liveSongComparison（实网按需）。没有新增 skip、删除或放松历史断言。部署仅继承 GUI 一项 skip。四服务原严格链接审计已经禁止 Widgets/Quick/Qml，本轮继续全部通过；它们的 CMake 图不查找 Quick。

## C1 覆盖对应关系

1. qrc Main/theme 存在、窗口可创建且可重复 load，不重复实例；标题 dev-c1 和最小尺寸。
2. 四页真实按钮导航、无效页拒绝、重复导航不重复通知、所有主题 tokens、主题切换和基本组件。
3. Library 曲目数量真实通知且合并更新；Playback 的标题/歌手/音量/状态/进度绑定；真实 facade 播放暂停。
4. NOTIFY 去重、queued 旧快照不覆盖最新权威状态；服务先销毁/桥先销毁、queued callback 后销毁；engine 优先销毁与连接释放。
5. 默认 Widgets、显式 Widgets/QML 的独立进程、一条连接/一个播放器、保存成功、数据哨兵及分类/歌单保留、锁释放。
6. 未知 UI/未知选项/不支持页面/非法退出时间/诊断组合拒绝，错误前不建资料目录；同资料锁拒绝第二模式。
7. 外部验证：system-only PATH 原生平台、Widgets/QML 两种 scale factor 截图、PCM、FFmpeg/ffprobe、启动器及 WM_CLOSE 无残留；最终 ZIP 再验证三项 DLL、QML/Widgets 启动、退出和资源。

`scripts/validate-phase-c-native.py`、`measure-phase-c.py` 和导航探针可复现上述外部验证。QtTest/CTest 原始日志、initial failure 与最终修复结果均保留。早期问题包括隔离 Quick 的 DLL PATH 丢失（用户看到了三个系统弹窗）、NavigationButton 重复 font 赋值、facade 析构信号特殊语义和隐藏窗口的导航测量超时；都没有删除对应检查以获得通过。

## QtTest 明细

| 套件 | 通过 | 失败 | 跳过 |
|---|---:|---:|---:|
| relaxer_ai_infrastructure | 30 | 0 | 0 |
| relaxer_ai_lyrics_translation | 113 | 0 | 0 |
| relaxer_ai_widgets | 12 | 0 | 0 |
| relaxer_core | 21 | 0 | 2 |
| relaxer_gui | 10 | 0 | 1 |
| relaxer_import_coordinator | 22 | 0 | 0 |
| relaxer_import_widgets | 5 | 0 | 0 |
| relaxer_importer_contract | 8 | 0 | 0 |
| relaxer_lyric_match | 116 | 0 | 0 |
| relaxer_lyric_match_compat | 117 | 0 | 0 |
| relaxer_lyrics_document | 7 | 0 | 0 |
| relaxer_lyrics_document_baseline | 10 | 0 | 0 |
| relaxer_lyrics_document_compat | 4 | 0 | 0 |
| relaxer_lyrics_persistence | 9 | 0 | 0 |
| relaxer_lyrics_search_coordinator | 69 | 0 | 0 |
| relaxer_media_smoke | 11 | 0 | 0 |
| relaxer_midi | 17 | 0 | 0 |
| relaxer_motion | 4 | 0 | 0 |
| relaxer_phase03 | 10 | 0 | 0 |
| relaxer_phase031 | 33 | 0 | 0 |
| relaxer_phase_b_integration | 10 | 0 | 0 |
| relaxer_phase_b_storage | 12 | 0 | 0 |
| relaxer_playback_facade | 31 | 0 | 0 |
| relaxer_popup | 7 | 0 | 0 |
| relaxer_qml_foundation | 20 | 0 | 0 |
| relaxer_queue | 22 | 0 | 0 |
| relaxer_queue_playback | 8 | 0 | 0 |
| relaxer_services | 4 | 0 | 0 |
| relaxer_settings_appearance | 26 | 0 | 0 |
| relaxer_settings_appearance_compat | 26 | 0 | 0 |
| relaxer_settings_appearance_widgets | 8 | 0 | 0 |
| relaxer_translation_classification_storage | 7 | 0 | 0 |
| relaxer_translation_coordinator | 46 | 0 | 0 |
| relaxer_translation_filter_performance | 5 | 0 | 0 |
| relaxer_translation_workflow | 29 | 0 | 0 |
| relaxer_v11 | 18 | 0 | 0 |
| relaxer_v110 | 6 | 0 | 0 |
| relaxer_v12 | 14 | 0 | 0 |
| relaxer_v13 | 18 | 0 | 0 |
| relaxer_v14 | 10 | 0 | 0 |
| relaxer_v15 | 7 | 0 | 0 |
| relaxer_v16 | 33 | 0 | 1 |
| relaxer_v19 | 27 | 0 | 0 |
