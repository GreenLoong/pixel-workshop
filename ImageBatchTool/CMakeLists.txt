cmake_minimum_required(VERSION 3.19)

set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MSVCDependencies.cmake")
project(ImageBatchTool VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 再次配置时重新查找部署工具，避免本机复现的缓存路径读取崩溃。
if(WIN32)
    unset(WINDEPLOYQT_EXECUTABLE CACHE)
endif()
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Svg Concurrent)
find_package(OpenCV 4.10 REQUIRED COMPONENTS core imgproc imgcodecs dnn)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
qt_standard_project_setup()

# 源文件只在所属模块列举一次，应用和测试链接同一实现。
add_library(pixel_domain STATIC
    src/domain/imageprocessor.cpp src/domain/imageprocessor.h
    src/domain/batchoptions.cpp src/domain/batchoptions.h
    src/domain/geometryprocessor.cpp
    src/domain/backgroundprocessor.cpp
    src/domain/humansegmentation.cpp src/domain/humansegmentation.h)
set_target_properties(pixel_domain PROPERTIES AUTOMOC OFF AUTOUIC OFF)
target_include_directories(pixel_domain PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src" ${OpenCV_INCLUDE_DIRS})
target_link_libraries(pixel_domain PUBLIC ${OpenCV_LIBS})

add_library(pixel_infrastructure STATIC
    src/infrastructure/batchpresets.cpp src/infrastructure/batchpresets.h
    src/infrastructure/imagefiles.cpp src/infrastructure/imagefiles.h
    src/infrastructure/imageconversion.cpp src/infrastructure/imageconversion.h
    src/infrastructure/modelresources.cpp src/infrastructure/modelresources.h)
target_link_libraries(pixel_infrastructure PUBLIC pixel_domain Qt::Gui)
add_library(pixel_application STATIC
    src/application/imageprocessing.cpp src/application/imageprocessing.h
    src/application/batchjob.cpp src/application/batchjob.h
    src/application/backgroundpreview.cpp src/application/backgroundpreview.h
    src/application/imagetask.cpp src/application/imagetask.h)
target_link_libraries(pixel_application PUBLIC pixel_infrastructure Qt::Concurrent)
add_library(pixel_presentation STATIC
    src/presentation/windows/mainwindow.cpp src/presentation/windows/mainwindow.h src/presentation/windows/mainwindow.ui
    src/presentation/pages/editorpage.cpp src/presentation/pages/editorpage.h
    src/presentation/dialogs/backgrounddialog.cpp src/presentation/dialogs/backgrounddialog.h
    src/presentation/dialogs/batchdialog.cpp src/presentation/dialogs/batchdialog.h
    src/presentation/dialogs/geometrydialog.cpp src/presentation/dialogs/geometrydialog.h
    src/presentation/widgets/resizepanel.cpp src/presentation/widgets/resizepanel.h src/presentation/widgets/resizepanel.ui
    src/presentation/dialogs/tonedialog.cpp src/presentation/dialogs/tonedialog.h
    src/presentation/widgets/batchparameters.cpp src/presentation/widgets/batchparameters.h
    src/presentation/widgets/dialogappearance.cpp src/presentation/widgets/dialogappearance.h
    src/presentation/widgets/previewlabel.cpp src/presentation/widgets/previewlabel.h
    src/presentation/widgets/selectionitem.cpp src/presentation/widgets/selectionitem.h
    src/presentation/widgets/sliderstyle.h src/presentation/widgets/previewimage.h
    src/presentation/widgets/brushpreview.cpp src/presentation/widgets/brushpreview.h
    src/presentation/widgets/zoomcontrols.cpp src/presentation/widgets/zoomcontrols.h)
target_link_libraries(pixel_presentation PUBLIC pixel_application Qt::Widgets Qt::Svg Qt::Concurrent)

# 对象库确保资源注册代码进入每个可执行文件，不被静态链接器裁掉。
add_library(pixel_resources OBJECT resources/indicators.qrc)
set_target_properties(pixel_resources PROPERTIES AUTORCC ON AUTOMOC OFF AUTOUIC OFF)
target_link_libraries(pixel_resources PRIVATE Qt::Core)
# Qt Creator 15+ 按 FOLDER 收拢目标，源文件沿用磁盘目录。
foreach(module IN ITEMS pixel_domain pixel_infrastructure pixel_application pixel_presentation pixel_resources)
    set_target_properties(${module} PROPERTIES FOLDER "Modules")
    get_target_property(module_sources ${module} SOURCES)
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${module_sources})
endforeach()
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES src/main.cpp)
if(MSVC)
    foreach(module IN ITEMS pixel_domain pixel_infrastructure pixel_application pixel_presentation)
        target_compile_options(${module} PRIVATE /utf-8)
    endforeach()
endif()
qt_add_executable(ImageBatchTool WIN32 MACOSX_BUNDLE src/main.cpp)
target_link_libraries(ImageBatchTool PRIVATE pixel_presentation pixel_resources)
target_compile_definitions(ImageBatchTool PRIVATE PIXEL_WORKSHOP_VERSION="${PROJECT_VERSION}")
if(WIN32)
    enable_language(RC)
    target_sources(ImageBatchTool PRIVATE resources/appicon.rc)
    configure_file(cmake/version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/version.rc" PROPERTIES GENERATED TRUE)
    target_sources(ImageBatchTool PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES resources/appicon.rc)
    source_group("Generated Files" FILES "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
    target_include_directories(ImageBatchTool PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
endif()
install(TARGETS ImageBatchTool
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
qt_generate_deploy_app_script(TARGET ImageBatchTool OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})

option(IMAGEBATCHTOOL_BUILD_TESTS "Build development regression tests" OFF)
option(IMAGEBATCHTOOL_BUILD_PACKAGE_CHECKS "Build the standalone delivery checker" OFF)
# 开关从 ON 改回 OFF 时也刷新 CTest 清单，避免继续引用旧测试。
enable_testing()
if(IMAGEBATCHTOOL_BUILD_TESTS OR IMAGEBATCHTOOL_BUILD_PACKAGE_CHECKS)
    add_subdirectory(tests)
endif()
