# Phase A 工程基线报告

## 1. 范围与结论

Phase 0、Phase 0.3.1 已由用户确认完成，最新 Windows 版本已通过实机验收。本阶段从最新 Codex 浏览器翻译源码建立本地、可回退的工程基础。

本阶段没有添加用户功能，没有 Qt Quick/QML 迁移，没有改动 `src/`、`resources/`、CMake 产品版本或产品更新日志。变更限于 Git 管理文件、只读审计脚本、三项小型行为测试及本组报告。现有 Widgets 仍是正式 GUI。

## 2. 来源核对

| 项目 | 核对值 |
| --- | --- |
| 来源包 | `Relaxer-V0.2.0-dev-browser-translation-src.zip` |
| ZIP SHA-256 | `309f8d68fba3464afa7521fceaf201e68b5bd6e8a1efb5f081e6f6587af59e37` |
| 来源 ZIP 条目 | 140 |
| 当前源码清单 | 138 个文件逐项 SHA-256 一致 |
| 源码子目录 | `src/tests/scripts/resources/third_party/licenses` 文件集合与 ZIP 完全一致 |
| ZIP 与工作区的说明差异 | ZIP 额外包含打包时生成的“浏览器翻译源码包说明”；ZIP 的清单包含该说明，工作区清单没有它 |
| 交接报告 SHA-256 | `6be9cefa78a6ca88510a8b0988c8298be6e0cfbcc4561f3e75c7c1dab9c1e180` |
| 原已验证构建的源码指纹 | `935b1b1f5ab05b7f4d06049ad7418209a2864cc808a00967d5c7d344d2538e2f` |

核对依据为 ZIP 内容、包内清单、工作区清单和上一份交接报告，未切换到此前其他维护者的 Phase 0.3 版本。详见 `evidence/baseline-audit.json`。

## 3. Git 基线

原工程此前没有 Git。使用独立 Git 目录初始化，原工作树保持在 C 盘，避免移动已经验收的源码、启动器、曲库和 runtime。

| 项目 | 值 |
| --- | --- |
| 工作树 | `C:\Users\email\Desktop\Relaxer\Relaxer-V0.2.0-dev` |
| 完整 Git 元数据 | `D:\Git\Relaxer\repository.git` |
| 工作树连接文件 | 工程根目录 `.git`，指向上面的 Git 元数据 |
| 原基线提交 | `faa44ded636e4808600e2ded04c93352918a8dda` |
| 原基线标签 | `baseline/windows-accepted-20261015` |
| `main` 分支 | 保留在原受验收基线 |
| 当前开发分支 | `phase-a/foundation` |
| Phase A 完成标签 | `phase-a/foundation-20261015` |
| 远程配置 | 无；未创建远程仓库、未推送 |

首次提交严格按已核对的源码清单显式暂存，避免初始 `.gitignore` 不完整时混入用户资料。后续完善忽略规则，并执行禁止文件、引导文件和 Git 完整性检查。

原环境未配置 Git 作者。仅在本仓库设置中性作者 `Relaxer Local Baseline <relaxer-local@localhost>`，没有更改全局 Git 身份。仓库配置 `core.autocrlf=false`，`.gitattributes` 保留现有源文件字节；本阶段不批量转换换行。

`D:\Git\Relaxer` 还保存交付报告副本、验收收据及本地 Git bundle。该 bundle 包含本地分支与标签的提交历史；源码 ZIP 不包含 `.git`。

## 4. 纳入与排除

纳入：全部业务源码、测试源码、资源、构建与准备脚本、许可证、必要 FFmpeg 源码、根目录两个引导 EXE、历史说明、本次工程管理文件及报告。

排除：个人 `data`、音乐和歌词资料、SQLite/数据库及 WAL、浏览器配置、runtime、编译目录、机器路径配置、日志、测试输出、预览截图、编辑器缓存、压缩包和 Git bundle。保留的 EXE 仅为根目录 `RelaxerStart.exe`、`RelaxerLockCheck.exe`；没有把已编译播放器纳入历史。

`.gitignore` 不会移除本地已存在的文件。忽略规则与受验收文件清单都经过只读审计；原曲库和运行环境不依赖 Git 跟踪才能继续运行。

## 5. 构建环境与重新验证

| 项目 | 本机值 |
| --- | --- |
| 系统 | Windows 10 专业版，10.0.19045，64 位 |
| CMake 内部版本 | 1.10.0，未变 |
| Git | 2.56.0 |
| CMake | 4.4.4 |
| Ninja | 1.13.2 |
| 编译器 | MSYS2 UCRT64 GCC 16.2.0 Rev5，`x86_64-w64-mingw32` |
| Qt | 6.11.2 |
| 构建配置 | Release，C++17，Ninja，并发 6 |
| Phase A 构建 | `C:\Users\email\Desktop\Relaxer\test\phase-a-20261015\build` |
| 新增测试后的源码指纹 | `eac6302e2c035edd43f20fe648303490c942ec9b58d6259c9bb22c0e1ee2252e` |

原测试先实跑一次：21/21，134.70 秒。加入三项测试后重新显式配置、编译和运行：21/21，137.48 秒。Phase031 保持 33 项通过。环境详细值在 `evidence/build-environment.json`，结果在 `evidence/verification.json`。

新版测试源码改变了构建指纹，这是预期现象；原已验收 runtime 仍对应原指纹。没有覆盖产品 runtime 或根启动器。新构建复制到隔离目录，配合原部署依赖运行独立导入、数据库、GUI 和 PCM 解码验证，全部通过。

## 6. 运行环境保护

阶段开始保存原 `data/`、`runtime/`、两个根引导 EXE、产品更新日志和构建缓存路径文件的哈希。阶段结束逐项复核 532 个文件及资料目录文件集合，无变化。

完整快照哈希清单留在未纳入 Git 的本地测试证据目录，避免把个人曲库文件名和数据库摘要公开进源码历史。可提交报告只保留核对结果和数量。

本阶段没有运行会自动覆盖原 runtime 的部署操作，没有安装/更新全局依赖，没有调用在线翻译服务或处理验证码。

## 7. 回退与恢复

查看原受验收代码：

```powershell
git -C 'C:\Users\email\Desktop\Relaxer\Relaxer-V0.2.0-dev' show --stat baseline/windows-accepted-20261015
git -C 'C:\Users\email\Desktop\Relaxer\Relaxer-V0.2.0-dev' diff baseline/windows-accepted-20261015 -- src resources CMakeLists.txt
```

推荐恢复到新的目录审查，保留当前开发现场和个人资料：

```powershell
git -C 'C:\Users\email\Desktop\Relaxer\Relaxer-V0.2.0-dev' archive `
  --format=zip --output='C:\Users\email\Desktop\Relaxer\test\accepted-recovery.zip' `
  baseline/windows-accepted-20261015
```

本次已实际生成并逐项验证基线恢复归档，确认能从 Git 恢复原清单文件，而非仅依赖标签存在。不要使用 `git clean -fdx`：它可能删除被刻意忽略的曲库、runtime 和浏览器配置。原工作树的 `.git` 指针与 D 盘 Git 数据需要一起保留；迁移电脑时优先使用已验证的 Git bundle 在新目录恢复。

## 8. 工程管理审计入口

```powershell
& 'C:\Software\Anaconda\python.exe' -I -S -B `
  'C:\Users\email\Desktop\Relaxer\Relaxer-V0.2.0-dev\scripts\verify-foundation.py' --phase-a
git -C 'C:\Users\email\Desktop\Relaxer\Relaxer-V0.2.0-dev' fsck --full
```

`--phase-a` 额外要求产品代码相对受验收标签不变、没有远程配置。该选项专用于本阶段验收，不应在未来获准修改产品代码的阶段继续当作不变约束。校验清单不包含自身；最终提交、bundle 和源码 ZIP 的哈希另存在交付收据中。
