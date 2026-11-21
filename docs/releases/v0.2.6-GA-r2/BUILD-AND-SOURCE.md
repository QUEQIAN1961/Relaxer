# V0.2.6-GA-r2 构建与对应源码

Windows x64，MSYS2 UCRT64，GCC 16.2，Qt 6.11.2，FFmpeg 9.0.2，CMake/Ninja。r2 是发行修订，程序版本保留 0.2.6-GA，产品源码指纹仍为 3337cdb3a5507eaafe9326b046faa021537db6c50f88c22827de3447d73010e4。使用发行标签的完整源码；先提交，再配置构建，以绑定提交、源码指纹、主程序、QML DLL 和便携入口。

```powershell
$env:PATH = 'C:\Software\MSYS2\ucrt64\bin;' + $env:PATH
python scripts/prepare-midi-runtime.py
python scripts/prepare-dash-ffmpeg.py --msys2 C:/Software/MSYS2
cmake -S . -B build-ga -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -g1" -DRELAXER_ENABLE_QML=ON -DCMAKE_PREFIX_PATH=C:/Software/MSYS2/ucrt64
cmake --build build-ga --target Relaxer relaxer_qml_runtime RelaxerPortable RelaxerStart RelaxerLockCheck --parallel 4
python scripts/package-rc.py --source . --build build-ga --baseline <V0.2.2-GA-Win64.zip> --destination <新的发行目录> --qt-prefix C:/Software/MSYS2/ucrt64 --release-revision r2 --acceptance-status independent-harness-pass-with-environment-note
python scripts/package-additional-sources.py --source . --build build-ga --assets <发行目录/Assets> --release-revision r2
```

`package-rc.py` 校验公开基础包的固定 SHA-256，添加匹配版本的 QtMultimedia QML 模块及依赖，并使用本版修补的 `avformat-63.dll`。打包时仅合并 runtime/bin 与 runtime 中 SHA-256 完全相同的 DLL，保留工具所需的不同版本库；便携入口的 PATH 提供共享目录，并逐个检查所有 PE 导入是否可解析。根目录是便携应用入口，不包含源码准备启动器。默认资料由首次启动创建，不收集个人资料或开发预览目录。压缩包附逐文件及附件 SHA-256；runtime/runtime-layout.json 记录共享库关系。OpenCC 的生成脚本、字典和上游构建参考保留在 Source.zip，运行包保留其许可、作者和改编说明。源码归档不含预编译启动工具与私人测试过程报告；基础依赖的来源记录见 docs/third-party/base-runtime。

- `Relaxer-V0.2.6-GA-r2-Source.zip`：发行提交完整应用源码、全部实际适配代码、许可、构建及安装脚本。平台与资源归属见根目录 `REFERENCES.md` 和 `docs/third-party/`。
- `Relaxer-V0.2.6-GA-r2-MIDI-Source.zip`：FluidSynth、GCEM、GeneralUser GS 的精确归档、摘要和准备脚本。原许可及音色独立许可保留。
- `Relaxer-V0.2.6-GA-r2-FFmpeg-Source.zip`：FFmpeg 9.0.2 原始完整归档、DASH 中断补丁、实际 config.h/config.mak、运行库身份和准备脚本；按该包 README 复现本版 avformat。
- 未修改的 Qt/MSYS2/FFmpeg 基础依赖源码及配套材料见公开 [V0.2.2-GA 第三方源码附件](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.2-GA/Relaxer-V0.2.2-GA-Third-Party-Source.zip)。这不能替代上面本版修改组件的额外材料。

Windows 普通便携程序不要求设备密钥或签名来安装修改版；自行重建时，用同一构建的入口、主程序、QML DLL 和依赖重新组包，由组包脚本重新生成完整性清单。程序不连接受锁定的用户产品安装机制。
