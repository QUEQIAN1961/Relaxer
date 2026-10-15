# Phase B0：核心行为契约冻结

## 1. 冻结范围与基线

本契约对应 Phase A 提交 7769139db79d53b533a03e39df09085c29527d3f，在独立分支 phase-b/b0-b1 上建立。它约束 B1 的构建边界拆分，也作为后续 B2—B9 提取工作的行为基准。

B0 只新增本契约文件，不改业务实现、数据库、界面、正式版本号、更新日志、启动器或运行时。

修改前，在原有 Phase A 状态上使用 MSYS2 UCRT64 的 CMake 4.4.4、Ninja 1.13.2、GCC 16.2.0，以 Release 重新构建并运行完整离线 CTest：**21/21 组通过，134.20 秒**。日志位于 test/phase-a-20261015/logs/phase-b0-baseline-build.log 和 test/phase-a-20261015/logs/phase-b0-baseline-ctest.log。

## 2. 已核对的构建与调用边界

| 当前目标 | 当前包含 | 当前直接依赖 | 本轮核对结果 |
| --- | --- | --- | --- |
| relaxer_core | library.cpp、importer.cpp、lyrics.cpp、lyric-text.cpp、playback.cpp | Qt Core、Gui、Sql、Multimedia；Windows 另有 winmm | 不依赖 Qt Widgets；负责现有存储、播放和歌词值逻辑 |
| relaxer_ui | Widgets 页面、lyricslookup.cpp、browser-translation.cpp、lyric-remote.cpp 等 | relaxer_core、Qt Widgets、Svg、Network；Windows 另有 user32、dwmapi | 搜索/翻译编排与 Widgets 对话框同属该目标；两个待拆组件本身没有 QWidget 依赖 |
| Relaxer | main.cpp 与 UI 库 | relaxer_ui | 正式 Widgets 程序入口 |
| Win32 启动目标 | RelaxerStart、RelaxerLockCheck 各自源码 | Win32 系统库或标准 C++ | 与本轮播放器库拆分无关 |

源码核对：

- src/lyricslookup.h 包含 lyric-remote.h，其 Attempt、任务和来源记录使用 relaxer::lyric_remote::Detail 与 Follow 类型。
- src/lyricslookup.cpp 创建 BrowserTranslation(m_library->root(), this)，即翻译服务由 LyricsLookup 作为 QObject 父对象持有；界面信号连接直接调用其 showBrowser()、resume()、cancel()、translate()。
- src/browser-translation.h/.cpp 使用 Qt Core 类型和 Qt Network 的 QTcpSocket；通过成员对象管理 QProcess、socket、timer、token、epoch、内存缓存及锁。析构继续执行取消和浏览器断开清理。
- src/lyric-remote.h/.cpp 提供 begin()、consume() 纯值输入输出协议转换，使用 Qt Core 的 JSON、URL、容器与正则类型；它不拥有网络请求或界面对象。
- src/lyricslookup.cpp 仍拥有搜索请求、响应处理、排序、翻译队列、进度和保存编排。B1 不迁移这些职责。
- src/lyricslookup.cpp 的 Widgets include、窗口行为和候选 UI 留在 relaxer_ui；不把 LyricsLookup 的 QDialog 拆出。

目标依赖方向为：relaxer_ui → relaxer_services → Qt Core/Network。服务目标只编译 browser-translation.cpp 与 lyric-remote.cpp，并由 relaxer_ui 链接。每个 Q_OBJECT 类型只归属一个目标的 AUTOMOC。B1 的独立服务测试只链接服务目标与 Qt Test；实际生成的链接命令和 Windows 导入表须证明它没有 Widgets 依赖。

## 3. 行为冻结条款

### 播放

保持播放、暂停、切歌、顺序/随机、循环、音量、进度恢复、统计和错误处理不变。Playback、Queue、媒体后端及其状态所有权均不在 B0/B1 修改范围。

### 曲库与持久化

保持 SQLite schema、歌曲 ID、歌曲/歌单成员关系、收藏、标签、简介、历史设置键和值编码、播放统计、歌词文件和缓存行为兼容。不清空、迁移或写入用户真实音乐库；所有测试继续用隔离临时目录。

### 歌词搜索与匹配

保持来源与查询顺序、搜索预算、候选顺序/评分/可信度、提前结束、缓存有效期、异步排序、任务代次保护、取消、自动保存门槛和失败恢复行为。LyricsLookup 仍管理窗口状态与搜索任务；B1 只改变两个服务文件的 CMake 所属目标。

### 浏览器翻译

保持 Bing/Baidu 选择、Edge/Chrome 独立进程及独立 Profile、CDP、逐行翻译、自动/手动流程、缓存复用、人工验证暂停/恢复、取消/超时、翻译完成后保存和旧任务过期回调保护。B1 不修改 URL、DOM 脚本、进程启动、任务状态和窗口关闭顺序，不调用在线网站作为默认测试。

BrowserTranslation 继续由 LyricsLookup 直接拥有。token、generation/epoch 及上下文回调的检查时序不得变化。独立服务测试只创建、取消并销毁对象，不启动真实浏览器。

### 界面与启动

保持 Qt Widgets 页面、控件、主题、动画、字体和交互不变。Windows 启动器、首次准备、依赖部署、启动选择与现有 Runtime 均不改动；验证使用隔离目录。

### 线程与对象生命周期

保持 QObject 父子关系、线程归属、QPointer、token、generation、epoch、取消终态、关闭窗口清理和浏览器进程管理。B1 不增加跨线程服务所有权，不将搜索或翻译协调器移出对话框。

## 4. 对应的既有回归入口

修改前完整 CTest 共 21 组，全部通过。与本契约直接相关的组及重点项：

- relaxer_phase03：候选排序与评分计数、可信度/自动保存、提前结束、过期排序结果、停止/关闭丢弃在途响应、评分期间 UI 响应。
- relaxer_phase031：浏览器验证取消和新任务隔离、provider/URL 持久化、双语预览、翻译进度、逐行回退、翻译失败保留旧文本、停止搜索、来源预算、挑战/拒绝响应、延迟恢复取消。
- relaxer_v16、relaxer_v19：搜索/取消、缓存与候选来源、播放队列、延时和歌词行为回归。
- relaxer_v110：当前版本核心库行为。
- 其余核心、GUI、启动器、准备流程和媒体测试：验证改动不会破坏共用构建或部署相关链路。

B1 新增一个离线服务测试组，单独覆盖 BrowserTranslation 对象创建/取消/销毁，以及远程歌词搜索与计时歌词解析；既有 CTest 组保持原名，新增组单独报告。

## 5. 回退与验收标记

B0 以独立提交保存本文件；B1 使用另一个提交记录 CMake 目标拆分和无 Widgets 服务测试。回退 B1 时保留本契约和 Phase A 基线；回退 B0 时不影响后续独立构建提交。

Phase A 的 baseline/windows-accepted-20261015、phase-a/foundation-20261015 标签与既有 Git bundle 均保留。B0/B1 不创建远程仓库、不推送，也不修改正式版本 1.10.0。