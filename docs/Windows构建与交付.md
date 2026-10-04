# Windows 构建与交付

日期：2026-10-04。本文供我和取得源码的开发者复现构建、准备运行目录与检查交付；适用于本机 Qt 6.11.1 MSVC 2022 x64 和 OpenCV 4.13.0。

## 构建与使用

1. 在 Qt Creator 打开 `ImageBatchTool/CMakeLists.txt`，选择 MSVC 64 位 Kit 和 Release。
2. 设置 `OpenCV_DIR=D:/Tools/OpenCV/opencv/build`。交付构建再设置 `IMAGEBATCHTOOL_BUILD_TESTS=ON`，使程序和 `PixelWorkshopCheck.exe` 一起生成，执行 CMake，然后构建。
3. 本机生成文件为 `ImageBatchTool/build/release-clean/ImageBatchTool.exe`。运行依赖必须随程序提供，不能只复制 exe。
4. 启动后打开图片，用“编辑图片”进入编辑页，顶部切换模式，点击“完成编辑”应用全部调整；取消返回主页且不修改主图。单张使用“另存为”。批量选择不同的输入、输出目录，开始后查看进度与结果表；取消等待当前图片运算结束，保留已完成输出。
5. 更新构建后，已打开的旧进程不会自动切换版本。先保存需要保留的结果，关闭旧窗口，再运行新程序。

## 生成运行目录和 ZIP

在项目根目录的 PowerShell 执行：

```powershell
./tools/package-windows.ps1
```

脚本要求源码已提交，检查 Release 构建及版本资源、复制程序与自检工具、调用 windeployqt 收集 Qt 依赖和编译器运行库、复制 OpenCV DLL，生成 `build/pixel-workshop-v<版本>-windows-x64-<日期时间>`、同名 ZIP 和 SHA256 文件。包内有源码提交、逐文件哈希清单、自动验收脚本、操作清单与依赖许可证。它使用新目录保留先前打包结果。更换电脑时通过脚本的 `BuildDirectory`、`QtDirectory`、`OpenCVDll` 参数传入实际路径。

windeployqt 负责 Qt 依赖，额外的 OpenCV 运行库单独复制。插件需要保持 `platforms`、`imageformats` 等子目录结构，`qt.conf` 让程序从包内寻找插件。依据见 [Qt 6.11 Windows 部署文档](https://doc.qt.io/qt-6.11/windows-deployment.html)。

目标电脑需要与构建工具兼容的 x64 Visual C++ 运行库。windeployqt 使用官方部署能力收集运行库；如果包内提供 `vc_redist.x64.exe`，由使用者在目标电脑安装。缺少运行库时，安装方式与下载见 [Microsoft 官方运行库文档](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)。脚本不自动安装目标电脑的软件。

## 验证记录与待办

v0.18.1 在 GitHub 上的 Release 配置构建和八项 CTest 已在普通与 200% 显示缩放下通过，记录见[远端运行](https://github.com/GreenLoong/pixel-workshop/actions/runs/37200344587)。v0.19.0 增加第九项部署自检，本机九项 CTest 已在普通与 200% 显示缩放下通过，并在工作流中增加打包和独立运行机验证；部署结果以对应 Actions 和包内 `verification-result.json` 为准。

独立运行机不安装 Qt/OpenCV，且自检将子进程 PATH 限制为 Windows 系统目录，但 GitHub Windows 镜像仍带有系统组件和 Visual C++ 运行库。实际电脑验收按[开始使用与验收](另一台电脑验收.md)执行；真实鼠标／触控板和使用者设备体验仍待完成。

## 常见问题

| 现象 | 检查与处理 |
| --- | --- |
| 缺少 OpenCV DLL | 检查包内 `opencv_world4130.dll`，必须与链接版本一致 |
| 缺少 Qt DLL | 重新运行与构建 Kit 对应的 windeployqt，不混用 MinGW 或 Debug 文件 |
| 无法初始化 Qt 平台插件 | 检查 `platforms/qwindows.dll` 和包内 `qt.conf`，排查环境中的旧 Qt 插件路径 |
| 某种图片无法读取 | 查看失败原因，确认格式插件已部署，文件是否损坏 |
| 输出失败 | 检查输出目录权限和磁盘空间，结果表列出具体失败文件 |

项目源码与说明：[pixel-workshop](https://github.com/GreenLoong/pixel-workshop)。

## 人像模型更新（2026-10-04）

PPHumanSeg 模型已嵌入可执行文件，运行时不依赖外部模型路径或 Python。构建包含 OpenCV dnn，打包脚本同时复制模型的许可证与来源说明。新增第四项模型测试，算法与验证见[人像分割算法替换](人像分割算法替换.md)。
