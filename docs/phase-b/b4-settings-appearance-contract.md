# Phase B4：设置与外观行为冻结契约

唯一基准 f1a60d4a3a3ffd54f9e5b02cf2c2bf8ea1f8555b（B3 主线审核通过）。本契约和新增回归必须在拆分前通过实际 B3 静态库运行并提交。

## 实际源码与依赖

- Library::open（library.cpp:60）建立本对象的命名 SQLite 连接，INSERT OR IGNORE theme='[2]'，不覆盖旧值；其余现有 schema/migration 保留不动。
- Library::setting（518）读取 JSON 文档数组的第一个元素；缺失、非数组、空数组、解析失败返回调用方 fallback。数组中的 null 保持 null，不换成 fallback。
- setSetting（527）以 QJsonArray{value} 的 Compact JSON UPSERT；setSettings（532）使用当前连接 transaction/rollback/commit 并调用原单写。二者不发送 changed/statsChanged。Library 的 deferChanges 只延迟其他业务操作通知，不能为 facade 新发通知。
- 默认值：新库 theme=2 由 Library.open 决定；其他键由各调用者 fallback 决定。保留 Window.applyTheme 和 AppearancePage 现有 fallback，即使与建库默认值不同也不统一重写。
- AppearanceStore（appearance.cpp:160起）当前与 QWidget 页面同编译单元，但 Store 自身只有 Library*，不持有窗口、页面或 QLabel；state/active/draft/save/use/rename/remove/withProgressColors/initializeProgressColors 均不操作 QWidget。
- state 过滤无id、重复id、非法mode，返回 items/active；只是读，不重写数据库。原实现构造规范化wrapper，历史未知wrapper顶层键不会参与后续保存，这是既有策略，不在本轮扩充。合法主题item里的未知嵌套字段及其他未知设置键应完整保留。
- draft 返回补默认值和图片绝对引用的副本，不写库；页面当前编辑副本 m_draft/m_dirty、100ms预览计时器、Window临时preview/current状态仍属于UI会话，不能再造权威设置缓存。
- save 校验颜色、复制并按PNG内容SHA256命名图片、用QSaveFile提交、清除source字段、约束blur0..50，再原子更新 appearanceHistory/theme=3；失败只清理本次创建的图片，不移除旧图。重名覆盖保留id/name；新建UUID与历史数量命名保留。
- use→theme3；删除非活跃项不改theme；删除活跃项→theme0。保留此原有浅色回退，不改成跟随系统。
- imagePath 保留 Appearance/前缀、cleanPath与canonical路径边界；图片原有QImageReader自动旋转/尺寸上限/缩放、QImage→PNG编码、颜色推荐、模糊算法必须字节原样移动。
- Store 的颜色与图片处理需要 Qt Gui（QColor/QImage/QImageReader），无 QPixmap。Surface::paintEvent 与页面缩略图的 QPixmap/QPainter/控件更新保留 Widgets GUI 线程。
- Window构造初始化历史进度颜色并applyTheme；colorSchemeChanged在theme2且无preview时singleShot(0)刷新；Windows真实明暗由注册表AppsUseLightTheme读取。Window继续负责palette/CSS/Surface和控件刷新，既有通知和刷新顺序不动。
- Preview由页面回调→Window临时preview→applyTheme，取消/不保存清除临时值并恢复，保存先持久化成功再restore。QMessageBox/QFileDialog/QColorDialog和交互定时器均留UI。

## 最小构建边界

新增无缓存、无QObject的 SettingsStore（get/set/setMany），借用同一Library与线程，直接委托原实现，不建立连接、不复制JSON编码或事务、不给字段强加类型检查。

AppearanceStore拆为独立header/cpp，经SettingsStore读取与提交设置，路径/error仍委托同一Library。图像值函数独立为appearance-image.h/cpp，Surface/Page继续从appearance.h兼容包含使用。

Library::saveLyrics已有对Lyrics::parse/simplifyTraditionalLyrics的依赖；不能为追求小型新库而造成static库环或搬迁其他业务。本轮优先把新门面、原Store和原图像值函数归入已有relaxer_core，不改Library、歌词或Playback实现。该target现有公开依赖Core/Gui/Sql/Multimedia/domain，不能声称整个core只有Core/Sql。以无Widgets独立配置、真实链接行/导入表检验界限。

## 冻结回归

settings-appearance-tests.cpp在基线通过Legacy adapter调用实际Library，兼容组包含原appearance.h；后续相同源码、相同断言还以SettingsStore+独立header在QCoreApplication下运行。固定断言覆盖raw JSON、null/错误/缺失、未知嵌套键、旧库、连接数、零设置通知、事务失败、外观历史/重命名/覆盖/图片路径/草稿副本/失败清理/多次重开。所有库和图均临时生成。

settings-appearance-widgets-tests.cpp冻结原页面保存、脏预览取消/不保存/保存与system信号的异步刷新。系统测试只读当前Windows偏好并发出Qt通知，不写真实系统主题或用户配置。真实Windows主题翻转的人工交互不能谎称完成。

既有V11/V12/V13/V16/PhaseA兼容、Phase03/031、B1服务、B2Queue和B3匹配用例不改、不断言降级。最终仅B4，版本1.10.0不变。
