# Windows 视觉系统与渲染验收 · 2026-11-12

## 实现

Windows 仍使用 Qt Quick Controls Basic 的 Button、Switch、Slider、ComboBox、SpinBox、Dialog、ScrollView 和 ListView。产品外观覆盖这些控件的 background/contentItem/delegate，不重新实现输入、键盘、焦点或范围约束，不混入另一套完整 UI 框架。完整 Lucide 字体与图标包继续保留。

`windows/qml/DesignTokens.qml` 定义文字、表面、强调色、状态色、控件高度、圆角与动效。正文使用 Microsoft YaHei UI，字号层级 12/14/18/24/26/42；标准控件高 40，控件圆角 10，弹窗 24、Dock 28、悬浮播放器 20、托盘 22。标准表面保持克制；玻璃使用自身光学轮廓，不叠加多层模糊阴影。焦点边界独立可见，图标按钮保留可访问名称和 tooltip。

封面经 `CoverPalette.h` 的颜色分布采样产生主色、辅助色与深色，避免把互补颜色整体平均成灰色。黑白封面保持中性；接近黑白的无效像素不会压过有代表性的色群。环境色与播放器品牌强调色分开。背景采用 Qt Quick Shapes 的原生渐变，不使用全屏封面模糊、灰色遮罩或全屏玻璃采样纹理。色场 900ms 插值；封面使用两个真实 Image 缓冲交叉淡入，保留不透明的旧图在下方，避免中途发暗。封面内容摘要不变时复用 URL，暂停状态更新不重新加载封面。

首页依据可用宽高比采用双区域或纵向布局。低矮宽窗压缩文字与歌词行数；低矮窄窗优先保留封面、音乐信息与操作，歌词独立窗口仍可用。Dock 最大宽 660、高 100，并在窄窗内收缩。设备行缩至 68，名称、当前状态与操作分层；设置内容宽度约束为 720，分类常驻搜索下方。搜索模态框根据内容隐式高度布局，避免头部、按钮区挤掉结果行。

## 玻璃组件与缩放修复

继续使用 [shayann07/liquidglass Qt preview](https://github.com/shayann07/liquidglass/tree/7f2814a22c2823ab8bc29fd4aa1808d79829b588/ports/qt)，Apache-2.0，固定提交 `7f2814a22c2823ab8bc29fd4aa1808d79829b588`。生成的 GLSL、material.js 和 GlassEffect 未被换成伪玻璃；适配层调整折射、细节散射、提亮、Fresnel 与方向性高光参数。变更详见 `windows/vendor/liquidglass/UPSTREAM.md`，许可证和 NOTICE 随包保留。

实际发现的尺寸缺陷是：离屏纹理与 sourceRect 随尺寸立即变化，而材质尺寸通过 Qt.callLater 更新，可能在同一帧使用新旧两套尺寸。现统一为立即同步，并共用整数设备像素尺寸；采样同时观察祖先布局变化，避免父容器移动后 sourceRect 过期。移除了全窗口光学层，玻璃仅用于 Dock、弹窗和悬浮窗口。隐藏窗口停用实时采样；不以持续轮询维持布局映射。

标准风格提供普通不透明表面；动画开关可关闭插值。Windows 高对比度状态通过系统 API 和调色板变化事件读取，切换到系统文字/背景并停用光学层。未修改操作系统的安全或显示设置。

## 运行截图

- [真实手机播放首页](screenshots/windows-ui/home-playing.png)
- [最终运行首页](screenshots/windows-ui/home-final.png)
- [真实设备与搜索弹窗](screenshots/windows-ui/search-live-final.png)
- [浅色设置页底部](screenshots/windows-ui/settings-light-end.png)
- [深色设置](screenshots/windows-ui/settings-dark.png)
- [浅色封面](screenshots/windows-ui/pale-light.png)
- [最小窗口](screenshots/windows-ui/minimum.png)
- [悬浮播放器](screenshots/windows-ui/floating.png)、[托盘](screenshots/windows-ui/tray.png)

真实播放截图来自授权连接的 PLU110。其余独立色域用真实专辑图片测试：Taylor Swift《Fearless (Taylor's Version)》、Coldplay《Ghost Stories: Live 2014》、Kanye West《Graduation》、Arctic Monkeys《AM》、Taylor Swift《1989 (Taylor's Version) [Deluxe]》。来源记录在工作区 `artifacts/cover-tests/sources.json`；原始测试封面不作为应用素材打包。隔离预览不伪造媒体状态，因此会显示“暂无媒体”及未连接状态。歌词布局测试文件明确标记 TEST，不代表实际歌曲。

## 验证与性能

环境：Windows、Qt 6.11.1、D3D11、NVIDIA GeForce RTX 5070 Ti Laptop GPU。每组在真实 QWindow 上连续改变尺寸 240 次，范围约 560–1440 × 520–980；同时检查可见玻璃材质尺寸、FBO 尺寸与背景像素连续性。以下是含同步截图读回的诊断运行结果，不等同于 GPU 帧耗时或稳定帧率承诺。

| 缩放 | 可见光学表面采样 | 尺寸错配 | 背景相邻像素最大通道差 | 呈现间隔 P95 | 单核 CPU | 工作集 | 隐藏后单核 CPU |
|---|---:|---:|---:|---:|---:|---:|---:|
| 100% | 637 | 0 | 1 | 13.06ms | 16.32% | 147.77MiB | 0.78% |
| 150% | 640 | 0 | 2 | 13.08ms | 12.31% | 144.49MiB | 0% |
| 200% | 490 | 0 | 1 | 15.92ms | 16.62% | 170.84MiB | 0% |

原始 JSON 位于工作区 `artifacts/ui-audit-{100,150,200}/resize-report.json`。隐藏采样为 2 秒，存在进程时间计数粒度，不能据此宣称永远零占用。未采集进程级 GPU 时间/功耗。

此外通过原生 Windows 鼠标拖动底边、右边，及最大化/还原，实际观察了宽窄布局切换，没有观察到截图中的垂直断层或控件溢出。真实鼠标点击浅色开关即时更新了页面与标题栏，并已恢复原设置。自动化使用原生 Qt 按钮点击验证浅色、动画、主页歌词及玻璃参数联动。设置列表已实际滚动到最后一项，底部操作完整可达。

新 Dock 的真实 Qt 播放按钮完成两次点击，收到认证手机的成功回执和播放状态变化，最后恢复原播放状态。音量显示改为手机真实 volume/maxVolume，未知时禁用。测试中手机发生 DHCP 地址变化；修复了发现新地址后旧连接尝试尚在进行时的切换时序，保存的证书身份和授权保持不变。

`core_tests` 1/1、协议 8 个正向与 13 个拒绝向量、双语资源检查通过。色板回归包括暖色、冷色、互补色、浅色与黑白；真实封面采样平均约 316µs。普通软件渲染后端的标准风格也已实际启动、截图。独立 GPT-6 Luna Medium 视觉审查与 Low 代码审查已执行；依据发现修复了背景层次、音量假初值、列表可达性及设置联动。

## 剩余验证范围

尚未覆盖所有 GPU/驱动、混合 DPI 多屏间拖动、远程桌面、HDR 与完整屏幕阅读器矩阵。Qt 玻璃上游仍是固定版本的预览端口。手机后台/OEM 电池策略、IPv6-only 网络等原有限制仍见 `known-limitations.md`。没有发布远端 Release 或更改仓库远端。
