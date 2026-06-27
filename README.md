# YZ-TOTP

Windows TOTP 验证码查看器，使用 core-ui 构建的桌面应用。

## 功能

- 从 `otpauth.txt` 或加密文件读取并解析 TOTP 配置
- 支持外部解密程序（可自定义加密/解密逻辑）
- 实时显示当前验证码和剩余时间
- 倒计时 5 秒内验证码变红提醒
- 一键复制验证码到剪贴板
- 支持多种算法（SHA1、SHA256、SHA512）

## 快捷键

- **Esc** - 退出程序
- **Ctrl + W** - 关闭窗口

## 构建

详见 [build.md](build.md)

## 配置

### 数据源优先级

程序按以下优先级确定数据源：

| 优先级 | 来源 | 说明 |
|--------|------|------|
| 1 | `/OTPAUTH:path` | 命令行参数，明文文件 |
| 2 | `/INI_PATH:path` | 命令行参数指定 INI 文件路径 |
| 3 | `totp_viewer.ini` | exe 所在目录的 INI 配置文件 |
| 4 | INI 中 `[decrypt]` | 加密文件 + 解密程序 |
| 5 | INI 中 `[totp_viewer] otpauth` | INI 配置的明文文件路径 |
| 6 | 当前目录 `otpauth.txt` | 默认路径 |
| 7 | exe 目录 `otpauth.txt` | 默认路径 |
| 8 | 自动创建 | 在 exe 目录创建默认文件 |

**规则说明**：
- **显式明文优先**：如果指定了明文 otpauth 文件路径，优先使用明文文件
- **命令行优先**：`/OTPAUTH` 优先级最高，`/INI_PATH` 次之
- **配置文件优先**：INI 配置（参数指定或 exe 目录）优先级高于默认路径

### 命令行参数

| 参数 | 说明 |
|------|------|
| `/OTPAUTH:path` | 指定明文 otpauth 文件路径（最高优先级） |
| `/INI_PATH:path` | 指定 INI 配置文件路径 |
| `/SHOW_SOURCE` | 显示当前使用的数据源信息 |

**注意**：如果同时使用 `/OTPAUTH` 和 `/INI_PATH`，忽略 `/INI_PATH`。

### 外部解密程序

详见 [examples/README.md](examples/README.md)

## 项目结构

```
YZ-TOTP/
├── 3rdparty/          # 第三方依赖
│   └── core-ui/       # core-ui GUI 库
├── lib/               # TOTP 核心库
├── include/           # 头文件目录
├── src/               # 源代码目录
├── examples/          # 解密程序示例
├── out/               # 构建输出目录
├── nob.c              # 构建脚本
├── README.md          # 主文档
├── build.md           # 构建指南
└── examples/README.md # 解密程序说明
```

## 依赖

- [core-ui](https://github.com/ghboke/core-ui) - Windows GUI 库
- [nob](https://github.com/tsoding/nob.h) - 单文件构建系统
- [hash-library](https://create.stephan-brumme.com/hash-library/) - SHA 哈希库
