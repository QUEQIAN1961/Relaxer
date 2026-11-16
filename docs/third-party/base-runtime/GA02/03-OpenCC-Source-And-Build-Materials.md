# GA-02 OpenCC 对应来源与生成材料

**PASS：实际嵌入数据的对应源、完整二进制表、生成转换和通知材料已核对。** 不推测最初导入者使用了哪一个历史提交。

实际运行包没有 OpenCC DLL/EXE、没有 OpenCC 的 MSYS2 所有者，也没有任一 PE 导入 OpenCC。它由 `src/lyric-text.cpp` include `src/opencc-st.inc`，进入 Relaxer 的静态代码，最终在 runtime/Relaxer.exe 与 RelaxerQmlUi.dll 中各有一份表。故 OpenCC 的二进制软件包修订、完整 C++ 库构建选项不适用。

| 项目 | 精确身份/证据 |
|---|---|
| 数据内容版本 | STCharacters Git blob `677e8fbc70dcdc1da477b9f6c9f42a6b33609ae0` |
| 对应上游快照 | OpenCC commit `fae2a0a911c30764e669a7d8b0591cd8e852934a` |
| 字典最后变更 | `178f05dbc5dbcedd09d4ab39d501094f4d9d8820`，2026-07-18；早于 Relaxer 导入 |
| 字典 SHA256 | `a0ca1601c70648cf48b33c3c6210ccbecc5c7eead4b4c3daf76587ba2c03582b` |
| 冻结生成表 SHA256 | `d00ffd753c8c87c973cb587fcac610770389e28a162563bd2da73554c53d63a4` |
| 有序映射规范摘要 | `c28d7d74e8ca1f1dd3155bee99b21fb006ed5a79d4dfc3b90c4c5c4462f0de0a` |
| Relaxer 原始导入记录 | `faa44ded636e4808600e2ded04c93352918a8dda`，2026-10-15 |
| 原始导入所用上游修订 | 没有记录；不虚构，不用当前引用填成历史事实 |

字典有 4,012 对，转换严格为：跳过注释，按 tab 分割，选择目标栏第一个候选，删除 131 对 source==target 的恒等映射，保持原顺序，输出固定 C++ 格式。结果 3,881 对，逐字节重现原表。

进一步直接读取两个未修改 PE 的 `kTradMap` 符号和 .rdata 指针：每个二进制均解码 7,762 个 UTF-16 字符串指针、3,881 对有序映射，**全部与冻结表一致**。这不是根据文件名或只有字符串出现次数推测来源；具体 VA、文件偏移和二进制 SHA 在 `opencc-binary-table-verification.json`。

构建材料为精确字典、生成脚本、独立生成结果验证及实际冻结 CMake 构建的 include 关系。没有凭空补一份 OpenCC 库 pacman 修订，也没有更新字典或改变纯/繁体转换逻辑。数据许可为 Apache-2.0；保留原 LICENSE、AUTHORS、项目说明与来源，未虚构上游不存在的 NOTICE 文件。

本轮在独立源码 `src/opencc-st.inc` 仅加 9 行许可证/来源/改编注释，明确一等候选选择、恒等映射删除及历史修订未知；映射数组原字节不变。完整许可及生成材料进入 `docs/third-party/OpenCC/`。源码 ZIP 因此可以携带源文件内的显著改编通知；现有播放器业务逻辑、版本和二进制均未改。

本结论关闭的是**实际嵌入数据的内容对应、可重生成及通知材料**。它不会把未知历史导入修订变成已知事实，也不代替其他组件的许可审阅。
