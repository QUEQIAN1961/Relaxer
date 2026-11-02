# V0.2.3-RC 构建与本地打包

使用 Windows、MSYS2 UCRT64、CMake/Ninja、Qt 6.11.2。沿用项目的 Qt/QML、FFmpeg 等依赖，不改变启动校验、线程或播放核心。

```powershell
$env:PATH='C:\Software\MSYS2\ucrt64\bin;'+$env:PATH
cmake -S D:\Git\Relaxer\Development -B D:\Git\Relaxer\Testing\build-ui -G Ninja -DCMAKE_BUILD_TYPE=Release -DRELAXER_ENABLE_QML=ON
cmake --build D:\Git\Relaxer\Testing\build-ui --target Relaxer RelaxerPortable RelaxerStart RelaxerLockCheck --parallel 6
python D:\Git\Relaxer\Development\scripts\package-rc.py --source D:\Git\Relaxer\Development --build D:\Git\Relaxer\Testing\build-ui --baseline "D:\Git\Relaxer Version\Release\V0.2.2-GA\Assets\Relaxer-V0.2.2-GA-Win64.zip" --destination "D:\Git\Relaxer Version\Release\V0.2.3-RC"
```

打包前提交源码，并在最终修改之后重新运行 CMake 配置和构建。应用、QML DLL、便携入口必须来自同一源码指纹。目标目录必须不存在，避免覆盖已有归档。

脚本核对正式 V0.2.2-GA 基线与现有清单，保留第三方运行文件和许可证，只替换应用、QML DLL 和便携入口，生成配套的内部完整性清单。普通包仅以根目录 `Relaxer.exe` 启动，不带源码准备入口、用户资料、测试程序或日志。

输出沿用版本归档布局：`Desktop-Win64/` 为完整可运行目录，`Assets/` 为一个运行包 ZIP 和一个应用源码 ZIP，`Source/` 为该源码 ZIP 的展开内容。`BUILD-IDENTITY.json` 记录实际构建提交与源码指纹；GitHub 发布状态为未发布。

第三方运行文件沿用 V0.2.2-GA，因此对应源码、补丁与构建材料继续使用正式版已有的 `Relaxer-V0.2.2-GA-Third-Party-Source.zip`，公开版本可从 [V0.2.2-GA Release](https://github.com/QUEQIAN1961/Relaxer/releases/tag/v0.2.2-GA) 获取。原许可证与版权声明保留。
