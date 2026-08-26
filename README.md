# Lizard's Biter.ProgramX
## 一款谁都可以做扩展的开源实用软件，但是，扩展需要按照要求

# 拓展要求:
## 1.若只有一个文件,只需要在主分支上传你的拓展包
## 2.如有多个文件,请创建一个文件夹，名字格式:Biter.ProgramX-Expend-你的名字(English)-版本
## 3.如果要创建一个包,请使用上面的命名格式


> 一款谁都可以做扩展的开源实用软件 —— Windows 命令行多功能工具箱，包含时钟、日历、记事本、计算器、待办事项列表等实用功能。  
> An open‑source multi‑tool for Windows command line – clock, calendar, notepad, calculator, todo list, and more. Everyone can extend it!

---

## 📖 中文版

### 项目简介
Biter.ProgramX 是一款轻量级的 Windows 命令行实用工具，设计初衷是提供日常常用的小功能，同时保持代码简洁、易于二次开发和扩展。项目完全开源，欢迎任何人提交功能扩展或改进建议。

### ✨ 功能列表
| 功能 | 说明 |
|------|------|
| 实时时钟 | 动态显示当前时间，按任意键退出 |
| 月历查看 | 输入年份和月份，打印该月日历 |
| 多行记事本 | 支持多行文本录入，输入 `.end` 结束并保存 |
| 简单计算器 | 支持 `+ - * / %` 四则运算和取模 |
| 待办事项列表 | 添加、删除、标记完成，数据持久化存储 |
| 系统托盘通知 | 程序启动和退出时显示气泡提示 |
| 用户登录/注册 | 单用户演示，用户名保存在本地文件 |
| 帮助系统 | 输入 `Help` 或 `help` 查看所有命令 |
| 清屏、退出、关于信息 | 常用辅助命令 |

### 🛠️ 编译与运行

#### 环境要求
- Windows 操作系统（Win7 及以上）
- MinGW-w64 或 MSVC（推荐使用 g++）
- 资源文件 `rec.res`（包含程序图标和版本信息）

#### 编译命令（官方推荐）
```bash
g++ Biter.ProgramX-Beta3.cpp rec.res -s -static -O2 -g3 -o Biter.ProgramX.exe -std=c++11 -lshell32 -luser32 -lgdi32 -ladvapi32

注意：rec.res 文件请从本仓库的 Release 中下载，或使用 windres 编译根目录下的 Biter.ProgramX.rc 文件自行生成。

🤝 贡献与反馈
欢迎提交 Issue 和 Pull Request。

如果你有新的想法或建议，也可以在 Discussions 中发起讨论。

📄 许可证
本项目采用 MIT 许可证，详情见 LICENSE 文件。

🌐 English Version
Introduction
Biter.ProgramX is a lightweight Windows command‑line utility that provides everyday tools while keeping the code simple and easy to extend. It is fully open‑source, and everyone is welcome to contribute extensions or improvements.

✨ Features
Feature	Description
Real‑time clock	Displays current time dynamically, press any key to exit
Monthly calendar	Enter year and month to print a calendar
Multi‑line notepad	Supports multi‑line input, end with .end to save
Simple calculator	Supports + - * / % (add, subtract, multiply, divide, modulo)
To‑do list	Add, delete, mark as done; data persists to disk
System tray notifications	Balloon tips on program start and exit
User login/register	Single‑user demo, username saved locally
Help system	Type Help or help to list all commands
Clear screen, exit, about	Common auxiliary commands
🛠️ Build & Run
Prerequisites
Windows OS (XP or later)

MinGW‑w64 or MSVC (g++ recommended)

Resource file rec.res (contains program icon and version info)

Build Command (Official)
bash
g++ Biter.ProgramX-Beta3.cpp rec.res -s -static -O2 -g3 -o Biter.ProgramX.exe -std=c++11 -lshell32 -luser32 -lgdi32 -ladvapi32
Note: Download rec.res from the Releases section of this repo, or generate it yourself using windres with the provided Biter.ProgramX.rc file.

Run
Double‑click Biter.ProgramX.exe. On first use, choose “Register” and enter a username. After that, log in and you will enter the Shell environment. Type help to see all available commands.

📦 Extension Guidelines (Important)
We welcome all developers to contribute extensions. To keep the project tidy, please follow these rules:

Single‑file extension: Submit your file (.cpp or .h) directly to the main branch, and describe its functionality in the file header comments.

Multi‑file extension: Create a folder with the naming format:
Biter.ProgramX-Expend-YourName(English)-Version
Example: Biter.ProgramX-Expend-John-v1.0

Packaged release: If you create an independent extension pack (multiple files or resources), use the above naming format, compress it, and provide a download link in the Release section.

All extensions must be under the same open‑source license (MIT) and must not contain any malicious code.

🤝 Contributing & Feedback
Issues and Pull Requests are always welcome.

Feel free to start a discussion in the Discussions tab if you have new ideas.

📄 License
This project is licensed under the MIT License – see the LICENSE file for details.
