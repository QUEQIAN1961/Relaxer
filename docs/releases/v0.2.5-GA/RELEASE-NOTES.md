# Relaxer V0.2.5-GA

新增网易云与QQ音乐发现、扫码登录、分类搜索、每日及AI推荐，支持云端歌单缓存、播放统计关联、下载与NCM自动转换。歌单独立记忆排序，提供48种图标；AI配置启动恢复，MIDI改用开源FluidSynth。完善个人页、模糊高亮、头像封面、歌词、滚轮及迷你切换，修复任务栏最小化、队列溢出和布局跳动。同步新导航与尺寸测试，修正歌词动画等待、原生窗口验证及QML交互诊断；复核后台资源释放与内存稳定性。

Windows x64 便携包：`Relaxer-V0.2.5-GA-Win64.zip`。完整解压到新的可写目录，运行根目录 `Relaxer.exe`；兼容界面使用 `--widgets`。升级前退出旧版并备份自己的 `data/`，不要混装 runtime 或让两个实例写入同一份资料。

本包不包含个人音乐、资料、AI API 配置或网易云/QQ 登录记录。默认资料在包根目录 `data/`，支持 `--data` 指定目录。

验证范围：继承 V0.2.5-RC 的功能与七项定向修复，独立复测已覆盖相关导航、歌词、D3D11 四角拖拽、阴影与诊断。本次完成发行编译、内存与动画局部检查及隔离资料启动检查；没有新增完整 96 项 CTest，不宣称全量回归通过。人工视觉和听音由用户确认。Windows 默认使用 D3D11；强制 software 后端在 150% DPI 拖拽时仍可能有 1px 位移，这是已知限制。在线服务范围受账号、会员权限及平台变更影响。

应用代码为 GPL-3.0-only；MIDI 使用 FluidSynth，GeneralUser GS 音色遵循单独许可，原文随包提供。应用源码见同版 `Source.zip`，新增 MIDI 对应源码及构建资料见 `MIDI-Source.zip`；基础 Qt/MSYS2/FFmpeg 对应源码沿用 [V0.2.2-GA 第三方源码附件](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.2-GA/Relaxer-V0.2.2-GA-Third-Party-Source.zip)。软件许可证不授予外部音乐、歌词、图片或在线服务内容的使用权。
