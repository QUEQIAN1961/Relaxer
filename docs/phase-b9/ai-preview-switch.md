# B9 — 未保存 AI 译文切歌保护

R7 OBS-1 — 用户授权改进：主动切歌前确认未保存的完整 AI 译文。

原行为：itemSelectionChanged 与 selectTrack 刷新均可直接丢弃 CompletedPendingSave。
新行为：主动切换默认取消；明确丢弃才调用现有 discard/forget。筛选和被动刷新保留所属歌曲，按 item 的 trackId 恢复选择，排序后仍保持保存目标。确认期间重复选择与关闭均不重入。

生产 LyricsLookup + TranslationCoordinator + 临时 SQLite + 本地 Mock：新增 12 个数据行，覆盖鼠标、键盘、排序、重入、取消后保存/关闭、明确丢弃、筛选、空选择、被动刷新和删除所属曲目。原始歌词逐字节核验，Mock 请求各为一次，真实付费请求为零。既有搜索回调与运行中取消测试保留。

验证：B8 基线 Release 234/234、CTest 49/49；AI 歌词翻译专项 112 passed / 0 failed / 0 skipped（含 init/cleanup），CTest 1/1。

R6 L-1 — 用户授权改进：未保存 AI 译文关闭确认，继承 B8 已验收行为。
