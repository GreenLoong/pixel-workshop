# pixel-workshop

基于 C++、Qt 6 和 OpenCV 4 开发的 Windows 桌面批量图像处理工具。

## 当前进度

- 已创建 Qt Widgets 与 CMake 工程。
- 已使用 Qt 6.11.1、MSVC 2022 x64 Kit 构建并运行窗口。
- CMake 已找到并链接 OpenCV 4.13.0。
- OpenCV 灰度转换、PNG 保存和重新读取验证已通过。
- 已实现图片选择、等比例预览、取消选择和读取失败提示；横图、竖图及损坏图片等场景待完成手动验收。
- 窗口大小改变后的预览适配、参数调整和批量处理仍待开发。

当前启动程序会运行环境检查，并在工作目录的 `temp` 文件夹中写入 `environment-check.png`。该检查后续将与产品启动流程分离。

## 开发环境

- Windows 64 位
- C++17
- Qt 6.11.1 Widgets
- MSVC 2022 x64
- CMake + Ninja
- OpenCV 4.13.0

本机配置方法和验收步骤见[环境配置文档](docs/环境配置.md)。项目范围、阶段计划和开发记录见[开发文档](开发文档.md)。
