# V0.2.6-GA 构建与对应源码

Windows x64，MSYS2 UCRT64，GCC 16.2，Qt 6.11.2，FFmpeg 9.0.2，CMake/Ninja。使用发行标签的完整源码；先提交，再配置构建，以绑定提交、源码指纹、主程序、QML DLL 和便携入口。

```powershell
$env:PATH = 'C:\Software\MSYS2\ucrt64\bin;' + $env:PATH
python scripts/prepare-midi-runtime.py
python scripts/prepare-dash-ffmpeg.py --msys2 C:/Software/MSYS2
cmake -S . -B build-ga -G Ninja -DCMAKE_BUILD_TYPE=Release -DRELAXER_ENABLE_QML=ON -DCMAKE_PREFIX_PATH=C:/Software/MSYS2/ucrt64
cmake --build build-ga --target Relaxer relaxer_qml_runtime RelaxerPortable RelaxerStart RelaxerLockCheck --parallel 4
python scripts/package-rc.py --source . --build build-ga --baseline <V0.2.2-GA-Win64.zip> --destination <新的发行目录> --qt-prefix C:/Software/MSYS2/ucrt64
python scripts/package-additional-sources.py --source . --build build-ga --assets <发行目录/Assets>
```

`package-rc.py` 校验公开基础包的固定 SHA-256，保留其运行依赖，添加匹配版本的 QtMultimedia QML 模块及依赖，并使用本版修补的 `avformat-63.dll`。根目录是便携应用入口，不包含源码准备启动器。默认资料由首次启动创建，不收集个人资料或开发预览目录。压缩包附逐文件及附件 SHA-256。

- `Relaxer-V0.2.6-GA-Source.zip`：发行提交完整应用源码、全部实际适配代码、许可、构建及安装脚本。平台与资源归属见根目录 `REFERENCES.md` 和 `docs/third-party/`。
- `Relaxer-V0.2.6-GA-MIDI-Source.zip`：FluidSynth、GCEM、GeneralUser GS 的精确归档、摘要和准备脚本。原许可及音色独立许可保留。
- `Relaxer-V0.2.6-GA-FFmpeg-Source.zip`：FFmpeg 9.0.2 原始完整归档、DASH 中断补丁、实际 config.h/config.mak、运行库身份和准备脚本；按该包 README 复现本版 avformat。
- 未修改的 Qt/MSYS2/FFmpeg 基础依赖源码及配套材料见公开 [V0.2.2-GA 第三方源码附件](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.2-GA/Relaxer-V0.2.2-GA-Third-Party-Source.zip)。这不能替代上面本版修改组件的额外材料。

Windows 普通便携程序不要求设备密钥或签名来安装修改版；自行重建时，用同一构建的入口、主程序、QML DLL 和依赖重新组包，由组包脚本重新生成完整性清单。程序不连接受锁定的用户产品安装机制。
