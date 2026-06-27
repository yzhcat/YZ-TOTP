import sys

def main():
    data = sys.stdin.read().strip()
    if not data:
        sys.stderr.write("error: input is empty\n")
        sys.exit(1)
    try:
        # 十六进制转字节
        plain = bytes.fromhex(data).decode('utf-8')
        sys.stdout.write(plain)
        sys.exit(0)
    except Exception as e:
        sys.stderr.write(f"error: {e}\n")
        sys.exit(1)

if __name__ == "__main__":
    main()