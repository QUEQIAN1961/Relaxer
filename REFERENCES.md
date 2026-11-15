# 引用与致谢

Relaxer 由 QUEQIAN 开发，自有代码以 GNU GPL 第 3 版发布，第三方材料保留各自许可。软件不提供任何担保；你可以依 GPLv3 的条件修改和再分发。完整条款见根目录 LICENSE，以及软件设置中的“开源许可与致谢”。

## wiliwili

- 项目：[wiliwili](https://github.com/xfangfang/wiliwili)
- 作者、维护者：xfangfang（源码中的 fang）及该项目贡献者。
- 核对、参考的固定提交：[`88e5876bea9502d06f46a8656e3530684d3aaf7d`](https://github.com/xfangfang/wiliwili/tree/88e5876bea9502d06f46a8656e3530684d3aaf7d)。这是上游参考版本；不是 Relaxer 当前开发提交或发布标记。
- 许可：GNU GPL v3，原文保存在 `docs/third-party/wiliwili/LICENSE`。上游根 LICENSE 与所列文件逐项核对，所列代码没有另行指定的文件级许可。不能把此结论扩展到整个上游依赖和资源目录。
- Relaxer 适配文件：`src/ui-bridge/bilibili-bridge.cpp`、`.h`；修改者 QUEQIAN，日期 2026-11-13。

| 上游文件（均相对于该提交） | 实际参考或改编内容 | 本项目修改 |
| --- | --- | --- |
| `wiliwili/source/api/mine_api.cpp` | Web 二维码生成、轮询、账号与收藏夹接口流程；保留其“Created by fang on 2022/7/26”归属 | 改为 Qt 网络请求、独立 Cookie/Windows DPAPI 存储、取消及界面状态，不移植 CPR/全局 Cookie 实现 |
| `wiliwili/source/api/search_api.cpp` | 视频分类搜索、页码与搜索参数；保留其“Created by fang on 2022/8/2”归属 | Qt 分页、富文本标题转纯文本、稳定视频 ID 和队列适配 |
| `wiliwili/source/api/video_detail_api.cpp` | bvid、cid、视频详情和播放地址解析流程 | 适配可用 DASH 音画轨道、画质上限和 Hi-Res 回退；由 Relaxer 生成临时本机 MPD，交给既有 Qt/FFmpeg 同步播放，保留单个 durl 合流回退，不移植上游 mpv 播放器 |
| `wiliwili/include/api/bilibili/api.h` | 本次使用的接口路径；保留其“Created by fang on 2022/5/1”归属 | 仅保留所需路径，不引入上游 API/模型库 |
| `wiliwili/source/api/util/wbi.cpp`、`wiliwili/include/api/bilibili/util/wbi.hpp` | WBI 协议流程和重排表 | 改为 QMap、QUrl 编码、QCryptographicHash；保留原文件注明的“Created for wiliwili based on bilibili-API-collect documentation”来源；没有复制该文档文字、示例或引入其仓库代码 |

所有实际适配文件保留 GPL 标识、来源、修改者与修改日期。上述表格不能替代源文件声明或 GPL 原文。

本次没有采用 wiliwili 的界面、图标、字体、音视频样本、borealis、CPR、pystring、第三方 MD5 代码、libmpv 或其预编译依赖。B站入口的小电视符号由 Relaxer 自行绘制；登录二维码沿用本项目已有 Nayuki QR Code generator（MIT），其原始版权与许可继续保留。Qt、FFmpeg 及其依赖沿用 Relaxer 已有实际构建，不能以 wiliwili 的 GPL 标签覆盖它们的独立许可。详情见 THIRD_PARTY_NOTICES.md。

## 对应源码与发行

- Relaxer 仓库：<https://github.com/QUEQIAN1961/Relaxer>；本版精确源码标签为 `v0.2.6-GA`，发行附件同处提供 `Relaxer-V0.2.6-GA-Source.zip`。
- 同版 `FFmpeg-Source.zip` 提供修改版 FFmpeg 的完整原归档、补丁、实际配置与构建脚本；`MIDI-Source.zip` 提供 MIDI 组件精确源码。未修改基础依赖沿用 V0.2.2-GA 的公开第三方源码附件。
- 对外发行二进制时，必须在同一下载位置明确提供与该二进制匹配的完整对应源码、所需构建/安装脚本和适用依赖源码材料；若 GPLv3 第 6 条要求提供安装信息，也必须一并提供。不能只附此致谢文档或旧版本仓库链接。
- GPL 许可不以“非商业”或“标注出处”作为充分条件。本项目没有增加禁止商业使用或限制修改、再分发的条件。
- wiliwili 的代码许可不等于 B站平台、账号或视频内容授权。软件只通过正常扫码及账号现有权限访问内容，不提供付费内容权限绕过。

## 其他引用

2026-11-14 歌单与封面补充：在下述 MIT 协议来源基础上，汽水补齐自建/收藏歌单双入口与分页，酷狗追加参考 `module/audio.js`、`util/helper.js` 实现缺图歌曲的批量详情读取。另核对同一 Folia 固定提交的 `qishuiNormalize.ts`、`qishuiCatalog.ts`、`kugouProvider.ts` 所展示的平台字段（歌单名称、封面、数量/归属及专辑封面）；未复制这些 AGPL TypeScript 实现。原生 Qt 字段解析、分页和封面回填修改者为 QUEQIAN，适用 MIT 原文继续完整保留，详见 `docs/third-party/music-platforms/README.md`。

### 汽水与酷狗原生适配补充（2026-11-14）

感谢 chthollyphile 及 Folia 贡献者提供接入思路。核对 [folia-major](https://github.com/chthollyphile/folia-major/tree/52159848123ade84a4089d52aa28eeacff4117e6) 的 `package.json`、`electron/qishui/qishuiApiBridge.cjs`、`session.cjs` 和 `electron/kugou/kugouApiBridge.cjs` 后，沿其依赖定位协议上游。Folia 主体为 AGPL-3.0；本次未复制其 Electron 桥、会话仓库、界面或资源，也未将这些文件加入 Relaxer。

- 实际改编来源：[GSQZ/qishui-music-api 0.2.2](https://github.com/GSQZ/qishui-music-api/tree/924add376cadf8a2268f11999051eb976e3b8e0d)，MIT，Copyright (c) 2026 sayqz。参考与改编 `src/auth.cjs`、`protocol.cjs`、`client.cjs`、`catalog.cjs`、`playback.cjs`：扫码授权码交换、P-256 Ticket Guard、账号核验/续期、歌单与单曲读取和明文音源选择。改为 Qt 网络和模型、Windows CNG 密钥/CSR/签名与既有 DPAPI 保存，不引入 Node/Electron 或解密受保护音轨。
- 酷狗沿用 [MakcRe/KuGouMusicApi v1.6.0](https://github.com/MakcRe/KuGouMusicApi/tree/v1.6.0)，MIT，Copyright (c) 2023 MakcRe；本次另核对 `module/everyday_recommend.js`、`playlist_track_all.js`、`playlist_track_all_new.js`、`register_dev.js`、`util/request.js`、`crypto.js`：空请求体 POST 日推、公有歌单默认网关、自建歌单 listid 请求与设备注册。AES-CBC/RSA-PKCS1 设备协议改写为 Windows CNG，保留此前已使用模块的归属，不引入其 JS 服务、node-forge 或 crypto-js。
- 本项目修改者 QUEQIAN，日期 2026-11-14；适配文件为 `src/ui-bridge/additional-music-bridge.cpp/.h`、`qishui-protocol.h`、`kugou-protocol.h`。原许可位于 `docs/third-party/music-platforms/`，同时在程序的“开源许可与致谢”提供全文；详细范围见该目录 README。对应源码按上文同版发行安排提供，未公开的新修改不能用旧发行源码冒充。

完整第三方说明见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。其中包括 Qt/FFmpeg、FluidSynth、二维码、morphicons、音乐平台适配与已有资源的独立归属和对应源码材料。
