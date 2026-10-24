# 第三方组件 / Third-party components

本文说明主项目许可证的边界。第三方组件保留自己的版权、许可证与源代码提供条件，不因仓库公开或主项目选择许可证而改变。

## Qt

Relaxer 使用 Qt 6 Core、Gui、Widgets、Sql、Multimedia、Svg、Network、Qml、Quick、QuickControls2 等共享库与插件。开发测试还使用 Qt Test。实际发行的版本与许可证以 Win64 包中的 `third-party-licenses/`（0.2.0 位于 `runtime/third-party-licenses/`）及对应组件为准。

Qt 源码与许可证：[Qt 下载](https://download.qt.io/)、[Qt 许可说明](https://www.qt.io/licensing/)、[Qt 源码仓库](https://code.qt.io/)。

## FFmpeg / FFprobe

FFmpeg、FFprobe 和 Qt 多媒体插件用于媒体探测、封面提取及音频解码。发行包采用实际收集的 MSYS2 构建；某些构建启用了 GPL 组件，不能将整个运行包统称为 LGPL 或 MIT。版本与构建配置可由包内 `ffmpeg -version` / `ffprobe -version` 查看，对应的 GPL/LGPL 文本保留在第三方许可目录中。

仓库内 `third_party/ffmpeg-7.1.tar.xz` 是历史源码准备流程使用的 FFmpeg 7.1 上游源码，不能据此推断当前所有运行库也是 7.1。

源码与构建资料：[FFmpeg 源码](https://ffmpeg.org/download.html)、[FFmpeg 许可说明](https://ffmpeg.org/legal.html)、[MSYS2 构建配方](https://github.com/msys2/MINGW-packages)。获取对应源码时须匹配所分发二进制的版本、补丁与构建选项。

## 其他依赖与启动器

- OpenCC 的 STCharacters 字表：Apache-2.0，原始归属与许可保留。
- MinGW-w64/GCC、libstdc++ 及其他运行库：保留 `licenses/startup/` 和发行包中对应许可文本；适用的运行库例外不改变其他组件的许可证。
- SQLite、图片编解码器、TLS、压缩库及其他实际依赖：以发行包的组件和许可目录为准。
- 原生首次准备窗口、缓存所有权及进程捕获部分根据用户提供的 Harbor C++ V1.4 项目改编，使用独立的 Relaxer 路径、标记及环境变量；原作者的权利与声明不因本次发布被移除。

发行包保留共享库、插件和许可目录。修改并重新分发时，请保留各组件声明并履行对应许可证。歌词、音乐、封面以及外部服务内容的权利不由本仓库许可证授予。

## English

The project license covers Relaxer-authored code; dependencies retain their own copyrights, licenses, exceptions, and corresponding-source requirements. Qt modules are dynamically linked. The distributed MSYS2 FFmpeg build may include GPL components; the bundled binaries must not all be described as LGPL or MIT. The historical FFmpeg 7.1 source archive does not identify the version of every current runtime library.

Keep all shipped license directories. Match dependency source, patches, and build options to the binaries you redistribute. Upstream Qt, FFmpeg, and MSYS2 source/build links are listed above. OpenCC data, compiler runtimes, SQLite, codecs, TLS libraries, and the adapted Harbor startup code retain their applicable notices. Music, artwork, lyrics, and third-party service content are not licensed by this repository.
