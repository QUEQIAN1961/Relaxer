# 性能观测基线

机器：AMD Ryzen 9 7945HX with Radeon Graphics，32 个逻辑 CPU，可见物理内存 47.21 GiB，Windows-10-10.0.19045-SP0；Qt 6.11.2 / GCC 16.2.0 / Release。native Windows QPA、默认 Qt Quick 渲染（没有强制 software），测试禁用实际音频设备初始化。系统 DPI 保留，截图额外测试 QT_SCALE_FACTOR=1 和 2；这两个是附加倍数，不把 factor=1 误称物理 100% DPI。

两种 UI 各 5 个样本，顺序交替，使用同一个成熟空库的逐份副本，包括数据库打开与唯一 Playback 初始化。APPDATA/LOCALAPPDATA/TEMP 独立。OS 文件缓存没有清空：重复样本属于暖缓存；没有伪称 OS 冷启动。测量目录首次 Widgets 建库运行单列于 unprimed.json。首个测量 QML 进程：OS 可见 1537.213ms，UI loaded 1518.949ms，首帧 1688.586ms；是新隔离应用缓存目录中的首次测量，系统 DLL/文件缓存仍可能已热。

以下是 5 个样本的中位数：

| 指标 | Widgets | QML |
|---|---:|---:|
| OS 可见窗口 ms | 1718.545 | 1563.102 |
| UI loaded ms | 1693.832 | 1526.658 |
| 首帧边界 ms | 1719.941 | 1695.985 |
| 空闲 working set MiB | 221.070 | 236.410 |
| 空闲 private MiB | 200.535 | 245.184 |
| CPU 单核百分比 | 0.000 | 0.000 |
| WM_CLOSE 到进程退出 ms | 55.321 | 79.486 |

OS 可见时间用 CreateProcess 前的外部时钟与每 25ms 枚举 HWND，包含 DLL 加载；UI loaded/首帧由 main 入口计时，不包含完整的 OS loader 成本。Widgets 首帧为 Paint 事件，QML 为 frameSwapped 回到 GUI 线程；两个边界不同，不作直接渲染性能排名。shutdownComplete 记录业务 shutdown，外部 WM_CLOSE 到进程退出还覆盖 engine/对象清理。

CPU 采样在可见后等待 1.5 秒，随后约 2 秒读取进程 kernel+user CPU 时间，结果按单逻辑核百分比表示，整机占比还需除 32。本次 5 次中位数均为 0，表示采样分辨率内接近空闲，不表示绝对零 CPU。内存为采样 working set/private，不含独立 GPU 记账。所有 10 个样本 saveSucceeded=true、Playback=1、connection=1、所属残留进程为空。

## 空页切换

使用同一原生部署环境中的独立探针，交替歌单/设置页 20 次，报告同步调用与下一次绘制边界，避免将仅修改页索引冒充视觉完成：

| 模式 | 样本 | 同步调用 µs 中位数 | 下一绘制边界 ms 中位数 |
|---|---:|---:|---:|
| widgets | 20 | 4558.900 | 8.646 |
| qml | 20 | 198.050 | 1.673 |

Widgets 为强制 update 后的 Paint dispatch，QML 为 frameSwapped queued；不包括桌面合成器，且页面功能数量不同。QML 仍是占位页，不能据此声称完整迁移后更快。首次探针错误地使用 SW_HIDE 而等待可见帧导致超时，已改为实际曝光的测试窗口，保留失败日志。

## 限制与复现

Widgets 构造完整设置/AI/导入/外观等 UI；QML 只构造 C1 必要的 Library、Playback、桥和 Shell。没有为了数字取消必要 Playback 初始化，但两种界面功能量不等，当前仅建立测量方法，不能推广到完整界面、大库、MIDI 活跃播放或冷机器。

运行 `python scripts/measure-phase-c.py --package <新便携目录> --output <新证据目录> --samples 5`。原始 performance.json、每个样本 UI JSON、stdout/stderr、原生导航 JSON 均随 validation 交付。后续 C2 应加相同真实数据规模的分页/模型刷新测量，不能沿用空库数字作大库结论。
