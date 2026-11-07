# Relaxer V0.2.5-RC

新增网易云与QQ音乐发现、扫码登录、分类搜索、每日及AI推荐，支持云端歌单侧栏缓存、播放统计关联、单任务下载与NCM自动转换。歌单独立记忆排序，提供48种图标。AI配置启动静默恢复，推荐适配模型限额。MIDI更换开源FluidSynth。个人页默认收起、歌单默认展开；图片与调色盘改用模糊高亮。优化头像、封面、滚轮、标题和迷你动画，补足切换背景；修复任务栏最小化、歌词、队列溢出及布局跳动。

Windows x64 便携包：`Relaxer-V0.2.5-RC-Win64.zip`。完整解压到新的可写目录，运行根目录 `Relaxer.exe`；兼容界面使用 `--widgets`。升级前退出旧版并备份自己的 `data/`，不要混装 runtime 或让两个实例写入同一份资料。

本包不包含个人音乐、资料、AI API 配置或网易云/QQ 登录记录。平台登录与 AI 配置由使用者自行设置。默认资料在包根目录 `data/`，支持 `--data` 指定目录。

这是 RC 预发行版。已完成相关功能的针对性验证及本次发行构建，不宣称完成新的全量 Harness 回归。在线服务的可用范围受账号、会员权限及平台变更影响。

应用代码为 GPL-3.0-only；MIDI 使用开源 FluidSynth，GeneralUser GS 音色遵循单独许可。原文许可随包提供。应用源码见同版 `Source.zip`，新增 MIDI 对应源码及构建资料见 `MIDI-Source.zip`；基础 Qt/MSYS2/FFmpeg 对应源码沿用 [V0.2.2-GA 第三方源码附件](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.2-GA/Relaxer-V0.2.2-GA-Third-Party-Source.zip)。软件许可证不授予外部音乐、歌词、图片或在线服务内容的使用权。
