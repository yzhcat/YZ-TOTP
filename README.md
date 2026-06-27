# YZ-TOTP

Windows TOTP 验证码查看器，使用 core-ui 构建的桌面应用。

## 功能

- 从 `otpauth.txt` 读取并解析 TOTP 配置
- 实时显示当前验证码和剩余时间
- 一键复制验证码到剪贴板
- 支持多种算法（SHA1、SHA256、SHA512）

## 快捷键

- **Esc** - 退出程序
- **Ctrl + W** - 关闭窗口

## 构建

### 环境要求

- Visual Studio 2026 Build Tools (x64)
- GCC (用于编译 nob 构建脚本)

### 构建步骤

```powershell
# 1. 使用 Developer PowerShell 启动 MSVC 环境
pwsh.exe -NoExit "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch x64 -HostArch x64

# 2. 编译构建脚本
gcc nob.c -o nob.exe

# 3. 执行构建
.\nob.exe

# 4. 运行程序
.\out\totp_viewer.exe
```

### 构建输出

- `build/` - 中间文件 (.obj)
- `out/totp_viewer.exe` - 主程序
- `out/core-ui.dll` - 运行时依赖

## 配置

### otpauth.txt 格式

每行一个 otpauth URI：
Reference: https://github.com/google/google-authenticator/wiki/Key-Uri-Format

```
otpauth://totp/Issuer:account@example.com?secret=JBSWY3DPEHPK3PXP&algorithm=SHA1&digits=6&period=30
otpauth://totp/Another:user@test.com?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&algorithm=SHA512&digits=8&period=60
```

### 配置项

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `secret` | Base32 编码的密钥 | 必填 |
| `algorithm` | 算法 (SHA1/SHA256/SHA512) | SHA1 |
| `digits` | 验证码位数 (6/8) | 6 |
| `period` | 更新周期（秒） | 30 |

## 项目结构

```
YZ-TOTP/
├── 3rdparty/          # 第三方依赖
│   └── core-ui/       # core-ui 库 (符号链接)
├── lib/               # TOTP 核心库 (符号链接)
│   ├── totp.hpp       # TOTP 算法实现
│   ├── base32.h       # Base32 编解码
│   ├── otpauth.h      # otpauth URI 解析
│   └── hash-library/  # SHA 哈希库
├── main.cpp           # 主程序
├── nob.c              # 构建脚本
├── nob.h              # nob 构建系统
├── otpauth.txt        # TOTP 配置文件
└── out/               # 构建输出
```

## 依赖

- [core-ui](https://github.com/...) - Windows GUI 库
- [nob](https://github.com/tsoding/nob.h) - 单文件构建系统

## License

MIT