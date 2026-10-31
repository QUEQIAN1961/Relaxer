# GA-01 公开元数据与版本审计

本轮工程包标记 **GA RELEASE-ENGINEERING TEST — NOT FOR PUBLIC DISTRIBUTION**。实际产品仍为 V0.2.2-RC7，目标版本 V0.2.2-GA 仅为设计字段。没有改变 Windows 资源为已发行 GA。

| 项目 | 本轮处理 | 状态 |
|---|---|---|
| deployment.json | 以冻结输入来源重建投影；删除 QML 扫描绝对路径、依赖绝对路径、日志目录；保留真实 Commit/Tree/Fingerprint、哈希、校验方式和相对目录 | PASS |
| COMPONENTS.json | 移除 origin/metadata 绝对路径；保留每个实际 PE 的包名、精确版本和摘要；纠正旧清单把 Relaxer 误写 RC6 的元数据 | PASS |
| PRIVATE-TEST-VALIDATION.json | 普通用户包不携带；原证明留在冻结归档和工程审计中 | PASS |
| BUILD-IDENTITY.json | 产品身份固定 TextFix；明确 GA=false、publicDistributionApproved=false，标明工程目的及实际入口 | PASS |
| README / Notices | 新建正常部署、恢复和许可说明，不要求用户理解源码准备与 ready 标记 | PASS |
| 风险资料 | 从用户包分离；`INTERNAL-KNOWN-RISKS.json` 及原始报告保留 ART-01 与 Escape 未解事实 | PASS |
| 更新日志 | 原 resources/changelog.json 与 changelog-summary.h 完全不动；RC 两组仍合并，GA 历史、日期、折叠行为保留 | PASS |
| Windows 版本资源 | 保留 RC7 / prerelease；未伪称 GA | PASS（工程包） / OPEN（最终 GA） |

最终 ZIP 全文件按 UTF-8 与 UTF-16 扫描本机 `C:\Users\email`、`D:\Git`、Harness 目录标识，发现 0；数据库、媒体、日志、测试 EXE、Qt Test、缓存和凭据文件残留 0。`COMPONENTS.json` 仅记录最终实际二进制。

第三方 Rust DLL 仍含其上游 CI 的 `C:\Users\runneradmin` 源码诊断字符串，原二进制未改动；这些不是当前机器的个人开发路径。不把这一点隐瞒为“所有绝对路径字符串为零”。内部审计与对应源码的 BUILDINFO 也保留真实上游构建路径，属于开发者资料。

历史 V0.2.0-GA 条目仍有先前任务明确要求逐字保留的 Harness 验证句；本轮没有新增内部词，也没有把已经验收的历史文案擅自改掉。因此不宣称整个 UI 的 Harness 词数为零。若最终对外文案要求全历史零内部词，应列入受控发行文案变更并重做历史真实性与显示复验，而不能静默改包或篡改旧 Git 历史。

## 最终 GA 的版本管理方案（尚未执行）

从本工程提交继续建立只含发行身份/部署说明的独立提交。统一修改 `src/release-version.h`、`src/phase-c-version.h` 与 CMake 实际使用的 `resources/phase-c-relaxer.rc`、`resources/portable.rc`；开发者准备工具如同步发行，其 `phase-c-start.rc` 另核对。显示值为 V0.2.2-GA，数值资源建议 0,2,2,0；GA 移除 prerelease 标记，仍不能在验收前宣称公开发行。是否新增 GA 日志条目须只描述已完成变化，并保留全部 RC/GA 真实历史。

任何这些编译输入变化都会产生新的 Fingerprint，必须重新构建全部同源核心、重生成清单并新做身份/回归/入口/版本资源验收。最终包名拟为 Relaxer-V0.2.2-GA-Win64.zip；不得简单将 RC7 ZIP 改名、手改身份 JSON 或制作 ready 文件。新 Commit、Tree、Fingerprint、软件包摘要、独立验收与公开发行批准一一绑定后才进入发行决策。没有创建 Tag 或 Release。
