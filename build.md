# 构建指南

## 前置条件

- **Visual Studio 2026 Build Tools** - MSVC 编译器
- **GCC** - 用于构建 nob 构建工具

## 构建步骤

### 1. 构建 nob 工具

nob 是一个单文件的 C 构建系统。

```sh
gcc nob.c -o nob.exe
```

### 2. 启动 MSVC 开发环境

```powershell
# PowerShell 方式
pwsh.exe -NoExit "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch x64 -HostArch x64

# 或使用 VS Developer Shell
pwsh.exe -NoExit "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\Launch-VsDevShell.ps1"
# 然后在启动的 shell 中运行: Enter-VsDevShell -Arch x64 -HostArch x64
```

### 3. 编译项目

```sh
.\nob.exe
```

编译产物位于 `out/` 目录：

| 文件 | 说明 |
|------|------|
| `totp_viewer.exe` | 主程序 |
| `core-ui.dll` | GUI 依赖库 |
| `otpauth.txt` | 默认配置文件 |

## 运行

```powershell
cd out
.\totp_viewer.exe
```
