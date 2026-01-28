#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自动OLED汉字字模生成工具
功能:自动检索工程中User和HardWare文件夹下所有文件中的OLED_Printf函数中的中文，
     生成对应的字模数据，并替换到OLED_Data.c文件的中文字库中
"""
import os
import re
import json
import zipfile
from PIL import Image, ImageDraw, ImageFont
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import ctypes
import matplotlib.font_manager as fm 
ctypes.windll.user32.ShowWindow(ctypes.windll.kernel32.GetConsoleWindow(), 0)

# 配置文件相关常量
CONFIG_FILE_NAME = "config.json"  # 配置文件名
CONFIG_FILE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), CONFIG_FILE_NAME)  # 配置文件路径

# 默认配置
DEFAULT_FONT_NAME = "SimHei"  # 默认字体名称
OLED_DATA_FILE = "./HardWare/OLED_Data.c"  # OLED数据文件路径
FONT_SIZE = 16

# 字体名称映射表（中文 -> 英文/拼音）
FONT_NAME_MAP = {
    "黑体": "SimHei",
    "微软雅黑": "Microsoft YaHei",
    "宋体": "SimSun",
    "仿宋": "FangSong",
    "楷体": "KaiTi",
    "华文宋体": "STSong",
    "华文中宋": "STZhongsong",
    "华文楷体": "STKaiti",
    "华文细黑": "STXihei",
    "华文仿宋": "STFangsong",
    "华文彩云": "STCaiyun",
    "华文行楷": "STXingkai",
    "Arial Unicode MS": "Arial Unicode MS",
    "幼圆": "SimYou",
    "新宋体": "NSimSun",
}

# 反转映射表（英文/拼音 -> 中文）
REVERSE_FONT_MAP = {v: k for k, v in FONT_NAME_MAP.items()}

# 配置类，用于保存用户设置
class GeneratorConfig:
    def __init__(self):
        self.font_name = DEFAULT_FONT_NAME
        self.font_name_var = tk.StringVar()
        self.font_size = FONT_SIZE
        self.mode = "列行式"
        self.code_type = "阳码"
        self.bit_order = "低位在前"
        self.last_project_path = "."
        # 新增配置项
        self.input_chars = ""  # 手动输入的汉字
        self.generate_mode = "仅搜索代码中的汉字"  # 生成方式
        self.clear_existing = False  # 是否清空现有字模
        self.duplicate_handle = "覆盖"  # 重复处理方式
        self.remember_choice = False  # 是否记住选择
        self.load_from_script()

    def save_to_script(self):
        """保存配置到外部config.json文件"""
        try:
            config_dict = {
                'font_name': self.font_name,
                'font_size': self.font_size,
                'mode': self.mode,
                'code_type': self.code_type,
                'bit_order': self.bit_order,
                'last_project_path': self.last_project_path,
                'input_chars': self.input_chars,
                'generate_mode': self.generate_mode,
                'clear_existing': self.clear_existing,
                'duplicate_handle': self.duplicate_handle,
                'remember_choice': self.remember_choice
            }
            
            # 确保配置文件目录存在
            config_dir = os.path.dirname(CONFIG_FILE_PATH)
            if not os.path.exists(config_dir):
                os.makedirs(config_dir)
            
            # 写入配置文件
            with open(CONFIG_FILE_PATH, 'w', encoding='utf-8') as f:
                json.dump(config_dict, f, indent=4, ensure_ascii=False)
            
            print(f"配置已保存到: {CONFIG_FILE_PATH}")
            return True
        except Exception as e:
            print(f"保存配置失败: {e}")
            return False

    def load_from_script(self):
        """从外部config.json文件加载配置，如果文件不存在则自动创建"""
        try:
            # 检查配置文件是否存在
            if not os.path.exists(CONFIG_FILE_PATH):
                # 配置文件不存在，创建默认配置
                print(f"配置文件不存在，创建默认配置到: {CONFIG_FILE_PATH}")
                
                # 创建默认配置字典
                default_config = {
                    'font_name': self.font_name,
                    'font_size': self.font_size,
                    'mode': self.mode,
                    'code_type': self.code_type,
                    'bit_order': self.bit_order,
                    'last_project_path': self.last_project_path,
                    'input_chars': self.input_chars,
                    'generate_mode': self.generate_mode,
                    'clear_existing': self.clear_existing,
                    'duplicate_handle': self.duplicate_handle,
                    'remember_choice': self.remember_choice
                }
                
                # 确保配置文件目录存在
                config_dir = os.path.dirname(CONFIG_FILE_PATH)
                if not os.path.exists(config_dir):
                    os.makedirs(config_dir)
                
                # 写入默认配置文件
                with open(CONFIG_FILE_PATH, 'w', encoding='utf-8') as f:
                    json.dump(default_config, f, indent=4, ensure_ascii=False)
            
            # 读取配置文件
            with open(CONFIG_FILE_PATH, 'r', encoding='utf-8') as f:
                config_dict = json.load(f)
                
            # 更新配置
            self.font_name = config_dict.get('font_name', self.font_name)
            self.font_size = config_dict.get('font_size', self.font_size)
            self.mode = config_dict.get('mode', self.mode)
            self.code_type = config_dict.get('code_type', self.code_type)
            self.bit_order = config_dict.get('bit_order', self.bit_order)
            self.last_project_path = config_dict.get('last_project_path', self.last_project_path)
            self.input_chars = config_dict.get('input_chars', self.input_chars)
            self.generate_mode = config_dict.get('generate_mode', self.generate_mode)
            self.clear_existing = config_dict.get('clear_existing', self.clear_existing)
            self.duplicate_handle = config_dict.get('duplicate_handle', self.duplicate_handle)
            self.remember_choice = config_dict.get('remember_choice', self.remember_choice)
            
            print(f"配置已从 {CONFIG_FILE_PATH} 加载")
            return self
        except Exception as e:
            print(f"加载配置失败: {e}")
            return self

class OLEDCharGenerator:
    def __init__(self, config):
        """初始化字模生成器"""
        self.config = config
        try:
            # 加载字体
            self.font = ImageFont.truetype(config.font_name, config.font_size)
        except IOError:
            # 如果直接通过名称加载失败，尝试查找字体文件
            try:
                # 查找字体文件路径
                font_path = self._find_font_path(config.font_name)
                if font_path:
                    self.font = ImageFont.truetype(font_path, config.font_size)
                else:
                    error_msg = f"无法加载字体 '{config.font_name}'"
                    print(error_msg)
                    messagebox.showerror("错误", error_msg + "\n请确保字体已安装，或在配置界面中选择其他字体")
                    raise
            except Exception as e:
                error_msg = f"无法加载字体 '{config.font_name}': {str(e)}"
                print(error_msg)
                messagebox.showerror("错误", error_msg + "\n请确保字体已安装，或在配置界面中选择其他字体")
                raise

    def _find_font_path(self, font_name):
        """查找字体文件路径"""
        # 检查是否是中文名称，如果是则转换为对应的英文名称
        english_font_name = FONT_NAME_MAP.get(font_name, font_name)
        
        # 首先尝试使用matplotlib的findSystemFonts
        fonts = fm.findSystemFonts()
        for font in fonts:
            if english_font_name.lower() in font.lower() or font_name.lower() in font.lower():
                return font
        
        # 尝试Windows系统字体目录
        windows_font_dir = "C:/Windows/Fonts"
        if os.path.exists(windows_font_dir):
            for font_file in os.listdir(windows_font_dir):
                if (english_font_name.lower() in font_file.lower() or 
                    font_name.lower() in font_file.lower()) and (font_file.endswith('.ttf') or font_file.endswith('.ttc') or 
                    font_file.endswith('.otf')):
                        return os.path.join(windows_font_dir, font_file)
        
        # 尝试其他可能的字体目录
        other_font_dirs = [
            "D:/python/python 3.15/Lib/site-packages/matplotlib/mpl-data/fonts/ttf",
            os.path.expanduser("~/.fonts")
        ]
        
        for font_dir in other_font_dirs:
            if os.path.exists(font_dir):
                for font_file in os.listdir(font_dir):
                    if (english_font_name.lower() in font_file.lower() or 
                        font_name.lower() in font_file.lower()) and (font_file.endswith('.ttf') or font_file.endswith('.ttc') or 
                        font_file.endswith('.otf')):
                            return os.path.join(font_dir, font_file)
        
        return None

    def generate_char_bitmap(self, char):
        """生成单个汉字的16x16点阵数据"""
        # 创建图像对象，模式为1位黑白
        image = Image.new('1', (self.config.font_size, self.config.font_size), 0)
        draw = ImageDraw.Draw(image)

        # 计算文字位置，居中显示
        bbox = draw.textbbox((0, 0), char, font=self.font)
        width = bbox[2] - bbox[0]
        height = bbox[3] - bbox[1]
        x = (self.config.font_size - width) // 2
        y = (self.config.font_size - height) // 2

        # 绘制文字，白色(1)为文字，黑色(0)为背景
        draw.text((x, y), char, font=self.font, fill=1)

        # 转换为点阵数据
        bitmap_data = []

        if self.config.mode == "列行式":
            # 先处理上半部分8行
            for col in range(self.config.font_size):
                byte1 = 0
                for row in range(8):
                    # 获取像素值
                    pixel = image.getpixel((col, row))
                    if self.config.code_type == "阴码":
                        pixel = pixel  # 阴码：反转像素值
                    if pixel:
                        byte1 |= (1 << row)  # 低位在前
                # 再处理下半部分8行
                byte2 = 0
                for row in range(8, 16):
                    # 获取像素值
                    pixel = image.getpixel((col, row))
                    if self.config.code_type == "阴码":
                        pixel = pixel  # 阴码：反转像素值
                    if pixel:
                        byte2 |= (1 << (row - 8))  # 低位在前
                bitmap_data.extend([byte1, byte2])
        else:  # 行列式
            for row in range(0, self.config.font_size, 8):
                for col in range(self.config.font_size):
                    byte = 0
                    for bit in range(8):
                        if row + bit < self.config.font_size:
                            pixel = image.getpixel((col, row + bit))
                            if self.config.code_type == "阴码":
                                pixel = pixel  # 阴码：反转像素值
                            if pixel:
                                if self.config.bit_order == "低位在前":
                                    byte |= (1 << bit)
                                else:
                                    byte |= (1 << (7 - bit))
                    bitmap_data.append(byte)

        return bitmap_data

    def search_chinese_in_files(self, search_dirs):
        """搜索指定文件夹下所有文件中的OLED_Printf函数中的中文"""
        chinese_chars = set()
        pattern = r'OLED_Printf\(.*?"(.*?)".*?\)'  # 匹配OLED_Printf函数中的字符串

        for search_dir in search_dirs:
            for root, _, files in os.walk(search_dir):
                for file in files:
                    if file.endswith(('.c', '.h')):
                        file_path = os.path.join(root, file)
                        try:
                            with open(file_path, 'r', encoding='utf-8') as f:
                                content = f.read()
                                # 找到所有OLED_Printf调用中的字符串
                                matches = re.findall(pattern, content)
                                for match in matches:
                                    # 提取字符串中的中文
                                    chinese_in_string = re.findall(r'[\u4e00-\u9fa5]+', match)
                                    for chinese in chinese_in_string:
                                        chinese_chars.update(chinese)
                        except UnicodeDecodeError:
                            # 如果utf-8解码失败，尝试使用gbk
                            try:
                                with open(file_path, 'r', encoding='gbk') as f:
                                    content = f.read()
                                    matches = re.findall(pattern, content)
                                    for match in matches:
                                        chinese_in_string = re.findall(r'[\u4e00-\u9fa5]+', match)
                                        for chinese in chinese_in_string:
                                            chinese_chars.update(chinese)
                            except Exception as e:
                                print(f"读取文件 {file_path} 失败: {e}")
                        except Exception as e:
                            print(f"读取文件 {file_path} 失败: {e}")

        return sorted(chinese_chars)

    def update_oled_data_file(self, chars, project_dir, config):
        """更新OLED_Data.c文件中的中文字库"""
        # 读取OLED_Data.c文件内容
        try:
            oled_data_file = os.path.join(project_dir, 'HardWare', 'OLED_Data.c')
            print(f"尝试读取文件: {oled_data_file}")
            with open(oled_data_file, 'r', encoding='utf-8') as f:
                content = f.read()
                print(f"成功读取文件，长度: {len(content)} 字符")

            # 查找中文字库部分
            start_pattern = r'const ChineseCell_t OLED_CF16x16\[\] = \{'
            end_pattern = r'\};'

            print("尝试匹配开始模式...")
            start_match = re.search(start_pattern, content)
            print("尝试匹配结束模式...")

            if not start_match:
                print("未找到中文字库定义，无法更新")
                return False
            
            end_match = re.search(end_pattern, content[start_match.end():], re.DOTALL)
            if not end_match:
                print("未找到中文字库结束标志，无法更新")
                return False
            
            # 提取现有字库内容
            font_lib_content = content[start_match.end():start_match.end() + end_match.start()]
            
            # 解析现有字模
            existing_fonts = {}
            # 匹配现有字模项，格式：{"字", {数据}, 16, 16},
            font_pattern = r'\s*\{"([^"]*)",\s*\{([^}]*?)\},\s*16,\s*16\},'
            matches = re.findall(font_pattern, font_lib_content)
            for match in matches:
                char = match[0]
                if char and char != "NULL":  # 跳过空字符和结束标志
                    existing_fonts[char] = match[1]  # 保存现有字模数据
            
            print(f"现有字模数量: {len(existing_fonts)}")
            
            # 生成新字模数据
            new_fonts = {}
            for char in chars:
                bitmap_data = self.generate_char_bitmap(char)
                data_str = ",".join(f"0x{byte:02x}" for byte in bitmap_data)
                new_fonts[char] = data_str
            
            print(f"新生成字模数量: {len(new_fonts)}")
            
            # 合并字模
            merged_fonts = {}
            
            # 根据配置决定是否保留现有字模
            if not config.clear_existing:
                merged_fonts.update(existing_fonts)
            
            # 处理重复字模
            for char, new_data in new_fonts.items():
                if char in merged_fonts:
                    # 字模已存在，根据配置处理
                    if config.duplicate_handle == "覆盖":
                        print(f"覆盖现有字模: {char}")
                        merged_fonts[char] = new_data
                    elif config.duplicate_handle == "保留":
                        print(f"保留现有字模: {char}")
                        # 不做处理，保留原有数据
                    else:  # 询问模式（默认覆盖，因为GUI中没有实现询问对话框）
                        print(f"询问模式，默认覆盖字模: {char}")
                        merged_fonts[char] = new_data
                else:
                    # 新增字模
                    merged_fonts[char] = new_data
            
            print(f"合并后字模数量: {len(merged_fonts)}")
            
            # 生成字模数据字符串
            font_data = []
            for char, data_str in merged_fonts.items():
                font_data.append('    {"{}", {{}}}, 16, 16}},'.format(char, data_str))

            # 添加默认图形和结束标志
            font_data.append("    {\"\", {0xFF,0x01,0x01,0x01,0x31,0x09,0x09,0x09,0x09,0x89,0x71,0x01,0x01,0x01,0x01,0xFF,")
            font_data.append("           0xFF,0x80,0x80,0x80,0x80,0x80,0x80,0x96,0x81,0x80,0x80,0x80,0x80,0x80,0x80,0xFF}, 16, 16},")
            font_data.append("    {NULL, {0}, 0, 0} // 结束标志")
            font_data.append("};")
            
            # 构建新的中文字库内容
            new_font_content = '\n' + '\n'.join(font_data) + '\n'
            
            # 计算替换位置
            start_pos = start_match.end()
            end_pos = start_match.end() + end_match.end()
            
            # 生成新内容
            new_content = content[:start_pos] + new_font_content + content[end_pos:]

            # 写入更新后的内容
            with open(oled_data_file, 'w', encoding='utf-8') as f:
                f.write(new_content)

            print(f"成功更新 {oled_data_file} 文件中的中文字库")
            print(f"共生成 {len(new_fonts)} 个汉字的字模数据")
            print(f"合并后字模总数: {len(merged_fonts)}")
            return True  # 更新成功

        except Exception as e:
            print(f"更新文件失败: {e}")
            return False  # 更新失败

# 配置窗口类
class ConfigWindow:
    def __init__(self, root, config):
        self.root = root
        self.config = config
        self.root.title("OLED字模生成工具")
        
        # 设置更大的初始窗口尺寸和最小尺寸
        self.root.geometry("650x640")
        self.root.minsize(600, 640)
        
        # 设置窗口居中
        self.root.update_idletasks()
        width = self.root.winfo_width()
        height = self.root.winfo_height()
        x = (self.root.winfo_screenwidth() // 2) - (width // 2)
        y = (self.root.winfo_screenheight() // 2) - (height // 2)
        self.root.geometry('{}x{}+{}+{}'.format(width, height, x, y))

        # 初始化配置
        self.config = GeneratorConfig()
        self.error_occurred = False

        # 设置现代主题
        self.style = ttk.Style()
        self.style.theme_use("clam")
        
        # 配置样式
        self.style.configure("TLabel", font=("SimHei", 10), padding=5)
        self.style.configure("TButton", font=("SimHei", 10), padding=5)
        self.style.configure("TRadiobutton", font=("SimHei", 10), padding=5)
        self.style.configure("TLabelFrame.Label", font=("SimHei", 10, "bold"))
        self.style.configure("TNotebook.Tab", font=("SimHei", 10, "bold"), padding=10)

        # 创建主框架，并设置填充和扩展属性
        main_frame = ttk.Frame(root, padding="15")
        main_frame.pack(fill=tk.BOTH, expand=True)
        main_frame.columnconfigure(0, weight=1)
        
        # 顶部标题栏
        # title_label = ttk.Label(main_frame, text="OLED字模生成工具", font=("SimHei", 14, "bold"), padding=10)
        # title_label.grid(row=0, column=0, sticky=tk.NSEW, pady=(0, 10))
        
        # 创建选项卡控件
        self.notebook = ttk.Notebook(main_frame)
        self.notebook.grid(row=1, column=0, sticky=tk.NSEW, pady=(0, 10))
        main_frame.rowconfigure(1, weight=1)
        
        # 创建字模页面
        self.char_tab = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.char_tab, text="字模")
        self.char_tab.columnconfigure(0, weight=1)
        
        # 创建图模页面
        self.img_tab = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.img_tab, text="图模")
        self.img_tab.columnconfigure(0, weight=1)
        
        # 创建预览页面
        self.preview_tab = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.preview_tab, text="预览")
        self.preview_tab.columnconfigure(0, weight=1)
        
        # 创建绘制页面
        self.draw_tab = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.draw_tab, text="绘制")
        self.draw_tab.columnconfigure(0, weight=1)
        
        # 初始化字模页面
        self.init_char_tab()
        
        # 初始化图模页面
        self.init_img_tab()
        
        # 初始化预览页面
        self.init_preview_tab()
        
        # 初始化绘制页面
        self.init_draw_tab()
    
    def init_char_tab(self):
        """初始化字模页面"""
        char_tab = self.char_tab

        # 工程目录设置
        project_frame = ttk.LabelFrame(char_tab, text="工程目录", padding="10")
        project_frame.grid(row=0, column=0, sticky=tk.NSEW, pady=(0, 10))
        project_frame.columnconfigure(1, weight=1)

        ttk.Label(project_frame, text="工程路径:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.project_path_var = tk.StringVar(value=self.config.last_project_path)
        project_entry = ttk.Entry(project_frame, textvariable=self.project_path_var, width=30)
        project_entry.grid(row=0, column=1, sticky=tk.EW, pady=5, padx=(5, 5))
        ttk.Button(project_frame, text="浏览...", command=self.browse_project_dir).grid(row=0, column=2, padx=5, pady=5)

        # 上半部分框架 - 包含字体与取模设置、输入与生成设置
        top_frame = ttk.Frame(char_tab, padding="0")
        top_frame.grid(row=1, column=0, sticky=tk.NSEW, pady=(0, 10))
        top_frame.columnconfigure(0, weight=1)
        top_frame.columnconfigure(1, weight=1)

        # 字体与取模设置
        font_mode_frame = ttk.LabelFrame(top_frame, text="字体与取模设置", padding="10")
        font_mode_frame.grid(row=0, column=0, sticky=tk.NSEW, padx=(0, 10))
        font_mode_frame.columnconfigure(0, weight=1)
        font_mode_frame.columnconfigure(1, weight=1)
        font_mode_frame.columnconfigure(2, weight=1)

        # 字体名称
        ttk.Label(font_mode_frame, text="字体名称:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.font_name_var = tk.StringVar()
        chinese_fonts = self._get_system_chinese_fonts()
        default_chinese_name = REVERSE_FONT_MAP.get(self.config.font_name, self.config.font_name)
        self.font_name_var.set(default_chinese_name)
        font_name_combo = ttk.Combobox(font_mode_frame, textvariable=self.font_name_var, values=chinese_fonts, state="readonly", width=15)
        font_name_combo.grid(row=0, column=1, sticky=tk.W, pady=5, padx=(5, 5), columnspan=2)

        # 取模方式
        ttk.Label(font_mode_frame, text="取模方式:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.mode_var = tk.StringVar(value=self.config.mode)
        ttk.Radiobutton(font_mode_frame, text="列行式", variable=self.mode_var, value="列行式").grid(row=1, column=1, sticky=tk.W, pady=5)
        ttk.Radiobutton(font_mode_frame, text="行列式", variable=self.mode_var, value="行列式").grid(row=1, column=2, sticky=tk.W, pady=5)

        # 编码类型
        ttk.Label(font_mode_frame, text="编码类型:").grid(row=2, column=0, sticky=tk.W, pady=5)
        self.code_type_var = tk.StringVar(value=self.config.code_type)
        ttk.Radiobutton(font_mode_frame, text="阴码", variable=self.code_type_var, value="阴码").grid(row=2, column=1, sticky=tk.W, pady=5)
        ttk.Radiobutton(font_mode_frame, text="阳码", variable=self.code_type_var, value="阳码").grid(row=2, column=2, sticky=tk.W, pady=5)

        # 位顺序
        ttk.Label(font_mode_frame, text="位顺序:").grid(row=3, column=0, sticky=tk.W, pady=5)
        self.bit_order_var = tk.StringVar(value=self.config.bit_order)
        ttk.Radiobutton(font_mode_frame, text="低位在前", variable=self.bit_order_var, value="低位在前").grid(row=3, column=1, sticky=tk.W, pady=5)
        ttk.Radiobutton(font_mode_frame, text="高位在前", variable=self.bit_order_var, value="高位在前").grid(row=3, column=2, sticky=tk.W, pady=5)

        # 输入与生成设置
        input_gen_frame = ttk.LabelFrame(top_frame, text="输入与生成设置", padding="10")
        input_gen_frame.grid(row=0, column=1, sticky=tk.NSEW)
        input_gen_frame.columnconfigure(1, weight=1)

        # 输入汉字
        ttk.Label(input_gen_frame, text="输入汉字:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.input_chars_var = tk.StringVar(value=self.config.input_chars)
        input_chars_entry = ttk.Entry(input_gen_frame, textvariable=self.input_chars_var, width=15)
        input_chars_entry.grid(row=0, column=1, sticky=tk.W, pady=5, padx=(5, 0))

        # 生成方式
        ttk.Label(input_gen_frame, text="生成方式:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.generate_mode_var = tk.StringVar(value=self.config.generate_mode)
        ttk.Radiobutton(input_gen_frame, text="仅搜索代码中的汉字", variable=self.generate_mode_var, value="仅搜索代码中的汉字").grid(row=1, column=1, sticky=tk.W, pady=5)
        ttk.Radiobutton(input_gen_frame, text="仅使用手动输入的汉字", variable=self.generate_mode_var, value="仅使用手动输入的汉字").grid(row=2, column=1, sticky=tk.W, pady=5)
        ttk.Radiobutton(input_gen_frame, text="两者结合（去重）", variable=self.generate_mode_var, value="两者结合（去重）").grid(row=3, column=1, sticky=tk.W, pady=5)

        # 字模处理选项
        process_frame = ttk.LabelFrame(char_tab, text="字模处理选项", padding="10")
        process_frame.grid(row=2, column=0, sticky=tk.NSEW, pady=(0, 10))
        process_frame.columnconfigure(0, weight=1)
        process_frame.columnconfigure(1, weight=1)
        process_frame.columnconfigure(2, weight=1)
        process_frame.columnconfigure(3, weight=1)
        process_frame.columnconfigure(4, weight=1)
        process_frame.columnconfigure(5, weight=1)

        # 清空现有字模
        self.clear_existing_var = tk.BooleanVar(value=self.config.clear_existing)
        ttk.Checkbutton(process_frame, text="清空现有字模", variable=self.clear_existing_var).grid(row=0, column=0, sticky=tk.W, pady=5)

        # 重复处理
        ttk.Label(process_frame, text="重复处理:").grid(row=0, column=1, sticky=tk.W, pady=5, padx=(20, 0))
        self.duplicate_handle_var = tk.StringVar(value=self.config.duplicate_handle)
        ttk.Radiobutton(process_frame, text="询问", variable=self.duplicate_handle_var, value="询问").grid(row=0, column=2, sticky=tk.W, pady=5, padx=(5, 0))
        ttk.Radiobutton(process_frame, text="覆盖", variable=self.duplicate_handle_var, value="覆盖").grid(row=0, column=3, sticky=tk.W, pady=5, padx=(5, 0))
        ttk.Radiobutton(process_frame, text="保留", variable=self.duplicate_handle_var, value="保留").grid(row=0, column=4, sticky=tk.W, pady=5, padx=(5, 0))

        # 记住选择 - 移到同一行右侧
        self.remember_choice_var = tk.BooleanVar(value=self.config.remember_choice)
        ttk.Checkbutton(process_frame, text="记住选择", variable=self.remember_choice_var).grid(row=0, column=5, sticky=tk.E, pady=5, padx=(20, 0))

        # 状态框
        self.status_var = tk.StringVar(value="就绪")
        status_frame = ttk.LabelFrame(char_tab, text="状态", padding="10")
        status_frame.grid(row=3, column=0, sticky=tk.NSEW, pady=(0, 10))
        ttk.Label(status_frame, textvariable=self.status_var).grid(row=0, column=0, sticky=tk.W, pady=5)

        # 操作按钮 - 关键改进区域
        button_frame = ttk.Frame(char_tab)
        button_frame.grid(row=4, column=0, sticky=tk.EW, pady=(15, 5))
        # 为按钮列设置权重，确保均匀分布
        button_frame.columnconfigure(0, weight=1)
        button_frame.columnconfigure(1, weight=1)
        button_frame.columnconfigure(2, weight=1)
        button_frame.columnconfigure(3, weight=1)

        # 创建按钮样式
        self.style.configure("Primary.TButton", foreground="white", background="#4CAF50")
        self.style.map("Primary.TButton", background=[("active", "#45a049")])

        self.style.configure("Secondary.TButton", foreground="white", background="#2196F3")
        self.style.map("Secondary.TButton", background=[("active", "#0b7dda")])

        self.style.configure("Danger.TButton", foreground="white", background="#f44336")
        self.style.map("Danger.TButton", background=[("active", "#d32f2f")])

        # 按钮布局 - 使用sticky确保按钮能水平扩展
        ttk.Button(button_frame, text="生成字模", command=self.generate, style="Primary.TButton").grid(row=0, column=0, padx=5, pady=10, sticky=tk.EW)
        ttk.Button(button_frame, text="保存配置", command=self.save_config, style="Secondary.TButton").grid(row=0, column=1, padx=5, pady=10, sticky=tk.EW)
        ttk.Button(button_frame, text="下载程序", command=self.pack_and_download_files, style="Secondary.TButton").grid(row=0, column=2, padx=5, pady=10, sticky=tk.EW)
        ttk.Button(button_frame, text="退出", command=self.root.quit, style="Danger.TButton").grid(row=0, column=3, padx=5, pady=10, sticky=tk.EW)

        # 绑定窗口调整事件
        self.root.bind("<Configure>", self._on_configure)
    
    def init_img_tab(self):
        """初始化图模页面"""
        img_tab = self.img_tab
        
        # 创建图模页面的基本框架
        img_frame = ttk.LabelFrame(img_tab, text="图模生成", padding="10")
        img_frame.pack(fill=tk.BOTH, expand=True)
        
        # 工程目录设置
        project_frame = ttk.LabelFrame(img_frame, text="工程目录", padding="5")
        project_frame.pack(fill=tk.X, pady=(0, 5))
        project_frame.columnconfigure(1, weight=1)
        
        ttk.Label(project_frame, text="工程路径:").grid(row=0, column=0, sticky=tk.W, pady=2)
        self.project_path_var = tk.StringVar(value=self.config.last_project_path)
        project_entry = ttk.Entry(project_frame, textvariable=self.project_path_var)
        project_entry.grid(row=0, column=1, sticky=tk.EW, pady=2, padx=(5, 5))
        ttk.Button(project_frame, text="浏览...", command=self.browse_project_dir, width=8).grid(row=0, column=2, padx=2, pady=2)
        
        # 图像路径设置
        img_path_frame = ttk.LabelFrame(img_frame, text="图像设置", padding="5")
        img_path_frame.pack(fill=tk.X, pady=(0, 5))
        img_path_frame.columnconfigure(1, weight=1)
        img_path_frame.columnconfigure(3, weight=1)
        
        # 第一行：图像路径 + 图像名称
        ttk.Label(img_path_frame, text="图像路径:").grid(row=0, column=0, sticky=tk.W, pady=2)
        self.img_path_var = tk.StringVar()
        img_path_entry = ttk.Entry(img_path_frame, textvariable=self.img_path_var)
        img_path_entry.grid(row=0, column=1, sticky=tk.EW, pady=2, padx=(5, 5))
        ttk.Button(img_path_frame, text="浏览...", command=self.browse_img_file, width=8).grid(row=0, column=2, padx=2, pady=2)
        
        ttk.Label(img_path_frame, text="图像名称:").grid(row=0, column=3, sticky=tk.W, pady=2, padx=(10, 0))
        self.img_name_var = tk.StringVar(value="new_image")
        img_name_entry = ttk.Entry(img_path_frame, textvariable=self.img_name_var, width=15)
        img_name_entry.grid(row=0, column=4, sticky=tk.W, pady=2, padx=(5, 0))
        
        # 第二行：尺寸 + 亮度阈值 + 反转图像
        second_row_frame = ttk.Frame(img_path_frame, padding="5")
        second_row_frame.grid(row=1, column=0, columnspan=5, sticky=tk.EW, pady=2)
        
        # 尺寸
        size_frame = ttk.Frame(second_row_frame)
        size_frame.pack(side=tk.LEFT, padx=(0, 20))
        ttk.Label(size_frame, text="尺寸:").pack(side=tk.LEFT, padx=(0, 5))
        self.img_width_var = tk.StringVar(value="16")
        width_entry = ttk.Entry(size_frame, textvariable=self.img_width_var, width=5)
        width_entry.pack(side=tk.LEFT, padx=(0, 2))
        ttk.Label(size_frame, text="x").pack(side=tk.LEFT, padx=(0, 2))
        self.img_height_var = tk.StringVar(value="16")
        height_entry = ttk.Entry(size_frame, textvariable=self.img_height_var, width=5)
        height_entry.pack(side=tk.LEFT, padx=(0, 2))
        
        # 亮度阈值
        threshold_frame = ttk.Frame(second_row_frame)
        threshold_frame.pack(side=tk.LEFT, padx=(0, 20))
        ttk.Label(threshold_frame, text="亮度阈值:").pack(side=tk.LEFT, padx=(0, 5))
        self.threshold_var = tk.StringVar(value="128")
        threshold_entry = ttk.Entry(threshold_frame, textvariable=self.threshold_var, width=6)
        threshold_entry.pack(side=tk.LEFT)
        
        # 反转图像
        invert_frame = ttk.Frame(second_row_frame)
        invert_frame.pack(side=tk.LEFT)
        self.invert_var = tk.BooleanVar(value=False)
        invert_check = ttk.Checkbutton(invert_frame, text="反转图像", variable=self.invert_var)
        invert_check.pack(side=tk.LEFT)
        
        # 屏幕大小配置
        options_frame = ttk.LabelFrame(img_frame, text="配置选项", padding="5")
        options_frame.pack(fill=tk.X, pady=(0, 5))
        
        # 屏幕大小配置
        screen_size_frame = ttk.Frame(options_frame, padding="5")
        screen_size_frame.pack(fill=tk.X, pady=2)
        
        ttk.Label(screen_size_frame, text="屏幕尺寸:").pack(side=tk.LEFT, padx=(0, 5))
        self.screen_width_var = tk.StringVar(value="128")
        width_entry = ttk.Entry(screen_size_frame, textvariable=self.screen_width_var, width=6)
        width_entry.pack(side=tk.LEFT, padx=(0, 2))
        
        ttk.Label(screen_size_frame, text="x").pack(side=tk.LEFT, padx=(0, 2))
        
        self.screen_height_var = tk.StringVar(value="64")
        height_entry = ttk.Entry(screen_size_frame, textvariable=self.screen_height_var, width=6)
        height_entry.pack(side=tk.LEFT, padx=(0, 2))
        
        # 状态框
        self.img_status_var = tk.StringVar(value="就绪")
        status_frame = ttk.LabelFrame(img_frame, text="状态", padding="5")
        status_frame.pack(fill=tk.X, pady=(0, 5))
        ttk.Label(status_frame, textvariable=self.img_status_var).grid(row=0, column=0, sticky=tk.W, pady=2)
        
        # 操作按钮
        button_frame = ttk.Frame(img_frame)
        button_frame.pack(fill=tk.X, pady=(5, 0))
        button_frame.columnconfigure(0, weight=1)
        
        ttk.Button(button_frame, text="生成图模", command=self.generate_img, style="Primary.TButton").grid(row=0, column=0, padx=5, pady=5, sticky=tk.EW)
    
    def init_preview_tab(self):
        """初始化预览页面"""
        preview_tab = self.preview_tab
        
        # 创建预览页面的基本框架
        preview_frame = ttk.Frame(preview_tab, padding="10")
        preview_frame.pack(fill=tk.BOTH, expand=True)
        
        # 预览效果区域
        canvas_frame = ttk.Frame(preview_frame, padding="5")
        canvas_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 5))
        
        # 创建预览画布，使其填充整个画布框架
        self.preview_canvas = tk.Canvas(canvas_frame, bg="#000000", relief="solid", borderwidth=1)
        self.preview_canvas.pack(fill=tk.BOTH, expand=True)
        
        # 绑定屏幕尺寸变化事件
        self.screen_width_var.trace_add("write", self.update_preview)
        self.screen_height_var.trace_add("write", self.update_preview)
        
        # 绑定画布大小变化事件
        self.preview_canvas.bind("<Configure>", self.update_preview)
        
        # 状态框
        self.preview_status_var = tk.StringVar(value="就绪")
        status_frame = ttk.LabelFrame(preview_frame, text="状态", padding="5")
        status_frame.pack(fill=tk.X, pady=(0, 5))
        ttk.Label(status_frame, textvariable=self.preview_status_var).grid(row=0, column=0, sticky=tk.W, pady=2)
        
        # 操作按钮
        button_frame = ttk.Frame(preview_frame)
        button_frame.pack(fill=tk.X, pady=(5, 0))
        button_frame.columnconfigure(0, weight=1)
        
        ttk.Button(button_frame, text="保存到文件", command=self.save_img_to_file, style="Secondary.TButton").grid(row=0, column=0, padx=5, pady=5, sticky=tk.EW)
    
    def init_draw_tab(self):
        """初始化绘制页面"""
        draw_tab = self.draw_tab
        
        # 创建绘制页面的基本框架
        draw_frame = ttk.Frame(draw_tab, padding="10")
        draw_frame.pack(fill=tk.BOTH, expand=True)
        
        # 绘制设置（无标签）
        draw_settings_frame = ttk.Frame(draw_frame, padding="5")
        draw_settings_frame.pack(fill=tk.X, pady=(0, 5))
        
        # 第一行：绘制尺寸和图像名称
        draw_settings_row1 = ttk.Frame(draw_settings_frame, padding="5")
        draw_settings_row1.pack(fill=tk.X, pady=0)
        
        # 绘制尺寸
        size_frame = ttk.Frame(draw_settings_row1)
        size_frame.pack(side=tk.LEFT, padx=(0, 5))
        ttk.Label(size_frame, text="绘制尺寸:").pack(side=tk.LEFT, padx=(0, 3))
        self.draw_width_var = tk.StringVar(value="16")
        draw_width_entry = ttk.Entry(size_frame, textvariable=self.draw_width_var, width=4)
        draw_width_entry.pack(side=tk.LEFT, padx=(0, 2))
        
        ttk.Label(size_frame, text="x").pack(side=tk.LEFT, padx=(0, 2))
        
        self.draw_height_var = tk.StringVar(value="16")
        draw_height_entry = ttk.Entry(size_frame, textvariable=self.draw_height_var, width=4)
        draw_height_entry.pack(side=tk.LEFT, padx=(0, 5))
        
        # 图像名称输入框
        name_frame = ttk.Frame(draw_settings_row1)
        name_frame.pack(side=tk.LEFT, padx=(0, 5))
        ttk.Label(name_frame, text="图像名称:").pack(side=tk.LEFT, padx=(0, 3))
        self.draw_img_name_var = tk.StringVar(value="new_image")
        img_name_entry = ttk.Entry(name_frame, textvariable=self.draw_img_name_var, width=10)
        img_name_entry.pack(side=tk.LEFT)
        
        # 绑定尺寸变化事件
        self.draw_width_var.trace_add("write", self.update_draw_canvas)
        self.draw_height_var.trace_add("write", self.update_draw_canvas)
        
        # 第二行：绘制工具按钮
        draw_settings_row2 = ttk.Frame(draw_settings_frame, padding="5")
        draw_settings_row2.pack(fill=tk.X, pady=0)
        
        buttons_frame = ttk.Frame(draw_settings_row2)
        buttons_frame.pack(side=tk.LEFT, fill=tk.X, expand=True)
        
        ttk.Button(buttons_frame, text="清空画布", command=self.clear_draw_canvas).pack(side=tk.LEFT, padx=5)
        ttk.Button(buttons_frame, text="填充画布", command=self.fill_draw_canvas).pack(side=tk.LEFT, padx=5)
        ttk.Button(buttons_frame, text="反转画布", command=self.invert_draw_canvas).pack(side=tk.LEFT, padx=5)
        
        # 绘制区域（无标签）
        draw_canvas_frame = ttk.Frame(draw_frame, padding="5")
        draw_canvas_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 5))
        
        # 创建绘制画布
        self.draw_canvas = tk.Canvas(draw_canvas_frame, width=320, height=200, bg="#000000", relief="solid", borderwidth=1)
        self.draw_canvas.pack(fill=tk.BOTH, expand=True)
        
        # 绑定绘制画布事件
        self.draw_canvas.bind("<Button-1>", self.on_draw_canvas_click)
        self.draw_canvas.bind("<B1-Motion>", self.on_draw_canvas_motion)
        self.draw_canvas.bind("<ButtonRelease-1>", self.on_draw_canvas_release)
        self.draw_canvas.bind("<MouseWheel>", self.on_draw_canvas_scroll)  # Windows系统
        self.draw_canvas.bind("<Button-4>", self.on_draw_canvas_scroll)    # Linux系统
        self.draw_canvas.bind("<Button-5>", self.on_draw_canvas_scroll)    # Linux系统
        # 添加右键拖动事件
        self.draw_canvas.bind("<Button-3>", self.on_draw_canvas_right_click)
        self.draw_canvas.bind("<B3-Motion>", self.on_draw_canvas_right_motion)
        self.draw_canvas.bind("<ButtonRelease-3>", self.on_draw_canvas_right_release)
        
        # 绘制状态变量
        self.is_drawing = False
        self.is_dragging = False
        self.last_x = 0
        self.last_y = 0
        self.canvas_offset_x = 0
        self.canvas_offset_y = 0
        # 上一次处理的像素坐标
        self.last_pixel_x = -1
        self.last_pixel_y = -1
        
        # 初始化绘制数据
        self.draw_data = []
        self.pixel_size = 20
        
        # 绘制点阵
        self.update_draw_canvas()
        
        # 操作按钮
        button_frame = ttk.Frame(draw_frame)
        button_frame.pack(fill=tk.X, pady=(5, 0))
        button_frame.columnconfigure(0, weight=1)
        
        ttk.Button(button_frame, text="生成图模", command=self.generate_img, style="Primary.TButton").grid(row=0, column=0, padx=5, pady=5, sticky=tk.EW)
    
    def _on_configure(self, event):
        """窗口大小变化时更新布局"""
        self.root.update_idletasks()

    # 在默认配置后添加全局字体映射表
    # 默认配置
    DEFAULT_FONT_NAME = "SimHei"  # 默认字体名称
    OLED_DATA_FILE = "./HardWare/OLED_Data.c"  # OLED数据文件路径
    FONT_SIZE = 16
    
    # 字体名称映射表（中文 -> 英文/拼音）
    FONT_NAME_MAP = {
        "黑体": "SimHei",
        "微软雅黑": "Microsoft YaHei",
        "宋体": "SimSun",
        "仿宋": "FangSong",
        "楷体": "KaiTi",
        "华文宋体": "STSong",
        "华文中宋": "STZhongsong",
        "华文楷体": "STKaiti",
        "华文细黑": "STXihei",
        "华文仿宋": "STFangsong",
        "华文彩云": "STCaiyun",
        "华文行楷": "STXingkai",
        "Arial Unicode MS": "Arial Unicode MS",
        "幼圆": "SimYou",
        "新宋体": "NSimSun",
    }
    
    # 反转映射表（英文/拼音 -> 中文）
    REVERSE_FONT_MAP = {v: k for k, v in FONT_NAME_MAP.items()}

    def _get_system_chinese_fonts(self):
        """获取系统中可用的中文字体列表"""
        chinese_fonts = set()
        
        # 添加一些常见的中文字体名称
        common_chinese_fonts = list(FONT_NAME_MAP.keys())
        chinese_fonts.update(common_chinese_fonts)

        # 获取系统字体列表
        try:
            fonts = fm.findSystemFonts()
            for font_path in fonts:
                try:
                    # 获取字体名称
                    font_name = fm.FontProperties(fname=font_path).get_name()
                    # 检查是否可能是中文字体
                    if any(chinese in font_name for chinese in ["黑", "宋", "仿", "楷", "雅", "隶", "圆", "云", "行"]):
                        # 如果字体名称在映射表中，使用中文名称，否则使用原名称
                        chinese_name = REVERSE_FONT_MAP.get(font_name, font_name)
                        chinese_fonts.add(chinese_name)
                except:
                    pass
        except Exception as e:
            print(f"获取系统字体失败: {e}")

        # 确保默认字体存在（使用中文名称）
        default_chinese_name = REVERSE_FONT_MAP.get(DEFAULT_FONT_NAME, DEFAULT_FONT_NAME)
        if default_chinese_name not in chinese_fonts:
            chinese_fonts.add(default_chinese_name)

        return sorted(chinese_fonts)

    def save_config(self):
        """保存配置"""
        try:
            # 更新配置
            chinese_font_name = self.font_name_var.get()
            self.config.font_name = FONT_NAME_MAP.get(chinese_font_name, chinese_font_name)
            self.config.mode = self.mode_var.get()
            self.config.code_type = self.code_type_var.get()
            self.config.bit_order = self.bit_order_var.get()
            self.config.last_project_path = self.project_path_var.get()  # 保存当前工程地址
            # 保存新增配置项
            self.config.input_chars = self.input_chars_var.get()
            self.config.generate_mode = self.generate_mode_var.get()
            self.config.clear_existing = self.clear_existing_var.get()
            self.config.duplicate_handle = self.duplicate_handle_var.get()
            self.config.remember_choice = self.remember_choice_var.get()

            # 保存到脚本
            if self.config.save_to_script():
                self.status_var.set("配置已保存")
            else:
                self.status_var.set("保存配置失败")
        except Exception as e:
            self.status_var.set(f"保存配置出错: {str(e)}")
            print(f"保存配置异常: {e}")

    def generate(self):
        # 保存配置
        self.save_config()
        self.status_var.set("正在生成字模...")
        self.root.update()

        try:
            # 创建字模生成器
            generator = OLEDCharGenerator(self.config)

            project_dir = self.project_path_var.get()
            if not project_dir:
                self.status_var.set("请先选择工程目录")
                messagebox.showerror("错误", "请先选择工程目录")
                self.error_occurred = True
                return

            chinese_chars = []
            generate_mode = self.generate_mode_var.get()

            if generate_mode == "仅搜索代码中的汉字" or generate_mode == "两者结合（去重）":
                # 构建完整的搜索目录路径
                search_dirs = [
                    os.path.join(project_dir, 'User'),
                    os.path.join(project_dir, 'HardWare')
                ]
                self.status_var.set("正在搜索代码中的中文...")
                self.root.update()
                # 搜索中文
                code_chars = generator.search_chinese_in_files(search_dirs)
                chinese_chars.extend(code_chars)

            if generate_mode == "仅使用手动输入的汉字" or generate_mode == "两者结合（去重）":
                # 获取手动输入的汉字
                input_text = self.input_chars_var.get()
                if input_text:
                    self.status_var.set("正在处理手动输入的汉字...")
                    self.root.update()
                    # 提取输入文本中的所有汉字
                    input_chars = re.findall(r'[\u4e00-\u9fa5]+', input_text)
                    if input_chars:
                        # 将列表合并为字符串
                        input_chars_str = ''.join(input_chars)
                        # 添加到汉字列表
                        chinese_chars.extend(list(input_chars_str))

            # 去重
            if generate_mode == "两者结合（去重）":
                chinese_chars = sorted(list(set(chinese_chars)))
            elif not chinese_chars:
                # 如果没有找到任何汉字
                self.status_var.set("未找到任何中文汉字")
                messagebox.showwarning("警告", "未找到任何中文汉字")
                self.error_occurred = True
                return

            update_success = False
            if chinese_chars:
                self.status_var.set(f"共处理 {len(chinese_chars)} 个中文汉字，正在更新字库...")
                self.root.update()
                print(f"处理的中文汉字: {''.join(chinese_chars)}")
                update_success = generator.update_oled_data_file(chinese_chars, project_dir, self.config)

            if update_success:
                self.status_var.set("字模生成成功")
                messagebox.showinfo("成功", f"共生成 {len(chinese_chars)} 个汉字的字模数据\n字库更新成功")
                # 保持窗口开启，不自动关闭
                # self.root.destroy()
            else:
                self.status_var.set("字模生成失败")
                self.error_occurred = True

        except Exception as e:
            error_msg = f"生成字模失败: {str(e)}"
            self.status_var.set(error_msg)
            print(error_msg)
            messagebox.showerror("错误", error_msg)
            self.error_occurred = True
    def browse_project_dir(self):
        """浏览工程目录"""
        directory = filedialog.askdirectory(title="选择工程目录")
        if directory:
            self.project_path_var.set(directory)
            self.config.last_project_path = directory  # 更新最后使用的工程地址
            self.save_config()  # 保存配置
    
    def browse_img_file(self):
        """浏览图像文件"""
        file_types = [
            ("图像文件", "*.bmp *.jpg *.jpeg *.png *.gif *.tiff *.ico"),
            ("所有文件", "*.*")
        ]
        file_path = filedialog.askopenfilename(title="选择图像文件", filetypes=file_types)
        if file_path:
            self.img_path_var.set(file_path)  # 更新图像路径
    
    def image_to_bitmap(self, img_path, width, height, threshold=128, invert=False):
        """将图像转换为点阵数据 - 纵向8点，高位在下格式（匹配Electron插件）"""
        try:
            img = Image.open(img_path)
            img = img.convert('L')
            img = img.resize((width, height), Image.LANCZOS)
            
            pixels = list(img.getdata())
            
            bytes_per_row = (height + 7) // 8
            total_bytes = width * bytes_per_row
            
            bitmap_data = [0] * total_bytes
            
            for row in range(0, height, 8):
                for col in range(width):
                    byte = 0
                    for bit in range(8):
                        y = row + bit
                        if y < height:
                            pixel = pixels[y * width + col]
                            if invert:
                                bit_val = 1 if pixel < threshold else 0
                            else:
                                bit_val = 1 if pixel >= threshold else 0
                            if bit_val:
                                byte |= (1 << bit)
                    
                    byte_index = (row // 8) * width + col
                    bitmap_data[byte_index] = byte
            
            return bitmap_data
        except Exception as e:
            raise Exception(f"图像转换失败: {str(e)}")
    
    def update_preview_size(self, *args):
        """更新预览尺寸（保持画布大小固定，只更新预览内容）"""
        try:
            # 直接更新预览内容，不调整画布大小
            if hasattr(self, 'current_img_data'):
                self.update_preview()
        except Exception as e:
            print(f"更新预览尺寸失败: {str(e)}")
    
    def update_preview(self, event=None):
        """更新预览效果"""
        try:
            # 清空画布
            self.preview_canvas.delete("all")
            
            # 获取画布尺寸（自适应大小）
            canvas_width = self.preview_canvas.winfo_width()
            canvas_height = self.preview_canvas.winfo_height()
            
            # 确保画布尺寸有效
            if canvas_width <= 0 or canvas_height <= 0:
                return
            
            if not hasattr(self, 'current_img_data') or not hasattr(self, 'current_img_width') or not hasattr(self, 'current_img_height'):
                # 如果没有预览数据，显示默认信息
                self.preview_canvas.create_text(canvas_width//2, canvas_height//2, text="请先生成图模", fill="#ffffff", font=("Arial", 12))
                return
            
            # 获取预览数据
            width = self.current_img_width
            height = self.current_img_height
            bitmap_data = self.current_img_data
            
            # 确保尺寸有效
            if width <= 0 or height <= 0:
                self.preview_canvas.create_text(canvas_width//2, canvas_height//2, text="无效的图像尺寸", fill="#ffffff", font=("Arial", 12))
                return
            
            # 确保bitmap_data有效
            if not bitmap_data:
                self.preview_canvas.create_text(canvas_width//2, canvas_height//2, text="无效的图像数据", fill="#ffffff", font=("Arial", 12))
                return
            
            # 计算缩放比例，使图像适应自适应画布
            scale_x = canvas_width / width
            scale_y = canvas_height / height
            scale = min(scale_x, scale_y)
            
            # 确保缩放比例至少为1，避免过小的像素
            scale = max(scale, 1)
            
            # 计算绘制位置（居中）
            draw_width = int(width * scale)
            draw_height = int(height * scale)
            offset_x = (canvas_width - draw_width) // 2
            offset_y = (canvas_height - draw_height) // 2
            
            # 绘制点阵
            for row in range(0, height, 8):
                for col in range(width):
                    byte_index = (row // 8) * width + col
                    if byte_index < len(bitmap_data):
                        byte = bitmap_data[byte_index]
                        for bit in range(8):
                            y = row + bit
                            if y < height:
                                if byte & (1 << bit):
                                    # 绘制点亮的像素
                                    x1 = offset_x + int(col * scale)
                                    y1 = offset_y + int(y * scale)
                                    x2 = x1 + int(scale)
                                    y2 = y1 + int(scale)
                                    self.preview_canvas.create_rectangle(x1, y1, x2, y2, fill="#ffffff", outline="#333333")
                                else:
                                    # 绘制未点亮的像素（黑色背景）
                                    x1 = offset_x + int(col * scale)
                                    y1 = offset_y + int(y * scale)
                                    x2 = x1 + int(scale)
                                    y2 = y1 + int(scale)
                                    self.preview_canvas.create_rectangle(x1, y1, x2, y2, fill="#000000", outline="#333333")
            
            # 绘制屏幕边框
            self.preview_canvas.create_rectangle(offset_x-1, offset_y-1, offset_x+draw_width+1, offset_y+draw_height+1, outline="#333333")
            
            # 绘制尺寸信息
            info_text = f"预览: {width}x{height} 像素"
            if canvas_height > 30:
                self.preview_canvas.create_text(canvas_width//2, 15, text=info_text, fill="#ffffff", font=("Arial", 10))
            
        except Exception as e:
            print(f"更新预览失败: {str(e)}")
    
    def update_draw_canvas(self, *args):
        """更新绘制画布"""
        try:
            # 获取绘制尺寸
            try:
                width = int(self.draw_width_var.get())
                height = int(self.draw_height_var.get())
                # 确保尺寸有效
                width = max(width, 1)
                height = max(height, 1)
            except:
                width, height = 16, 16
                print("使用默认绘制尺寸: 16x16")
            
            # 清空画布
            self.draw_canvas.delete("all")
            
            # 获取画布尺寸
            canvas_width = self.draw_canvas.winfo_width()
            canvas_height = self.draw_canvas.winfo_height()
            
            # 确保画布尺寸有效
            canvas_width = max(canvas_width, 1)
            canvas_height = max(canvas_height, 1)
            
            # 只有在首次初始化或pixel_size未设置时才计算
            if not hasattr(self, 'pixel_size') or self.pixel_size <= 0:
                try:
                    self.pixel_size = min(canvas_width // width, canvas_height // height, 20)
                    # 确保像素大小至少为1
                    self.pixel_size = max(self.pixel_size, 1)
                    print(f"计算得到的像素大小: {self.pixel_size}")
                except Exception as e:
                    print(f"计算像素大小失败: {str(e)}, 使用默认值")
                    self.pixel_size = 10
            else:
                # 尊重用户通过滚轮设置的像素大小
                print(f"使用用户设置的像素大小: {self.pixel_size}")
            
            # 计算绘制区域大小
            draw_width = width * self.pixel_size
            draw_height = height * self.pixel_size
            # 计算基础偏移量（居中）
            base_offset_x = (canvas_width - draw_width) // 2
            base_offset_y = (canvas_height - draw_height) // 2
            # 应用用户拖动的偏移量
            offset_x = base_offset_x + self.canvas_offset_x
            offset_y = base_offset_y + self.canvas_offset_y
            
            # 初始化绘制数据
            if not hasattr(self, 'draw_data') or len(self.draw_data) != height:
                self.draw_data = [[0 for _ in range(width)] for _ in range(height)]
                print(f"初始化绘制数据: {height}x{width}")
            elif len(self.draw_data) == height:
                # 调整宽度
                for row in self.draw_data:
                    if len(row) < width:
                        row.extend([0] * (width - len(row)))
                    elif len(row) > width:
                        row[:] = row[:width]
            
            # 绘制网格
            for y in range(height):
                for x in range(width):
                    x1 = offset_x + x * self.pixel_size
                    y1 = offset_y + y * self.pixel_size
                    x2 = x1 + self.pixel_size
                    y2 = y1 + self.pixel_size
                    
                    # 根据数据绘制像素
                    fill_color = "#ffffff" if self.draw_data[y][x] else "#000000"
                    self.draw_canvas.create_rectangle(x1, y1, x2, y2, fill=fill_color, outline="#333333")
        except Exception as e:
            print(f"更新绘制画布失败: {str(e)}")
    
    def clear_draw_canvas(self):
        """清空绘制画布"""
        try:
            width = int(self.draw_width_var.get())
            height = int(self.draw_height_var.get())
        except:
            width, height = 16, 16
        
        self.draw_data = [[0 for _ in range(width)] for _ in range(height)]
        self.update_draw_canvas()
    
    def fill_draw_canvas(self):
        """填充绘制画布"""
        try:
            width = int(self.draw_width_var.get())
            height = int(self.draw_height_var.get())
        except:
            width, height = 16, 16
        
        self.draw_data = [[1 for _ in range(width)] for _ in range(height)]
        self.update_draw_canvas()
    
    def invert_draw_canvas(self):
        """反转绘制画布"""
        if hasattr(self, 'draw_data'):
            for y in range(len(self.draw_data)):
                for x in range(len(self.draw_data[y])):
                    self.draw_data[y][x] = 1 - self.draw_data[y][x]
            self.update_draw_canvas()
    
    def on_draw_canvas_click(self, event):
        """处理绘制画布点击事件"""
        try:
            # 开始绘制
            self.is_drawing = True
            # 调用移动事件处理函数，实现点击绘制
            self.on_draw_canvas_motion(event)
        except Exception as e:
            print(f"处理画布点击失败: {str(e)}")
    
    def on_draw_canvas_motion(self, event):
        """处理绘制画布鼠标移动事件"""
        try:
            if not self.is_drawing:
                return
            
            # 获取绘制尺寸
            try:
                width = int(self.draw_width_var.get())
                height = int(self.draw_height_var.get())
                # 确保尺寸有效
                width = max(width, 1)
                height = max(height, 1)
            except:
                width, height = 16, 16
            
            # 确保像素大小有效
            if not hasattr(self, 'pixel_size') or self.pixel_size <= 0:
                self.update_draw_canvas()
                return
            
            # 获取画布尺寸
            canvas_width = self.draw_canvas.winfo_width()
            canvas_height = self.draw_canvas.winfo_height()
            
            # 计算绘制区域大小
            draw_width = width * self.pixel_size
            draw_height = height * self.pixel_size
            # 计算基础偏移量（居中）
            base_offset_x = (canvas_width - draw_width) // 2
            base_offset_y = (canvas_height - draw_height) // 2
            # 应用用户拖动的偏移量
            offset_x = base_offset_x + self.canvas_offset_x
            offset_y = base_offset_y + self.canvas_offset_y
            
            # 计算点击的像素坐标
            try:
                x = (event.x - offset_x) // self.pixel_size
                y = (event.y - offset_y) // self.pixel_size
            except ZeroDivisionError:
                print("除零错误: pixel_size为0")
                self.update_draw_canvas()
                return
            
            # 检查坐标是否有效
            if 0 <= x < width and 0 <= y < height:
                # 确保draw_data有效
                if not hasattr(self, 'draw_data') or len(self.draw_data) <= y or len(self.draw_data[y]) <= x:
                    self.update_draw_canvas()
                    return
                
                # 只有当鼠标移动到新的像素块时才切换状态
                if x != self.last_pixel_x or y != self.last_pixel_y:
                    # 切换像素状态
                    self.draw_data[y][x] = 1 - self.draw_data[y][x]
                    
                    # 更新画布
                    x1 = offset_x + x * self.pixel_size
                    y1 = offset_y + y * self.pixel_size
                    x2 = x1 + self.pixel_size
                    y2 = y1 + self.pixel_size
                    
                    fill_color = "#ffffff" if self.draw_data[y][x] else "#000000"
                    self.draw_canvas.create_rectangle(x1, y1, x2, y2, fill=fill_color, outline="#333333")
                    
                    # 更新上一次处理的像素坐标
                    self.last_pixel_x = x
                    self.last_pixel_y = y
        except Exception as e:
            print(f"处理画布移动失败: {str(e)}")
    
    def on_draw_canvas_release(self, event):
        """处理绘制画布鼠标释放事件"""
        try:
            # 结束绘制
            self.is_drawing = False
            # 重置上一次处理的像素坐标
            self.last_pixel_x = -1
            self.last_pixel_y = -1
        except Exception as e:
            print(f"处理画布释放失败: {str(e)}")
    
    def on_draw_canvas_scroll(self, event):
        """处理绘制画布鼠标滚轮事件，实现缩放功能"""
        try:
            # 确定缩放方向
            if event.num == 4 or event.delta > 0:
                # 放大
                if hasattr(self, 'pixel_size'):
                    self.pixel_size = min(self.pixel_size + 1, 50)  # 最大放大到50
            elif event.num == 5 or event.delta < 0:
                # 缩小
                if hasattr(self, 'pixel_size'):
                    self.pixel_size = max(self.pixel_size - 1, 1)  # 最小缩小到1
            
            # 更新画布
            self.update_draw_canvas()
            print(f"缩放后像素大小: {self.pixel_size}")
        except Exception as e:
            print(f"处理画布缩放失败: {str(e)}")
    
    def on_draw_canvas_right_click(self, event):
        """处理绘制画布右键点击事件"""
        try:
            # 开始拖动
            self.is_dragging = True
            self.last_x = event.x
            self.last_y = event.y
        except Exception as e:
            print(f"处理画布右键点击失败: {str(e)}")
    
    def on_draw_canvas_right_motion(self, event):
        """处理绘制画布右键拖动事件"""
        try:
            if not self.is_dragging:
                return
            
            # 计算拖动距离
            delta_x = event.x - self.last_x
            delta_y = event.y - self.last_y
            
            # 更新偏移量
            self.canvas_offset_x += delta_x
            self.canvas_offset_y += delta_y
            
            # 更新画布
            self.update_draw_canvas()
            
            # 更新最后位置
            self.last_x = event.x
            self.last_y = event.y
        except Exception as e:
            print(f"处理画布右键拖动失败: {str(e)}")
    
    def on_draw_canvas_right_release(self, event):
        """处理绘制画布右键释放事件"""
        try:
            # 结束拖动
            self.is_dragging = False
        except Exception as e:
            print(f"处理画布右键释放失败: {str(e)}")
    
    def generate_img(self):
        """生成图模数据"""
        try:
            # 获取参数
            img_name = self.img_name_var.get()
            if not img_name:
                self.img_status_var.set("请输入图像名称")
                messagebox.showwarning("警告", "请输入图像名称")
                return
            
            # 检查是否有图像文件路径
            img_path = self.img_path_var.get()
            if img_path:
                # 从图像文件生成图模
                try:
                    width = int(self.img_width_var.get())
                    height = int(self.img_height_var.get())
                    threshold = int(self.threshold_var.get())
                except ValueError:
                    self.img_status_var.set("无效的尺寸或阈值")
                    messagebox.showwarning("警告", "请输入有效的尺寸和阈值")
                    return
                
                invert = self.invert_var.get()
                
                self.img_status_var.set("正在生成图模...")
                self.root.update()
                
                # 转换图像为点阵数据
                bitmap_data = self.image_to_bitmap(img_path, width, height, threshold, invert)
                
                # 保存生成的图模数据到实例变量
                self.current_img_data = bitmap_data
                self.current_img_name = img_name
                self.current_img_width = width
                self.current_img_height = height
                
                # 更新预览
                self.update_preview_size()
                
                self.img_status_var.set(f"图模生成成功: {img_name}, 尺寸: {width}x{height}, 数据长度: {len(bitmap_data)} 字节")
                messagebox.showinfo("成功", f"图模生成成功\n图像名称: {img_name}\n尺寸: {width}x{height}\n数据长度: {len(bitmap_data)} 字节")
                
                # 跳转到预览页面
                self.notebook.select(self.preview_tab)
            elif hasattr(self, 'draw_data') and len(self.draw_data) > 0:
                # 从绘制内容生成图模
                height = len(self.draw_data)
                width = len(self.draw_data[0]) if height > 0 else 0
                
                if width == 0 or height == 0:
                    self.img_status_var.set("绘制区域为空")
                    messagebox.showwarning("警告", "绘制区域为空，请先绘制内容")
                    return
                
                # 使用绘制页面中定义的图像名称
                if hasattr(self, 'draw_img_name_var'):
                    draw_img_name = self.draw_img_name_var.get()
                    if draw_img_name:
                        img_name = draw_img_name
                
                self.img_status_var.set("正在从绘制内容生成图模...")
                self.root.update()
                
                # 转换绘制数据为点阵数据（纵向8点，高位在下格式）
                bitmap_data = []
                for row in range(0, height, 8):
                    for col in range(width):
                        byte = 0
                        for bit in range(8):
                            y = row + bit
                            if y < height and self.draw_data[y][col]:
                                byte |= (1 << bit)
                        bitmap_data.append(byte)
                
                # 保存生成的图模数据到实例变量
                self.current_img_data = bitmap_data
                self.current_img_name = img_name
                self.current_img_width = width
                self.current_img_height = height
                
                # 更新预览
                self.update_preview_size()
                
                self.img_status_var.set(f"图模生成成功: {img_name}, 尺寸: {width}x{height}, 数据长度: {len(bitmap_data)} 字节")
                messagebox.showinfo("成功", f"从绘制内容生成图模成功\n图像名称: {img_name}\n尺寸: {width}x{height}\n数据长度: {len(bitmap_data)} 字节")
                
                # 跳转到预览页面
                self.notebook.select(self.preview_tab)
            else:
                # 既没有图像文件，也没有绘制数据
                self.img_status_var.set("请选择图像文件或在绘制标签页中绘制内容")
                messagebox.showwarning("警告", "请选择图像文件或在绘制标签页中绘制内容")
                return
        except Exception as e:
            self.img_status_var.set(f"生成图模失败: {str(e)}")
            messagebox.showerror("错误", f"生成图模失败: {str(e)}")
    
    def save_img_to_file(self):
        """将生成的图模保存到OLED_Data.c文件"""
        try:
            # 检查是否已生成图模
            if not hasattr(self, 'current_img_data'):
                self.preview_status_var.set("请先生成图模")
                messagebox.showwarning("警告", "请先生成图模")
                return
            
            # 获取工程路径
            project_dir = self.project_path_var.get()
            if not project_dir:
                self.preview_status_var.set("请选择工程目录")
                messagebox.showwarning("警告", "请选择工程目录")
                return
            
            # 构建OLED_Data.c文件路径
            oled_data_file = os.path.join(project_dir, 'HardWare', 'OLED_Data.c')
            if not os.path.exists(oled_data_file):
                self.preview_status_var.set("未找到OLED_Data.c文件")
                messagebox.showwarning("警告", f"未找到文件: {oled_data_file}")
                return
            
            # 读取文件内容
            with open(oled_data_file, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # 生成图模数组定义
            img_name = self.current_img_name
            bitmap_data = self.current_img_data
            
            # 图像名称重复检测
            response = False  # 默认为不覆盖
            # 查找现有的图像定义
            existing_img_pattern = rf'const\s+uint8_t\s+{re.escape(img_name)}\[\]\s*='
            if re.search(existing_img_pattern, content):
                # 图像名称已存在，询问用户处理方式
                response = messagebox.askyesnocancel(
                    "图像名称已存在",
                    f"图像名称 '{img_name}' 已存在，是否覆盖？\n选择 '是' 覆盖现有图像，选择 '否' 重新命名，选择 '取消' 取消操作"
                )
                
                if response is None:
                    # 取消操作
                    self.preview_status_var.set("保存操作已取消")
                    return
                elif response is False:
                    # 重新命名
                    new_name = img_name
                    counter = 1
                    while True:
                        new_name = f"{img_name}_{counter}"
                        existing_img_pattern = rf'const\s+uint8_t\s+{re.escape(new_name)}\[\]\s*='
                        if not re.search(existing_img_pattern, content):
                            break
                        counter += 1
                    
                    # 更新图像名称
                    img_name = new_name
                    self.current_img_name = new_name
                    self.img_name_var.set(new_name)
                    self.preview_status_var.set(f"图像名称已改为: {new_name}")
            
            # 格式化数据为C语言数组格式
            data_str = []
            for i, byte in enumerate(bitmap_data):
                if i % 16 == 0:
                    if i > 0:
                        data_str.append('\n\t')
                    else:
                        data_str.append('\t')
                data_str.append(f'0x{byte:02X},')
                if i % 16 < 15:
                    data_str.append(' ')
            
            # 移除最后一个逗号
            if data_str and data_str[-1] == ',':
                data_str.pop()
            
            img_array_def = f"const uint8_t {img_name}[] = {{\n{' '.join(data_str)}\n}};"
            
            # 查找图像数据区域
            # 尝试多种可能的位置查找策略
            img_data_pos = None
            
            # 策略1: 查找图像数据注释
            img_data_patterns = [
                r'/\*\*\*\*\*\*\*\*\*\*\*\*\*图像数据\*/',  # 原格式
                r'/\*图像数据\*/',  # 简化格式
                r'/\*相同的汉字只需要定义一次，汉字不分先后顺序\*/'  # 汉字注释
            ]
            
            for pattern in img_data_patterns:
                matches = re.finditer(pattern, content)
                for match in matches:
                    img_data_pos = match.end()
                    break
                if img_data_pos:
                    break
            
            # 策略2: 查找汉字字模数组结束
            if not img_data_pos:
                font_end_pattern = r'\{NULL, \{0\}, 0, 0\} // 结束标志'
                match = re.search(font_end_pattern, content)
                if match:
                    img_data_pos = match.end()
            
            # 策略3: 查找文件末尾
            if not img_data_pos:
                img_data_pos = len(content)
            
            if not img_data_pos:
                self.preview_status_var.set("未找到合适的插入位置")
                messagebox.showwarning("警告", "未找到合适的插入位置")
                return
            
            # 在图像数据区域末尾添加新的图像数据
            # 查找文件末尾的注释标记
            end_comment = r'/\*\*\*\*\*江协科技\|版权所有\*\*\*\*\*/'
            end_match = re.search(end_comment, content[img_data_pos:])
            
            if end_match:
                # 在版权注释前插入新图像数据
                insert_pos = img_data_pos + end_match.start()
                new_content = content[:insert_pos] + '\n\n' + img_array_def + '\n' + content[insert_pos:]
            else:
                # 在文件末尾添加新图像数据
                new_content = content + '\n\n' + img_array_def
            
            # 更新OLED_Data.h文件
            oled_data_h_file = os.path.join(project_dir, 'HardWare', 'OLED_Data.h')
            if os.path.exists(oled_data_h_file):
                with open(oled_data_h_file, 'r', encoding='utf-8') as f:
                    h_content = f.read()
                
                # 检测头文件中是否已存在该图像声明
                existing_decl_pattern = rf'extern\s+const\s+uint8_t\s+{re.escape(img_name)}\[\]\s*;'
                if re.search(existing_decl_pattern, h_content):
                    # 头文件中已存在声明，需要处理
                    if response is True:  # 如果用户选择覆盖
                        # 删除现有的声明
                        h_content = re.sub(existing_decl_pattern, '', h_content)
                    
                # 查找图像数据声明区域
                img_decl_pattern = r'// 图像数据'
                img_decl_match = re.search(img_decl_pattern, h_content)
                
                if img_decl_match:
                    # 找到图像数据注释，现在找到所有图像声明的结束位置
                    # 从注释位置开始查找所有图像声明
                    start_pos = img_decl_match.end()
                    # 查找所有extern const uint8_t声明，直到遇到非声明行
                    img_decl_end = start_pos
                    lines = h_content[start_pos:].split('\n')
                    for i, line in enumerate(lines):
                        line = line.strip()
                        if line.startswith('extern const uint8_t') and line.endswith(';'):
                            img_decl_end = start_pos + sum(len(lines[j]) + 1 for j in range(i + 1))
                        elif line and not line.startswith('//'):
                            break
                    
                    # 在图像数据声明区域末尾添加新的声明
                    new_decl = f'\nextern const uint8_t {img_name}[];'
                    # 确保在新行添加
                    if img_decl_end > start_pos and h_content[img_decl_end - 1] != '\n':
                        new_decl = f'\n{new_decl}'
                    new_h_content = h_content[:img_decl_end] + new_decl + h_content[img_decl_end:]
                    
                    # 写入更新后的头文件
                    with open(oled_data_h_file, 'w', encoding='utf-8') as f:
                        f.write(new_h_content)
                else:
                    # 如果未找到图像数据声明区域，在文件末尾添加
                    new_h_content = h_content + f'\n// 新增图像数据\nextern const uint8_t {img_name}[];\n'
                    with open(oled_data_h_file, 'w', encoding='utf-8') as f:
                        f.write(new_h_content)
                
                # 如果用户选择覆盖，需要先删除源文件中的现有定义
                if response is True:
                    # 删除现有图像定义
                    # 匹配完整的图像定义，包括数组内容
                    # 使用非贪婪匹配，从{开始到};结束
                    # 正确转义大括号
                    img_def_pattern = rf'const\s+uint8_t\s+{re.escape(img_name)}\[\]\s*=\s*\{{([^}}]*\}})*?\}};'
                    content = re.sub(img_def_pattern, '', content, flags=re.DOTALL)
                    
                    # 清理可能留下的多余空行
                    content = re.sub(r'\n\s*\n', '\n', content)
            
            # 如果用户选择覆盖，在写入新内容之前执行覆盖逻辑
            if response is True:
                # 删除现有图像定义
                # 匹配完整的图像定义，包括数组内容
                # 处理数值之间有空格的情况
                img_def_pattern = rf'const\s+uint8_t\s+{re.escape(img_name)}\[\]\s*=\s*\{{[^{{}}]*\}}\s*;' 
                content = re.sub(img_def_pattern, '', content, flags=re.DOTALL)
                
                # 清理可能留下的多余空行
                content = re.sub(r'\n\s*\n', '\n', content)
                
                # 重新生成new_content，使用更新后的content
                if end_match:
                    # 在版权注释前插入新图像数据
                    insert_pos = img_data_pos + end_match.start()
                    new_content = content[:insert_pos] + '\n\n' + img_array_def + '\n' + content[insert_pos:]
                else:
                    # 在文件末尾添加新图像数据
                    new_content = content + '\n\n' + img_array_def
            
            # 写入更新后的C文件
            with open(oled_data_file, 'w', encoding='utf-8') as f:
                f.write(new_content)
            
            self.preview_status_var.set(f"图模已保存到文件: {img_name}")
            messagebox.showinfo("成功", f"图模已成功保存到文件\n图像名称: {img_name}\n文件位置: {oled_data_file}")
        except Exception as e:
            self.preview_status_var.set(f"保存图模失败: {str(e)}")
            messagebox.showerror("错误", f"保存图模失败: {str(e)}")
    
    def pack_and_download_files(self):
        """打包并下载OLED相关文件"""
        try:
            # 定义需要打包的文件路径
            base_path = "e:/公司资料/个人资料/字模生成工具/基础工程模板 3.0 - 调度器优化与屏幕驱动压缩/HardWare"
            files_to_pack = [
                os.path.join(base_path, "OLED_Data.c"),
                os.path.join(base_path, "OLED_Data.h"),
                os.path.join(base_path, "OLED.c"),
                os.path.join(base_path, "OLED.h")
            ]
            
            # 检查文件是否存在
            for file_path in files_to_pack:
                if not os.path.exists(file_path):
                    messagebox.showerror("错误", f"文件不存在: {file_path}")
                    return
            
            # 创建临时zip文件
            temp_zip = "OLED_Files.zip"
            with zipfile.ZipFile(temp_zip, 'w') as zipf:
                for file_path in files_to_pack:
                    file_name = os.path.basename(file_path)
                    zipf.write(file_path, file_name)
            
            # 让用户选择保存位置
            save_path = filedialog.asksaveasfilename(
                title="保存OLED文件",
                defaultextension=".zip",
                filetypes=[("ZIP文件", "*.zip")],
                initialfile="OLED_Files.zip"
            )
            
            if save_path:
                # 移动zip文件到用户指定位置
                import shutil
                shutil.move(temp_zip, save_path)
                messagebox.showinfo("成功", f"文件已成功下载到: {save_path}")
            else:
                # 用户取消选择
                os.remove(temp_zip)
        except Exception as e:
            messagebox.showerror("错误", f"下载失败: {str(e)}")
            # 清理临时文件
            if os.path.exists("OLED_Files.zip"):
                try:
                    os.remove("OLED_Files.zip")
                except:
                    pass

if __name__ == "__main__":
    
    # 先创建根窗口
    root = tk.Tk()
    root.title("汉字字模生成工具")

    # 然后再创建配置实例
    config = GeneratorConfig()

    # 启动主循环
    app = ConfigWindow(root, config)
    root.mainloop()