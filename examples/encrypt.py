#  加密脚本
#  将输入的文本转换为十六进制字符串
#  cat otpauth.data | python encrypt.py > otpauth.enc
import sys

def main():
    data = sys.stdin.read()
    if not data:
        sys.stderr.write("error: input is empty\n")
        sys.exit(1)
    try:
        # 将文本转换为十六进制字符串
        hex_str = data.encode('utf-8').hex()
        sys.stdout.write(hex_str)
        sys.exit(0)
    except Exception as e:
        sys.stderr.write(f"Encrypt failed: {e}\n")
        sys.exit(1)

if __name__ == "__main__":
    main()