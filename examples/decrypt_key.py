#!/usr/bin/env python3
"""
解密脚本 - 支持密码认证
将输入的十六进制字符串转换为文本
使用方法: cat otpauth.enc | python decrypt_key.py > otpauth.txt

这只是一个演示实现，密码/密钥验证是简单的字符串比较。
实际应用中应该使用真正的加密算法（如 AES）。
"""
import sys
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog

def get_password_from_dialog():
    """弹出密码输入对话框"""
    root = tk.Tk()
    root.withdraw()  # 隐藏主窗口
    
    # 设置窗口标题和提示
    password = simpledialog.askstring(
        "认证",
        "请输入解密密码:",
        parent=root,
        show='*'  # 显示为星号
    )
    
    root.destroy()
    return password

def get_keyfile_from_dialog():
    """弹出选择密钥文件对话框"""
    root = tk.Tk()
    root.withdraw()  # 隐藏主窗口
    
    keyfile = filedialog.askopenfilename(
        title="选择密钥文件",
        filetypes=[("Key files", "*.key"), ("All files", "*.*")]
    )
    
    root.destroy()
    return keyfile if keyfile else None

def choose_auth_method():
    """选择认证方式"""
    root = tk.Tk()
    root.title("认证方式")
    root.geometry("300x150")
    root.resizable(False, False)
    
    result = None
    
    def on_password():
        nonlocal result
        result = "password"
        root.destroy()
    
    def on_keyfile():
        nonlocal result
        result = "keyfile"
        root.destroy()
    
    def on_cancel():
        nonlocal result
        result = "cancel"
        root.destroy()
    
    tk.Label(root, text="请选择认证方式:", font=("Arial", 12)).pack(pady=20)
    
    tk.Button(root, text="输入密码", width=15, command=on_password).pack(pady=5)
    tk.Button(root, text="选择密钥文件", width=15, command=on_keyfile).pack(pady=5)
    tk.Button(root, text="取消", width=15, command=on_cancel).pack(pady=5)
    
    root.mainloop()
    return result

def decrypt_with_password(encrypted_data, password):
    """使用密码解密（示例：简单验证）"""
    # 这里是示例实现
    # 实际应用中应该使用真正的加密算法（如 AES）
    
    # 示例：检查密码是否为 "demo"
    if password != "demo":
        raise ValueError("密码错误")
    
    # 实际解密逻辑（这里只是 hex 解码）
    plain = bytes.fromhex(encrypted_data).decode('utf-8')
    return plain

def decrypt_with_keyfile(encrypted_data, keyfile_path):
    """使用密钥文件解密"""
    try:
        with open(keyfile_path, 'r') as f:
            key = f.read().strip()
        
        # 示例：检查密钥文件内容是否为 "demo-key"
        if key != "demo-key":
            raise ValueError("密钥文件无效")
        
        # 实际解密逻辑（这里只是 hex 解码）
        plain = bytes.fromhex(encrypted_data).decode('utf-8')
        return plain
    except FileNotFoundError:
        raise ValueError("密钥文件不存在")

def main():
    # 读取加密数据
    data = sys.stdin.read().strip()
    if not data:
        sys.stderr.write("error: input is empty\n")
        sys.exit(1)
    
    # 选择认证方式
    auth_method = choose_auth_method()
    
    if auth_method == "cancel":
        sys.stderr.write("error: authentication cancelled\n")
        sys.exit(1)
    
    try:
        if auth_method == "password":
            password = get_password_from_dialog()
            if not password:
                sys.stderr.write("error: no password provided\n")
                sys.exit(1)
            plain = decrypt_with_password(data, password)
        
        elif auth_method == "keyfile":
            keyfile = get_keyfile_from_dialog()
            if not keyfile:
                sys.stderr.write("error: no keyfile selected\n")
                sys.exit(1)
            plain = decrypt_with_keyfile(data, keyfile)
        
        else:
            sys.stderr.write("error: invalid auth method\n")
            sys.exit(1)
        
        sys.stdout.write(plain)
        sys.exit(0)
    
    except Exception as e:
        # 显示错误对话框
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror("解密失败", str(e))
        root.destroy()
        sys.stderr.write(f"Decrypt failed: {e}\n")
        sys.exit(1)

if __name__ == "__main__":
    main()