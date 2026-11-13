# DroidMediaLink

[下载 v1.0](https://github.com/XHZ-Steve/DroidMusicLink/releases/tag/v1.0) · [English](README.en.md) · [构建说明](docs/build-and-release.md) · [GitHub Actions](https://github.com/XHZ-Steve/DroidMusicLink/actions)

将 Android 正在播放的音乐带到 Windows 桌面：同步歌曲、封面和进度，控制手机播放，显示同步歌词，并接入 Windows 系统媒体控件。音频仍由手机和原有耳机播放，不进行音频串流。

## 实际界面

Windows 客户端连接真实手机媒体会话的播放画面，使用当前默认外观与歌词配置。

![Windows 实际运行动图](docs/screenshots/windows-demo.gif)

![Windows 实际播放：封面氛围、同步歌词和胶囊控制栏](docs/screenshots/windows-current.png)

Android 实机截图已裁去状态栏；截图与 Windows 画面拍摄于不同时间。

<img src="docs/screenshots/android-current.png" width="360" alt="Android 实机：连接服务与当前媒体" />

## 功能

- **局域网连接**：电脑发现手机，首次核对两端数字并在手机确认；支持保存授权、撤销授权和自动重连。
- **媒体控制**：播放、暂停、切歌、进度和音量同步；收藏按钮仅在播放器提供标准接口时出现。
- **Windows 集成**：SMTC 歌曲与封面、托盘菜单、透明悬浮播放器和置顶桌面歌词。
- **歌词**：本地导入、多来源匹配、手机公开歌词、逐字与逐行同步、翻译补全；主页与桌面歌词共用媒体时间轴。
- **外观**：封面取色、可选 Liquid Glass 材质、胶囊进度条与水玻璃播放按钮；支持中文、英文及设置导入导出。

拖动窗口或悬浮组件时暂停界面状态刷新，结束后恢复最新状态；连接心跳持续运行。默认外观来自维护者实际使用的偏好，不包含配对身份、地址或凭据。

## 开始使用

1. 从 [v1.0 Release](https://github.com/XHZ-Steve/DroidMusicLink/releases/tag/v1.0) 下载 Windows 安装包或便携包，以及 Android APK；使用便携版时完整解压。
2. 手机安装 APK，按照首次引导授权通知使用权与必要后台权限，开启连接服务。
3. 两端连接同一局域网，在电脑设置的“设备与连接”中搜索手机，核对两端数字并在手机确认。
4. 在手机原来的音乐 App 中播放；之后可从电脑控制。关闭主窗口后可通过托盘返回。

Android 被动监听，Windows 主动发现和连接。不需要手工输入地址、证书指纹或配对码。请勿将手机服务端口暴露到公网。

## 兼容性与限制

Windows 使用 C++20、Qt 6 Quick/QML 与 Win32/C++/WinRT；Android 使用 Kotlin、Jetpack Compose 和 Material 3，最低 Android 8.0/API 26。Windows 11 x64 为主要目标。

播放器必须发布标准 MediaSession。收藏、跳转、完整手机歌词等能力因播放器而异，未提供的接口不会被伪装为可用。网络歌词来源可能失败并降级。测试 APK 使用开发签名，Windows 包未做 Authenticode 签名；Actions 上传构建产物，不自动发布 Release。

## 开发

见 [构建与打包](docs/build-and-release.md)、[测试记录](docs/testing.md)、[协议](protocol/README.md)、[架构](docs/architecture.md)、[隐私](docs/privacy.md)及[已知限制](docs/known-limitations.md)。

仓库名称为 DroidMusicLink，产品显示名为 **DroidMediaLink**。本地产物位于 `artifacts/`，不纳入 Git。第三方组件与授权见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)，项目许可证见 [LICENSE](LICENSE)。
