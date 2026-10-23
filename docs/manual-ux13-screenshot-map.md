# V0.2.1 手动体验修复：截图与模块映射

基线：`b5c66ebeea3319e4197ade08971afc06fb4825e1`。独立分支：`fix/v021-manual-ux-13`。
原图来源：用户指定 `C:/Users/email/Desktop/picture/1.png` 至 `15.png`。p6–p15 与原会话导出的十张图一致；p1–p5 已逐张查看。原图和 SHA256 在交接包 evidence/reference-screenshots 中保存。历史基线与旧报告不改写。

| 图 | 亲眼核对的内容 | 对应代码与反馈 |
| --- | --- | --- |
| p1 | “使用所选歌词版本？”每次保存前弹出 | LyricsWorkspace.qml 的 replaceConfirm；反馈1 |
| p2 | 黄色待确认歌曲、第一项“最推荐”选中并在右侧预览 | LyricsSearchBridge 的候选快照与 LyricsWorkspace.qml；反馈2、3 |
| p3 | 三个竖排筛选/多选，推荐尚未选中、使用按钮禁用 | LyricsWorkspace.qml 左栏；反馈4 |
| p4 | 更多操作内的纯音乐菜单项 | LyricsWorkspace.qml moreMenu；反馈5 |
| p5 | 中栏底部“使用选中版本”整行按钮 | LyricsWorkspace.qml；反馈5 |
| p6 | 搜索页顶部返回、三栏、底部大块留白 | LyricsWorkspace.qml 页头/页脚与布局；反馈6 |
| p7 | 手动滚动时仅有时间按钮，没有定位高亮与横线 | LyricsList.qml；反馈7 |
| p8 | 网易云参照：滚动到的歌词高亮、时间、横向定位线 | LyricsList.qml；反馈7，仅作交互参照 |
| p9 | 未保存外观弹窗按钮贴边且分散 | AppearancePage.qml unsaved；反馈9 |
| p10 | 浅色模式保存/恢复按钮占第二行 | AppearancePage.qml 顶部工具栏；反馈10 |
| p11 | QML 简版选色器 | AppearancePage.qml ColorDialog；反馈11 |
| p12 | 旧版 QColorDialog，基本/自定义颜色、取色、HSV/RGB/HTML | appearance.cpp 的现有 Widgets QColorDialog；反馈11 |
| p13 | 旧版外观上半部：均分模式按钮、三列色块及当前颜色 | AppearancePage.qml；反馈12 |
| p14 | 旧版外观下半部：图片路径、共用选项、三列模糊滑块与数值 | AppearancePage.qml；反馈12 |
| p15 | 新版图片模式的“底栏与页面共用图片”开关 | Main.qml、PlayerBar.qml；反馈13。图中路径为空，不能仅凭该图判断实际图片裁剪；代码核对确认原实现各区域独立裁剪。 |

反馈8没有单独标注截图：Main.qml 的 navigateOriginal/openPlaylist 无重复目标短路，重复点击仍进入快照动画；以代码和定向交互验证。

用户补充确认：反馈4进入多选后，第三项改为“取消”，右侧第四项为“全选”，动作是选中当前筛选下全部歌曲。

实现以截图中可见控件及旧版现有代码为依据。选色器复用 Qt Widgets 完整非原生对话框；图片共享沿用旧 Widgets 的整窗画布坐标（左右分图时右图和底栏共享整窗画布，左侧独立）。只有正常歌词保存免重复确认；确有翻译在运行或未保存预览时，保留该冲突的定向提示。

本轮仅运行必要构建、定向用例与一次候选启动冒烟。禁止全量 CTest/QtTest、47启动矩阵、重复稳定性、WASAPI；全量验收交给独立 Harness。不得 Tag、Push、Release。
