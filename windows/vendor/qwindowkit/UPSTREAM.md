# QWindowKit

Official repository: https://github.com/stdware/qwindowkit
Pinned commit: 2072958b04820e9e7629f6c40068dee0ce0e9915
Project version: 1.5.1.0; retrieved 2026-11-12.
Apache-2.0 LICENSE and source copyright/SPDX notices retained.
Runtime source is unmodified. Only the Core and Quick modules are statically linked;
Qt itself remains dynamically linked. Qt private APIs are version sensitive and this
integration must be rebuilt and verified for the exact packaged Qt version.

NativeWindowController is the app adapter. It installs QuickWindowAgent once per
window, registers caption system button roles for native Windows hit testing and
Snap Layout, and marks interactive titlebar items through setHitTestVisible.
Caption button actions remain explicit QML calls. Do not change window flags after
attachment; give floating windows their intended flags before component completion.
