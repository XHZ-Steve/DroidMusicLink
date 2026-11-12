# SMTC 封面同步修复

## 原因与修改

Qt 的 `QUrl::toLocalFile()` 生成正斜杠路径。原桥接直接传给
`StorageFile::GetFileFromPathAsync()`，在实际 Windows 上返回
`0x800700A1`（ERROR_BAD_PATHNAME）；catch 静默清空 Thumbnail，随后仍返回成功。
因此歌曲文字能够同步，而系统媒体会话没有封面。

现有桥接还在 Qt 界面线程同步等待 WinRT 文件操作。Microsoft 建议不要在
[UI 线程阻塞等待 WinRT 异步操作](https://learn.microsoft.com/en-us/windows/apps/develop/cpp-winrt/concurrency)。

修复包括：

- Qt 边界统一传入原生路径，并附带内容摘要作为封面 revision。
- Native 层兼容旧版正斜杠路径，以有界本地读取取得不超过 2MiB 的封面快照。
- 用 Windows 自带 `SHCreateMemStream`、`CreateRandomAccessStreamOverStream`
  和 `RandomAccessStreamReference::CreateFromStream` 发布独立内存流；不再在 STA 上调用文件异步 `.get()`，也不让 SMTC 持有会被覆盖的封面文件。
- 正常进度更新复用相同摘要的快照；旧 payload 没有 revision 时仍刷新，保持 ABI 兼容。
- 封面不存在或读取失败时清理旧封面；失败不缓存，后续可以重试。
- 缩略图错误通过 HRESULT 返回并记录，仍尝试提交文字和播放状态。

## 实际验证

`scripts/test-smtc-artwork.ps1` 创建独立 `TEST SESSION`，从 Windows 的
GlobalSystemMediaTransportControlsSessionManager 读回该测试会话的缩略图字节。
测试不读取其他播放器的歌曲数据，也不发送媒体控制指令。

旧 DLL：会话被系统识别，Thumbnail 不存在，测试失败。

修复后通过：

1. STA 调用、中文及空格路径、正斜杠路径。
2. 相同文件路径变更封面内容及 revision，系统读回新字节。
3. 发布后覆盖源文件，先前发布的快照保持不变。
4. 旧版无 revision 的 payload 可刷新封面。
5. 已有封面后文件丢失，返回失败且系统不保留旧缩略图。
6. 文件恢复后，相同 revision 能重试成功。
7. 无封面更新正确清空缩略图。

独立路径探针：MTA 上正斜杠路径返回 `0x800700A1`，原生路径返回 `0`。
Qt 回归覆盖原生路径、去除 URL query、摘要随换图更新以及清空封面。
原始日志位于 `artifacts/smtc-artwork-before.log`、`smtc-artwork-after.log` 和
`smtc-qt-tests.log`。

由于修复包含 `WindowsNative.dll`，必须完整更新程序目录，并退出旧进程后重新启动；只替换 EXE 不能完成更新。
