# 第三方版权、许可与对应源码

Relaxer 自有代码采用 GPL-3.0-only。第三方代码、数据、字体（如实际提供）和各自例外保留原授权与版权，不被统一重新许可。

本 Windows 构建使用 Qt 6.11.2、Qt Multimedia FFmpeg 后端及 QML 插件，FFmpeg/ffprobe 9.0.2 和其实际共享/嵌入依赖。FFmpeg 的实际 MSYS2 构建启用 GPL 与 version3，按 GPL-3.0-or-later 路线提供材料，不能把整个包称为纯 LGPL。精确软件包版本、320 个第三方 PE 的 SHA-256 及所有者见 COMPONENTS.json 和配套对应源码索引。

## 版权及许可证材料

- 圆圈到勾的形状变形数据由 morphicons 2.0.2（MIT，Guillermo）生成；原文许可和原生 QML 适配说明见 `docs/third-party/morphicons/`。平台身份图标的官网来源见 `resources/music-platforms/README.md`。

- `third_party/qrcodegen/`：Nayuki QR Code generator，MIT 许可；源文件中保留完整版权和许可，用于本机生成音乐平台登录二维码。
- 汽水与酷狗原生适配参考 MIT 许可的 `guowenye/qishui-api`（2026 arrow）与 `MakcRe/KuGouMusicApi`（2023 MakcRe）；来源、适配边界及原文许可证见 `docs/third-party/music-platforms/`。

- `third-party-licenses/`：原部署提供的软件包许可证与版权，包含所用包之外保留的原通知，不按目录数量推定实际分发组件。
- `licenses/startup/`：GCC/MinGW 运行代码版权、GPL 和适用例外材料。
- `licenses/supplemental/`：既有 Qt 精确源归属与许可、Rust crate 许可及 OpenCC 归属材料。
- `licenses/ga/`：本次从既有精确源码/上游归属保留的补充版权、LICENSE/COPYING/NOTICE、Rust 运行材料和 OpenCC 作者/改编说明；NOTICE-INDEX.json 绑定每份材料原位置与内容摘要。

Qt 自有库文件的开源选项与其内置第三方文件分别处理。qgrayraster.c 可依 FTL 分支，相关 FreeType 工作和作者致谢保留；qimagescale.cpp 的 BSD 与 Imlib2 条件分别保留，Unicode 数据和 XSVG 等独立声明不被 Qt 总许可覆盖。FreeType 相关软件基于 FreeType 项目及其贡献者的工作，原版权、修改记录和许可见附带材料。

This software is based in part on the work of the Independent JPEG Group.

本软件的部分组件基于 Independent JPEG Group 的工作。相关 IJG、libjpeg-turbo 版权与原始条款随许可证目录保留。

QtQml 的 JavaScriptCore/masm、Yoga、Material 资源等按各自适用声明；上游源码中的测试、开发工具和其他平台内容不因此全部成为 Windows 运行库。若完整对应源码侧包提供这些内容，其源码分发条件仍保留。

shaderc 静态构建相关精确来源为 glslang 16.3.0-1、SPIRV Tools/Headers 1.4.363.0 的相应 MSYS2 修订。保留 Apache/BSD/MIT 等实际文件声明，以及 glslang 生成解析器中的 Bison 特别例外；不能删除例外后推定冲突，也不能仅用 shaderc 包级 Apache 声明覆盖所有文件。

Rust crate、标准运行代码和生成代码按实际适用许可处理，host 过程宏不机械视为直接运行静态库。原始/重放锁的身份和来源记录保留，重放锁不冒充原 CI 原件。

OpenCC STCharacters 嵌入数据采用 Apache-2.0，原 LICENSE、AUTHORS、改编说明、精确字典与生成脚本随材料提供。冻结映射内容和原来源结论保持，历史最早导入修订不虚构。

## 对应源码、修改、构建与替换

`Relaxer-V0.2.3-GA-Source.zip` 提供本版完整应用源码与打包/重建脚本。基础第三方运行文件沿用正式 V0.2.2-GA，字节保持一致；新增的 MIDI 依赖另见下节。基础依赖的精确第三方源归档、补丁、配方、原构建信息、锁、crate、通知和来源索引继续使用 `Relaxer-V0.2.2-GA-Third-Party-Source.zip`。公开发行时将完整对应源码与二进制放在同一 Release，提供同等访问；仅上游主页链接不替代对应源码。未签发书面要约。

允许依各库许可证修改、调试和替换。ABI 兼容 DLL 可由拥有者主动执行完整源码中的 `scripts/ga01-update-manifest.py --package <目录> --acknowledge-modified-package` 更新校验清单；不需要厂商密钥或联网批准。应用修改须从同源重建应用、QML DLL 和根入口。没有新增禁止为调试库修改而逆向工程的条款；适用 LGPL/GPL 的源代码、通知、用户权利及必要重链接材料按实际组合处理。

本说明记录本次实际组件的许可选择、通知、源码提供与用户权利。基础依赖的原核对不覆盖新增专有 MIDI 依赖的 GPL 兼容性；该兼容性仍未确认。实际上传发布状态以 GitHub Release 为准。

音乐、封面、歌词和第三方服务内容不属于本项目软件许可授权范围。

## 本次分发采用的许可路线

以下选择针对当前实际分发组合；独立文件的原许可、版权及例外继续保留。完整源码侧包是对应源码交付，不能代替二进制随附通知。

|实际组件/文件范围|本次处理及接收者权利|可核验材料|
|---|---|---|
|Qt 6.11.2 库及随包插件|对具有 LGPL-3.0 选项的 Qt 文件使用该选项，保留 GPL/LGPL 全文及 Qt 版权；独立第三方文件按自身授权处理。共享库可由用户修改、调试、替换；完整应用源码也可重建。|COMPONENTS.json 的精确 PE/包身份；源侧包 components 的原 Qt 源/补丁/配方及 provenance/binary-packages 的 BUILDINFO；licenses 中原文与归属。|
|Qt Base 的 qgrayraster、qimagescale、Unicode、PSL|qgrayraster 选择 FTL 并保留 FreeType 致谢；qimagescale 保留 BSD-2-Clause 与 Imlib2 双重条件；Unicode 按 Unicode-3.0。PSL 的 MPL-2.0 数据与 BSD 查询/生成器分开；原始 DAT、生成器及原表均提供。|licenses/ga/NOTICE-INDEX.json；源侧包 supplements/qt-psl 与精确 Qt Base 源。|
|QtQml 的 masm/Yoga 等|保留每个非空候选源文件的原版权及 LGPL/GPL 选项、BSD 或 MIT 声明；不以 Qt 总许可覆盖它们。空 LLIntData.h 不构成缺失授权的代码。|licenses/ga/static/qtqml-source-notices.txt 与 Qt Declarative 精确源；其他资源的原 Qt attribution 和许可原文。|
|FFmpeg 9.0.2-1 及随包工具/库|实际 configure 含 --enable-gpl、--enable-version3、--enable-shared，无 --enable-nonfree，按 GPL-3.0-or-later 中的第 3 版履约。提供完整对应源、补丁、配方及依赖来源，保留各依赖的独立条款。|COMPONENTS.json；源侧包 components/mingw-w64-ffmpeg-9.0.2-1 与原 BUILDINFO。|
|shaderc 2026.4-1 / glslang / SPIRV|保留 Apache/BSD/MIT 原文、NOTICE、精确静态构建输入和修改配方。glslang Bison 解析器的 GPL-3.0-or-later 及特别例外一并保留；本组合提供 GPLv3 完整对应源码，不依赖例外来省略解析器源。未将 CC-BY 文档或未分发工具算作运行代码。|源侧包 static-build-source-supplement、shaderc 的 components/recipe 及 licenses/ga/static。|
|librsvg 2.63.2-1 / Rust 嵌入代码|librsvg 保留 LGPL-2.1-or-later 及允许后续版本的选择；本 GPLv3 组合保留 LGPL 用户权利。MPL-2.0 covered source 以原许可提供，并在 GPLv3 组合所需范围采用 MPL 2.0 §3.3 允许的 Secondary License 路线。所核实际 MPL 源未声明 Incompatible With Secondary Licenses。其他 crate 按各自许可；不重新许可全部 crate。|源侧包 rust 的精确归档/锁及 mapping 的 CI/目标图；运行包 crate 通知和 Rust 标准库版权。|
|GCC/libstdc++/libgcc/libgomp 与静态启动代码|保留 GPLv3 及 GCC Runtime Library Exception；应用本身即按 GPLv3 提供完整对应源，本次不依赖例外去隐藏源代码。编译工具/文档不因提供源归档成为运行库。|原 GCC 源/配方/BUILDINFO；licenses/startup 及原运行库通知。|
|OpenAL、FreeType、IJG、其余实际共享/嵌入组件|OpenAL 包级 GPL-2.0-or-later 与源内 Library GPL v2 声明均保留，不误称 GPL-2.0-only 冲突。FreeType 可选文件使用 FTL 并保留其他独立许可；IJG 致谢如上。其余按精确组件/源文件原声明保留通知、修改配方与所需源码；不将包级汇总当单文件再许可。|COMPONENTS.json；源侧包 SOURCE-INDEX.json、components、notices、mapping；运行包完整通知索引。|

库源码若经 MSYS2 修改，随包提供原上游归档、独立命名的原补丁和执行修改的 PKGBUILD，保持修改与上游的区别。shaderc 原配方具体修改 libshaderc_util/CMakeLists.txt 的链接顺序、顶层 CMakeLists.txt 的开发/测试子目录、glslc/CMakeLists.txt 的版本生成步骤，并生成 glslc/src/build-version.inc；这些构建修改不被表述为 Relaxer 修改了运行期算法。OpenCC 的作者、原字典及改编声明单独保留。

原始 librsvg Cargo.lock 未取得；重放锁按重放材料标识。原 CI 的实际编译版本、registry checksum、647 个精确 crate 和配方支撑来源/构建依赖对应；该限制影响原构建的逐字节复现，未发现因此缺失的特定 covered source 或用户修改权利，不以补造旧锁或更新 crate 代替原件。

MPL covered source 和所有适用 GPL/LGPL 对应源码的获取方式：在同一 GitHub Release 下载完整应用源码及第三方源码两个附件，链接入口为 https://github.com/QUEQIAN1961/Relaxer/releases/tag/v0.2.4-GA 。二进制公开时必须同时提供这两份附件及校验值；GitHub 自动源码归档不包含全部第三方材料。此为网络同址提供方式，不使用书面要约。

## MIDI 依赖

Relaxer 使用 FluidSynth 2.6.2（LGPL-2.1-or-later）。V0.2.7-RC 便携包使用 MSYS2 libsndfile 1.2.2（LGPL-2.1-or-later）及动态编解码库支持 SF3。确切版本、摘要及来源见 `runtime/runtime-midi.json`，新增组件的原文许可位于 `runtime/licenses/midi/msys2/`，对应源码及构建配方随 MIDI 源码附件提供。构建 FluidSynth 所用 GCEM 数学头文件为 MIT 许可。GeneralUser GS 2.0.3（S. Christian Collins）按作者独立许可使用，原文位于 `runtime/licenses/midi/`。MIDI PCM 由 Qt 输出。

V0.2.7-RC 携带一个基于 Qt Multimedia 6.11.2 修改的 FFmpeg 插件，按 GPL-3.0-only 许可发布。对应的原始 MSYS2 源码包、构建配方、Relaxer 修改与许可原文见同版 Qt 多媒体源码附件及 `docs/third-party/qt-peq/`。

来源、固定版本、准备方式及对应源码链接见 [docs/MIDI-BASS.md](docs/MIDI-BASS.md) 和 `resources/midi-dependencies.json`；分发须满足 LGPL 的原文许可、对应源码与重新链接要求，既有第三方源码附件不自动涵盖新增版本。

## 歌单标记与界面图标

V0.2.5-RC 的 48 个歌单标记为 Relaxer 独立重绘的矢量图，遵循项目许可证。较早版本曾使用 Lucide 0.468.0 / Feather 派生图形；历史版权及 ISC、MIT 许可继续保留在 `licenses/playlist-marks/LICENSE.txt` 和资源目录内。

界面的返回箭头、大头钉及下载状态图形参考 Morphicons 所用的 Lucide 开源图形，并按 Relaxer 的尺寸、线宽和动画规则适配。原图形的 ISC / MIT 声明保留，不重新许可为 GPL。来源：https://github.com/lucide-icons/lucide/tree/0.468.0/icons ，https://www.morphicons.com/ 。

## wiliwili 接口适配（V0.2.6-RC）

哔哩哔哩原生适配参考并改编 xfangfang / 贡献者的 wiliwili，固定提交 88e5876bea9502d06f46a8656e3530684d3aaf7d，GNU GPL v3。逐文件来源、作者说明、修改者/日期、采用范围及对应源码安排见根 REFERENCES.md；保留的原文见 docs/third-party/wiliwili/LICENSE。未引入其 UI、字体、图标、CPR、borealis、libmpv 或预编译依赖。致谢不能替代 GPL 的声明、许可与精确对应源码义务。


## 原生音乐平台接口

汽水/酷狗原生接口的独立 MIT 归属、许可原文与本次改编范围见 `docs/third-party/music-platforms/README.md` 及 `REFERENCES.md` 的平台补充。新汽水来源为 GSQZ/qishui-music-api 0.2.2（Copyright (c) 2026 sayqz，MIT）；协议与签名改写为 Qt/Windows CNG，未复制 AGPL Folia 的应用代码。三份平台 MIT 原文也嵌入“开源许可与致谢”。本地新改动公开发行时需随同匹配的完整应用源码提供。

## Phase G 的 FFmpeg DASH 取消修复

当前 V0.2.6-RC 本地构建额外采用已修改的 FFmpeg 9.0.2 avformat-63.dll，只修复 DASH 取消时的错误返回，保持原版权与 GPL-3.0-or-later 许可路线。来源、补丁、修改日期、精确源码及重建方式见 docs/third-party/ffmpeg/README.md，构建身份见 runtime-ffmpeg.json。公开发行时必须附匹配的新组件源码/补丁/配置与许可证，不沿用旧 GA 二进制身份。
