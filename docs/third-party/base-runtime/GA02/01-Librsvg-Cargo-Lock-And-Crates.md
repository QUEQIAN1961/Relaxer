# GA-02 librsvg、Cargo.lock 与 crates 来源闭环

日期：2026.10.31。**来源内容 PASS；原 CI post-prepare 锁文件的原始字节及哈希未取得，不作原件认证。** 静态纳入和文件级许可的剩余问题另见报告 02、05。

## 二进制、源码与配方

实际包为 `mingw-w64-ucrt-x86_64-librsvg 2.63.2-1`。两份 `librsvg-2-2.dll` 的 SHA256 为 `278bfe70d237c5e49fdde37e2b6e7fba1edf641c054798ec10dc79ccb97fab61`。GA-01 的签名、MTREE、BUILDINFO 与 PKGBUILD 绑定继续有效。

- PKGBUILD SHA256：`33f298efef2d5f5595e8de1958860e4fe9584462032930dc447b45402cad97fe`。
- 原源码 `librsvg-2.63.2.tar.xz` SHA256：`852b18e1a00b8605528825a27dc7748bff2a5dd254028f59dc22a34ea57e81b6`。
- 原补丁 `0002-avoid-link-whole.patch` SHA256：`4cb597eef01a636ad7007b3a47b76e395f30b2347183b5acb1af94d39577a239`。
- GNU patch 实际应用：第一 hunk fuzz 2 / 偏移 6，第二 hunk 偏移 10。未改补丁内容。Rust 静态档经 `link_with` 链入 DLL，依赖图可达不代表全部对象都保留。

## 原始 CI 证据与重建锁

此前匿名 HTTP 请求返回 403/404；随后通过已连接 GitHub 的只读日志接口成功取得 `msys2/msys2-autobuild` run **35444657495**、job **105901402423** 的原始日志。保留包构建区段 1,903 行，SHA256 `4c32e23280f82bdbbe766914acdc8e5b22162af2b22c6490dd31def6819598ba`。原日志没有返回 post-prepare Cargo.lock 文件；artifacts 列表为空。早期请求失败不再作为“原 CI 无证据”的结论。

原日志明确记录：`windows-sys 0.61.2 → 0.60.2`；新增 `windows-targets 0.53.5`，及八种架构的 `windows_* 0.53.1`。该完整十项变化与隔离 Cargo 1.98.1 的原生 crates.io 重放**完全一致**，没有无关版本变动。

| 文件 | SHA256 | 身份 |
|---|---|---|
| 上游原始 Cargo.lock | `e79e2a2735a0b5b151bb4133becd3008223fabedd34a8b787a872edab9854941` | 签名源码归档内原件，366 项 |
| GA-02 重建有效 Cargo.lock | `e5f54deb10851bbecdac99c161478255123bde0474db3bb56ee2ab84c43c852d` | 同工具/同命令重放，原 CI 依赖 delta 与实际编译版本交叉印证 |
| 原 CI post-prepare 文件原件 | 未取得 | 不填推测的原始文件哈希，不把重建文件冒称原件 |

原始 shared-DLL 步骤为 `cargo cbuild --locked --library-type staticlib --release --features pixbuf -p librsvg-c`（完整原命令、目录与参数在证据 JSON）。13:16:23 开始，13:19:02 Rust release 完成，13:19:04 链接 `librsvg-2-2.dll`。这段日志有 **177 个唯一 crate/version**，其中 **175 个 registry crate、2 个本地项目**；全部在重建锁及已恢复源码内。它们包括构建依赖/过程宏，不能全部算作 DLL 的运行代码。

本轮 metadata 的含构建闭包为 190 项，原编译日志未出现的 13 项逐项列在 `librsvg-original-ci-corroboration.json`。这是 workspace feature 合并形成的保守超集，不能把 190 写成原始真实编译数。原 DLL 另能恢复 69 个 crate 版本路径，均与该集合相容。编译日志也不替代最终链接对象/实际 feature 的文件级许可判断。

## 637 个来源与遗漏版本

GA-01 的 637 个归档是 librsvg、rav1e、libdovi 原始锁的并集，**637/637 摘要与原锁一致**。它们包含测试、可选、其他平台依赖，不是实际编入数量。原 637 集合未包含下游 pin 所需的 10 个精确版本；现已逐项按官方 registry cksum 补齐，合计 **647 个选择明确的 crate 归档**。十项发布时间均早于原构建；真正印证其选择的是原 CI delta，而非仅发布时间。

重建不更新任意 Rust 依赖版本。原版 Cargo/rustc 1.98.1 只解压到隔离工作目录，无系统安装/更新；目录型 source replacement 曾使 pin 命令无效，该实验未被用于原始选择推断。原生 registry 重放才是本轮保存的最终输入。实验性 windows-targets 0.53.2 / windows-link 0.1.3 不进入交付。

三个离线、锁定的 Windows 目标图：librsvg 146 普通闭包 / 190 含构建闭包，rav1e 73 / 131，libdovi 24 / 29。原 637 中 486 出现在过滤后的 metadata 包定义中，151 未出现；这两个数也不是静态编入数量。

## 通知核对

原 22 项“无独立顶层许可文件”中，12 项已通过精确 VCS、完整载荷匹配、已有 LICENSES 子目录或源文件明确授权核对。其中覆盖所有当前普通目标候选；**普通目标通知待补项为 0**。av-metrics 0.9.1 的 13 个源文件与上游 `dd256e7e…` 的 `av_metrics/` 逐文件一致；winapi-x86_64 0.4.0 的 1,418 个文件与上游 `fa4e2c5a…` 一致，完整许可已补入。selectors 的每个 Rust 源文件与其记录的 VCS blob 相同，并有 MPL-2.0 文件头；提供完整 MPL 文本与对应源码。crc-catalog 的完整许可原本就在发布归档 LICENSES 中。

其余 10 项为构建/开发/其他平台范围，原始声明及源内通知保留；不能据此捏造十个实际运行二进制缺陷，源包超集的文件级审阅状态也不冒称全部通过。详见 `crate-notice-final-triage.json`。

**结论：librsvg 原包、源码、补丁、有效依赖内容和来源可用性在上述范围闭环。原锁原件未取得是证据边界，当前不再单独列为阻断；完整静态纳入/许可证义务仍须按报告 05 完成。**
