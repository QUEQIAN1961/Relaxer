# Phase B2：Windows / MSYS2 UCRT64 待验收

本轮在 Ubuntu 24.04 / Qt 6.4.2 执行，未运行 Windows 构建、DLL 部署、Win32 启动器或物理声卡验证。
以下命令供原 MSYS2 UCRT64 / Qt 6.11.2 环境复核。源码应解压至全新目录，所有验证数据留在该目录内；不覆盖现有工程、Runtime 或真实曲库。
代码与平台验收状态应分别判断；Linux 结果不写成 Windows 通过。

## 1. 完整构建与全部测试

在 MSYS2 **UCRT64** 终端进入新解压的 Relaxer-V0.2.0-dev 目录：

```bash
set -euo pipefail
test "${MSYSTEM:-}" = UCRT64
cmake -S . -B build-b2-windows -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-b2-windows --parallel 6
ctest --test-dir build-b2-windows -N
ctest --test-dir build-b2-windows --output-on-failure \
  --output-junit "$PWD/b2-windows-ctest.xml"
```

在具有 GNU objdump 的该工具链中应注册 **25 组**：B1 的 22 组加 Queue、Queue 链接审计、Queue/Playback 集成三组。
Linux 不注册的原四组是 relaxer_client_frame、relaxer_launcher_env、relaxer_prepare、relaxer_pacman_lock。
不要删除、禁用或过滤原组来获得全通过。当前测试入口按原规则跳过硬件音频和可选实网 QA，须另记录 QtTest 的 skipped 数。

## 2. 显式队列回归和独立边界

```bash
export QT_QPA_PLATFORM=offscreen
export QT_MEDIA_BACKEND=ffmpeg
export RELAXER_SKIP_AUDIO_DEVICE_TESTS=1
./build-b2-windows/relaxer_tests.exe queueModes shuffle -v1
./build-b2-windows/relaxer_v16_tests.exe \
  v17ShuffleForecastMatchesActualPlaybackAcrossRounds -v1
./build-b2-windows/relaxer_queue_tests.exe -v1
./build-b2-windows/relaxer_queue_playback_tests.exe -v1
objdump -p build-b2-windows/relaxer_queue_tests.exe
objdump -p build-b2-windows/relaxer_services_tests.exe

cmake -S . -B build-b2-queue -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRELAXER_QUEUE_ONLY=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Widgets=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Gui=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Multimedia=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Sql=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Network=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Svg=ON
cmake --build build-b2-queue --parallel 2 --verbose
ctest --test-dir build-b2-queue --output-on-failure
```

Queue 测试直接导入的 Qt DLL 应仅有 Qt6Core/Qt6Test，不能出现 Qt6Widgets/Gui/Sql/Multimedia/Network。
服务测试应仅有 Qt6Core/Network/Test，保持 B1 无 Widgets 属性。核对传递 DLL 闭包及 MinGW 静态链接顺序。

## 3. 新目录中的 Windows Runtime 部署

新源码目录中需没有 runtime。使用现有脚本部署新编译的 EXE；FFmpeg 来自原已准备的 runtime-ffmpeg 或 /ucrt64/bin，不使用旧播放器 EXE 或旧 Qt DLL。
先确认原 FFmpeg/ffprobe、windeployqt 和 PowerShell 可用，再在该新目录执行：

```bash
unset QT_QPA_PLATFORM RELAXER_SKIP_AUDIO_DEVICE_TESTS
export RELAXER_BUILD_DIR="$(cygpath -w "$PWD/build-b2-windows")"
export RELAXER_SOURCE_ROOT="$(cygpath -w "$PWD")"
bash scripts/collect-runtime.sh
powershell.exe -NoProfile -ExecutionPolicy Bypass \
  -File "$(cygpath -w "$PWD/scripts/verify-runtime.ps1")"
```

脚本应在隔离 Runtime 验证导入、SQLite、歌单/统计、GUI 与真实 PCM，不能仅凭 EXE 启动判通过。
已知原 verify-runtime.ps1 的 proof.version 仍写 1.5.0；本轮按范围保留，不将该历史标记当成产品版本。产品实际仍为 1.10.0。

## 4. 需要设备的人工验收

- 四种播放模式的自动/手动下一首、上一首和轮边界。
- 连续多轮反复打开预告，点选预告中间歌曲，核对实际播放及历史。
- 随机状态下删除当前或其他歌曲、切换模式、保存退出并从数据副本恢复。
- 暂停、进度、错误跳过、MIDI/转码与播放统计。上述实现均未改，但仍需 Windows 目标环境验收。
- 原启动器与首次准备、DLL/插件闭包、独立 Runtime，不触碰真实资料库。

实网翻译 QA 保持前阶段的独立人工事项，不属于本轮 Queue 改造；本次不调用在线翻译网站。
验收通过后再决定建立正式阶段标签及进入 B3，本次没有执行 B3。
