# 高清封面（1.1.0 preview）

## 使用

设置 → 媒体 → 播放器选择中提供“封面画质”：关闭（默认）、自动增强、优先高清。自动模式在手机原图达到目标分辨率时跳过联网；优先高清始终尝试来源，但不会用更低分辨率图片替换原图。“仅使用手机原始封面”优先于联网模式。目标可选 500、1200、2048px；无法获得足够大的原图时不放大冒充高清。

目前唯一联网来源是 MusicBrainz / Cover Art Archive，因此来源优先级仅有这一项，不提供无效的其他来源。可手动重新获取。设置 → 高级 → 缓存与数据可限制封面缓存（32–2048MB）并清理。清理后立即回到手机封面，不触发重新下载；需要时使用手动刷新或切歌。

## 手机与传输

Android 比较 ART_URI、ALBUM_ART_URI、DISPLAY_ICON_URI 中可读取的本地 content/android.resource 图片与发布的 Bitmap，优先更大的有效候选。2048px 内、2MiB 内的有效 URI 图片保持原始编码；Bitmap 不放大，至多缩至 2048px，以 JPEG 92 编码，超过 1.5MiB 时单次降至 85。超大图片有解码上限。读取/编码在 IO 协程中按元数据事件执行，保持已有有界失败重试；不添加周期性任务。

手机不再自行下载 HTTPS 封面。若播放器仅公开远程 URI 而未提供可读本地图片/Bitmap，手机原图可能不可用，可由用户启用 Windows 的来源增强。未变封面不随播放状态重复传输；新连接与曲目变化会发送完整封面。协议 artwork 的 Base64 上限更新为 2796204 字符，链路仍有 4MiB 帧上限。

Windows 不再把手机封面统一转成 1024px JPEG，保留收到的有效字节和尺寸。手机图片与增强图片有独立身份，重复媒体快照不会把高清图降回缩略图。

## Windows 来源与缓存

CoverArtProvider 定义独立来源接口；MusicBrainzCoverArtProvider 实现查询、限速和 CAA 获取。CoverArtService 管理工作线程解码、缓存、取消与容量约束；不与歌词网络任务共用取消状态。

匹配必须有歌曲名、艺术家、专辑与时长。名称及艺术家规范化后须一致，时长容差 2.5–5 秒；Live、Remix、Acoustic、Instrumental、Remaster、Single/Album 等版本标记需一致，拒绝不相符的合集及歧义专辑组。利用唯一可信专辑组的 CAA canonical front 获取封面，而非任意同名歌曲的图片。此策略偏保守，可能漏配，尤其是播放器缺少专辑、协作歌手格式差异或别名时。

歌曲缓存包含元数据身份与目标分辨率；新歌曲仍验证名称、歌手、时长及版本，再按可信 MusicBrainz 专辑组标识复用图片；不靠艺术家/专辑同名直接复用，避免绕过版本校验。图片保存为 PNG，避免再次有损压缩；映射 30 天过期，失败短暂退避，可手动刷新。缓存按文件修改时间淘汰。后台下载限制 10MiB，解码限制 8192 边长/16M 像素；不会为目标清晰度突破资源上限。

手机图立即显示，下载和有界解码在异步任务完成后才发布有效路径，复用现有双纹理交叉淡入淡出。增强时保留手机图的环境色，避免背景突然跳色。切歌、模式变化和清理缓存使旧回调失效；拖动期间暂存高清结果，结束后再发布。任何失败都不会清空已有手机图。

## 接口规则与图片条款

- [MusicBrainz API 限速](https://musicbrainz.org/doc/MusicBrainz_API/Rate_Limiting)：请求间隔至少 1100ms，携带应用版本及项目联系地址作为 User-Agent；429/503 尊重 Retry-After，并有失败退避。
- [CAA API](https://musicbrainz.org/doc/Cover_Art_Archive/API)：经 coverartarchive.org 的 release-group/front 入口，使用官方缩略图或原图；仅接受 HTTPS，跳转限定在 MusicBrainz、CAA 与 archive.org 域。图片与 JSON 都有尺寸、时限和重定向检查。
- [MusicBrainz 数据许可](https://musicbrainz.org/doc/About/Data_License)明确不涵盖封面图片。图片版权仍属于原权利人，接口公开不等于图片获得开源或再分发授权。
- [版权与移除规则](https://musicbrainz.org/doc/Copyright_and_DMCA_Compliance)：CAA 图片问题由 Internet Archive 处理。本实现只在用户启用后用于本机当前媒体显示与可清理缓存，不把下载图片打包进软件或源码。
- 启用联网增强会向 MusicBrainz 发送歌曲名、艺术家和专辑；默认关闭，可仅用手机图。

## 验证与实际限制（2026-11-13）

- Windows 完整构建、原有媒体/歌词/设置回归与新增封面用例；记录见 artifacts/hd-cover-tests.log。
- 受控端到端 HTTP 测试：一个正确版本与六种拒配条件；真实来源格式解析、异步下载解码、歌曲缓存命中不发 HTTP；新歌验证专辑组后复用图片，只查询元数据、不重复下载图片、损坏图片回退、容量淘汰、取消旧回调、清理缓存均验证。匹配测试样本通过不代表真实世界匹配准确率。
- Bridge 回归验证 1600×1600 手机原图不会缩成 1024px，增强后重复手机快照不降级，关闭后恢复原图。
- 实际 CAA 下载成功：Coldplay《A Rush of Blood to the Head》canonical front，1200×1200、169767 字节。把同一真实图缩至 512px 再放大回原尺寸的 RGB MSE 为 120.975；直接使用源分辨率的额外重采样 MSE 为 0。该数字度量人为降采样引入的误差，不是“AI 清晰度”或真实手机输出测量。
- 在受控 HTTP/解码缓存路径中，真实图片冷处理约 83ms，测试进程工作集约 32.4MiB、峰值约 41.9MiB；这不代表完整窗口/GPU 占用，完整窗口运行另有截图与资源记录。
- 实际 MusicBrainz 搜索返回 HTTP 503，已验证不替换现有封面；完整联网自动匹配的正向实测仍受来源服务可用性限制，没有绕过官方服务。CAA 正向下载不等于自动匹配已成功。
- Android 单元测试、lint、assembleDebug 通过；当前 ADB 无设备，未声称已测真实手机清晰度、能耗或所有播放器 URI 行为。


完整窗口的隔离预览运行（同一真实 CAA 图像，模拟 512px 基线与 1200px 源尺寸，不冒充真机自动增强）：两个窗口均正常退出且无 QML 错误。约 7 秒冷启动/渲染期间，采样峰值工作集分别为 261.3MiB 与 279.2MiB（增加约 17.9MiB），进程 CPU 总时间分别为 2.17s 与 2.44s。该数值包含 Qt/材质/字体初始化和截图，不是稳态 CPU 或 GPU 显存；设置窗口也正常运行，采样峰值工作集 263.5MiB。截图在 artifacts/hd-baseline.png、hd-high.png、hd-settings.png，原始资源记录在 artifacts/hd-window-resource-results.json。
