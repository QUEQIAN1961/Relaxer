# MIDI 播放后端

V0.2.5-RC 将 BASS / BASSMIDI 替换为开源 FluidSynth 2.6.2，继续使用 GeneralUser GS 2.0.3 音色。普通音频仍走 Qt Multimedia / FFmpeg。

首次打开 MIDI 才动态加载 FluidSynth，并创建该曲独立的合成器和 MIDI 文件播放器。FluidSynth 解析、调度音符、控制器与 SysEx，按采样时钟生成 48 kHz 立体声 PCM，经 Qt 音频输出播放。应用只读取 SMF 的时间信息，将毫秒进度换算为 tempo map 中的 tick；支持 SMF 0/1、变速和 SMPTE 时间分度。暂停冻结输出和进度；切回普通音频或关闭最后一个实例时释放音频设备、音色、合成器并卸载 DLL。

## 准备与构建

```powershell
python scripts/prepare-midi-runtime.py
cmake --build <构建目录> --target Relaxer --parallel 6
```

准备脚本使用 `resources/midi-dependencies.json` 中固定的版本、地址和 SHA-256，复用 `.dependencies/downloads/` 缓存，输出 `.dependencies/midi/`。启动时不下载依赖。构建通过 `RELAXER_MIDI_RUNTIME_DIR` 指定离线准备目录。

运行目录包含 `libfluidsynth-3.dll`、`midi/GeneralUser-GS.sf2`，以及 `licenses/midi/` 内的原文许可。`runtime-midi.json` 列出当前引擎的准确文件及散列；原 BASS DLL 不再使用或分发。文档沿用旧文件名，保持已有链接有效。

## 许可

FluidSynth 为 LGPL-2.1-or-later，允许商业使用，可与 Relaxer 的 GPL-3.0-only 代码组合；这不改变 Relaxer 的许可证。发行 DLL 从固定源码构建，禁用不使用的音频输出、可选 Signalsmith 引擎和外部编解码库（保留内置混响），使用固定 GCEM 数学头文件（MIT）；PCM 由 Qt 输出，不分发 SDL3、libsndfile 或其静态依赖。分发保留原文许可、构建选项与对应源码，遵循 LGPL 的源码与重新链接要求。

GeneralUser GS 使用独立音色许可，继续保留作者原文，包括早期部分采样来源的不确定性说明。更换合成引擎不会改变音色自身的许可。

上游源码与许可：

- [FluidSynth v2.6.2](https://github.com/FluidSynth/fluidsynth/tree/v2.6.2)，[许可](https://github.com/FluidSynth/fluidsynth/blob/v2.6.2/LICENSE)
- [GeneralUser GS](https://github.com/mrbumpy409/GeneralUser-GS)

## 验证

`relaxer_fluid_midi_tests` 覆盖真实 PCM、密集音符、中文长路径、跳转、多流独立所有权及失败后的释放。设置 `RELAXER_MIDI_AUDIO_TEST=1` 执行本机音频设备上的暂停/继续/跳转和实际播放队列 MIDI↔普通音频切换（音量为 0）。自动化不等于主观听音确认；FluidSynth 与 BASSMIDI 的混响、插值等实现不同，声音不保证逐样本一致。

同版 `Relaxer-V0.2.5-RC-MIDI-Source.zip` 提供 FluidSynth 固定源码、GeneralUser GS 原始音色来源及准备脚本；基础 MSYS2/Qt 对应源码沿用 V0.2.2-GA 附件。构建时使用 MSYS2 UCRT64，先将其 bin 加入 PATH，再执行准备脚本。
