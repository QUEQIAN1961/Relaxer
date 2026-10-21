#!/usr/bin/env python3
"""Write six C6 reports from actual frozen-source verification evidence.

Use new report/output paths. These reports are distributed only when the
release assembler's final extracted-copy and archive checks also succeed.
"""
import argparse
from datetime import datetime, timezone, timedelta
import json
from pathlib import Path
import shutil
import subprocess


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'build', 'summary', 'native', 'isolation', 'performance',
                 'reports', 'publish', 'evidence-root', 'verification', 'toolchain'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--git', default=shutil.which('git'))
    args = parser.parse_args()
    source = args.source.resolve()
    def git(*command):
        return subprocess.check_output([args.git, *command], cwd=source, text=True, encoding='utf-8').strip()
    assert not git('status', '--porcelain')
    head, tree, parent = git('rev-parse', 'HEAD'), git('show', '-s', '--format=%T', 'HEAD'), git('show', '-s', '--format=%P', 'HEAD')
    full, native = read(args.summary), read(args.native / 'result.json')
    proof, isolation = read(args.native / 'verification-proof.json'), read(args.isolation / 'results.json')
    launcher = read(args.native / 'launcher-result.json')
    fingerprint = (args.build / 'relaxer-source-fingerprint.txt').read_text().strip()
    assert full['commit'] == proof['sourceCommit'] == head
    assert full['tree'] == proof['sourceTree'] == tree
    assert full['sourceFingerprint'] == proof['sourceFingerprint'] == fingerprint
    assert full['ctest']['passed'] >= 75 and full['qtTotals']['failed'] == 0 and full['baselineTestNamesRetained']
    assert len(native['suites']) == 33 and native['qtTestTotals'][1] == 0 and native['paidRequests'] == 0
    assert len(isolation) == 8 and all(not row['remainingOwned'] for row in isolation)
    performance = [read(args.performance / (str(count) + '.json')) for count in (0, 1000, 10000)]
    assert all(row['sourceFingerprint'] == fingerprint and row['sortAllRowsOrderVerified'] and row['shutdownSaved'] for row in performance)
    args.reports.mkdir(parents=True, exist_ok=False)
    timestamp = datetime.now(timezone(timedelta(hours=8))).isoformat()
    root = args.evidence_root.resolve()
    def link(path, label=None):
        return '[' + (label or path.name) + '](' + path.resolve().as_posix() + ')'
    context = f'''记录时间：{timestamp}（UTC+8）。分支 `{git('branch', '--show-current')}`。

Commit：`{head}`  
Tree：`{tree}`  
Parent：`{parent}`  
Source Fingerprint：`{fingerprint}`  
最终 Clean Release 构建：{link(args.build)}。

本报告随通过最终解压运行、源码逐字节与历史独立克隆校验的交付包分发；
最终交付身份及状态见 {link(args.publish / 'DELIVERY.json')}。
'''
    def write(name, title, text):
        (args.reports / name).write_text('# ' + title + '\n\n' + context + '\n' + text.strip() + '\n', encoding='utf-8')
    q = full['qtTotals']; nq = native['qtTestTotals']
    services = '\n'.join('| ' + name + ' | ' + '/'.join(map(str, values)) + ' |' for name, values in full['fourServices'].items())
    binaries = '\n'.join('- `' + name + '`: `' + value + '`' for name, value in proof['binarySHA256'].items())
    performance_table = '\n'.join(f"| {row['tracks']} | {row['sortCompleteIncludingMappingMs']:.4f} | {row['filterUntilNextFrameMs']:.4f} | {row['maxVisibleAndCachedDelegates']} | {row['steadyMemoryBeforeLegacyBenchmark']['privateBytes']/1048576:.2f} |" for row in performance)
    skips = '\n'.join('- `' + row['suite'] + '`: ' + row['line'] for row in full['skips'])
    write('00-C6-RC1-Handoff.md', 'Relaxer V0.2.1-RC1 — 总纲回传', f'''
V0.2.1-RC1 — DEVELOPER-VERIFIED — AWAITING HARNESS R16

C6-A、C6-B 开发门槛已通过；C6-C 在上述最终源码上完成统一集成。
未 push，未创建正式 v0.2.1 标签；未修改 v0.2.0/main 历史。

- 最终全量 CTest：{full['ctest']['passed']}/75 PASS，原有 74 项名称全部保留。
- 全量 QtTest：{q['passed']} PASS / {q['failed']} FAIL / {q['skipped']} SKIP，{full['qtSuites']} 份日志。
- 系统 PATH、中文空格便携路径：33 个 QtTest 套件，{nq[0]} PASS / {nq[1]} FAIL / {nq[2]} SKIP。
- 8 个组件隔离案例通过：锁冲突 exit 3；缺 Quick/模块时 Widgets 正常、QML exit 11、不创建或修改数据；原副本恢复组件后双入口正常。
- 性能在最终构建重测 0/1000/10000 曲库，完整排序、筛选、队列、80/5000 行歌词及退出保存通过。见 04。
- AI 仅使用本地 Mock，真实付费调用 0。用户数据与凭据未用于测试或打包。

R15-01、R15-02 为开发方修复并回归通过，等待 R16 独立关闭。
R15-03 为 HARNESS INVOCATION ERROR CONFIRMED，不宣称产品导入修复。
R15 历史账目仍为 0 P0 / 0 P1 / 0 P2 / 26 P3，不能把开发方结论替代独立关闭。

交付目录：{link(args.publish)}；六份报告：{link(args.reports)}。
其中包含 Win64 ZIP、逐字节 Git Tree Source ZIP、可独立克隆 History.bundle、
精简 Evidence ZIP、Complete ZIP、DELIVERY、源码清单及 SHA256SUMS。
归档/最终解压复验结果：{link(args.verification / 'RESULT.json')}。

实际声卡听音、物理全局按键和 Explorer 托盘鼠标操作未执行。
音频服务探测曾使测试退出延迟；原始失败和调用栈完整保留，不能称永久修复。
C6-D01 仍仅记录，未改变无本地歌词时的替换弹窗逻辑。
''')
    write('01-C6-A-R15-Defect-Closure.md', 'C6-A：R15 缺陷核对', f'''
| 缺陷 | 开发方结论 | 证据与独立关闭边界 |
|---|---|---|
| D-R15-01 / P3 | 修复四个 Declarative 目标的测试运行环境 | 复用已有 CMake 声明；最终 75/75，未靠 CTest 外部注入 QML 路径；等待 R16 |
| D-R15-02 / P3 | 修复测试把 HTTP 到达顺序当窗口身份的错误 | 按实际请求身份释放响应，保留跨歌曲/保存断言；历史 10×47 套件、10×4 目标均通过，最终全量再次通过；等待 R16 |
| D-R15-03 / P3 待确认 | HARNESS INVOCATION ERROR CONFIRMED | 夹具含字面反斜杠 Unicode 转义导致 WAV 创建失败；有效中文空格路径、正式文件夹菜单用户 PASS；未修改产品导入实现 |

R15 历史余额 26 P3 原样继承。其余历史 P3 见源码 `V0.2.0-Known-Issues.md`
及 R15 原报告，未批量改为 FIXED。

C6-A 最初要求提交 `4ac6ffe` 的独立全量结果为 73/74；其截图耗时跨过了动画反向测试的
墙钟等待，尚未观测中间帧便反向。测试改为等待真实中间进度，保留位移、连续性与收敛断言。
修复后的 A 门槛源码 `b4dd4b6`：74/74，QtTest 1178/0/4；检查点 `dfb9b90` 只追加文档。
这些历史成绩不混称为本报告最终提交的测试。

历史证据：{link(root / 'C6-A-final/Run-20261021T063829Z-a7c0273f/GATE.json')}；
最初失败：{link(root / 'C6-A-head-ctest/Run-20261021-142655-53b4c5/AUDIT.json')}；
请求身份稳定性：`C6-A2-repeat-suite`、`C6-A2-repeat-case`。
''')
    write('02-C6-A-Evidence-Closure.md', 'C6-A：运行证据与用户需求', f'''
最终构建的原生 C6 套件提供 16/16 QtTest 运行证据，包括：

- 歌词同步、当前行 1.2 倍、双语配对、±5000 ms 边界及持久化。
- 展开/全屏平滑跟随；滚轮/拖拽/惯性停止后 3 秒回正，再操作重计时，仅悬停不影响；减少动画不关闭歌词动效。
- 手动滚动时，左侧固定垂直中线时间对应中线歌词，点击跳播，回正后隐藏。
- 问号与菜单离开两者 3 秒收起；查找完成后仍可进入；已保存/确认纯音乐绿勾、待选橙色警告、查找中旋转。
- 搜索中退出确认：取消继续，确认中断，重启不自动联网续查。
- Esc/按钮退出全屏与几何恢复；下一首/背景动效逐帧与反向连续性。
- 黑闪：深浅主题 × 普通/减少动画、最小化恢复；保留真实位移和帧断言。
- AI Mock 未授权拒绝、授权后正向派发、保存/放弃/重新打开、跨歌曲异步身份隔离。
- 中文空格文件夹导入、单一 Playback/SQLite 连接、持久化和生命周期。

证据：{link(args.native / 'native-evidence')}，原生逐套件结果 {link(args.native / 'suites.json')}，
完整 CTest 对应的 RuntimeMeasurements 和 QtResults 位于 {link(Path(full['run']) / 'ctest')}。

用户明确人工 PASS 仅包含文件夹导入和 Review2 切页黑闪，不扩张为全体人工验收。
Review1/Review2 与用户确认文件保留在工作目录；C6-D01 仅记录，未实现。

历史 4 项跳过在最终源码仍原样披露：

{skips}

歌词同步使用真实解码/受控时间与原生渲染测量；没有把这些证据写成物理听音或实际显示器 FPS。
''')
    parity = (source / 'docs/phase-c/c6-parity.md').read_text(encoding='utf-8')
    write('03-C6-B-Widgets-QML-Parity.md', 'C6-B：Widgets/QML 功能对等', f'''
以下矩阵始于 A 检查点、在 B 源码 `d3ae2f2` 定向验证；最终冻结源码完整回归和便携复验再次通过。
B 检查点 `5fab3561`，定向 4/4 CTest、新原生对等 6/6。
字体实际缺口已补齐：footer、next、译文及部分歌曲元数据曾未遵守区域字体；
现沿现有 fontRegion/SettingsBridge/Fonts 存储，五个区域逐一检查实际 QML 属性。
未增加第二套核心、存储、schema、商业 AI 提供商或云同步。

{parity}
''')
    version_tools = '; '.join(name + ': ' + value['output'].splitlines()[0] for name, value in read(args.toolchain).items())
    write('04-C6-C-Verification.md', 'C6-C：最终统一验证', f'''
最终结果由 {link(args.summary)}、{link(args.native / 'result.json')}、
{link(args.isolation / 'results.json')} 读取，不继承旧提交成绩。
工具链：{version_tools}；实际路径和配置以 configure/build/ctest 的 command.json 为准。
Base 与 Declarative 使用显式参数；脚本未硬编码个人 Overlay。

| 门槛 | 结果 |
|---|---|
| 新建 Release、全目标及两个性能/导航探针 | configure/build exit 0 |
| 全量 CTest、保留原 74 名称 | {full['ctest']['passed']}/75；PASS |
| QtTest | {q['passed']} PASS / 0 FAIL / {q['skipped']} SKIP / 0 BLACKLISTED，{full['qtSuites']} 日志 |
| 系统 PATH 原生便携 | 33 套件，{nq[0]}/0/{nq[2]}，四种双 UI DPI 截图、PCM、导航 |
| 启动器主程序关闭 | 实际子进程 exit {launcher['childExit']}，{launcher['closeMs']:.2f} ms，锁释放，无残留 |
| 可选 QML 隔离 | 8/8；缺组件 Widgets 正常 / QML 11 / 锁 3 / 无数据副作用 / 原副本恢复后双入口 |
| 真实中文与空格 | 安装、运行、媒体及测试路径已执行 |
| 数据与授权边界 | 单一 Library/Playback/Queue/连接；既有 schema；Mock-only，真实付费 0 |
| 最终 ZIP 解压、Source、Bundle | 由 assembly 的 RESULT.json 记录最终校验 |

四服务（PASS/FAIL/SKIP/BLACKLISTED）：

| 服务 | 数量 |
|---|---|
{services}

最终性能：每行独立进程，合成元数据共用测试 WAV；未清 OS 缓存。
完整排序强制映射全部行并逐项校验顺序，未用惰性 API 返回时间代替。

| 曲目 | 完整排序+映射 ms | 筛选至帧 ms | 最大曲目委托 | 稳态 private MiB |
|---:|---:|---:|---:|---:|
{performance_table}

10k 队列/播放就绪 {performance[-1]['queueAndPlaybackReadyMs']:.4f} ms；
5000 行歌词读取/模型/帧 {performance[-1]['longLyricsReadModelFrameMs']:.4f} ms，
最大歌词委托 {performance[-1]['longLyricsMaxDelegates']}；播放进度引发模型重置 0。
测量是软件渲染响应指标，不是 FPS、物理显示流畅性或一万真实文件的文件系统基准。
原始性能与截图：{link(args.performance)}。

二进制 SHA256：

{binaries}

失败记录均保留，包括原始动画反向时序失败、无前台桌面的 SendInput 前提失败、
部署工具 Git 路径/Unicode objdump 问题、便携退出延迟及 launcher 检查失败。
退出调查见 `docs/phase-c/c6-c-exit-observation.md` 和 05。
四项跳过、真实声卡与浏览器登录未执行边界见 02、05。
''')
    write('05-C6-RC1-Defects-And-R16-Handoff.md', 'RC1 已知限制与 Harness R16 交接', f'''
本次未发现新的开放产品 P0/P1/P2；历史 R15 的 26 P3 等待独立逐项核对。
开发方修复状态不等于 Harness 独立关闭，正式版本发布仍需 R16。

| 项目 | 状态 | R16 操作 |
|---|---|---|
| R15-01 / 02 | Developer fixed；最终回归 PASS | 净环境全量、并发身份独立重跑后决定关闭 |
| R15-03 | HARNESS INVOCATION ERROR CONFIRMED | 确认夹具真实生成 WAV，正式菜单导入中文目录与文件；不是产品修复 |
| 物理声卡与听音 | NOT EXECUTED；3 个设备依赖测试沿既定条件 SKIP | 交互桌面真实播放/Seek/续播/循环、退出与重开 |
| 音频探测退出延迟 | 测试桌面 Qt WASAPI→AudioSes/RPC 等待有栈证据；未称 Windows 永久修复 | 正常声卡路径多轮启动/退出，确认无 60 s 残留；保存故障时栈和端点信息 |
| 快捷键 | 实际 RegisterHotKey 冲突/回滚/释放与 OS WM_HOTKEY 队列派发 PASS；物理 SendInput NOT EXECUTED | 实际键盘全局/应用内、输入框焦点、冲突提示与恢复 |
| 托盘 | 真实 QSystemTrayIcon 与 QAction PASS；Explorer 鼠标未执行 | 鼠标点击图标、菜单、最小化关闭/显式退出 |
| 真实网络歌词/浏览器登录/CAPTCHA | 本轮未执行；Mock/fake 注意与恢复通过 | 只读站点探测分别记录网络/站点与产品结论；不调用付费 AI |
| C6-D01 无本地歌词弹窗 | INTENTIONALLY DEFERRED，仅记录 | 未获新授权不实现，不记已完成 |
| ≥1h 长稳、ASan/UBSan、Linux、断电/拔设备 | NOT EXECUTED | 按独立验收范围补验 |

退出隔离修正：原有 skip 声卡变量仍构造 QAudioOutput，触发物理设备探测。
现 headless/decode-only 不构造输出对象，音量与设置仍可读写；正常路径使用原有设备。
基础启动等待保持 15 s，改为处理宿主事件；启动器检查保持 20 s，直接等待子进程句柄。
未用延长超时、强制终止、跳过断言或删失败将结果变绿。

必保留的失败证据：`C6-C-preflight-native/Run-20261021T080309Z-5efe654d`、
`Run-20261021T083707Z-af787db2`、`C6-C-launcher-close/Run-20261021T084627Z-b305f270`、
`C6-C-exit-stacks/Run-20261021T084851Z-2ed7f6fe`；均在 {link(root)}。
后者 8 次启动中 4 次延迟（约 18.8/60 s），4 次约 1.8–2.0 s；附 debugger 栈，不能当性能基准。

复现入口：Source ZIP 中 `scripts/verify-phase-c6.py`、`summarize-phase-c6.py`、
`deploy-phase-c.py`、`validate-phase-c-native.py`、`validate-c1-fix1-isolation.py`、
`write-phase-c6-reports.py`、`assemble-phase-c6-release.py`；
显式传 Qt Base/Declarative 前缀和新建 build/data/evidence/output，不使用真实资料库。
准备 final staging 时引用相同 Commit/Tree/指纹/二进制的 native verification-proof；
不对已执行 QA runtime 清理后打包。历史启动协议仍为 `1.5.0`，产品显示版本为 RC1。
''')
    print(json.dumps({'reports': str(args.reports.resolve()), 'commit': head, 'files': 6}, ensure_ascii=False), flush=True)


if __name__ == '__main__':
    main()
