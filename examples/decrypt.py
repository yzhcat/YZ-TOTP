#  解密脚本
#  将输入的十六进制字符串转换为文本
#  cat otpauth.enc | python decrypt.py > otpauth.data
import sys

def main():
    data = sys.stdin.read().strip()
    if not data:
        sys.stderr.write("error: input is empty\n")
        sys.exit(1)
    try:
        plain = bytes.fromhex(data).decode('utf-8')
        sys.stdout.write(plain)
        sys.exit(0)
    except Exception as e:
        sys.stderr.write(f"Decrypt failed: {e}\n")
        sys.exit(1)

if __name__ == "__main__":
    main()