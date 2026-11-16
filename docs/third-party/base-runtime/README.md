# 基础运行依赖的来源记录

这里保留 V0.2.2 基础运行包的许可矩阵、来源与构建配方、静态依赖审计和义务清单。文件移自当时的工程目录，原文与摘要保持不变；其中的工程阶段状态属于历史记录，不代表当前发行状态或新的合规认证。

基础运行库的完整对应源码、配方、补丁和来源材料由正式 [V0.2.2-GA 第三方源码附件](https://github.com/QUEQIAN1961/Relaxer/releases/download/v0.2.2-GA/Relaxer-V0.2.2-GA-Third-Party-Source.zip) 提供，SHA-256 为 `a892dd05c2bbb14c73e1c49d930ba4e8374d3fd148f1a00e3bf0916a3c52b760`。

- `GA01/`：第三方许可矩阵、对应源码配方、静态链接审计和结构化义务清单。
- `GA02/`：Rust 静态依赖、OpenCC 生成材料、补充许可义务、来源布局与重建说明。

本版修改的 FFmpeg 和新增 MIDI 组件仍使用同版 FFmpeg-Source.zip、MIDI-Source.zip，不能以旧基础源码包替代。详见 [本版构建和对应源码说明](../../releases/v0.2.6-GA/BUILD-AND-SOURCE.md)。运行包中的许可证和版权通知完整保留。

其余私人测试、内部发行门和阶段性调查原件保留在 [清理前的固定提交](https://github.com/QUEQIAN1961/Relaxer/tree/d33d8f7b82a2d02207029181be9921f613e9b793/docs/releases)，不再随当前应用源码复制。
