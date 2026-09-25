# pixel-workshop

基于 C++、Qt 6 和 OpenCV 4 开发的 Windows 桌面批量图像处理工具。

## 当前进度

- 已创建 Qt Widgets 与 CMake 工程。
- 已使用 Qt 6.11.1、MSVC 2022 x64 Kit 构建并运行窗口。
- CMake 已找到并链接 OpenCV 4.13.0。
- OpenCV 运行时调用、图像读写和灰度转换仍待验收。
- 图片预览、参数调整和批量处理功能仍待开发。

## 开发环境

- Windows 64 位
- C++17
- Qt 6.11.1 Widgets
- MSVC 2022 x64
- CMake + Ninja
- OpenCV 4.13.0

本机配置方法和验收步骤见[环境配置文档](docs/环境配置.md)。项目范围、阶段计划和开发记录见[开发文档](开发文档.md)。
