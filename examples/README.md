# 自定义解密程序

totp_viewer 本身不处理数据加密和存储，只负责显示数据。如果需要加密存储，可以配合外部解密程序使用。

## 工作原理

```
┌─────────────┐    读取    ┌─────────────┐    stdin     ┌─────────────┐
│ totp_viewer │ ────────→  │ 加密文件    │ ────────→   │ 解密程序    │
│             │            │ (otpauth.enc)│             │ (decrypt.py)│
└─────────────┘            └─────────────┘             └──────┬──────┘
                                                            │
                                                           stdout
                                                            ↓
                                                    ┌─────────────┐
                                                    │ otpauth URI │
                                                    │  明文数据   │
                                                    └─────────────┘
```

1. totp_viewer 读取加密文件内容（二进制）
2. 将加密数据通过 stdin 传递给解密程序
3. 解密程序解密数据，通过 stdout 返回明文
4. totp_viewer 解析明文并显示 TOTP 验证码

## 接口规范

解密程序需要遵循以下规范：

| 规范 | 说明 |
|------|------|
| 输入 | 从 stdin 读取加密数据（二进制安全） |
| 输出 | 将解密后的文本写入 stdout |
| 返回值 | 成功返回 0，失败返回非零 |
| 错误信息 | 写入 stderr（可选） |

## 配置文件

totp_viewer 通过 `totp_viewer.ini` 配置解密程序：

```ini
[totp_viewer]

[decrypt]
; 解密程序路径（可以是相对路径或绝对路径）
program = python3 decrypt.py
; 加密数据文件路径
file = otpauth.enc
```

### 配置查找顺序

1. **命令行参数** `/INI_PATH:path`（最高优先级）
2. **exe 所在目录** `totp_viewer.ini`
3. **当前工作目录** `totp_viewer.ini`

## 示例文件

本目录包含以下示例文件：

| 文件 | 说明 |
|------|------|
| `totp_viewer.ini` | 配置文件 |
| `decrypt.py` | 解密脚本（hex 编码） |
| `encrypt.py` | 加密脚本（hex 编码） |
| `otpauth.enc` | 加密后的 otpauth 数据（hex 格式） |
| `otpauth.data` | 原始 otpauth 数据（用于对比） |

## 使用方法

### 方式一：使用 INI 配置文件

```powershell
# 进入 examples 目录
cd examples

# 使用默认配置（totp_viewer.ini）
../out/totp_viewer.exe

# 或指定 INI 路径
../out/totp_viewer.exe /INI_PATH:./totp_viewer.ini
```

### 方式二：手动测试解密程序

```powershell
# 加密数据
cat otpauth.data | python3 encrypt.py > otpauth.enc

# 解密数据
cat otpauth.enc | python3 decrypt.py

# 输出应为 otpauth.data 的内容
```

## 自定义解密程序示例

### Python (hex 编码)

```python
#!/usr/bin/env python3
import sys

def main():
    data = sys.stdin.read().strip()
    if not data:
        sys.stderr.write("Error: empty input\n")
        sys.exit(1)
    try:
        # 将 hex 解码为文本
        plain = bytes.fromhex(data).decode('utf-8')
        sys.stdout.write(plain)
        sys.exit(0)
    except Exception as e:
        sys.stderr.write(f"Decrypt failed: {e}\n")
        sys.exit(1)

if __name__ == "__main__":
    main()
```

### Python (AES 解密示例)

```python
#!/usr/bin/env python3
import sys
import base64
from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad

KEY = b'your-32-byte-key-here!!'  # 替换为你的密钥

def main():
    data = sys.stdin.buffer.read()
    if not data:
        sys.stderr.write("Error: empty input\n")
        sys.exit(1)
    try:
        # base64 解码
        encrypted = base64.b64decode(data)
        # AES 解密
        cipher = AES.new(KEY, AES.MODE_ECB)
        plain = unpad(cipher.decrypt(encrypted), AES.block_size)
        sys.stdout.write(plain.decode('utf-8'))
        sys.exit(0)
    except Exception as e:
        sys.stderr.write(f"Decrypt failed: {e}\n")
        sys.exit(1)

if __name__ == "__main__":
    main()
```

## 注意事项

1. **路径**：program 可以是相对路径（相对于 INI 文件目录）或绝对路径
2. **加密格式**：本示例使用 hex 编码，便于测试和调试
3. **安全性**：实际使用时建议使用更强的加密算法（如 AES）和密钥管理方案
4. **错误处理**：解密失败时程序会显示错误消息并退出
