# FFmpeg 9.0.2 DASH 取消修复

修改者 QUEQIAN，2026-11-13。上游 FFmpeg 项目及原文件版权/许可证保持不变。仅修改 libavformat/dashdec.c：dash_read_packet 因中断离开循环时返回 AVERROR_EXIT，避免返回成功却没有有效数据包，引起解码循环及退出/切换卡死。见 dash-cancel.patch。

源归档：https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz
SHA-256：8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e。
实际原包为 mingw-w64-ucrt-x86_64-ffmpeg-9.0.2-1，配置以 resources/ffmpeg-dash-runtime.json 固定。

构建工具需要 MSYS2 的 make、diffutils（`pacman -S --needed make diffutils`）。在已有匹配 MSYS2 UCRT64 开发环境执行 `python scripts/prepare-dash-ffmpeg.py --msys2 C:/Software/MSYS2`，再正常 CMake configure/build。准备脚本校验归档、应用补丁，只构建 avformat-63.dll，链接现有 avcodec/avutil 导入库。没有覆盖系统安装或替换现有解码器。配置移除程序/静态库、汇编及本机缺少开发头文件的 NVENC/AMF 编码器；这些均不改变本次保留的 avcodec 硬件/软件解码器。完整实际配置及二进制、补丁摘要写入 .dependencies/ffmpeg/runtime-ffmpeg.json，config.h/config.mak 留存于同目录。

当前构建启用 GPL 和 version3，按 GPL-3.0-or-later 提供材料；原 LGPL/GPL 文件归属及第三方独立声明仍保留。CMake 将修补 DLL、身份和许可证复制到测试构建，部署工具保留这份 DLL，不能重新从系统拾取未修版本。

对外发行时需同时提供精确原归档、此补丁、准备脚本、配置、适用依赖及对应源码，并更新实际依赖清单；旧 GA 依赖材料不能替代这份修改组件。原归档保存在 .dependencies/downloads，完整修改源码/构建信息保存在 .dependencies/ffmpeg-build，V0.2.6-GA 的 FFmpeg-Source.zip 提供该原归档、补丁、实际配置和构建说明；其余未修改基础依赖沿用 V0.2.2-GA 对应源码附件。
