# whwd

Windows Hardware Detection — 类 mhwd 的 Windows 驱动检查与安装工具。

模仿 mhwd（Manjaro Hardware Detection）的设计哲学：检测硬件 → 匹配驱动来源 → 安装。
`whwd` CLI 命令风格也参照 mhwd。

## 状态

骨架阶段（v0.1.0）。已实现：

- [x] 设备枚举（SetupAPI，含当前驱动版本/日期/厂商/类别）
- [x] 运行时特性探测（WUA / pnputil / OS 版本）
- [x] Windows Update 驱动更新列表（IUpdateSearcher）
- [x] CLI 参数解析 + JSON 输出
- [x] GUI（Win32 CreateWindow + ListView，设备按类别分组）
- [x] GitHub Actions 构建（x86 / x64）

未实现：

- [ ] 驱动安装（按需提升 + pnputil / IUpdateInstaller）
- [ ] 厂商适配器（Realtek、Intel，接口已预留）
- [ ] ARM64 构建

## 构建

Windows 10/11（VS2019+）或 GitHub Actions：

```
cmake -B build -A x64
cmake --build build --config Release
```

三架构：`-A Win32`、`-A x64`、`-A ARM64`（arm64 未来启用）。当前 CI 矩阵为 x86 + x64。

系统支持：

| 架构 | 正式支持 | 尽力而为 |
|---|---|---|
| x86 | Windows 7 SP1+ | Windows XP SP3（`-DWHWD_TARGET_XP=ON`，v141_xp 工具集） |
| x86_64 | Windows 7 SP1+ | — |
| arm64 | Windows 10+ | — |

## 用法

```
whwd -d              列出检测到的硬件与已装驱动
whwd -l              列出各来源可用更新
whwd -i <id>         安装更新（未实现）
whwd --json          任意子命令输出 JSON
whwd -h             帮助
whwd -V             版本
```

GUI（`whwd-gui.exe`）与 CLI 共享 core：设备枚举与更新检查在后台线程执行，不阻塞界面；安装按钮接入提权流程后即可用。

## 驱动来源

| 来源 | 状态 |
|---|---|
| Windows Update（WUA COM API） | 可用 |
| Realtek（声卡/网卡） | 占位 |
| Intel（蓝牙/无线） | 占位 |

## 架构

```
whwd/
├── src/
│   ├── core/           # 静态库：设备枚举、特性探测、来源分发
│   │   └── sources/    # 来源适配器（统一接口）
│   ├── cli/            # whwd.exe，mhwd 风格参数 + JSON 输出
│   └── gui/            # whwd-gui.exe，Win32 CreateWindow + ListView
├── manifest/           # UAC asInvoker + 视觉样式 + 系统兼容清单
└── .github/workflows/
```

- CLI 与 GUI 皆无 .NET 依赖（纯 Win32 / SetupAPI / COM）
- 安装采用按需提升（asInvoker，UAC 仅在安装时弹出）
- JSON 是 GUI 与 CLI 的约定接口

## 许可

GNU GPL v3。见 [LICENSE](LICENSE)。

## 提交规范

提交须原子化，格式 `<bracket><action><bracket> <module>: <description>`。

- bracket：`{}` 大改（架构/新模块）、`[]` 中改（功能/重构）、`()` 小改（typo/文档）
- action：`Add` `Fix` `Refactor` `Update` `Remove` `Docs` `Test` `Typo` `Chore`（CamelCase）
- module：小写，全局用 `project`，嵌套用 `/`
- description：英文短句，句号结尾

```
{Add} project: Bootstrap CMake build with MSVC toolchain
[Add] core: Implement device enumeration via SetupAPI
() Docs: Fix typo in README
```