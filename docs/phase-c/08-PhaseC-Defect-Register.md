# 问题与风险台账

继承正式版 / Harness R10 的 22 项开放 P3，未删除、合并或冒称本轮重新验收。原始逐项台账仍在源码根 `V0.2.0-Known-Issues.md`；D-R9-02、D-R6-01 的历史关闭状态保持。以下是 C1 新发现项：

| ID | 等级 / 状态 | 触发、原因与处理 | 验证 |
|---|---|---|---|
| C1-D01 | 交付门禁 P1 / 已修复，最终 ZIP 另行复验 | 早期直接运行 build 目录时 Windows PATH 重复项使隔离 Qt DLL 路径丢失，出现 Qt6Qml.dll、Qt6Quick.dll、Qt6QuickControls2.dll 三个系统错误框。用户明确报告全部三项，截图保留；初始测试未抑制系统弹窗是执行问题。已清理 PATH、用匹配官方包收集整个 PE/QML 依赖闭包；外部驱动设置 SetErrorMode，记录非零码。不能只补一个 DLL。 | 三 DLL 与官方 SHA 一致；system-only PATH 原生 QML/Widgets/20 项专项测试通过；最终解压包验证另见 delivery verification |
| C1-D02 | P2 / 已修复 | 旧 facade 析构先 wildcard disconnect 自身，单靠 facade.destroyed 无法通知桥清空状态。监听唯一 backend.destroyed 并先失效借用 facade，保持核心不变。 | borrowedObjectsMayDisappear、桥先销毁、queued callback、engine 优先销毁全部通过 |
| C1-D03 | P2 / 已修复 | 初始 NavigationButton 同时赋 font 和 font.weight，QML 创建失败。改为各字体属性绑定。 | 资源/窗口/导航/无 QML warnings 自动断言通过 |
| C1-T01 | 测量工具 / 已修复 | SW_HIDE 的硬件 Quick 窗口不会产生预期的曝光帧，导航测量超时。 | 实际曝光窗口后，两种模式各 20 个原生样本通过 |
| C1-P301 | P3 / 开放，C2 处理 | AppBridge 只需数量却通过 Library.tracks().size() 读取全曲库，Library.changed 中设置保存也可能触发 O(N) 查询。0ms timer 合并重复刷新，但尚未消除周期性全表读取。 | 空库和小 fixture 通过；未做大库性能承诺 |

C1-P301 与历史 D-R8-03（周期保存无 dirty 判断）存在性能触发关系，但不是同一缺陷，不吞并原项。按本轮开发方登记口径：历史开放 P3=22，新增开放 P3=1，合计23；没有新增未解决 P0/P1/P2。此结论以最终包复验通过为交付门禁，不代表独立 Harness 的新验收。

Qt 历史 misleading-indentation / QDateTime deprecated 与 CMake SQLite target alias 警告未在 C1 顺手修改。列表/歌单/歌词/完整设置/AI 尚未迁移属于已明确的范围，不计为“功能回归”或伪装完成。真实硬件听音、付费 API、历史用户生产库、长稳/实网没有由本轮再次执行。
