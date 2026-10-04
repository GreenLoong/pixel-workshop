# pixel-workshop

[![Windows build and tests](https://github.com/GreenLoong/pixel-workshop/actions/workflows/windows.yml/badge.svg)](https://github.com/GreenLoong/pixel-workshop/actions/workflows/windows.yml)

我基于 C++17、Qt 6 和 OpenCV 4 开发 Windows 桌面图像编辑与批量处理工具。

## 已实现功能

- 打开、恢复原图、PNG 另存为；记住上次打开目录，输出重名自动编号。
- 预览缩放 1%～800%，滚轮在整个预览区生效，拖动限制在图片范围；黑色背景的图片全屏预览，Esc 返回，双击切换适应与实际大小。
- 像素与百分比调整尺寸，保持宽高比，显示原图、当前和目标尺寸。
- 自由与固定比例裁剪、左右旋转 90°、任意角度旋转、水平和垂直翻转。
- 灰度、亮度、对比度、曝光、饱和度、色温、色调、高光、阴影、清晰度、晕影组合处理。
- 背景模糊、移除、纯色／图片替换；主体范围与前景／背景画笔修正，透明 PNG 输出。
- 关闭或替换图片前检查未保存结果与编辑草稿，提供保存、放弃、取消；保存位置随撤销重做同步。
- 最多 50 次参数编辑撤销与重做，打开新图片清空历史。
- 独立批量参数、逐图长边／百分比计算，内置预设和自定义预设保存／删除。
- 文件夹批量处理，后台逐张执行，进度、成功／失败记录、取消、CSV 报告；原文件保护和重名保护。
- 单张预览与完整图像处理在后台执行，只应用最新结果；完整处理可取消。
- 圆角无标题栏界面、窗口内编辑页和原创猫娘画师应用图标。

## 使用方法

1. 打开图片，点击“编辑图片”进入编辑页。在顶部切换裁剪旋转、颜色光线、尺寸和背景；切换保留参数，点击“完成编辑”统一应用，“取消”放弃本次会话。
2. 单张图片使用“另存为”。选择的是完整处理结果，预览缩放不改变输出尺寸。
3. 选择“文件夹批量处理”，设置不同的输入与输出目录，点击“开始处理”。参数可独立修改，可选保持尺寸、固定宽高、限制长边、百分比及颜色／背景设置，支持内置和自定义预设，也可沿用主页参数。
4. 查看结果表。需要时取消任务或导出 CSV；取消会等待当前图片运算结束，保留已完成文件。

批量仅扫描当前目录，不包含子目录。输出 `<原文件名>-result.png`，重名生成 `(1)`、`(2)` 等编号。默认背景分割使用内置 PPHumanSeg 人像模型，非人像可以选择 GrabCut 通用区域模式；复杂发丝可能需要画笔修正；缩略预览与完整输出的细节可能存在差异。

主页面保留打开、编辑、另存为、批量处理四个入口。标题栏左上角显示应用名，窗口按钮仍在右上角。Ctrl+O 打开、Ctrl+S 保存、Ctrl+E 编辑；编辑页内 Ctrl+Z / Ctrl+Y 操作本次草稿历史，返回主页后操作已完成会话的历史。详细操作见[统一编辑页](docs/统一编辑页与交互修复.md)。

## 构建环境

本机已验证：Windows 64 位、Qt 6.11.1 MSVC 2022 x64、OpenCV 4.13.0、CMake 与 Ninja。不能把 MinGW Qt 与 MSVC OpenCV 混用。

在 Qt Creator 打开 `ImageBatchTool/CMakeLists.txt`，选择 MSVC 64 位 Kit，为 CMake 配置 `OpenCV_DIR` 指向安装目录内含 `OpenCVConfig.cmake` 的文件夹。具体步骤见[环境配置文档](docs/环境配置.md)。

构建测试时启用 `IMAGEBATCHTOOL_BUILD_TESTS=ON`，构建后运行 CTest。OpenCV DLL 所在目录需要位于测试进程的 PATH。九项检查覆盖处理算法、参数对话框与预览交互、后台批量任务、人像模型推理、图像内存适配、单张后台任务、未保存修改保护、批量参数预设和部署自检。真实鼠标、触控板和不同电脑上的运行仍需手动验收。

每次推送 main 或提交 PR，GitHub 会自动构建并运行普通与 200% 显示缩放下的测试，生成 ZIP，再在不安装 Qt/OpenCV 的独立 Windows 运行机验证包内文件、处理流程和主窗口启动；记录及 `windows-package` 下载见 [Actions](https://github.com/GreenLoong/pixel-workshop/actions/workflows/windows.yml)。

Windows 分发方法见[Windows 构建与交付](docs/Windows构建与交付.md)。取得包后整体解压，运行 `ImageBatchTool.exe`；在包目录执行 `powershell -NoProfile -ExecutionPolicy Bypass -File .\verify-package.ps1` 可生成自动验收记录。另一台电脑的人工检查见[开始使用与验收](docs/另一台电脑验收.md)。

## 源码组织

源码位于 `ImageBatchTool/src`：`domain` 管理 OpenCV 算法与参数，`infrastructure` 管理文件、模型及格式适配，`application` 管理处理流程和后台任务，`presentation` 管理窗口、页面、对话框和控件。图标、样式与模型位于 `ImageBatchTool/resources`。CMake 为四层建立模块目标，程序和测试复用同一实现。

完整目录、重构清单和关键代码见[项目分层与重构清单](docs/项目分层与重构清单.md)。

## 项目文档

- [开发文档](开发文档.md)：目标、范围、工程思维与开发记录。
- [编辑功能与批量处理](docs/功能扩展与批量处理.md)：参数、后台任务、取消行为与验证。
- [图像处理模块设计](docs/图像处理组合与模块设计.md)：模块边界与处理流程。
- [预览缩放与拖动](docs/预览缩放与拖动.md)：预览交互。
- [图标来源与制作](docs/图标来源与制作.md)：网络参考、生成方式与文件。
- [OpenCV 环境验证示例](docs/OpenCV环境验证示例.md)：独立验证示例，程序入口仅负责应用初始化。

背景算法的来源、许可证和替换依据见[人像分割算法替换](docs/人像分割算法替换.md)。

分层之后的稳定性与发布完善记录见[小版本记录](docs/稳定性与发布完善.md)。当前版本：v0.19.0 候选版，实际设备人工验收待完成。
