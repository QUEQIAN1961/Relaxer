# 第三方版权、许可与对应源码

Relaxer 自有代码采用 GPL-3.0-only。第三方代码、数据、字体（如实际提供）和各自例外保留原授权与版权，不被统一重新许可。

本 Windows 构建使用 Qt 6.11.2、Qt Multimedia FFmpeg 后端及 QML 插件，FFmpeg/ffprobe 9.0.2 和其实际共享/嵌入依赖。FFmpeg 的实际 MSYS2 构建启用 GPL 与 version3，按 GPL-3.0-or-later 路线提供材料，不能把整个包称为纯 LGPL。精确软件包版本、320 个第三方 PE 的 SHA-256 及所有者见 COMPONENTS.json 和配套对应源码索引。

## 版权及许可证材料

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

同批 `Relaxer-V0.2.2-GA-Source.zip` 提供完整应用源码与打包/重建脚本；`Relaxer-V0.2.2-GA-Third-Party-Source.zip` 提供对应的精确第三方源归档、补丁、配方、原构建信息、锁、crate、通知和来源索引。公开发行时将完整对应源码与二进制放在同一 Release，提供同等访问；仅上游主页链接不替代对应源码。未签发书面要约。

允许依各库许可证修改、调试和替换。ABI 兼容 DLL 可由拥有者主动执行完整源码中的 `scripts/ga01-update-manifest.py --package <目录> --acknowledge-modified-package` 更新校验清单；不需要厂商密钥或联网批准。应用修改须从同源重建应用、QML DLL 和根入口。没有新增禁止为调试库修改而逆向工程的条款；适用 LGPL/GPL 的源代码、通知、用户权利及必要重链接材料按实际组合处理。

本说明记录本次实际组件的许可选择、通知、源码提供与用户权利。工程核对未发现仍未履行的具体发行义务；这不是专业法律意见。实际上传发布状态以 GitHub Release 为准。

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

MPL covered source 和所有适用 GPL/LGPL 对应源码的获取方式：在同一 GitHub Release 下载完整应用源码及第三方源码两个附件，链接入口为 https://github.com/QUEQIAN1961/Relaxer/releases/tag/v0.2.2-GA 。二进制公开时必须同时提供这两份附件及校验值；GitHub 自动源码归档不包含全部第三方材料。此为网络同址提供方式，不使用书面要约。
