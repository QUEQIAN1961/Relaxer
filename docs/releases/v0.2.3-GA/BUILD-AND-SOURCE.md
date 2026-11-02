# V0.2.3-GA 构建与组装

环境：Windows x64、MSYS2 UCRT64、C++17、Qt 6.11.2、CMake/Ninja。MIDI 固定依赖与校验值见 `resources/midi-dependencies.json`，原文许可随准备文件保留。

```powershell
$env:PATH='C:\Software\MSYS2\ucrt64\bin;'+$env:PATH
python scripts/prepare-midi-runtime.py
cmake -S . -B D:\Git\Relaxer\Testing\build-ui -G Ninja -DCMAKE_BUILD_TYPE=Release -DRELAXER_ENABLE_QML=ON
cmake --build D:\Git\Relaxer\Testing\build-ui --target Relaxer relaxer_qml_runtime RelaxerPortable RelaxerStart RelaxerLockCheck --parallel 6
python scripts/package-rc.py --source D:\Git\Relaxer\Development --build D:\Git\Relaxer\Testing\build-ui --baseline "D:\Git\Relaxer Version\Release\V0.2.2-GA\Assets\Relaxer-V0.2.2-GA-Win64.zip" --destination "D:\Git\Relaxer Version\Release\V0.2.3-GA"
```

`package-rc.py` 沿用原名称，同时支持 RC 与 GA。先提交源码，再重新配置和构建；应用、QML DLL 与入口必须匹配同一源码指纹。组装目标必须为新目录，保留所有既有归档。已有 MIDI 准备目录可以离线传给 `RELAXER_MIDI_RUNTIME_DIR`。

输出：`Desktop-Win64/` 为可运行目录，`Assets/` 为运行包、应用源码 ZIP 与说明，`Source/` 为应用源码展开内容。包中不包含用户 data、测试日志或凭据。内部完整性清单覆盖 runtime，每次启动仍执行完整校验；不需要联网许可或厂商密钥即可依已有说明更新用户替换库后的清单。

基础第三方运行文件来自正式 V0.2.2-GA，保留原版权与许可，对应源码沿用 `Relaxer-V0.2.2-GA-Third-Party-Source.zip`。新增 BASS/BASSMIDI 为专有依赖，该源码附件不包含它们的源码；许可范围及尚未确认的 GPL 兼容性见 `docs/MIDI-BASS.md`，不能把附件名称当作新增依赖的授权证明。
