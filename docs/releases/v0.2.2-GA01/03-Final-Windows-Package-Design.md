# GA-01 Windows 用户部署结构

**确定采用 Windows x64 便携分发。** 根目录唯一 EXE 为普通入口 Relaxer.exe；`runtime/` 保留全部 1859 个运行文件。源码准备工具不属于用户包。

```
Relaxer/
  Relaxer.exe
  runtime/                 # 配套应用、共享库、QML、插件、ffmpeg/ffprobe
  PORTABLE-SHA256SUMS.txt   # 每次启动完整读取并校验
  PACKAGE-MANIFEST.json     # 交付外层文件清单
  BUILD-IDENTITY.json
  COMPONENTS.json
  deployment.json
  READ-ME-FIRST.md
  LICENSE / LICENSE-NOTICE.txt / THIRD_PARTY_NOTICES.md
  licenses/ / third-party-licenses/
  data/                    # 首次运行创建，不随 ZIP 分发
```

不包含 RelaxerStart.exe、RelaxerLockCheck.exe、两个 CMD、PRIVATE-TEST-VALIDATION.json、内部风险报告或 ready 标记。开发工具单列 `Relaxer-GA01-Developer-Tools-NOT-USER-PACKAGE.zip`；源码准备流程没有因此被验证或宣称完成。

普通入口使用 root Relaxer.exe；Widgets 兼容入口为 `Relaxer.exe --widgets`。runtime 下 EXE 留作内部依赖和开发诊断，直接运行会绕过根启动器校验，不是建议给普通用户的入口；QML/Widgets 两种界面实际已独立验证。

## 程序和资料的边界

`runtime/` 的内容由清单约束，应用运行未修改其任何字节；这是程序行为与校验边界，**不是 OS ACL 的强制只读隔离**。默认资料为同级 `data/`，包根目录需可写。因此本方案不能称为 Program Files / 多用户只读安装方案。当前工程范围不增加安装器，不修改默认资料规则。

需要外部资料时使用已存在的 `--data`（优先）或 RELAXER_DATA。程序不自动迁移、删除或升级个人目录。验收用的均为新建空库及 8 曲合成资料库。移动整个便携目录后默认 data 保留；使用外部资料时指向同一路径。运行中的目录不移动。

## 更新和恢复

退出全部界面；备份整个 data/；在新目录解压完整新包，不覆盖混装 runtime；使用已有资料的副本先验收，再明确指定原资料。失败时关闭程序，重新使用原程序目录和原备份；本轮没有数据库格式迁移。杀毒隔离时保留保护历史、核实来源并重新完整解压，不关闭防护或设置全目录排除。

最终工程包 3527 个清单文件，323 个二进制、4124 条导入闭合。根启动器、runtime 和 PORTABLE 清单与冻结包逐字节一致。准备入口已移除，普通入口启动不再依赖其准备流程：此技术发行门具备关闭证据。
