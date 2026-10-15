# Phase A：稳定基线与架构审查

日期：2026-10-15（Asia/Singapore）。本阶段完成后等待审核；未执行 Phase B、C、D 的代码迁移。

| 交付 | 内容 |
| --- | --- |
| [01 工程基线报告](C:/Users/email/Desktop/Relaxer/Relaxer-V0.2.0-dev/docs/phase-a/01-baseline.md) | 来源核对、Git 基线、环境、保护范围、回退方法 |
| [02 当前架构依赖分析](C:/Users/email/Desktop/Relaxer/Relaxer-V0.2.0-dev/docs/phase-a/02-architecture.md) | 实际类依赖、状态归属、后台任务、存储契约及可拆分模块 |
| [03 测试覆盖报告](C:/Users/email/Desktop/Relaxer/Relaxer-V0.2.0-dev/docs/phase-a/03-regression.md) | 原有与新增回归、实跑结果、覆盖矩阵和剩余缺口 |
| [04 Phase B 实施方案](C:/Users/email/Desktop/Relaxer/Relaxer-V0.2.0-dev/docs/phase-a/04-phase-b-plan.md) | 模块边界、接口、目录建议、小步迁移和验收门槛 |
| `evidence/*.json` | 来源核对、环境、验证汇总、直接 include 清单 |

关键结果：原基线与新增测试后的 CTest 均为 21/21；Phase031 保持 33 项通过。新构建在隔离目录通过独立 Runtime 验证。原曲库、runtime、启动器和产品更新日志等 532 个受保护文件逐项未变。

源代码工作区：`C:\Users\email\Desktop\Relaxer\Relaxer-V0.2.0-dev`。Git 元数据：`D:\Git\Relaxer\repository.git`。原受验收基线标签：`baseline/windows-accepted-20261015`。Phase A 完成标签：`phase-a/foundation-20261015`。

最终提交哈希、Git bundle 与源码 ZIP 的 SHA-256 记在仓库外的 `D:\Git\Relaxer\phase-a-receipt.json`，避免把提交或压缩包自己的哈希写入其自身。
