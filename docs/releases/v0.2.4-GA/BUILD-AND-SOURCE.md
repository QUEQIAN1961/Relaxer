# V0.2.4-GA 构建与组装

环境：Windows x64、MSYS2 UCRT64、C++17、Qt 6.11.2、CMake/Ninja。沿用本机已准备的 `.dependencies/midi`，无需重新下载依赖。

先提交完整源码及资源，再配置和构建；所有核心 PE 必须与源码指纹一致。开发构建和打包不替代独立 Harness 全量回归。

```powershell
$env:PATH = 'C:\Software\MSYS2\ucrt64\bin;' + $env:PATH
cmake -S D:\Git\Relaxer\Development -B D:\Git\Relaxer\Testing\build-ui -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DRELAXER_ENABLE_QML=ON `
  -DRELAXER_MIDI_RUNTIME_DIR=D:\Git\Relaxer\Development\.dependencies\midi
cmake --build D:\Git\Relaxer\Testing\build-ui --target Relaxer relaxer_qml_runtime RelaxerPortable RelaxerStart RelaxerLockCheck --parallel 4
python D:\Git\Relaxer\Development\scripts\package-rc.py `
  --source D:\Git\Relaxer\Development --build D:\Git\Relaxer\Testing\build-ui `
  --baseline 'D:\Git\Relaxer Version\Release\V0.2.2-GA\Assets\Relaxer-V0.2.2-GA-Win64.zip' `
  --destination D:\Git\Relaxer\Releases\V0.2.4-GA
```

组包脚本沿用名称，同时支持 RC 与 GA；目标必须是新目录，不能覆盖既有归档。`Desktop-Win64/` 为运行目录，`Assets/` 包含运行包、应用源码 ZIP 和说明，`Source/` 是只读源码导出，不是新的开发工作区。基础第三方对应源码沿用原有归档并通过同批说明引用。

运行包不包含 data、音乐、歌词、日志或凭据。运行包内部清单继续覆盖 runtime，根入口每次启动执行完整校验。组包元数据保留 `published=false`、`publicReleaseApproved=false` 和待独立回归状态；版本标签 GA 不授予发布资格。Harness 通过后发布同一组已验证附件，不重建、改写或静默替换包内文件。
