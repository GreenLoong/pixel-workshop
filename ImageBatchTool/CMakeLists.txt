cmake_minimum_required(VERSION 3.19)

set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MSVCDependencies.cmake")
project(ImageBatchTool VERSION 0.19.2 LANGUAGES CXX)
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
    target_sources(ImageBatchTool PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
    target_include_directories(ImageBatchTool PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
endif()
install(TARGETS ImageBatchTool
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
qt_generate_deploy_app_script(TARGET ImageBatchTool OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})

option(IMAGEBATCHTOOL_BUILD_TESTS "Build image-processing checks" OFF)
if(IMAGEBATCHTOOL_BUILD_TESTS)
    enable_testing()
    add_executable(imageprocessor_tests tests/imageprocessor_tests.cpp)
    set_target_properties(imageprocessor_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF)
    target_link_libraries(imageprocessor_tests PRIVATE pixel_domain)
    qt_add_executable(dialog_interaction_tests tests/dialog_interaction_tests.cpp)
    target_link_libraries(dialog_interaction_tests PRIVATE pixel_presentation pixel_resources)
    qt_add_executable(batch_tests tests/batch_tests.cpp)
    target_link_libraries(batch_tests PRIVATE pixel_application pixel_resources Qt::Widgets)
    qt_add_executable(segmentation_tests tests/segmentation_tests.cpp)
    target_link_libraries(segmentation_tests PRIVATE pixel_application pixel_resources)
    qt_add_executable(imageconversion_tests tests/imageconversion_tests.cpp)
    target_link_libraries(imageconversion_tests PRIVATE pixel_infrastructure)
    qt_add_executable(imagetask_tests tests/imagetask_tests.cpp)
    target_link_libraries(imagetask_tests PRIVATE pixel_application pixel_resources)
    qt_add_executable(unsaved_tests tests/unsaved_tests.cpp)
    target_link_libraries(unsaved_tests PRIVATE pixel_presentation pixel_resources)
    qt_add_executable(batch_parameters_tests tests/batch_parameters_tests.cpp)
    target_link_libraries(batch_parameters_tests PRIVATE pixel_presentation pixel_resources)
    qt_add_executable(package_checks tests/package_checks.cpp)
    set_target_properties(package_checks PROPERTIES OUTPUT_NAME PixelWorkshopCheck)
    target_link_libraries(package_checks PRIVATE pixel_application pixel_resources)
    foreach(test IN ITEMS imageprocessor_tests dialog_interaction_tests batch_tests segmentation_tests imageconversion_tests imagetask_tests unsaved_tests batch_parameters_tests package_checks)
        if(MSVC)
            target_compile_options(${test} PRIVATE /utf-8)
        endif()
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
    # qt.conf 可能指向部署插件；交互测试明确使用开发环境离屏插件。
    get_target_property(_qt_core_library Qt::Core IMPORTED_LOCATION_RELEASE)
    get_filename_component(_qt_bin_directory "${_qt_core_library}" DIRECTORY)
    get_filename_component(_qt_install_directory "${_qt_bin_directory}" DIRECTORY)
    set_tests_properties(dialog_interaction_tests batch_tests unsaved_tests batch_parameters_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_qt_install_directory}/plugins;QT_QPA_PLATFORM_PLUGIN_PATH=${_qt_install_directory}/plugins/platforms"
        TIMEOUT 30)
endif()
