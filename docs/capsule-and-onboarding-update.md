# 封面布局、胶囊 Dock 与 Android 引导更新

## Windows

- 封面交叉淡入淡出改由 Qt Image 直接绘制；阴影使用 Qt Quick Effects RectangularShadow，图片不再经过 MultiEffect 离屏纹理。窗口缩放、最大化和还原会取消旧布局插值、重新 polish 当前约束，避免旧尺寸快照继续控制封面。
- 外观 → 主题与配色中增加主页封面比例（0.55–1.35，受可用空间约束）、播放控制栏形状与播放按钮风格；默认保留旧样式。
- 胶囊进度轨道沿两段四分之一圆弧及顶部直线绘制；Qt Quick Controls Slider 保留键盘、拖动和无障碍交互。透明水玻璃按钮复用已集成 liquidglass 的折射、畸变与边缘高光。
- 窗口按钮改用 Lucide SVG 固定图标盒，独立于文字字体基线；保留 QWindowKit 拖动、Snap Layout 和系统按钮注册。
- 逐词显示取消每词向上取整加一像素及行宽附加值，保留实际字体 advance 与歌词时序。
- 使用 Qt 6.11+，与现有 6.11.1 CI/打包环境一致。

## Android

- 透明 Scaffold 及内容树统一继承主题 onSurface，修复暗色主题普通文字与图标呈黑色。
- 底部胶囊使用既有 Kyant Backdrop 1.0.1；选中填充移除，扁圆角透镜通过弹簧位移与短暂轻微形变切换，无常驻动画。Android 13 以下与关闭玻璃时保留 Material 降级。
- 设置在同一页面从上到下排列外观、后台、媒体、应用四组，媒体默认折叠；恢复默认增加后果确认，不清除电脑配对授权。
- 新的逐页权限引导覆盖媒体访问、连接通知、电池优化与启动设置。特殊权限返回应用时读实际系统状态；通知授权通过 Activity Result。已授权步骤自动前进，未授权可重试或跳过。电池优化使用系统设置入口，无新增权限。
- 厂商自启动状态没有统一公开读取 API，最后一步明确由用户确认，不伪造授权结果。现有安装升级后可体验新版引导一次，也可从连接与后台重新打开。

## 参考与依赖

- 保留原有 liquidglass、QWindowKit、Lucide 与 Kyant Backdrop 的版本及许可声明；没有引入新的 UI 框架。
- 分页组织参考 FlowBell 的 Compose OnboardingScreen（https://github.com/fathiraz/flowbell/tree/main/app/src/main/kotlin/com/fathiraz/flowbell/presentation/screens/onboarding），仅研究交互结构，未复制源码或增加依赖。
- 权限状态处理依据 Android 官方 Activity Result / 特殊权限文档（https://developer.android.com/training/permissions/requesting-special），使用公开 API，无轮询或 OEM 私有接口。

## 本轮验证边界

按用户要求，本轮只执行 Windows 主程序构建、Android assembleDebug 与产物打包，不执行自动测试、实际交互测试或视觉审查。封面缩放、透明材质、弹性动画和权限返回行为仍需用户运行新版确认；不将编译通过当作视觉或设备回归通过。

## 边缘与移动端布局精修

- 玻璃轮廓覆盖使用屏幕空间导数计算抗锯齿范围，改善小圆按钮的斜向边缘；未添加额外全屏采样或模糊。
- 胶囊轨道复用已有渐隐 Shader，仅对顶部半轮廓建立小尺寸图层；两侧弧线末端柔和消隐，拖动命中区域不变。软件渲染保留清晰轨道降级。
- Android 设置取消切换分类，纵向顺序固定；媒体通过标准按钮与 AnimatedVisibility 展开，展开状态可随页面返回保留。
- 导航选中态改为 72 × 40 dp 的圆角矩形透镜，降低镜面畸变与形变幅度；仍使用原有 Backdrop 光学实现与 Material 导航交互。
- 本次同样只编译和打包，不执行测试与视觉审查。
