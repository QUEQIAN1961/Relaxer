# Relaxer V0.2.2-GA-01 发行工程交付

**V0.2.2-GA-01 RELEASE ENGINEERING BLOCKED — ISSUES DOCUMENTED**

已完成可验证的便携结构和工程交付；许可来源/通知范围及用户验收门仍开放。本包不是 GA，也没有直接发布。

1. [工程裁决与七项回答](00-GA01-Engineering-Verdict.md)
2. [第三方许可矩阵](01-Third-Party-License-Matrix.md)
3. [对应源码与配方](02-Corresponding-Source-And-Build-Recipes.md)
4. [Windows 用户包设计](03-Final-Windows-Package-Design.md)
5. [首次启动与完整性](04-First-Launch-And-Integrity-Strategy.md)
6. [元数据与最终版本方案](05-Public-Metadata-And-Version-Audit.md)
7. [GA 门与责任](06-GA-Release-Gates.md)
8. [最终 Harness 计划](07-Final-GA-Harness-Plan.md)

实际最终工程包：[Relaxer-V0.2.2-GA01-ENGINEERING-TEST-NOT-PUBLIC-Win64.zip](Relaxer-V0.2.2-GA01-ENGINEERING-TEST-NOT-PUBLIC-Win64.zip)，桌面有同名哈希一致副本。此前 Win64 在名称中间的 ZIP 为未补充许可证材料的过程样本，不能混作本次最终交付。

其余固定文件：`ENGINEERING-IDENTITY.json`、`THIRD-PARTY-OBLIGATIONS.json`、`INTERNAL-KNOWN-RISKS.json`、`Relaxer-GA01-License-Supplement.zip`、`Relaxer-GA01-Third-Party-Source-Materials.zip`、`Relaxer-GA01-Engineering-Source.zip`、`Relaxer-GA01-History.bundle`、`Relaxer-TextFix-to-GA01.patch`、`TextFix-to-GA01-Full.diff`、`Relaxer-GA01-Developer-Tools-NOT-USER-PACKAGE.zip`、`Relaxer-GA01-Audit.zip`、`SHA256SUMS.txt`。

先验证外层 SHA256SUMS，再按报告 02 独立恢复来源、签名、配方和源码身份。产品二进制身份仍为冻结 TextFix；新工程提交只包含发行脚本/测试和报告。交付根报告为权威阅读入口，source/docs 中为相同内容源控快照。
