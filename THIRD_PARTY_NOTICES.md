# 第三方组件

Relaxer 主程序使用 C++17、Qt 6 Core / Gui / Widgets / Sql / Multimedia / Svg，以共享库形式运行。Qt 组件与插件的许可应以实际下载的 MSYS2 软件包及收集的 `third-party-licenses` 为准。

FFmpeg / ffprobe 用于本地音频标签、时长、封面读取，以及首次准备后对稀有格式的转码。`third_party/ffmpeg-7.1.tar.xz` 是随包附带的官方源码（约 10.5MB，LGPL-2.1-or-later），首次准备在本地编译精简解码器，不再联网下载；编译出的程序不另放进播放器安装目录以外的发行包。MIDI 在 Windows 上使用系统合成器。歌词可查询 LRCLIB、Lyrics.ovh，以及网易云、QQ 音乐、酷狗的非官方接口；外语译文在来源没有翻译时尝试免密钥在线翻译。繁简转换使用 OpenCC STCharacters 单字表，Apache-2.0。所分发运行库里的 FFmpeg 插件许可位于实际安装的软件包中。

Windows 原生启动器与数据库锁检查组件由 MinGW-w64 GCC 交叉编译，静态链接 GCC 运行库。运行库相关许可保留在 `licenses/startup`。首次准备窗口、缓存所有权检查、Windows 进程捕获等代码根据用户提供的 Harbor C++ V1.4 项目改编，使用独立的 Relaxer 路径、标记和环境变量。

播放器源代码随包提供，便于修改与重新链接。运行库收集时保留 MSYS2 第三方许可文本。发行或商业使用前，请核对实际运行库的许可及源代码提供义务。
