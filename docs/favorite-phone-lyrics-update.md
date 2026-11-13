# 动效、设置首开、收藏与手机歌词（2026-11-13）

## 交互调整

- 首页布局移动 460 → 320 ms；原文聚焦 170 → 95 ms，歌词模式收起总时长约 630 → 415 ms。封面图片交叉淡入淡出上限 240 ms。
- 原文补间不再逐帧请求中间字重的 QFont；复用两个固定字重，仅混合覆盖度和 GPU 缩放。仍保留逐词路径、遮罩、媒体时钟与标题避让。
- 常规设置在启动后单次异步准备；设置入口悬停、打开指定分类时也提前准备对应内容。首批内容完成后 140 ms 淡入与轻位移，仍保留分类缓存与折叠项按需创建。没有常驻预热轮询。

## 收藏

- Android 只在当前会话同时公开 ACTION_SET_RATING 和 RATING_HEART（或用户评分为爱心类型）时声明 favorite 命令，Windows Dock 才显示心形按钮。
- 通过公开 TransportControls.setRating(Rating.newHeartRating(...)) 操作；不猜测播放器私有 CustomAction、不把拇指或五星评分误当成收藏。
- 指令携带会话、标题、歌手、专辑，手机在执行前与当前真实会话再次核对，切歌后拒绝旧指令。保留认证、重放保护、命令能力校验和频率限制。
- 收藏状态仅根据播放器返回的 USER_RATING 更新；不乐观写成成功。状态未知时只能发送“收藏”，直到播放器报告爱心状态后才允许按其实际状态取消。API 接收指令不等于播放器一定执行成功。
- 评分/歌词元数据更新保留当前歌曲已有封面，避免重复清空、编码与重载。

## 手机歌词选项

位置：Windows 设置 → 歌词 → 歌词来源 → 手机歌词。

- 优先手机歌词（默认）：公开歌词可用时先用手机文本，否则继续现有缓存与在线来源；用户手动映射仍优先。
- 仅手机歌词：不自动向在线来源搜索；现有手动选择缓存仍保留优先级。明确手动搜索行为沿用原逻辑。
- 关闭：忽略手机歌词，按原缓存和在线配置工作。

Android 从当前 MediaMetadata / MediaController.extras 中名字明确对应 lyrics/lrc 的公开文本字段读取，兼容公开社区 lyricInfo JSON 的 rawLyric / lyric。有标题、歌手标识的 JSON 必须与当前媒体一致。拒绝过大的文本、控制字符和只有一行的即时状态；同步 LRC 至少有两个时间标签，非同步文本至少有三条非空行。最多 64 KiB，保持现有解析器的逐字、逐行及非同步降级能力。

歌词与歌曲信息放在同一认证快照中，切歌不会把旧消息异步套到新歌。相同会话/歌曲的重复快照省略未变化歌词，暂停或调音量不反复传输全文。未变化文本不触发重复解析或在线查询。切歌、歌词撤回、模式切换都按当前来源策略重新选择，未破坏手动绑定。

这只是播放器主动公开数据时的能力；Android 没有通用的完整歌词标准字段。不能保证网易云、QQ、Spotify、盐播放器等特定版本均能提供完整歌词。不会读取通知正文、应用私有目录、系统数据库，不申请新权限，不使用反射、无障碍抓取或 Root。社区 lyricInfo 不是 Android 平台稳定 API，本项目只读取已有字段，不安装或依赖其 Bridge/Hook。

## 公开接口依据

- Android MediaController.getRatingType：https://developer.android.com/reference/android/media/session/MediaController#getRatingType()
- TransportControls.setRating：https://developer.android.com/reference/android/media/session/MediaController.TransportControls#setRating(android.media.Rating)
- MediaMetadata 公共字段与自定义 key 读取：https://developer.android.com/reference/android/media/MediaMetadata
- 社区播放器自有 lyricInfo 格式：https://github.com/Andrea-lyz/ColorOS-Live-Lyrics-Bridge/blob/main/docs/PLAYER_INTEGRATION.md （仅格式兼容，未复制其实现或引入依赖）

## 本轮验证边界

按用户要求，只构建 Windows 主程序、Android assembleDebug 并打包，没有运行程序、连接设备、运行自动测试或执行视觉审查。收藏及手机歌词是否能在用户当前播放器中生效，需要后续实际连接确认；界面显示的能力来自播放器声明，不代表已完成设备实测。
