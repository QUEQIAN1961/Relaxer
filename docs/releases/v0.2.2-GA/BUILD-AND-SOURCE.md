# V0.2.2-GA 构建与对应源码

本版基于冻结产品 1acde7532ebc34aaa0fa07653798d62cffe64f11 与 GA02 工程 37c00d330833aa65b0e0152d659e86b65b43487a。仅整理版本资源、版本显示、更新日志、文档、打包及许可材料，不改变播放器业务、数据格式或完整性校验算法。

Windows Release 构建使用 CMake/Ninja 与 MSYS2 UCRT64。Qt Base/Multimedia/其他模块及 Declarative 需版本一致；完整应用、QML DLL、便携根入口从同一源码指纹生成。`cmake --build build --parallel` 构建应用及测试，`ctest --test-dir build --output-on-failure` 执行测试。`RelaxerStart` 等源码准备工具不放入普通用户包。

`scripts/package-ga.py` 从原已验证 GA01 用户包保留第三方运行文件，替换本次实际编译的 Relaxer.exe、RelaxerQmlUi.dll 与根便携 EXE，重生原有四线程校验使用的 PORTABLE-SHA256SUMS，不新增启动机制。原版打包脚本和历史记录保持不变。

配套 Third-Party-Source ZIP 提供精确组件源码归档、原补丁/PKGBUILD/BUILDINFO、647 crate、原/重放锁（重放锁不冒充原件）、OpenCC 数据与生成材料、通知和来源摘要。SOURCE-INDEX/COMPONENTS 绑定实际库版本。公共发行时应将对应完整源码与二进制置于同一 Release 提供同等访问，不能只提供上游主页链接；本轮未签发书面要约。

允许按各自许可证修改/调试/替换库。对 ABI 兼容 DLL，拥有者可用 `scripts/ga01-update-manifest.py --package <目录> --acknowledge-modified-package` 主动更新完整性清单；不需要厂商密钥/联网批准。修改应用时从同源重建应用、QML DLL、根入口并重生清单。适用 LGPL 条件按相应库版本/组合保留，不把所有库都机械要求同一组目标文件。

本次完整重建的实际命令、工具版本、源提交、文件摘要与验证边界随最终发行报告提供。历史风险保留于独立内部记录；不以本说明宣称已取得法律保证或已公开发布。
