# Phase B3：抽离前歌词匹配行为契约

唯一基线：3a0dbd9cc67f6524c93ddf68b55ac2b3b403b4ec，Windows B2 验收标签 phase-b/b2-queue-windows-accepted-20261016。用户已确认 B2 人工验收通过。正式版本 1.10.0 不变。

## 实际职责、调用及线程

| 原函数（B2 lyricslookup.cpp） | 输入 → 输出 | 副作用 | 实际调用与线程 |
| --- | --- | --- | --- |
| normalized，134 行 | QString → NFKC/小写/仅字母数字 | 无 | 评分、候选去重、复用键、匹配理由；GUI 与评分 worker |
| cleanTitle，140 行 | QString → 去扩展名/序号/音质标签后的标题 | 无；原静态 const 正则 | searchTrack、候选标题、手动搜索框；GUI 与 worker |
| knownArtist/artistForms/sameArtist，156–176 行 | 歌手字符串 → 已知标记/规范形式/匹配布尔 | 无；原静态 const 正则 | confident、matchScore、matchReason，以及查询构造；GUI 与 worker |
| versionMark/titleWithoutFeaturing/baseTitle，177–196 行 | 标题 → 版本标记/基础标题 | 无；原静态 const 正则 | 评分、可信度、原查询构造；GUI 与 worker |
| similarity，197–205 行 | 两字符串 → 编辑距离相似度 | 只使用局部缓冲 | matchScore；主要 worker，亦供静态入口测试 |
| searchTrack，206 行 | Track 值 → 清理标题/歌手/专辑后的完整 Track 副本 | QFileInfo 仅处理路径字符串，不查询文件内容 | 建立 Job、手动搜索、confident、matchScore；GUI 与 worker |
| confident，221 行 | Track + 候选 JSON → 严格匹配布尔 | 无 | computeRank 对 original/cover alias 分别判断；worker；静态测试 |
| matchScore，232 行 | Track + 候选 JSON → double | 兼容入口递增原有原子诊断计数 | computeRank 对 match 评分一次；rankCandidates 预计算一次；worker/测试 |
| credibleLyrics，293 行 | 原始字节 + Track → 布尔及可选拒绝理由 | 可写调用方 reason；调用现有 Lyrics::parse | computeRank；worker/静态测试；不直接保存文件 |
| rankCandidates，251 行 | query + 候选值列表 → 稳定排序后的副本 | 每个候选调用一次原评分计数入口 | 离线回归入口；保留一次评分/稳定排序结构 |

原唱/翻唱来源识别 originalCredit 已在 src/lyric-text.cpp 作为 Qt Core 纯函数存在；不搬移该文件或其中翻译工具。beginOnline 的原唱查询构造次序及 computeRank 的 original/match/cover 三种上下文保持原代码。

## 最小抽离边界

新建 domain/lyric-match.h/.cpp，纯计算只链接 Qt Core；复用当前 Core-only model.h 的 Track 与 lyrics.h 的只读解析结果值，不移动这些类，也不引入 Library、SQLite 或 QWidget。

Lyrics::parse 的实现及 CMake 归属维持 relaxer_core。原 credibleLyrics 适配入口先执行原始 HTML 拒绝判断，再调用原解析器，最后委托 domain 对解析结果作判断。这保留原始拒绝顺序、编码回退、时间轴、行数和理由文字，不复制或重写解析器。独立策略测试仅为生成真实解析值而单独编译既有 lyrics.cpp；domain 库本身不调用解析器，不依赖 core，避免循环链接。

原静态 normalized/cleanTitle/searchTrack/confident/matchScore/credibleLyrics 入口保留。matchScore 原子计数留在适配入口，domain 无共享计数/任务状态。原局部纯 helper 名称用薄委托保留，调用点和查询构造不改。

正则模式及选项保持原值，新模块使用每线程 const 正则，避免 worker 共享同一个可懒编译对象；不得借机更改模式、大小写、标点、版本或歌手规则。

## 冻结的算法与性能语义

- 分数仍为标题相似度×300、已知歌手最高180、专辑相似度×80、时长最高240减每秒15、同步歌词加60、版本不一致减450、严格可信加500、manualOnly 减180。
- confident 的原始 artist/duration 前置检查、清理标题、版本/专辑版本、多歌手匹配与 min(4,max(2,duration/100000)) 秒容差保持不变。
- credible 原始/解析后 HTML 拒绝、至少两条有效行、最后时间不超过歌曲时长+30000ms及原拒绝文字保持不变；不把它扩充成完整自动保存规则。
- 自动保存还受 applyAccept 中原唱可信、cover 可信、纯音乐、歧义、已有文件及任务状态约束；整段保持原实现，不下沉到纯策略。
- computeRank、scheduleRanking、applyRanking、applyAccept、rankingCurrent、cacheRankingCurrent、appendCollected、trackContext、rebuildCandidates 不重写。
- collectionEpoch/cacheEpoch、original 与 match 指纹、单歌曲 scoreInFlight、generation/hash/QPointer、取消和过期结果丢弃保留。
- 候选只评分一次再 stable_sort；比较器不调用评分。原匹配目标 score 与原唱 original/cover confidence 不混用。GUI 不执行批量重复评分。

## 抽离前真实行为样本

tests/fixtures/lyric-match-baseline.json 的输出由 tests/support/lyric-match-export.cpp 链接本机已经验证的 B2 Windows Release 静态库生成；不是重写一套公式来伪造预期。输入包含标准/多语言/括号/多歌手/大小写标点/Remix/Live/Cover、原唱标记、缺失异常元数据、时长边界、编码与无效歌词，以及等分排序和原唱/匹配目标不同的列表。

每行记录 normalized、cleanTitle、searchTrack、精确分数、confident、原 UI 理由、credible 与拒绝理由、originalCredit；排序列表记录完整候选顺序和评分调用数。2000 候选记录真实 B2 时间和一次评分的调用数。默认测试只读样本，无更新 golden 的入口。

该冻结提交先于产品模块抽离。后续必须逐项比较，而非以更高搜索成功率为理由改原评分或门槛。保持默认测试离线；网站/浏览器验证不纳入本轮。
