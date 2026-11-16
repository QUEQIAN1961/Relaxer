# GA-01 对应源码与构建资料

已收集 98 个精确 MSYS2 源码归档，合计 1,353,978,463 字节；101 个二进制软件包（gcc 三个运行库共用源码、Qt Multimedia 两个子包共用源码）逐项绑定。另有 637 个按 Cargo.lock 校验的 crate，83,264,418 字节。源码归档、补丁、配方、`.SRCINFO`、`.PKGINFO`、`.BUILDINFO`、`.MTREE`、签名和公钥快照全部保留。

完整材料在 `Relaxer-GA01-Third-Party-Source-Materials.zip`。其中 `third-party/source-archives/` 是官方归档，`recipes/` 是从中读出的原配方/补丁，`package-evidence/` 是实际二进制包的构建元数据；不要用上游最新版本或旧 FFmpeg 7.1 代替这些资料。Rust 依赖的完整性限制见报告 01。

## 恢复与核对

1. 验证交付 SHA256SUMS，再验证源码包和二进制包的官方签名。签名日志中有完整 signer fingerprint。
2. 用二进制 `.MTREE` 对照 `COMPONENTS.json` 的文件 SHA256；用 `.BUILDINFO` 中的 PKGBUILD SHA256 对照源码包；按 `.SRCINFO` 校验载荷。
3. 在新的独立 MSYS2 构建环境中恢复 `.BUILDINFO` 所列工具链/依赖版本，使用原归档和补丁。不要向本机执行系统级 pacman 更新，也不要在正式仓库构建或写入个人资料。
4. 配方入口为每个源码包的 PKGBUILD；在已备齐依赖的隔离环境使用 `MINGW_ARCH=ucrt64 makepkg-mingw --cleanbuild --nodeps`。这是复建说明，本轮没有执行这些第三方构建。
5. Qt 的 configure/CMake 参数、FFmpeg 的双 static/shared 构建参数、下游 patch 均在各精确 PKGBUILD 中。FFmpeg 运行时导出的真实配置同时存于 `logs/native-finish-ga01-ffmpeg-config.stdout.log`，不能用宿主 gcc 版本代替该包 `.BUILDINFO` 的编译器修订。

本轮独立重建的是冻结的 Relaxer 产品并跑 93 项测试；分发工程包复用已验收 TextFix 的原始二进制，逐字节核对，**没有声称重建 Qt/FFmpeg 获得相同二进制**。对这些依赖的构建完成度为：原构建配置已收集，受控重建未执行。

## 最终源码提供方案

计划选 GPLv3 §6(d) 的同址、同等访问方式提供完整对应源码，随每个最终二进制包发布固定的源码、补丁、配方与恢复索引，维护精确摘要和版本关联。不得只放上游主页链接。本轮只在本地提供工程资料，没有建立公开源码下载服务，也没有签发未来书面要约。若改用书面要约，必须另行确认适用条款、期限、对象、成本与实际履约能力，不能把一个 README 当作已履行的承诺。

## Qt 替换与重新构建

第三方 ABI 兼容库可由包拥有者主动替换，然后运行：

```
python scripts/ga01-update-manifest.py --package "绝对包目录" --acknowledge-modified-package
```

工具保留原清单备份，拒绝 reparse point；不在正常启动时运行，不认证修改后的来源。此后官方包的哈希证明不再适用。它提供用户修改的可行路径，不削弱普通启动仍执行全量校验的行为。

修改应用代码时，用同一源码重建 runtime/Relaxer.exe、RelaxerQmlUi.dll 和根 Relaxer.exe（构建目标 RelaxerPortable），随后重建清单。没有必须向发行者索取的签名密钥或激活信息。需另做实际重建 Qt 的替换测试，以及适用 LGPL/GPL 安装信息检查。

## 封存补充：shaderc 静态构建来源

shaderc 2026.4-1 的原始 `.BUILDINFO` 绑定 glslang 16.3.0-1、spirv-headers 2~1.4.363.0-1、spirv-tools 3~1.4.363.0-1；配方明确移除 vendored third_party 并使用已安装组件。不能以本机当前 glslang 16.6.0 替代。已追加收集这 3 个精确版本的官方源码归档，签名全部通过，合计 8,300,239 字节，位于来源侧包 `third-party/static-source-supplement/`。它们是原 98 个 PE 归属来源包之外的静态/构建材料，不增加实际分发的 DLL 个数。

这 3 个补充包本轮完成的是精确版本归档获取与签名验证；没有将它们冒充为原 101 个包已经执行的二进制 `.MTREE` / `.BUILDINFO→PKGBUILD` 绑定。原始链接对象、编译范围和通知范围仍 OPEN；详情在源控 `STATIC-LINK-AUDIT.json`、审计 `evidence/static-link-findings.json`。原 98 归档、101 二进制包的来源结果保持不变。
