# GA-02 对应源码交付与验证

目录规范由 `source/scripts/assemble-ga02-materials.py` 实际执行，完整产物为 `Relaxer-V0.2.2-GA02-Corresponding-Source-REVIEW-REQUIRED.zip`。仅包含所选正式来源、补丁、构建元数据、647 个 crate、原/重放锁、通知、映射和 OpenCC 生成材料；候选/未认证状态保留在包内 STATUS.json。

原 98 个精确源归档、3 个 shaderc 静态构建补充包和已验二进制元数据复用 GA-01；6 个 VCS 来源按 makepkg Git archive 校验重现。误把 Git 目录当普通单文件校验导致的表面缺口已排除；GA-01 正则断言的空计数错误已纠正为 305/305，不修改原交付。

正式目录分为 components、static-build-source-supplement、rust/crates/locks、opencc、notices、mapping、provenance。工具安装、工作克隆、下载分片、运行目录、音乐、SQLite 和付费凭据均不加入。官方源码/发布 crate 内自带的构建必需输入按原归档保留，不抽换包内容。

来源哈希、每个拷贝、内部清单、归档 CRC、路径唯一性、独立解压后全量文件摘要都需通过才封存。提供三个打包器负向测试：来源摘要错误、覆盖旧目录、覆盖已有文件均拒绝。另保存材料只读投影的输入哈希和转换范围。

新工程 Commit/Tree/Fingerprint 见 ENGINEERING-IDENTITY.json。完整源码 ZIP、Git bundle 和 GA-01→GA-02 patch 能恢复本轮源文件注释、正式材料组装脚本及报告；补丁需从固定 GA-01 提交重放到同一 Tree。

本轮没有更换运行二进制，仅改法律注释与交付材料，故不无意义重跑全部 GUI/CTest。执行了来源/目标 metadata/资源注册表/映射不变/打包与恢复验证；不把 GA-01 的 93/93、硬件音频或用户手工验收记成本轮重跑结果。
