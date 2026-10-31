# GA-02 尚未关闭问题与关闭条件

**GA-02 THIRD-PARTY COMPLIANCE BLOCKED — REMAINING ISSUES DOCUMENTED**

## 已解除的来源/通知疑点

原 LC-01 不再作为独立阻断：已取得原始 MSYS2 CI 依赖调整和 shared-DLL 编译日志；全部十项调整、177 个编译版本与重建输入一致，来源摘要均通过。仍没有原 post-prepare Cargo.lock 的文件原件/原始字节哈希；这是保留的事实，不把文件保管链要求冒充源码义务。

当前普通目标候选的通知待补项为 0。原 22 项中 12 项已明确核对，包括 av-metrics 的精确源码内容、winapi 载荷、selectors 文件头与 crc-catalog LICENSES 子目录；不再说“14 个运行 crate 无完整许可”。

## LC-02：静态/嵌入代码及适用义务的最终交叉表（OPEN，阻断）

现有结果足以恢复来源和保守候选范围，但仍不能回答每个实际二进制内所有第三方文件的适用许可与通知。具体待完成项：

1. **Qt 6.11.2 模块和插件**：候选如 qtbase 的内置实现、qtdeclarative 的 masm、qtmultimedia 的 dr_libs/eigen、qtimageformats 的内置格式代码。精确源、配方、91 份归属及通知超集已提供；尚未对最终 feature/文件集合逐项选择适用的开源许可并排除测试/工具路径。包级 AND 汇总不是文件级许可证。
2. **shaderc 2026.4-1 的静态 glslang/SPIRV 及编解码链**：原 BUILDINFO 对应 glslang 16.3.0-1、SPIRV-Tools/Headers 1.4.363.0 的精确源码已补。原配方/动态边可查，最终静态对象与源文件的完整纳入、补丁和通知适用范围尚未建立逐项完成的交叉表。
3. **Rust 与 C/C++ 嵌入代码**：librsvg 原编译范围已显著收窄；rav1e/libdovi 的锁和目标图已核对。实际静态裁剪、host 过程宏与输出代码、代码内的进一步第三方实现仍需文件级审阅。19 个来源包中的 244 项是自动识别的候选目录/文件组，包含测试、构建文件和示例，**不是已经确认的 244 个实际静态库，也不是 244 项合规失败**。

关闭可采用两种证据路线：取得可核对的原始编译/链接/资源与配置记录；或对保守源码超集逐项完整核对兼容许可、所需源、通知及用户权利，并证明实际纳入必然包含在该超集内。**法律上不要求逐字节可重复构建或必须拿到原始 link map**；它们只是可用的工程证据手段。若做受控重建，须单列新第三方构建身份并验证替代组件，本轮未执行替换。

现有通知/源码超集已经收集，不等于上述逐文件判定已经完成。没有发现本轮新确证的必然许可证冲突；这也不是全部兼容的证明。详细路径、许可表达式、源组与摘要位于静态矩阵 JSON，不应再重复已通过的 101 包签名归属审计。

## 源码包超集中的其他通知范围

以下 10 项原始归档有许可声明；源内已有声明完整保留。它们不是当前普通运行目标候选；单独保留其构建/源码分发范围复核，不能当作十项运行包缺失授权的断言。

| 项目 | 当前角色 | 原声明 |
|---|---|---|
| block 0.1.6 | 不在当前目标图 | MIT |
| difflib 0.4.0 | librsvg: dev/other-workspace/optional candidate, rav1e: dev/other-workspace/optional candidate | MIT |
| malloc_buf 0.0.6 | 不在当前目标图 | MIT |
| objc-foundation 0.1.1 | 不在当前目标图 | MIT |
| objc_id 0.1.1 | 不在当前目标图 | MIT |
| r-efi 5.2.0 | 不在当前目标图 | MIT OR Apache-2.0 OR LGPL-2.1-or-later |
| r-efi 5.3.0 | 不在当前目标图 | MIT OR Apache-2.0 OR LGPL-2.1-or-later |
| r-efi 6.0.0 | 不在当前目标图 | MIT OR Apache-2.0 OR LGPL-2.1-or-later |
| simd_helpers 0.1.0 | rav1e: build/proc-macro-candidate | MIT |
| winapi-i686-pc-windows-gnu 0.4.0 | 不在当前目标图 | MIT/Apache-2.0 |

其中 simd_helpers 是 host 过程宏（其源码实现输出 cfg_attr 属性），不能当作直接静态运行库。若选择继续分发整个开发/其他平台源码超集，应同时审阅该超集自身的通知义务；不能用“没有编入”抹去源归档分发的适用条件。

## 保留事实

OpenCC 的内容与生成来源闭环，但未知最早导入者所用上游修订仍未知。ART-01 / Escape 风险记录、历史报告与产品冻结不变。本轮未公开发布，不把尚未公开上传源码另造为阻断；当前阻断为 LC-02 的实际审阅缺口。
