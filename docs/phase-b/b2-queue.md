# Phase B2：Queue 独立边界与行为保护

开发范围仅为 Queue 与 Mode。产品仍为 1.10.0，Widgets、数据库、歌词、媒体后端、统计与启动系统均沿用 B1。
Windows 验收命令见 [b2-windows-acceptance.md](b2-windows-acceptance.md)。

## 代码与依赖

| 文件或目标 | 职责与依赖 |
| --- | --- |
| src/domain/queue.h | 原 Mode 枚举和 Queue 声明；仅包含 QVector 与 QtGlobal |
| src/domain/queue.cpp | 原 Queue 的六个非内联方法；仅额外使用 QSet、QRandomGenerator |
| relaxer_domain | 显式静态库，公开依赖 Qt6::Core，关闭 AUTOMOC/AUTORCC |
| src/playback.h | 包含 domain/queue.h 兼容旧调用方；Playback 仍持有唯一一份 m_queue |
| src/playback.cpp | 仅移走 Queue 方法；Playback 构造、播放、错误、恢复、持久化等实现逐字节不变 |
| relaxer_core | 保留原有 Core/Gui/Sql/Multimedia 与 Windows winmm，公开链接 relaxer_domain |
| relaxer_services | B1 的 Core/Network 边界和 AUTOMOC 归属均不变 |
| relaxer_queue_tests | 只链接 relaxer_domain 和 Qt6::Test；QCoreApplication 入口 |
| relaxer_queue_playback_tests | 链接 relaxer_core 和 Qt6::Test，使用临时数据库及合成 PCM，不依赖 Widgets |

旧 window.cpp 通过 playback.h 间接使用 QRandomGenerator，因此保留该兼容包含，不修改 GUI 源码。
所有已有调用者继续包含 playback.h 即可，不引入第二份 Queue、转发对象或状态存储。
没有移动 Track、Playlist、Session、歌词文本或时间轴，也没有开始 B3。

## 随机袋与状态语义

完整 Queue 声明和方法体从 B1 原样移动；没有换算法、调整调用时机、增加随机种子接口或改变数据成员。
Mode 仍按 Sequential=0、ListLoop=1、SingleLoop=2、Shuffle=3 编码。

- set 过滤非正数和重复 ID，保留输入顺序，选择指定歌曲或首项，重建袋并清空历史/计划袋。
- setMode 保留当前歌曲，清除计划袋和历史，重新生成初始袋；重复设置同一模式也保留原有重置语义。
- refill(false) 排除当前歌曲；refill(true) 先消耗已预测袋，否则生成完整一轮并避免轮边界立即重复。
- next 在随机模式优先重放历史，其次消耗当前袋；手动下一首与自动结束在 SingleLoop 下仍有区别。
- previous 保留随机历史回退及历史起点的既有索引回绕行为。
- upcoming **仍然有状态且非 const**：复制 Queue，在副本上执行 next(true)，把新生成的完整袋加入原实例的 m_plannedBags。
- 重复查看预告不移动当前歌曲和索引；实际 next 消耗此前预告的袋。没有将预测改为独立随机计算。
- Queue 的值复制保留袋、计划袋和历史；副本的后续修改不写回原实例。

Playback::playUpcoming 仍先预告，再按选中位置逐次 next(true)；forget 仍以 set 重建剩余队列。
保存键仍为 queue/current/position/activeSession，mode 仍单独保存。原契约没有持久化袋和随机历史：恢复调用 set 重建随机状态，不新增跨重启随机序列保证。

## 构建方式

默认构建包含全部原有平台适用目标，新增 Queue、Queue 链接审计及 Playback 集成三组测试，不删除或放宽旧测试。
`RELAXER_QUEUE_ONLY=ON` 是额外的独立验证配置，只查找 Qt Core/Test；它不代替完整回归。

```bash
cmake -S . -B build-queue -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRELAXER_QUEUE_ONLY=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Widgets=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Gui=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Multimedia=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Sql=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Network=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Svg=ON
cmake --build build-queue --parallel 2
ctest --test-dir build-queue --output-on-failure
```

scripts/verify-queue-link.cmake 在支持 objdump 的 Windows/Linux 上随链接自动执行，并另注册为 CTest。
它检查真实 PE/ELF 导入，要求 Qt Core/Test，拒绝 Gui、Widgets、Network、Sql、Multimedia、Svg、Quick、Qml、OpenGL。
Linux 额外用 ldd 验证传递 Qt 依赖仅为 Core/Test；服务测试的传递 Qt 依赖仅为 Core/Network/Test。
符号审计确认六个非内联 Queue 方法只定义在 domain 中，core 不含重复实现。
负对照将原 core 测试交给 Queue 审计，确实因不允许的 Qt 模块而失败。

## 测试设计

tests/queue-tests.cpp 的 20 个业务用例（含 QtTest 初始化/清理共 22 项）覆盖：初态与枚举编码、ID 过滤、四种模式、空/单首队列、边界、初始及完整随机袋、多轮预测、重复/部分预告、历史回退重放、模式重置、队列替换、复制赋值、原持久化值契约和按预告跳转。
随机测试检查同一实例已缓存的预测与实际输出，不要求独立实例共享随机顺序，不更改正式随机源。
其中四种队列大小各验证 48 轮，并在每轮反复查询；另覆盖 500 次部分消费和 40 个完整随机袋。

tests/queue-playback-tests.cpp 的 6 个业务场景（四种模式的数据行，含初始化/清理共 8 项）使用真实 Library/Playback：恢复过滤与数据库重开、预告跳转与非法索引、随机历史前进后退、模式保存、删除当前/非当前歌曲及清空。
测试只读写 QTemporaryDir，不访问用户曲库或浏览器。

先在 B1 原实现上运行新增测试，再迁移并运行同一套断言。原 queueModes、shuffle 和 V17 多轮预测测试保持原样，均另行显式执行。

## 验收边界

Linux 测试、动态依赖、符号和隔离进程检查不能替代 MSYS2 UCRT64/Qt 6.11.2、Windows DLL 部署、启动器或物理音频验收。
交付报告和 JSON 收据记录实际平台、首轮问题、修复、最终结果与 Git 校验值。Windows 未执行项目明确保留为待验收。
正式全平台验收标签须待 Windows 验收通过；本次只可建立注明 Linux 验证状态的检查点。
