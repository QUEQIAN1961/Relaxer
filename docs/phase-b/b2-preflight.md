# Phase B2 前置检查与文件级方案

起点：`edba4f34c51dd42ab415613699239d5a30821789`，标签 `phase-b/b0-b1-20261015`。
已完整读取用户的 780 行任务说明及七份附件。执行范围只限 Queue/Mode 独立化。

- 输入 ZIP SHA256：`2f8fc6924ef5d93bf37ee89ccfa0149b35914550856bbc44eee7bd84ec7af442`。
- 输入 bundle SHA256：`311340c11bbcd97b07a9afc1ec4e46e740054b97188354b98736279fed840b5e`。
- ZIP 内 153 个文件逐字节匹配 B1 提交树；原 SHA256SUMS 的 152 个条目全部通过。
- bundle verify 确认完整历史；main、Phase A、B0、B1 提交及原标签保留。
- 新工作区 `work/Relaxer`，分支 `phase-b/b2-queue`；输入 `sources/` 设为只读，初始工作树干净。
- 环境 Ubuntu 24.04 x86_64 / GCC 13.3；初始缺少 CMake、Ninja、Qt，准备依赖后运行适用验证。没有 Windows 执行环境。

## 审查结果

Queue 在 playback.h 中声明、playback.cpp 中实现；只使用 QVector、QSet、QRandomGenerator 和 Qt 整数/全局函数，不需要 Library、QObject、Widgets 或 Multimedia。
Mode 的顺序是 Sequential=0、ListLoop=1、SingleLoop=2、Shuffle=3，被持久化设置依赖，必须原样保留。
Playback 只持有一份 `m_queue`，媒体结束、手动切歌、错误跳过、playUpcoming、forget、restore 均操作它。
Window 通过 Playback 的 upcoming/playUpcoming、mode 和只读 queue 访问队列，不应改动调用方。

upcoming 在副本上调用 next(true)，将新生成的随机袋写回原对象 m_plannedBags；它是有状态的预测。
refill(true) 优先消耗缓存的袋；随机历史回退后 next 优先前进到历史记录。set 与 setMode 清除计划袋及历史。
持久化仍由 Playback 使用原 queue/current/position/activeSession 键完成；不持久化袋或随机历史。恢复时 set 重建袋，这是原有行为。

## 实施顺序

1. 在原实现上增加 Queue 行为保护测试并运行原 queueModes、shuffle、V17 多轮预测用例。
2. 原样移动 Queue/Mode 到 src/domain/queue.h/.cpp；playback.h 保留兼容 include，Playback 实现逐字节保留。
3. 增加仅依赖 Qt Core 的静态 relaxer_domain；core 公开链接它，独立 Queue 测试只链接 domain/Qt Test。
4. 审计独立程序实际链接与动态依赖，保留 B1 services 边界；执行平台适用的完整回归。
5. 更新源码校验清单，提交并记录平台限制，生成完整 ZIP、历史 bundle、收据和三份报告，停在 B2。

不调整随机算法，不添加测试用生产接口，不移动 Track/Playlist/Session、歌词或其他模块，不修改产品版本、GUI、schema、播放后端。
