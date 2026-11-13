# Windows 启动修复

ui7 无窗口、无托盘的直接原因：NowPlayingCanvas.qml 的补间文字层同时为整个 font 对象和 font.weight 赋值。QML 运行时报告重复赋值，导致 NowPlayingCanvas / Main 组件加载失败，程序在创建系统托盘之前返回 1。Qt QML 缓存构建未阻止该运行时错误。

- 改为逐项指定字体 family、pixelSize、letterSpacing 与 weight，避免整对象和子属性重复赋值。
- 为胶囊命中遮罩补齐 point / bool 方法签名，避免 Qt 忽略命中区域。
- 修复 CapsuleProgress 的 implicitWidth 依赖回环；为标准 Slider 提供独立隐式尺寸，不再让背景几何反过来决定控件隐式宽度。
- 组件无法创建时显示原生 Qt 错误对话框，不再无提示退出。启动错误收集有数量上限，界面加载结束即断开，不在运行期间无限积累日志。
- 使用实际打包目录运行首页、设置页、托盘与歌词布局启动检查；独立测试配置，不修改真实设备配对或设置。输出见 artifacts/startup-ui8-*.log 与相应 PNG。

本轮未验证实时手机收藏或手机歌词接口，修复范围为 Windows 启动与界面加载。
