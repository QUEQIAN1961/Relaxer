Relaxer V0.2.2-RC4 Windows x64 私人测试便携版
TEST-UNVERIFIED / 不得公开分发 / 不是 GA

1. 解压
将 ZIP 完整解压到一个新的可写目录。不要直接在压缩包中运行，不要覆盖旧版。
包内没有真实音乐、个人数据库、账户或 API 密钥。测试时使用全新的资料目录。

2. 启动 QML（测试包默认入口）
双击 RelaxerQml.cmd。
命令行也可使用：runtime\Relaxer.exe --ui=qml --data "新的测试资料目录"
如果 Windows 显示未签名程序提示，先核对交付报告中的 SHA-256，再自行决定是否运行。

3. 切换 Widgets
先退出 QML，再双击 RelaxerWidgets.cmd。
命令行：runtime\Relaxer.exe --ui=widgets --data "同一个测试资料目录"
两种界面共用资料库锁，同一资料目录不能同时打开两个实例。
直接双击 runtime\Relaxer.exe 沿用既有 Widgets 默认值；QML 入口由上述 CMD 显式选择。

4. 首次启动器限制
RelaxerStart.exe、RelaxerLockCheck.exe 来自本轮 RC4 构建，均已包含。
首次启动器正向部署仍待完整验收。本私人包刻意不含 relaxer-ready.json。
请使用上面的独立入口，不要为了绕过限制自行伪造 ready marker，也不要运行首次在线部署。
测试候选启动成功不代表启动器、发行授权或公开分发已通过验收。

5. 资料与日志
未指定 --data 时，资料存放在本包根目录 data 下；建议显式指定新的测试目录。
详细启动与退出诊断可在命令提示符运行（先切换到本包目录）：
  mkdir test-logs
  RelaxerQml.cmd --data "%CD%\test-data" --diagnostics "%CD%\test-logs\qml.json" --quit-after 5000
  RelaxerWidgets.cmd --data "%CD%\test-data" --diagnostics "%CD%\test-logs\widgets.json" --quit-after 5000
控制台输出也可重定向至 test-logs。交付方的解压/依赖/启动/退出日志保存在交付目录 logs 和 evidence，未混入运行包。

6. 已知限制与未关闭的发行门
- NV-05 系统级 100% DPI 未验证；未调整宿主 DPI。
- Qt 人工鼠标输入未验证；QtTest 合成首击仍可能丢失，内部机制未定论。
- 真实声卡播放未验证；自动化静音/解码检查不能替代听音。
- RC2 免费 LRCLIB 实网补充检查通过；RC4 的实网歌词与翻译门仍需独立验收，未调用真实付费服务。
- Qt/FFmpeg 许可证、构建材料和对应源码仍未闭环。
- 首次启动器正向部署待完整验收。
- 正式可公开分发的 Win64 包待最终验收。
RC1 的兼容歌词候选可在显式重搜无结果后保留供人工选择；不会自动写入歌词。

7. 依赖与许可
本包使用 Qt 6.11.2 和启用 GPL 的 FFmpeg 9.0.2；不是纯 LGPL/MIT 运行包。
已有 LICENSE、LICENSE-NOTICE.txt、THIRD_PARTY_NOTICES.md、licenses 和 third-party-licenses 已保留。
FFmpeg 7.1 历史源码不能证明 FFmpeg 9.0.2 对应源码完整。
精确二进制来源、版本、许可证和源码缺口见随包 COMPONENTS.json 及交付报告。
仅限所有者私人内部测试。禁止公开上传、镜像、正式发布或标记 Release Ready。

8. RC4 测试稳定性范围
本轮只修复Win32前台判定和动画状态验证；除版本标识外产品C++、QML和数据库逻辑与RC3完全相同。
仅在实际自有测试窗口为Win32前台时允许原生键盘注入；其他状态明确记录环境未执行，线程消息验证独立计数。
动画位移、尺寸变化、实际中间进度、反向连续性和终态均需满足；循环次数只作诊断，截图与写入在采样后执行。
连续3轮完整CTest、针对性重复结果与未关闭门见交付报告，不能据此公开分发或宣布GA。

完整回归发现的媒体夹具初始化条件也已同步：先确认实际播放就绪，再暂停并确认暂停。产品播放逻辑未修改。
