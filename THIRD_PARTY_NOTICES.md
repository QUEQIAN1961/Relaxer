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

本说明记录材料与权利，不构成法律保证，也不以文本存在宣称所有适用性问题已经解决。当前公开发行决定见 BUILD-IDENTITY.json 与同批最终发行报告。

音乐、封面、歌词和第三方服务内容不属于本项目软件许可授权范围。
