# V0.2.0-rc1 人工与最终验收清单

以下项目没有自动获得 PASS。正式发布前由用户和独立 Harness 确认适用范围。

- **MANUAL-VERIFICATION-REQUIRED**：受控扬声器/耳机播放有声、暂停静音、恢复继续、Seek 后继续、切歌听感和音量变化。Playing 状态不证明声波输出。
- **MANUAL-VERIFICATION-REQUIRED**：真实 MIDI 硬件；分别记录 MIDI 解析、后端调用、软件合成器和硬件输出，不能合并为一个通过结论。
- **NOT-EXECUTED**：真实 DeepSeek 付费请求与翻译质量。任何真实费用验证需要单独授权；本轮只用 localhost Mock 和假凭据。
- **NOT-EXECUTED**：真实外部歌词站点可用性及浏览器服务，不从 Mock 推断网络可用性。
- **NOT-EXECUTED**：突然断电、磁盘断开与进程强杀恢复；受控 SQLite 事务失败不等同于这些事件。
- **NOT-EXECUTED**：一小时以上运行、长期 RSS 趋势、Linux 和 sanitizer。已执行的 1000 曲、100 次切歌、100 次 Facade 生命周期仅属有界负载。
- **待批准的产品契约**：D-R8-02 新增退出失败“重试/仍然退出”交互；当前 RC 保持已验收幂等退出行为。
- **发布前置**：Harness R9 独立 PASS、剩余 P3 处置/接受、备份与便携 data 升级演练、用户正式发布确认。只创建候选标签，不创建正式 v0.2.0 标签。
