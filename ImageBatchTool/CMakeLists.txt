cmake_minimum_required(VERSION 3.19)
project(ImageBatchTool LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
#core:cv::Mat 等基础数据结构。
#imgproc：灰度转换、缩放等图像处理。
#imgcodecs：图片读取和保存。
find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")

qt_standard_project_setup()

qt_add_executable(ImageBatchTool
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_include_directories(ImageBatchTool PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(ImageBatchTool
    PRIVATE
        Qt::Core
        Qt::Widgets
        ${OpenCV_LIBS}
)

install(TARGETS ImageBatchTool
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ImageBatchTool
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
