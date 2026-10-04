cmake_minimum_required(VERSION 3.19)

set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MSVCDependencies.cmake")
project(ImageBatchTool LANGUAGES CXX)

# Work around a locally reproduced CMake crash when Qt reads the cached
# windeployqt path on reconfigure. Let Qt locate the deployment tool again.
if(WIN32)
    unset(WINDEPLOYQT_EXECUTABLE CACHE)
endif()

# Svg 用于加载样式表中的单选框和复选框图标。
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Svg)
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
    imageprocessor.h
    imageprocessor.cpp
    imageprocessing.h imageprocessing.cpp
    tonedialog.h tonedialog.cpp
    dialogappearance.h dialogappearance.cpp
    sliderstyle.h
    geometrydialog.h geometrydialog.cpp selectionitem.h selectionitem.cpp
    previewlabel.h
    previewlabel.cpp
    indicators.qrc
    resizedialog.h resizedialog.cpp resizedialog.ui
)

set_target_properties(ImageBatchTool PROPERTIES AUTORCC ON)
if(WIN32)
    enable_language(RC)
    target_sources(ImageBatchTool PRIVATE appicon.rc)
endif()

target_include_directories(ImageBatchTool PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(ImageBatchTool
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Svg
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

option(IMAGEBATCHTOOL_BUILD_TESTS "Build image-processing checks" OFF)
if(IMAGEBATCHTOOL_BUILD_TESTS)
    enable_testing()
    add_executable(imageprocessor_tests tests/imageprocessor_tests.cpp imageprocessor.cpp)
    set_target_properties(imageprocessor_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    target_compile_features(imageprocessor_tests PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(imageprocessor_tests PRIVATE /utf-8)
    endif()
    target_include_directories(imageprocessor_tests PRIVATE ${OpenCV_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(imageprocessor_tests PRIVATE ${OpenCV_LIBS})
    add_test(NAME imageprocessor_tests COMMAND imageprocessor_tests)

    qt_add_executable(dialog_interaction_tests
        tests/dialog_interaction_tests.cpp
        mainwindow.cpp mainwindow.h mainwindow.ui
        previewlabel.cpp previewlabel.h
        resizedialog.cpp resizedialog.h resizedialog.ui
        tonedialog.cpp tonedialog.h dialogappearance.cpp dialogappearance.h
        geometrydialog.h geometrydialog.cpp selectionitem.h selectionitem.cpp
        imageprocessing.cpp imageprocessing.h imageprocessor.cpp imageprocessor.h
        indicators.qrc)
    set_target_properties(dialog_interaction_tests PROPERTIES AUTORCC ON)
    target_compile_features(dialog_interaction_tests PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(dialog_interaction_tests PRIVATE /utf-8)
    endif()
    target_include_directories(dialog_interaction_tests PRIVATE ${OpenCV_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(dialog_interaction_tests PRIVATE Qt::Widgets Qt::Svg ${OpenCV_LIBS})
    add_test(NAME dialog_interaction_tests COMMAND dialog_interaction_tests)
    # 本机验证目录含部署产生的 qt.conf，测试显式使用开发环境的离屏插件。
    get_target_property(_qt_core_library Qt::Core IMPORTED_LOCATION_RELEASE)
    get_filename_component(_qt_bin_directory "${_qt_core_library}" DIRECTORY)
    get_filename_component(_qt_install_directory "${_qt_bin_directory}" DIRECTORY)
    set_tests_properties(dialog_interaction_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_qt_install_directory}/plugins;QT_QPA_PLATFORM_PLUGIN_PATH=${_qt_install_directory}/plugins/platforms"
        TIMEOUT 30)
endif()
