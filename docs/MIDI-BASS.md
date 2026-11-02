# MIDI 播放后端（本地开发与测试）

开发版使用 BASS 2.4.18.3、BASSMIDI 2.4.16 和 GeneralUser GS 2.0.3 替代默认 Windows MCI sequencer。普通音频仍由原 Qt Multimedia / FFmpeg 路径播放。

首次打开 MIDI 时才加载 DLL、初始化输出并预加载该曲需要的音色采样。SF2 使用内存映射；播放进度直接读取 BASS 通道位置。暂停时保留用户暂停位置，避免设备残余输出缓冲让进度继续移动。音量应用于本 MIDI 通道，不更改系统音量。

关闭最后一个 MIDI 通道后释放音色、输出设备并显式卸载 DLL。切回普通音频时释放这些 MIDI 资源。未加入自动退回 MCI 的路径；缺少运行库或音色时显示具体错误。

## 准备与构建

```powershell
python scripts/prepare-midi-runtime.py
cmake -S . -B <构建目录> -G Ninja -DCMAKE_BUILD_TYPE=Release -DRELAXER_ENABLE_QML=ON
cmake --build <构建目录> --target Relaxer RelaxerPortable --parallel 6
```

准备脚本按 `resources/midi-dependencies.json` 的固定地址、修订和 SHA-256 获取依赖，默认放入未跟踪的 `.dependencies/midi/`。网络代理沿用 Python 的现有环境/系统代理设置。已有下载可用 `--cache <缓存目录>`；构建可用 `-DRELAXER_MIDI_RUNTIME_DIR=<已准备目录>` 离线读取，不在程序启动时下载。

运行布局：`bass.dll`、`bassmidi.dll` 与实际运行的核心 `Relaxer.exe` 同目录；音色为 `midi/GeneralUser-GS.sf2`，原文许可位于 `licenses/midi/`。便携包中这些文件均在 `runtime/` 下，并加入原有启动完整性清单。只复制 EXE 无法播放 MIDI。

## 许可与分发范围

本地测试不更改 Relaxer 自有代码的 GPL-3.0-only 许可，也没有上传新的发行包。

BASS 是专有运行库，其免费使用条款限定非商业用途；BASSMIDI 随 BASS 使用。GeneralUser GS 另有作者自定许可。原文均随准备的文件保留，不能将这些文件标为 GPL/LGPL 开源组件。GeneralUser GS 的许可也保留了作者对部分早期采样来源的不确定性说明。

当前集成用于个人本地非商业测试；公开分发前仍须解决 BASS 与现有 GPL 分发组合的许可兼容性。本文件不授予新的库许可、不增加 GPL 链接例外，也不代表公开分发获批。`package-rc.py` 仅保留本地候选包的未发布状态和相应标识。

来源：

- BASS / BASSMIDI：https://www.un4seen.com/bass.html
- GeneralUser GS：https://github.com/mrbumpy409/GeneralUser-GS
- 采样预加载：https://www.un4seen.com/doc/#bassmidi/BASS_MIDI_StreamLoadSamples.html

## 验证

`relaxer_bass_midi_tests` 验证真实 PCM 渲染、密集音符实时预算、中文长路径、Seek、多流所有权、错误恢复及关闭后的 DLL 卸载。显式设置 `RELAXER_MIDI_AUDIO_TEST=1` 可执行本机输出的暂停/继续/跳转和真实 Playback 队列 MIDI↔普通音频切换测试（测试通道音量为 0）。离线解码不等同听音验收。
