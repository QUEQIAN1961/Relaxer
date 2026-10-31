# GA-01 第三方许可矩阵

日期：2026.10.31。范围：实际工程包 323 个 PE 文件中，320 个第三方 PE 对应 101 个 MSYS2 软件包；另外 3 个为冻结的 Relaxer 构建。包含 QML、插件及 runtime/bin 中的工具依赖。

**来源资料核对 PASS；整体再分发许可门 OPEN / 有具体 BLOCKED 项。** 没有仅凭许可文本或下载链接宣布合规。所有表格路径相对于本次交付根目录；源控内报告为同内容快照。

## 已检查的证据

- 101/101 二进制归属匹配官方二进制包的 `.MTREE` SHA-256，101/101 二进制签名通过。
- 101/101 `.BUILDINFO` 中 `pkgbuild_sha256sum` 精确匹配对应源码包的 PKGBUILD；98 个独立源码归档及签名全部通过。
- `.SRCINFO` 列出的全部 SHA-256 / SHA-512 载荷摘要均找到匹配文件；未运行任何配方，也未更新系统。
- 签名依据为现有 MSYS2 公钥环的隔离只读副本，指纹和原始 gpgv 结果逐项留存；不是重新建立独立信任根。
- 637/637 crates 与原始 Cargo.lock 的 SHA-256 匹配。它们是含可选/测试/其他平台依赖的超集，不能当作最终编译列表。
- 91 份 Qt 上游归属记录及引用的许可证材料已经整理，缺失引用文件 0。新增 1,390 个补充通知文件进入工程包。

机器可查的九类义务见 `THIRD-PARTY-OBLIGATIONS.json`：精确版本及修订、许可证判断、源码、补丁、配方/配置、通知、源码提供路线、替换信息、未关闭项。原始证据见 `third-party/component-evidence.json`、`source-validation.json` 和 `signature-logs/`。

## 实际二进制对应矩阵

表中“来源 PASS”仅指二进制、签名、源码归档、配方与载荷的一致性。右列是许可综合状态；本轮不把这些子项通过换算成“101 个包已全部合规”。软件包许可证是汇总声明，不能替代文件级范围判断。

| 组件（均为 ucrt64 x86_64） | MSYS2 精确版本 | 许可判断 | 来源 | 许可综合状态/待办 |
|---|---|---|---|---|
| aom | 3.15.2-1 | BSD-2-Clause | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| brotli | 1.2.0-1 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| bzip2 | 1.0.8-4 | custom | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| cairo | 1.18.6-2 | LGPL-2.1-or-later OR MPL-1.1 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| dav1d | 1.5.4-1 | BSD-2-Clause | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| double-conversion | 3.4.0-1 | BSD-3-Clause | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| expat | 2.9.0-1 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| ffmpeg | 9.0.2-1 | GPL-3.0-or-later（实际 --enable-gpl --enable-version3） | PASS | BLOCKED：传递 Rust 库有效锁文件/编译来源未闭合 |
| fontconfig | 2.18.3-1 | custom | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| freetype | 2.14.3-1 | GPL-2.0-or-later OR FTL | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| fribidi | 1.0.17-1 | LGPL-2.1-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| gdk-pixbuf2 | 2.44.8-1 | LGPL-2.1-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| gettext-runtime | 1.0-1 | GPL-3.0-or-later AND LGPL-2.1-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| ggml | 0.26.0-1 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| glib2 | 2.90.1-1 | LGPL-2.1-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| gmp | 6.3.0-2 | LGPL3; GPL | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| gnutls | 3.8.13-3 | GPL-3.0-or-later; LGPL-2.1-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| graphite2 | 1.3.15-1 | LGPL-2.1-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| gsm | 1.0.24-1 | custom | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| harfbuzz | 14.6.0-1 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| highway | 1.4.0-3 | Apache-2.0 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| icu | 78.3-4 | ICU | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| jasper | 4.2.9-3 | JasPer-2.0 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| jbigkit | 2.1-6 | GPL-2.0-or-later，源码 libjbig/jbig.c 明确含 later；可选 GPLv3 | PASS | OPEN：已纠正软件包 GPL-2.0 简写；最终源码提供方式待闭合 |
| lame | 3.100-3 | LGPL | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| lcms2 | 2.19.1-1 | MIT AND GPL-3.0-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| lerc | 4.1.1-1 | Apache-2.0 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libass | 0.17.5-1 | ISC | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libb2 | 0.98.1-3 | custom:CC0 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libbluray | 1.5.0-1 | LGPL-2.1-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libcaca | 0.99.beta20-8 | LGPL | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libdatrie | 0.2.14-1 | LGPL | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libdeflate | 1.26-1 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libdovi | 3.4.0-1 | MIT | PASS | OPEN：原锁文件 crates 已补齐；实际编译 feature / notice 范围未闭合 |
| libffi | 3.8.0-1 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libgcc | 16.2.0-5 | GPL-3.0-or-later WITH GCC-exception-3.1 AND GFDL-1.3-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libgme | 0.6.5-2 | LGPL | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libgomp | 16.2.0-5 | GPL-3.0-or-later WITH GCC-exception-3.1 AND GFDL-1.3-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libiconv | 1.19-1 | LGPL-2.1-or-later; documentation:GPL-3.0-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libidn2 | 2.3.8-4 | GPL-2.0-or-later; LGPL-3.0-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libjpeg-turbo | 3.2.0-1 | custom:BSD-like | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libjxl | 0.12.0-1 | BSD-3-Clause | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| liblc3 | 1.1.3-1 | Apache-2.0 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libmng | 2.0.3-7 | Zlib | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libmodplug | 0.8.9.0-5 | PublicDomain | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libogg | 1.3.6-1 | BSD-3-Clause | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libplacebo | 7.360.1-2 | LGPL2.1 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libpng | 1.6.59-1 | custom | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| librsvg | 2.63.2-1 | LGPL-2.1-or-later | PASS | BLOCKED：配方 cargo update 后的有效 Cargo.lock 与构建范围缺证据 |
| libsoxr | 0.1.3-6 | LGPL-2.1-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libssh | 0.12.2-1 | LGPL-2.1-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libstdc++ | 16.2.0-5 | GPL-3.0-or-later WITH GCC-exception-3.1 AND GFDL-1.3-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libtasn1 | 4.21.0-1 | GPL3, LGPL | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libthai | 0.1.30-1 | LGPL-2.1-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libtheora | 1.2.0-1 | BSD-3-Clause | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libtiff | 4.7.2-1 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libunibreak | 7.0-1 | Zlib | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libunistring | 1.4.2-1 | LGPL-3.0-or-later OR GPL-3.0-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libva | 2.24.1-1 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libvorbis | 1.3.7-3 | custom | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libvpl | 2.17.0-1 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libvpx | 1.17.0-1 | BSD-3-Clause | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libwebp | 1.6.0-1 | BSD-3-Clause | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libwinpthread | 14.0.0.r426.g4564ee4b5-1 | MIT AND BSD-3-Clause-Clear | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libx264 | 0.165.r3222.b35605a-3 | custom | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| libxml2 | 2.15.4-1 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| md4c | 0.6.0-1 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| nettle | 4.0-1 | GPL-2.0-or-later; LGPL-3.0-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| openal | 1.25.2-1 | 软件包标 GPL-2.0-or-later；上游 COPYING 为 Library GPL v2，须按代码范围确认 | PASS | OPEN：包汇总与源文件许可范围不能混为一谈 |
| opencore-amr | 0.1.6-1 | Apache | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| openjpeg2 | 2.5.4-2 | BSD-2-Clause | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| openssl | 3.6.5-1 | Apache-2.0 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| opus | 1.6.1-1 | BSD-3-Clause | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| p11-kit | 0.26.5-1 | BSD-3-Clause | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| pango | 1.58.2-1 | LGPL-2.1 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| pcre2 | 10.49-1 | BSD-3-Clause | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| pixman | 0.46.4-3 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| qt6-base | 6.11.2-2 | 运行时代码含 LGPL-3.0-only / GPL-3.0-only 等选项；第三方与工具单列 | PASS | OPEN：逐模块许可选择、静态第三方范围与实际替换验收 |
| qt6-declarative | 6.11.2-1 | 运行时代码含 LGPL-3.0-only / GPL-3.0-only 等选项；第三方与工具单列 | PASS | OPEN：逐模块许可选择、静态第三方范围与实际替换验收 |
| qt6-imageformats | 6.11.2-1 | 运行时代码含 LGPL-3.0-only / GPL-3.0-only 等选项；第三方与工具单列 | PASS | OPEN：逐模块许可选择、静态第三方范围与实际替换验收 |
| qt6-multimedia | 6.11.2-1 | 运行时代码含 LGPL-3.0-only / GPL-3.0-only 等选项；第三方与工具单列 | PASS | OPEN：逐模块许可选择、静态第三方范围与实际替换验收 |
| qt6-multimedia-ffmpeg | 6.11.2-1 | 运行时代码含 LGPL-3.0-only / GPL-3.0-only 等选项；第三方与工具单列 | PASS | OPEN：逐模块许可选择、静态第三方范围与实际替换验收 |
| qt6-svg | 6.11.2-1 | 运行时代码含 LGPL-3.0-only / GPL-3.0-only 等选项；第三方与工具单列 | PASS | OPEN：逐模块许可选择、静态第三方范围与实际替换验收 |
| rav1e | 0.8.1-1 | BSD-2-Clause | PASS | OPEN：原锁文件 crates 已补齐；实际编译 feature / notice 范围未闭合 |
| rtmpdump | 2.6-2 | GPL-2.0-or-later; LGPL-2.1-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| shaderc | 2026.4-1 | Apache-2.0 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| speex | 1.2.1-1 | BSD | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| spirv-cross | 1~1.4.363.0-1 | Apache-2.0 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| sqlite3 | 3.54.0-1 | PublicDomain | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| srt | 1.5.7-1 | MPL-2.0 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| svt-av1 | 4.2.0-1 | BSD-3-Clause-Clear | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| vid.stab | 1.1.2-1 | GPL-2.0-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| vulkan-loader | 1~1.4.363.0-1 | Apache-2.0 | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| whisper.cpp | 1~1.9.4.r183.g60c0be6ac-1 | MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| x265 | 4.3-1 | GPL | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| xvidcore | 1.3.7-5 | GPL | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| xz | 5.8.4-1 | 0BSD AND LGPL-2.1-or-later AND GPL-2.0-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| zimg | 3.0.6-3 | WTFPL | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| zlib | 1.3.2-2 | Zlib | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| zstd | 1.5.7-2 | BSD-3-Clause OR GPL-2.0-or-later | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |
| zvbi | 0.2.45-1 |  GPL-2.0-or-later AND BSD-2-Clause AND LGPL-2.1-or-later AND MIT | PASS | OPEN：通知与精确源码已收集；按实际链接范围及分发方式完成最终许可核对 |

## 关键判断

1. **FFmpeg 9.0.2-1**：实际配置含 `--enable-gpl --enable-version3 --enable-shared`，不是纯 LGPL 构建。完整命令与 `-L` 结果已留存。Relaxer 自有代码 GPL-3.0-only 不会把依赖重新授权；依赖可用 GPLv3 路线仍须逐项履行对应源码义务。
2. **Qt 6.11.2**：base 为 `-2`，declarative / imageformats / multimedia（含 ffmpeg 子包）/ svg 为 `-1`。实际源码同时存在 LGPL/GPL 选项、GPL-only 工具/测试与第三方代码。不能把包级 `AND` 汇总误读为每个 DLL 必须遵守其中每项，也不能笼统宣称全是 LGPL。最终应记录实际分发模块采用的许可选项。
3. **Qt LGPL 替换**：根启动器不固定 Qt 文件的厂商签名；用户主动替换第三方 DLL 并重建清单后能够启动。已验证的是合法 PE 加 overlay 后重新校验的机制，**不是**实际重建 Qt 的 ABI 验收。修改 Relaxer 自身还需同源重建应用、QML DLL 和根启动器；源码与操作信息无密钥障碍。
4. **GCC / MinGW**：原始运行库版权与 GCC exception 3.1 保留；精确 gcc / winpthreads 源码和构建配方已收集。例外只适用于其范围，不免除其他组件义务。
5. **Rust 组件**：librsvg 配方执行 `cargo update -p windows-sys@0.61.2 --precise 0.60.2`。原始锁文件及 637 个依赖归档不能证明更新后的实际锁文件。rav1e、libdovi 的原始锁文件已覆盖，但最终 feature / 静态依赖/归属范围仍需确认。
6. **嵌入资料**：OpenCC 的 3,881 对映射均与固定上游引用的首选映射相同，上游另有 131 个键；这不证明最初生成所用修订。Apache-2.0 和真实项目归属已补充，原始生成来源与显著改编声明仍 OPEN。未虚构不存在的上游 NOTICE。
7. 637 个 crate 中有 22 个没有被本轮规则找到独立的 LICENSE/COPYING 文件；不能认为它们没有许可证，也不能认为现有通知已经覆盖。清单中保留作者、许可声明和源代码，下一步核对 README/源文件头中的实际归属。

## 关闭条件

取得原始构建的有效 lockfile、静态依赖/feature 和对应通知证据，或在独立工程中进行可追溯的同版本受控重建；核对逐模块许可及 OpenCC 改编来源；选择并落实同址完整源码下载路线。任意二进制替换均需新包身份、完整性与相关回归复验。

公开参考： [Qt LGPL 义务](https://www.qt.io/licensing/open-source-lgpl-obligations)、[FFmpeg 许可](https://ffmpeg.org/legal.html)、[GCC runtime exception](https://www.gnu.org/licenses/gcc-exception-3.1.html)、[MSYS2 源码归档](https://repo.msys2.org/mingw/sources/)。网页和原始许可证副本/摘要随审计材料保存；GNU 文本网页两次 TLS 超时不影响已取得的精确 Qt 源码 LICENSES 中的 GPL/LGPL 文本。
