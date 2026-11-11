# V0.2.5-GA 构建与组装

Windows x64、MSYS2 UCRT64、GCC 16.2、Qt 6.11.2、CMake/Ninja。先提交源码，再配置并构建，以确保应用、QML DLL、便携入口与源码指纹一致。

```powershell
$env:PATH = 'C:\Software\MSYS2\ucrt64\bin;' + $env:PATH
python scripts/prepare-midi-runtime.py
cmake -S . -B D:/Git/Relaxer/Testing/build-ui -G Ninja -DCMAKE_BUILD_TYPE=Release -DRELAXER_ENABLE_QML=ON
cmake --build D:/Git/Relaxer/Testing/build-ui --target Relaxer relaxer_qml_runtime RelaxerPortable RelaxerStart RelaxerLockCheck --parallel 6
python scripts/package-rc.py --source . --build D:/Git/Relaxer/Testing/build-ui --baseline 'D:/Git/Relaxer Version/Release/V0.2.2-GA/Assets/Relaxer-V0.2.2-GA-Win64.zip' --destination 'D:/Git/Relaxer Version/Release/V0.2.5-GA'
```

既有 `.dependencies/midi` 可直接复用；重新准备时使用 MIDI-Source.zip 中的原始归档及其 SHA-256。组包脚本同时支持 RC/GA。运行依赖取自固定 SHA-256 的 V0.2.2-GA 公开便携包，新增 MIDI 来自经过清单校验的本机构建；应用 PE 来自本次同一源码构建。仅选择运行文件、许可、说明和 Git 跟踪源码，不收集开发资料目录。默认资料 data/ 由用户首次启动时创建。

应用对应源码为同版 Source.zip；FluidSynth、GCEM、GeneralUser GS 及完整构建选项见同版 MIDI-Source.zip。基础 Qt/MSYS2/FFmpeg 源码沿用 V0.2.2-GA 的 Third-Party-Source.zip。保留所有许可及原版权声明。

发行元数据记录用户批准发布及本次定向检查范围；GA 标签不表示本次运行了完整 Harness CTest。实际验证边界与已知 software 后端限制见 RELEASE-NOTES.md。
