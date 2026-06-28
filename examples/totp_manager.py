#!/usr/bin/env python3
"""
TOTP 管理器 - AES 加密版本

两种运行模式：
1. 直接运行：显示 GUI 界面，编辑 TOTP 条目
2. 被调用（stdin 有输入）：静默模式，输入密码后直接输出解密内容

使用 AES-256-CBC 加密，密码通过 PBKDF2 派生密钥。
"""
import sys
import os
import base64
import tkinter as tk
from tkinter import ttk, messagebox, filedialog, simpledialog

try:
    from Crypto.Cipher import AES
    from Crypto.Util.Padding import pad, unpad
    from Crypto.Protocol.KDF import PBKDF2
    HAS_CRYPTO = True
except ImportError:
    HAS_CRYPTO = False

# =============================================================================
# 加密/解密函数
# =============================================================================

def derive_key(password: str, salt: bytes) -> bytes:
    """使用 PBKDF2 从密码派生 AES 密钥"""
    return PBKDF2(password, salt, dkLen=32, count=100000)

def encrypt(plaintext: str, password: str) -> str:
    """AES-256-CBC 加密，盐值与加密数据一起存储

    文件格式: base64(salt(16) + iv(16) + encrypted_data)
    """
    if not HAS_CRYPTO:
        raise RuntimeError("pycryptodome 未安装，请运行: pip install pycryptodome")

    salt = os.urandom(16)
    key = derive_key(password, salt)
    iv = os.urandom(16)
    cipher = AES.new(key, AES.MODE_CBC, iv)

    data = plaintext.encode('utf-8')
    encrypted = cipher.encrypt(pad(data, AES.block_size))

    return base64.b64encode(salt + iv + encrypted).decode('ascii')

def decrypt(ciphertext: str, password: str) -> str:
    """AES-256-CBC 解密，从加密数据中提取盐值

    文件格式: base64(salt(16) + iv(16) + encrypted_data)
    """
    if not HAS_CRYPTO:
        raise RuntimeError("pycryptodome 未安装，请运行: pip install pycryptodome")

    raw = base64.b64decode(ciphertext)
    salt = raw[:16]
    iv = raw[16:32]
    encrypted = raw[32:]

    key = derive_key(password, salt)
    cipher = AES.new(key, AES.MODE_CBC, iv)
    decrypted = cipher.decrypt(encrypted)

    return unpad(decrypted, AES.block_size).decode('utf-8')

# =============================================================================
# GUI 模式
# =============================================================================

class TotpManagerGUI:
    def __init__(self, filename: str):
        self.filename = filename
        self.entries = []
        self.password = None
        self.modified = False

        # 获取密码
        self.password = self.get_password()
        if not self.password:
            sys.exit(1)

        # 加载数据
        self.load_entries()

        # 创建 UI
        self.root = tk.Tk()
        self.root.title("TOTP 管理器")
        self.root.geometry("700x500")

        # 主框架
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)

        # 文件信息
        file_frame = ttk.Frame(main_frame)
        file_frame.pack(fill=tk.X, pady=(0, 10))
        ttk.Label(file_frame, text=f"文件: {self.filename}").pack(side=tk.LEFT)
        ttk.Label(file_frame, text=f"条目数: {len(self.entries)}").pack(side=tk.RIGHT)

        # 列表框和滚动条
        list_frame = ttk.Frame(main_frame)
        list_frame.pack(fill=tk.BOTH, expand=True)

        scrollbar = ttk.Scrollbar(list_frame)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        self.listbox = tk.Listbox(list_frame, font=("Consolas", 10), height=20)
        self.listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.listbox.config(yscrollcommand=scrollbar.set)
        scrollbar.config(command=self.listbox.yview)

        # 按钮框架
        btn_frame = ttk.Frame(main_frame)
        btn_frame.pack(fill=tk.X, pady=(10, 0))

        ttk.Button(btn_frame, text="添加", command=self.add_entry).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_frame, text="编辑", command=self.edit_entry).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_frame, text="删除", command=self.delete_entry).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_frame, text="上移", command=lambda: self.move_entry(-1)).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_frame, text="下移", command=lambda: self.move_entry(1)).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_frame, text="导入", command=self.import_entries).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_frame, text="导出", command=self.export_entries).pack(side=tk.LEFT, padx=2)

        # 保存和取消按钮
        ttk.Button(btn_frame, text="保存", command=self.save).pack(side=tk.RIGHT, padx=2)
        ttk.Button(btn_frame, text="取消", command=self.quit).pack(side=tk.RIGHT, padx=2)

        # 填充列表
        self.refresh_list()

        # 绑定关闭事件
        self.root.protocol("WM_DELETE_WINDOW", self.quit)

        self.root.mainloop()

    def get_password(self) -> str:
        """显示密码对话框"""
        is_new = not os.path.exists(self.filename)
        
        while True:
            if is_new:
                password1 = simpledialog.askstring(
                    "设置初始密码",
                    "请设置初始密码:",
                    show='*',
                    parent=self.root if hasattr(self, 'root') else None
                )
                if password1 is None:
                    return None
                if not password1:
                    messagebox.showwarning("警告", "密码不能为空")
                    continue
                
                password2 = simpledialog.askstring(
                    "确认密码",
                    "请再次输入密码确认:",
                    show='*',
                    parent=self.root if hasattr(self, 'root') else None
                )
                if password2 is None:
                    return None
                if password1 != password2:
                    messagebox.showwarning("警告", "两次输入的密码不一致")
                    continue
                
                return password1
            else:
                password = simpledialog.askstring(
                    "输入密码",
                    "请输入解密密码:",
                    show='*',
                    parent=self.root if hasattr(self, 'root') else None
                )
                if password is None:
                    return None
                if not password:
                    messagebox.showwarning("警告", "密码不能为空")
                    continue
                return password

    def load_entries(self):
        """加载条目"""
        if os.path.exists(self.filename):
            try:
                with open(self.filename, 'r', encoding='utf-8') as f:
                    encrypted = f.read().strip()
                if encrypted:
                    plaintext = decrypt(encrypted, self.password)
                    self.entries = [line.strip() for line in plaintext.split('\n') if line.strip()]
            except Exception as e:
                messagebox.showerror("错误", f"解密失败: {e}")
                sys.exit(1)
        else:
            # 创建示例文件
            self.entries = [
                "otpauth://totp/Test:user@example.com?secret=JBSWY3DPEHPK3PXP&algorithm=SHA1&digits=6&period=30"
            ]
            self.save_to_file()
            messagebox.showinfo("提示", f"已创建新文件: {self.filename}")

    def save_to_file(self):
        """保存到文件"""
        plaintext = '\n'.join(self.entries)
        encrypted = encrypt(plaintext, self.password)
        with open(self.filename, 'w', encoding='utf-8') as f:
            f.write(encrypted)
        self.modified = False

    def refresh_list(self):
        """刷新列表"""
        self.listbox.delete(0, tk.END)
        for i, entry in enumerate(self.entries):
            # 简化显示
            label = entry.replace('otpauth://totp/', '').split('?')[0]
            self.listbox.insert(tk.END, f"{i+1}. {label}")

    def add_entry(self):
        """添加条目"""
        entry = simpledialog.askstring("添加条目", "输入 otpauth URI:", parent=self.root)
        if entry and entry.strip():
            if entry.startswith('otpauth://'):
                self.entries.append(entry.strip())
                self.modified = True
                self.refresh_list()
            else:
                messagebox.showwarning("警告", "请输入有效的 otpauth URI")

    def edit_entry(self):
        """编辑条目"""
        selection = self.listbox.curselection()
        if not selection:
            messagebox.showwarning("警告", "请先选择要编辑的条目")
            return

        idx = selection[0]
        current = self.entries[idx]

        new_entry = simpledialog.askstring("编辑条目", "修改 otpauth URI:", initialvalue=current, parent=self.root)
        if new_entry and new_entry.strip():
            if new_entry.startswith('otpauth://'):
                self.entries[idx] = new_entry.strip()
                self.modified = True
                self.refresh_list()
            else:
                messagebox.showwarning("警告", "请输入有效的 otpauth URI")

    def delete_entry(self):
        """删除条目"""
        selection = self.listbox.curselection()
        if not selection:
            messagebox.showwarning("警告", "请先选择要删除的条目")
            return

        idx = selection[0]
        if messagebox.askyesno("确认", f"确定删除条目 {idx+1}?"):
            del self.entries[idx]
            self.modified = True
            self.refresh_list()

    def move_entry(self, direction: int):
        """移动条目"""
        selection = self.listbox.curselection()
        if not selection:
            messagebox.showwarning("警告", "请先选择要移动的条目")
            return

        idx = selection[0]
        new_idx = idx + direction

        if 0 <= new_idx < len(self.entries):
            self.entries[idx], self.entries[new_idx] = self.entries[new_idx], self.entries[idx]
            self.modified = True
            self.refresh_list()
            self.listbox.select_set(new_idx)

    def import_entries(self):
        """导入条目"""
        file = filedialog.askopenfilename(
            title="选择要导入的文件",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
        )
        if file:
            try:
                with open(file, 'r', encoding='utf-8') as f:
                    for line in f:
                        line = line.strip()
                        if line.startswith('otpauth://'):
                            self.entries.append(line)
                self.modified = True
                self.refresh_list()
                messagebox.showinfo("提示", "导入成功")
            except Exception as e:
                messagebox.showerror("错误", f"导入失败: {e}")

    def export_entries(self):
        """导出条目"""
        file = filedialog.asksaveasfilename(
            title="保存导出的文件",
            defaultextension=".txt",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
        )
        if file:
            try:
                with open(file, 'w', encoding='utf-8') as f:
                    f.write('\n'.join(self.entries))
                messagebox.showinfo("提示", f"导出成功: {file}")
            except Exception as e:
                messagebox.showerror("错误", f"导出失败: {e}")

    def save(self):
        """保存并退出"""
        self.save_to_file()
        messagebox.showinfo("提示", "保存成功")
        self.root.destroy()
        sys.exit(0)

    def quit(self):
        """退出"""
        if self.modified:
            if messagebox.askyesno("确认", "有未保存的更改，确定要退出吗?"):
                self.root.destroy()
                sys.exit(0)
        else:
            self.root.destroy()
            sys.exit(0)

# =============================================================================
# 静默模式（被调用）
# =============================================================================

def silent_mode():
    """静默模式：读取加密数据，输入密码，输出明文"""
    # 读取加密数据
    encrypted = sys.stdin.read().strip()
    if not encrypted:
        sys.stderr.write("error: no encrypted data received\n")
        sys.exit(1)

    # 尝试从环境变量获取密码（可选）
    password = os.environ.get('TOTP_PASSWORD')

    # 如果没有环境变量，尝试命令行参数
    if not password:
        # 检查是否有密码文件
        pwd_file = os.path.join(os.path.dirname(sys.argv[0]) or '.', '.totp_pwd')
        if os.path.exists(pwd_file):
            with open(pwd_file, 'r') as f:
                password = f.read().strip()

    # 如果还是没有，使用 Windows credential 或其他方式
    if not password:
        # 尝试使用 keyring 或其他密码管理器
        try:
            import keyring
            password = keyring.get_password("totp_manager", "password")
        except:
            pass

    # 最后尝试从 stdin 读取密码（如果前一个进程传递了密码）
    if not password:
        # 在 Windows 上使用简单的对话框
        if sys.platform == 'win32':
            import ctypes
            try:
                # 使用 Windows API 显示密码输入对话框
                def get_password_gui():
                    root = tk.Tk()
                    root.withdraw()
                    pwd = simpledialog.askstring("密码", "请输入解密密码:", show='*')
                    root.destroy()
                    return pwd
                password = get_password_gui()
            except:
                pass

    if not password:
        sys.stderr.write("error: no password provided\n")
        sys.exit(1)

    try:
        plaintext = decrypt(encrypted, password)
        sys.stdout.write(plaintext)
        sys.exit(0)
    except Exception as e:
        sys.stderr.write(f"error: decrypt failed: {e}\n")
        sys.exit(1)

# =============================================================================
# 主入口
# =============================================================================

def main():
    if not HAS_CRYPTO:
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror(
            "缺少依赖",
            "请安装 pycryptodome:\n\npip install pycryptodome"
        )
        root.destroy()
        sys.exit(1)

    # 检查是否有 stdin 输入
    if not sys.stdin.isatty():
        # 有 stdin 数据，使用静默模式
        silent_mode()
    else:
        # 无 stdin 数据，启动 GUI 模式
        filename = os.path.join(os.path.dirname(sys.argv[0]) or '.', 'totp_data.enc')
        if len(sys.argv) > 1:
            filename = sys.argv[1]
        TotpManagerGUI(filename)

if __name__ == "__main__":
    main()
