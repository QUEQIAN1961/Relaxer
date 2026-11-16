# GA-02 静态传递依赖与许可矩阵

101 个实际 PE 所有者的直接来源结论沿用 GA-01。新增核对其配方、static/shared 选项、功能开关、去 vendoring 处理及原构建环境；所有 4,124 条 PE 导入边已映射至具体包/Windows System。机器链条见 `final-binary-dependency-edges.json` 与 `THIRD-PARTY-OBLIGATIONS-GA02.json`。

每条链采用：**二进制及 SHA → 包/修订 → 动态导入或静态源组证据 → 许可声明及文件通知 → 精确源码包/补丁 → 原配方和 BUILDINFO**。动态 PE 导入是链接证据；配方出现 static variant、某包已安装、源码中有 third_party 目录，都不单独证明该代码实际纳入最终 DLL。

## 101 包逐项范围

| 包 | 修订 | PE 数 | 动态依赖摘要（完整列表在 JSON） | 静态审查结论 |
|---|---|---:|---|---|
| aom | 3.15.2-1 | 2 | libwinpthread | 源码组/通知清点；静态范围 OPEN |
| brotli | 1.2.0-1 | 6 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| bzip2 | 1.0.8-4 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| cairo | 1.18.6-2 | 4 | fontconfig, freetype, glib2, libgcc, libpng, libstdc++, pixman, zlib | 配方/导入已查；静态范围 OPEN |
| dav1d | 1.5.4-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| double-conversion | 3.4.0-1 | 1 | libgcc, libstdc++ | 配方/导入已查；静态范围 OPEN |
| expat | 2.9.0-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| ffmpeg | 9.0.2-1 | 14 | aom, bzip2, cairo, dav1d, fontconfig, freetype, fribidi, ggml, glib2, gnutls, gsm, harfbuzz, lame, libass, libbluray, libcaca, libgcc, libgme, libiconv, libjxl, liblc3, libmodplug, libplaceb | 源码组/通知清点；静态范围 OPEN |
| fontconfig | 2.18.3-1 | 2 | expat, freetype, gettext-runtime, libgcc | 配方/导入已查；静态范围 OPEN |
| freetype | 2.14.3-1 | 2 | brotli, bzip2, harfbuzz, libpng, zlib | 源码组/通知清点；静态范围 OPEN |
| fribidi | 1.0.17-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| gdk-pixbuf2 | 2.44.8-1 | 2 | gettext-runtime, glib2, libjpeg-turbo, libpng, libtiff | 配方/导入已查；静态范围 OPEN |
| gettext-runtime | 1.0-1 | 2 | libiconv | 配方/导入已查；静态范围 OPEN |
| ggml | 0.26.0-1 | 2 | libgcc, libgomp, libstdc++, libwinpthread | 源码组/通知清点；静态范围 OPEN |
| glib2 | 2.90.1-1 | 8 | gettext-runtime, libffi, pcre2, zlib | 配方/导入已查；静态范围 OPEN |
| gmp | 6.3.0-2 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| gnutls | 3.8.13-3 | 2 | brotli, gettext-runtime, gmp, libidn2, libtasn1, libunistring, libwinpthread, nettle, p11-kit, zlib, zstd | 配方/导入已查；静态范围 OPEN |
| graphite2 | 1.3.15-1 | 2 | libgcc, libstdc++ | 配方/导入已查；静态范围 OPEN |
| gsm | 1.0.24-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| harfbuzz | 14.6.0-1 | 2 | freetype, glib2, graphite2, libgcc | 配方/导入已查；静态范围 OPEN |
| highway | 1.4.0-3 | 2 | libgcc, libstdc++ | 配方/导入已查；静态范围 OPEN |
| icu | 78.3-4 | 3 | libgcc, libstdc++, libwinpthread | 源码组/通知清点；静态范围 OPEN |
| jasper | 4.2.9-3 | 1 | libjpeg-turbo, libwinpthread | 配方/导入已查；静态范围 OPEN |
| jbigkit | 2.1-6 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| lame | 3.100-3 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| lcms2 | 2.19.1-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| lerc | 4.1.1-1 | 2 | libgcc, libstdc++ | 配方/导入已查；静态范围 OPEN |
| libass | 0.17.5-1 | 1 | fontconfig, freetype, fribidi, harfbuzz, libiconv, libunibreak | 配方/导入已查；静态范围 OPEN |
| libb2 | 0.98.1-3 | 1 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libbluray | 1.5.0-1 | 2 | freetype, libxml2 | 配方/导入已查；静态范围 OPEN |
| libcaca | 0.99.beta20-8 | 1 | zlib | 配方/导入已查；静态范围 OPEN |
| libdatrie | 0.2.14-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libdeflate | 1.26-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libdovi | 3.4.0-1 | 1 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libffi | 3.8.0-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libgcc | 16.2.0-5 | 2 | libwinpthread | 源码组/通知清点；静态范围 OPEN |
| libgme | 0.6.5-2 | 2 | libstdc++, zlib | 配方/导入已查；静态范围 OPEN |
| libgomp | 16.2.0-5 | 2 | libwinpthread | 源码组/通知清点；静态范围 OPEN |
| libiconv | 1.19-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libidn2 | 2.3.8-4 | 2 | gettext-runtime, libiconv, libunistring | 配方/导入已查；静态范围 OPEN |
| libjpeg-turbo | 3.2.0-1 | 2 | Windows/自身包 | 源码组/通知清点；静态范围 OPEN |
| libjxl | 0.12.0-1 | 6 | brotli, highway, lcms2, libgcc, libstdc++, libwinpthread | 源码组/通知清点；静态范围 OPEN |
| liblc3 | 1.1.3-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libmng | 2.0.3-7 | 1 | lcms2, libjpeg-turbo, zlib | 配方/导入已查；静态范围 OPEN |
| libmodplug | 0.8.9.0-5 | 2 | libstdc++ | 配方/导入已查；静态范围 OPEN |
| libogg | 1.3.6-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libplacebo | 7.360.1-2 | 1 | lcms2, libdovi, libgcc, libstdc++, shaderc, spirv-cross, vulkan-loader | 源码组/通知清点；静态范围 OPEN |
| libpng | 1.6.59-1 | 2 | zlib | 配方/导入已查；静态范围 OPEN |
| librsvg | 2.63.2-1 | 2 | cairo, gdk-pixbuf2, glib2, libgcc, libxml2, pango | 配方/导入已查；静态范围 OPEN |
| libsoxr | 0.1.3-6 | 2 | libgomp | 配方/导入已查；静态范围 OPEN |
| libssh | 0.12.2-1 | 2 | libwinpthread, openssl, zlib | 配方/导入已查；静态范围 OPEN |
| libstdc++ | 16.2.0-5 | 2 | libgcc, libwinpthread | 源码组/通知清点；静态范围 OPEN |
| libtasn1 | 4.21.0-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libthai | 0.1.30-1 | 2 | libdatrie | 配方/导入已查；静态范围 OPEN |
| libtheora | 1.2.0-1 | 4 | libogg | 配方/导入已查；静态范围 OPEN |
| libtiff | 4.7.2-1 | 2 | jbigkit, lerc, libdeflate, libjpeg-turbo, libwebp, xz, zlib, zstd | 配方/导入已查；静态范围 OPEN |
| libunibreak | 7.0-1 | 1 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libunistring | 1.4.2-1 | 2 | libiconv | 配方/导入已查；静态范围 OPEN |
| libva | 2.24.1-1 | 4 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libvorbis | 1.3.7-3 | 4 | libogg | 配方/导入已查；静态范围 OPEN |
| libvpl | 2.17.0-1 | 2 | libgcc, libstdc++ | 配方/导入已查；静态范围 OPEN |
| libvpx | 1.17.0-1 | 2 | libwinpthread | 配方/导入已查；静态范围 OPEN |
| libwebp | 1.6.0-1 | 7 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libwinpthread | 14.0.0.r426.g4564ee4b5-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libx264 | 0.165.r3222.b35605a-3 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| libxml2 | 2.15.4-1 | 2 | libiconv, zlib | 配方/导入已查；静态范围 OPEN |
| md4c | 0.6.0-1 | 1 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| nettle | 4.0-1 | 4 | gmp | 配方/导入已查；静态范围 OPEN |
| openal | 1.25.2-1 | 1 | libgcc, libstdc++, libwinpthread | 源码组/通知清点；静态范围 OPEN |
| opencore-amr | 0.1.6-1 | 4 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| openjpeg2 | 2.5.4-2 | 2 | libgcc | 配方/导入已查；静态范围 OPEN |
| openssl | 3.6.5-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| opus | 1.6.1-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| p11-kit | 0.26.5-1 | 2 | gettext-runtime, libffi | 配方/导入已查；静态范围 OPEN |
| pango | 1.58.2-1 | 8 | cairo, fontconfig, freetype, fribidi, glib2, harfbuzz, libthai | 配方/导入已查；静态范围 OPEN |
| pcre2 | 10.49-1 | 3 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| pixman | 0.46.4-3 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| qt6-base | 6.11.2-2 | 15 | brotli, double-conversion, freetype, harfbuzz, icu, libb2, libgcc, libjpeg-turbo, libpng, libstdc++, libwinpthread, md4c, pcre2, sqlite3, zlib, zstd | 源码组/通知清点；静态范围 OPEN |
| qt6-declarative | 6.11.2-1 | 70 | libgcc, libstdc++, libwinpthread, qt6-base, qt6-svg | 源码组/通知清点；静态范围 OPEN |
| qt6-imageformats | 6.11.2-1 | 7 | jasper, libmng, libstdc++, libtiff, libwebp, qt6-base | 源码组/通知清点；静态范围 OPEN |
| qt6-multimedia | 6.11.2-1 | 1 | libgcc, libstdc++, libwinpthread, qt6-base | 源码组/通知清点；静态范围 OPEN |
| qt6-multimedia-ffmpeg | 6.11.2-1 | 1 | ffmpeg, libgcc, libstdc++, libwinpthread, qt6-base, qt6-multimedia | 源码组/通知清点；静态范围 OPEN |
| qt6-svg | 6.11.2-1 | 3 | libstdc++, qt6-base, zlib | 源码组/通知清点；静态范围 OPEN |
| rav1e | 0.8.1-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| rtmpdump | 2.6-2 | 2 | gmp, gnutls, nettle, zlib | 配方/导入已查；静态范围 OPEN |
| shaderc | 2026.4-1 | 1 | Windows/自身包 | 源码组/通知清点；静态范围 OPEN |
| speex | 1.2.1-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| spirv-cross | 1~1.4.363.0-1 | 1 | libgcc, libstdc++ | 配方/导入已查；静态范围 OPEN |
| sqlite3 | 3.54.0-1 | 1 | Windows/自身包 | 源码组/通知清点；静态范围 OPEN |
| srt | 1.5.7-1 | 2 | libgcc, libstdc++, libwinpthread, openssl | 配方/导入已查；静态范围 OPEN |
| svt-av1 | 4.2.0-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| vid.stab | 1.1.2-1 | 1 | libgomp | 配方/导入已查；静态范围 OPEN |
| vulkan-loader | 1~1.4.363.0-1 | 1 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| whisper.cpp | 1~1.9.4.r183.g60c0be6ac-1 | 1 | ggml, libgcc, libstdc++ | 源码组/通知清点；静态范围 OPEN |
| x265 | 4.3-1 | 2 | libstdc++ | 配方/导入已查；静态范围 OPEN |
| xvidcore | 1.3.7-5 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| xz | 5.8.4-1 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| zimg | 3.0.6-3 | 1 | libgcc, libstdc++ | 配方/导入已查；静态范围 OPEN |
| zlib | 1.3.2-2 | 2 | Windows/自身包 | 配方/导入已查；静态范围 OPEN |
| zstd | 1.5.7-2 | 2 | Windows/自身包 | 源码组/通知清点；静态范围 OPEN |
| zvbi | 0.2.45-1 | 2 | gettext-runtime, libiconv, libpng, libwinpthread | 配方/导入已查；静态范围 OPEN |

## 已确认的重点

- librsvg 的 Rust C API 档被链接进 DLL，原始 CI delta 和 177 个编译版本已印证重建有效依赖；原锁文件原件未取得，未冒称原件。
- rav1e 默认/capi 与 libdovi all-features 的原锁、来源和目标 metadata 已核对；依赖闭包不能替代最终链接/裁剪结果。
- FFmpeg 9.0.2-1 实际配置含 `--enable-gpl --enable-version3 --enable-shared`，因此按 GPL-3.0-or-later 路线审查；不是纯 LGPL 构建。4,124 条边能确认其明确的共享库依赖，不能单凭它们排除静态或内置编解码代码。
- shaderc 去 vendoring 后使用原构建 glslang 16.3.0-1、SPIRV-Tools/Headers 1.4.363.0 对应 epoch 修订。精确三个源包已保留；未用本机较新的 glslang 替换。原始静态链接对象清单仍未恢复。
- 对 19 个有明确 bundled/static 风险的来源包做定向源组/通知清点，列出 244 个候选组和 297 个许可/版权/通知文件；测试/示例与实际纳入状态分开，未把数量换算成合规通过。其余包也有逐包配方/动态链/来源记录，但不宣称已排除所有隐含静态代码。
- Qt 模块、QML 插件和传递依赖的包身份保持原修订。91 份精确上游归属记录与其许可全文继续保留。Qt 包级 AND 汇总不能当作每个文件适用的许可；最终模块许可选项/静态文件纳入仍 OPEN。

## 字体与资源

1,459 个实际松散资源全部找到包 MTREE 或冻结项目资源的 SHA256 对应。实际 Qt 模块/插件通过 QCoreApplication 的资源注册表检查：加载 78 个、失败 0、资源 3,603 个，其中字体扩展名文件 0；未启动播放器 GUI、未打开用户数据库。松散字体文件也为 0。

上游源码中发现的 47 个字体多为测试/示例资源，独立标记为 source-only，不能据此说运行包分发了这些字体。该探针覆盖已加载 Qt 资源，不自动覆盖所有非 Qt PE 的任意私有资源格式；未建立的范围仍如实保持 OPEN。

## GPL/LGPL 与用户权利

Relaxer 自有代码的 GPL-3.0-only 与第三方各自许可分开保留。GPL-2.0-or-later 等存在版本选择时应记录实际选择，不能把包级 `GPL-2.0` 简写当作 GPL-2.0-only 冲突；GA-01 已从 jbig.c 文件头确认 later 选项，本轮沿用该证据。未发现本轮新增的确证必然不兼容组件，但尚不能据此排除全部文件级问题。

Qt/LGPL 替换机制沿用 GA-01 已验证的共享库/拥有者主动重建清单路径；没有厂商密钥要求，所有应用重建源码与脚本另附。原验证为 PE overlay 替换机制，不是重建 Qt 的 ABI 验收。本轮不把“缺少 ABI 重建试验”自动称为法律违规；它是尚未完成的工程验证。实际源代码、所需通知、构建材料及允许修改/调试的用户权利仍须逐项完整保留。

结论：**静态范围已形成可查矩阵，全部静态传递依赖的纳入与文件级义务尚未全部闭合。**
