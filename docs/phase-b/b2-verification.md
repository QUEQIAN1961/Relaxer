# Phase B2：实际验证记录

本记录基于 Queue 拆分源码提交 aca274f。文档和校验清单收口不改变构建源码指纹。

## 平台和结果

- Ubuntu 24.04.3 LTS x86_64；GCC 13.3.0；CMake 3.28.3；Ninja 1.11.1；Qt/QtTest 6.4.2；GNU objdump 2.42；FFmpeg 6.1.1。
- Release / C++17。Windows 10 + MSYS2 UCRT64 + Qt 6.11.2 未在本环境执行。
- 最终未过滤 CTest：21/21 组通过、0 失败、0 组跳过，127 秒（JUnit 记录）。
- B1 在 Linux 可注册的原组为 18；新增 Queue、链接审计、Playback 集成三组。原四个 Windows 专属组不在 Linux 注册，不计为已通过或 CTest 跳过。
- QtTest 合计 261 passed、0 failed、4 skipped、0 blacklisted，包含各组 initTestCase/cleanupTestCase；不含两个纯 C++ 组和 CMake 审计。
- 独立 Queue-only 配置：2/2 组通过。没有删除旧组、放宽断言或改变默认完整回归的过滤条件。

| CTest 组 | QtTest 通过 | 失败 | 跳过 | 组结果 |
| --- | ---: | ---: | ---: | --- |
| relaxer_queue | 22 | 0 | 0 | 通过 |
| relaxer_queue_link | — | — | — | 通过 |
| relaxer_core_update | — | — | — | 通过 |
| relaxer_readonly_lock | — | — | — | 通过 |
| relaxer_core | 21 | 0 | 2 | 通过 |
| relaxer_queue_playback | 8 | 0 | 0 | 通过 |
| relaxer_services | 4 | 0 | 0 | 通过 |
| relaxer_gui | 9 | 0 | 1 | 通过 |
| relaxer_v11 | 18 | 0 | 0 | 通过 |
| relaxer_v12 | 14 | 0 | 0 | 通过 |
| relaxer_v13 | 18 | 0 | 0 | 通过 |
| relaxer_popup | 7 | 0 | 0 | 通过 |
| relaxer_motion | 4 | 0 | 0 | 通过 |
| relaxer_v14 | 10 | 0 | 0 | 通过 |
| relaxer_v15 | 7 | 0 | 0 | 通过 |
| relaxer_v16 | 33 | 0 | 1 | 通过 |
| relaxer_v19 | 27 | 0 | 0 | 通过 |
| relaxer_v110 | 6 | 0 | 0 | 通过 |
| relaxer_phase03 | 9 | 0 | 0 | 通过 |
| relaxer_phase031 | 33 | 0 | 0 | 通过 |
| relaxer_media_smoke | 11 | 0 | 0 | 通过 |

QtTest 的四项跳过为原有硬件音频条件（core 两项、GUI 一项）及 V16 的可选实网 QA 一项。本轮没有开启实网翻译或访问用户音频设备。

## 迁移前保护和诊断

1. 基线 ZIP 和 bundle 指纹符合要求；153 个 ZIP 文件匹配 B1 Git 树，152 项原清单全部通过。
2. B1 原 Queue 上运行新增测试：Queue 22/0/0，Playback 集成 8/0/0，均含 QtTest 初始化和清理。
3. 原 queueModes、shuffle 独立运行：4/0/0（两个业务用例加初始化/清理）；原 V17 多轮预测：3/0/0。拆分后相同原测试再次通过。
4. B1 保护基线完整 Linux CTest 首轮 19/20；V14 进程启动失败，检查得到构建目录中该程序是零字节文件。只重链接原 B1 对象后该组 1/1 通过，没有改源码或断言。原 18 组和新增两组均有通过记录。
5. 第一次 B2 编译发现 window.cpp 间接使用 playback.h 暴露的 QRandomGenerator；恢复兼容 include 后全工程编译通过，没有修改 GUI。Queue 声明/方法和 Playback 其余实现逐字节不变。
6. B2 第一轮完整 CTest 为 20/21，集成测试程序同样受零字节构建产物影响。重链接不完整产物，检查程序大小与执行位后，再次执行完整未过滤套件：21/21。
7. 长进程的部分文本日志未完整保留；最终组结果以 CTest 结束时写出的 JUnit XML 为准。为补全两组 QtTest 计数，另行完整记录集成测试 8/0/0、V14 10/0/0；这两组复核不冒充全量回归。
8. 原 browser-translation.cpp 的六条 misleading-indentation 警告仍保留，没有通过修改翻译模块消除。Queue 目标没有产生新的编译警告。

以上三段数字在 QtTest 场景下依次表示通过/失败/跳过。修复构建产物没有修改旧测试或降低兼容要求。

## 本轮执行命令

路径相对于云端工作目录；在源码包根目录复现时，把 work/Relaxer 改为 .、构建路径按需调整。

```bash
cmake -S work/Relaxer -B builds/b1-protected -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build builds/b1-protected --parallel 6
ctest --test-dir builds/b1-protected --output-on-failure --output-junit logs/b1-protected-ctest.xml

cmake -S work/Relaxer -B builds/b2-full -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build builds/b2-full --parallel 6
ctest --test-dir builds/b2-full --output-on-failure --output-junit logs/b2-final-ctest.xml

QT_QPA_PLATFORM=offscreen QT_MEDIA_BACKEND=ffmpeg RELAXER_SKIP_AUDIO_DEVICE_TESTS=1 builds/b2-full/relaxer_tests queueModes shuffle -v1
QT_QPA_PLATFORM=offscreen QT_MEDIA_BACKEND=ffmpeg RELAXER_SKIP_AUDIO_DEVICE_TESTS=1 builds/b2-full/relaxer_v16_tests v17ShuffleForecastMatchesActualPlaybackAcrossRounds -v1
```

完整 CTest 使用原 CMake 环境设置：offscreen、ffmpeg、RELAXER_SKIP_AUDIO_DEVICE_TESTS=1。Queue-only 禁用 Widgets/Gui/Multimedia/Sql/Network/Svg 包的完整命令见 b2-queue.md。

## 真实链接与符号审计

- Queue 独立程序链接行仅包含 domain、Qt Core/Test；objdump 检查实际 ELF NEEDED。
- ldd 传递 Qt 依赖：Queue 为 Qt6Core、Qt6Test；services 为 Qt6Core、Qt6Network、Qt6Test。没有 Widgets、Gui、Sql、Multimedia 进入 Queue 程序。
- nm 确认六个非内联 Queue 方法只在 librelaxer_domain.a 定义，core 没有第二份定义。domain 关闭 AUTOMOC/AUTORCC，Playback 的 MOC 继续归 core，服务的 MOC 归属未动。
- 将含 Gui/Sql/Multimedia 的原 core 测试作为负对照，Queue 审计如期拒绝。
- Windows DLL 导入、静态链接顺序和 MinGW 行为仍待原平台执行，不能用 Linux ELF 结果代替。

```bash
ldd builds/b2-full/relaxer_queue_tests
ldd builds/b2-full/relaxer_services_tests
objdump -p builds/b2-full/relaxer_queue_tests
objdump -p builds/b2-full/relaxer_services_tests
ninja -C builds/b2-full -t commands relaxer_queue_tests
nm -C --defined-only builds/b2-full/librelaxer_domain.a
nm -C --defined-only builds/b2-full/librelaxer_core.a
```

## Linux 隔离进程验证

将本轮新构建的 Relaxer 复制到独立目录，仍使用系统 Qt；这不是可移植 Windows Runtime 部署。

- --self-test：合成音频导入、SQLite、歌单和播放统计通过。
- --screenshot：offscreen Widgets 窗口初始化并生成 PNG，通过；仅用于内部验证。
- --media-smoke：真实 Qt FFmpeg PCM 解码通过，单声道、44,100 Hz、22,528 帧、510,829 微秒；无需声卡，未创建传入的禁止资料目录。
- 没有验证扬声器输出、Windows MIDI、Win32 启动器或 Windows 隔离 DLL 部署。

## 判定和后续门槛

B2 源码与 Linux 验证完成，Windows 平台验收待完成。没有已知未解决的 B2 代码失败；不完整测试产物已处理，最终套件通过。
Windows 复验见 b2-windows-acceptance.md。原 verify-runtime.ps1 的 proof.version=1.5.0 保持原状，不在本阶段修复。
本次只建立注明 Linux 已验证的检查点，正式全平台标签待 Windows 验收；B3 仅提供准备建议，未执行。
