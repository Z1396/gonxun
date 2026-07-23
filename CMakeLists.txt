# ==== 工创赛智能物流搬运系统 - 统一构建配置 ====
# 模块：Core(核心库) + Vision(视觉算法) + GUI(Qt可视化)
# 编译：mkdir build && cd build && cmake .. && make -j4

# 指定最低 CMake 版本
cmake_minimum_required(VERSION 3.16)

# 定义项目名称、版本、描述、编程语言
project(GonxunSystem
    VERSION 3.0.0
    DESCRIPTION "工创赛2025智能物流搬运系统"
    LANGUAGES CXX
)

# ============================================================
# 【全局编译选项】统一所有代码的编译规则
# ============================================================

# 强制使用 C++17 新标准（支持lambda、智能指针、结构化绑定等新语法）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 生成 clangd 语法识别文件（给VSCode/Clion代码补全用）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# GCC/Clang 编译器开启警告，帮你查Bug
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wpedantic)
endif()

# 统一编译输出目录
# 可执行文件输出到 build/bin
# 库文件输出到 build/lib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================
# 【依赖查找阶段】自动检测电脑/板子装了哪些库
# ============================================================

# 必须依赖 OpenCV（视觉所有功能的根基）
# 按需加载模块：图像核心、处理、解码、视频、窗口、物体检测
find_package(OpenCV REQUIRED COMPONENTS
    core
    imgproc
    imgcodecs
    videoio
    highgui
    objdetect
)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")

# 多线程支持（串口、相机、异步任务全部依赖）
find_package(Threads REQUIRED)

# yaml-cpp 配置文件解析库（可选，读取 config.yaml）
find_package(yaml-cpp QUIET)
if(yaml-cpp_FOUND)
    message(STATUS "yaml-cpp found: ${yaml-cpp_VERSION}")
    set(YAML_CPP_AVAILABLE TRUE)
else()
    message(STATUS "yaml-cpp not found, config.yaml parsing disabled")
    set(YAML_CPP_AVAILABLE FALSE)
endif()

# ===================== Qt 检测（你的赛场地图GUI依赖）=====================
# 优先找 Qt6，找不到自动降级 Qt5，都没有就禁用GUI
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
if(Qt6_FOUND)
    message(STATUS "Qt6 found: ${Qt6_VERSION}")
    set(QT_VERSION_MAJOR 6)
    set(QT_AVAILABLE TRUE)
else()
    find_package(Qt5 QUIET COMPONENTS Core Gui Widgets)
    if(Qt5_FOUND)
        message(STATUS "Qt5 found: ${Qt5_VERSION}")
        set(QT_VERSION_MAJOR 5)
        set(QT_AVAILABLE TRUE)
    else()
        message(STATUS "Qt not found, GUI module disabled")
        set(QT_AVAILABLE FALSE)
    endif()
endif()

# 全局启用 Qt 自动生成工具
if(QT_AVAILABLE)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif()

# ===================== CUDA / TensorRT 检测（AI加速）=====================
# Jetson Nano 专用：GPU加速 YOLO 目标检测
find_package(CUDA QUIET)
if(CUDA_FOUND)
    message(STATUS "CUDA found: ${CUDA_VERSION}")
    set(CUDA_AVAILABLE TRUE)

    # 查找 TensorRT 推理库头文件
    find_path(TENSORRT_INCLUDE_DIR NvInfer.h
        PATHS
            /usr/include/aarch64-linux-gnu
            /usr/include
            /usr/local/include
        PATH_SUFFIXES tensorrt
    )

    # 查找 TensorRT 库文件
    find_library(TENSORRT_LIB nvinfer
        PATHS
            /usr/lib/aarch64-linux-gnu
            /usr/lib
            /usr/local/lib
    )
    find_library(TENSORRT_ONNX_LIB nvonnxparser
        PATHS
            /usr/lib/aarch64-linux-gnu
            /usr/lib
            /usr/local/lib
    )

    # 判定是否可用 TensorRT（YOLO加速）
    if(TENSORRT_INCLUDE_DIR AND TENSORRT_LIB)
        message(STATUS "TensorRT include: ${TENSORRT_INCLUDE_DIR}")
        message(STATUS "TensorRT lib: ${TENSORRT_LIB}")
        set(TENSORRT_AVAILABLE TRUE)
    else()
        message(STATUS "TensorRT not found, YOLO detection disabled")
        set(TENSORRT_AVAILABLE FALSE)
    endif()
else()
    message(STATUS "CUDA not found, TensorRT disabled")
    set(CUDA_AVAILABLE FALSE)
    set(TENSORRT_AVAILABLE FALSE)
endif()

# ============================================================
# 【模块1：Core 核心动态库】全局通用底层库
# ============================================================
# SHARED = 动态库 .so
# 存放：共享内存、配置读取
add_library(gonxun_core SHARED
    core/src/app_signals.cpp
    core/src/logger.cpp
    core/src/vision_controller.cpp
    core/src/task_state_machine.cpp
    core/src/grid5_path_planner.cpp
    config/src/config_loader.cpp
    # 头文件（用于 AUTOMOC）
    core/include/vision_controller.hpp
)

# 指定头文件搜索路径
target_include_directories(gonxun_core PUBLIC
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/config/include
    ${CMAKE_SOURCE_DIR}/vision_cpp/include
    ${OpenCV_INCLUDE_DIRS}
)

# 强制 C++17
target_compile_features(gonxun_core PUBLIC cxx_std_17)

# 链接线程库
target_link_libraries(gonxun_core PUBLIC
    Threads::Threads
    ${OpenCV_LIBS}
)

# 如果有 yaml 库，开启配置文件功能
if(YAML_CPP_AVAILABLE)
    target_link_libraries(gonxun_core PUBLIC yaml-cpp)
    target_compile_definitions(gonxun_core PUBLIC YAML_CPP_AVAILABLE=1)
endif()

# 如果有 Qt，开启 Qt 功能
if(QT_AVAILABLE)
    target_link_libraries(gonxun_core PUBLIC Qt5::Core Qt5::Widgets)
    target_compile_definitions(gonxun_core PUBLIC QT_AVAILABLE=1)
endif()

# ============================================================
# 【模块2：Vision 视觉静态库】所有视觉算法
# ============================================================
# STATIC = 静态库 .a
# 包含：相机、串口、颜色识别、圆环、二维码、障碍物、YOLO、卡尔曼滤波、Jetson优化

set(VISION_SOURCES
    vision_cpp/src/vision_system.cpp      # 视觉总控
    vision_cpp/src/utils.cpp              # 工具函数
    vision_cpp/src/kalman_filter.cpp      # 卡尔曼滤波（坐标平滑）
    vision_cpp/src/serial_comm.cpp        # 串口通信
    vision_cpp/src/camera_manager.cpp     # 相机管理
    vision_cpp/src/color_detector.cpp     # 颜色识别
    vision_cpp/src/ring_detector.cpp      # 色环定位
    vision_cpp/src/qr_detector.cpp        # 二维码识别
    vision_cpp/src/obstacle_detector.cpp  # 障碍物检测
    vision_cpp/src/yolo_detector.cpp      # YOLO深度学习检测
    vision_cpp/src/task_display.cpp       # 任务信息绘制
)

# 编译成静态库
add_library(gonxun_vision STATIC ${VISION_SOURCES})

# 头文件路径
target_include_directories(gonxun_vision PUBLIC
    ${CMAKE_SOURCE_DIR}/vision_cpp/include
    ${CMAKE_SOURCE_DIR}/core/include
    ${OpenCV_INCLUDE_DIRS}
)

# 链接核心库 + OpenCV + 线程
target_link_libraries(gonxun_vision PUBLIC
    gonxun_core
    ${OpenCV_LIBS}
    Threads::Threads
)

# 如果有CUDA，开启GPU加速宏
if(CUDA_AVAILABLE)
    target_compile_definitions(gonxun_vision PUBLIC CUDA_AVAILABLE=1)
    target_link_libraries(gonxun_vision PUBLIC
        ${CUDA_LIBRARIES}
        cudart
    )
endif()

# 如果有TensorRT，开启AI推理加速
if(TENSORRT_AVAILABLE)
    target_include_directories(gonxun_vision PUBLIC ${TENSORRT_INCLUDE_DIR})
    target_compile_definitions(gonxun_vision PUBLIC TENSORRT_AVAILABLE=1)
    target_link_libraries(gonxun_vision PUBLIC
        ${TENSORRT_LIB}
        ${TENSORRT_ONNX_LIB}
    )
endif()

# ============================================================
# 【模块3：GUI 可视化界面】你的赛场地图窗口
# 仅在检测到Qt时编译
# ============================================================
if(QT_AVAILABLE)
    # 开启Qt自动编译工具（moc、资源、ui）
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    # GUI源码：主函数、主窗口、画布控件、仿真控制器
    set(GUI_SOURCES
        main.cpp
        gui/src/mainwindow.cpp
        gui/src/courtmapwidget.cpp
        gui/src/simulation_controller.cpp
        gui/src/motion_controller.cpp
        gui/src/touchhandler.cpp
    )

    # GUI头文件（统一 .hpp 扩展名）
    set(GUI_HEADERS
        gui/include/mainwindow.hpp
        gui/include/courtmapwidget.hpp
        gui/include/simulation_controller.hpp
        gui/include/motion_controller.hpp
        gui/include/touchhandler.hpp
    )
    set(EXISTING_GUI_HEADERS "")
    foreach(H ${GUI_HEADERS})
        if(EXISTS "${CMAKE_SOURCE_DIR}/${H}")
            list(APPEND EXISTING_GUI_HEADERS "${H}")
        endif()
    endforeach()

    # 生成最终可执行程序 CourtMapViewer（赛场可视化软件）
    add_executable(CourtMapViewer ${GUI_SOURCES} ${EXISTING_GUI_HEADERS})

    # 头文件路径
    target_include_directories(CourtMapViewer PRIVATE
        ${CMAKE_SOURCE_DIR}/gui/include
        ${CMAKE_SOURCE_DIR}/core/include
        ${CMAKE_SOURCE_DIR}/vision_cpp/include
        ${OpenCV_INCLUDE_DIRS}
    )

    # 适配 Qt6 / Qt5 双版本
    if(QT_VERSION_MAJOR EQUAL 6)
        target_link_libraries(CourtMapViewer
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
        )
    else()
        target_link_libraries(CourtMapViewer
            Qt5::Core
            Qt5::Gui
            Qt5::Widgets
        )
    endif()

    # GUI链接视觉库+核心库+OpenCV
    target_link_libraries(CourtMapViewer
        ${OpenCV_LIBS}
        gonxun_core
        gonxun_vision
    )

    # 设置动态库查找路径（解决Jetson运行缺库问题）
    set_target_properties(CourtMapViewer PROPERTIES
        BUILD_RPATH "/lib/x86_64-linux-gnu;/usr/lib/x86_64-linux-gnu"
        INSTALL_RPATH "/lib/x86_64-linux-gnu;/usr/lib/x86_64-linux-gnu"
        LINK_FLAGS "-Wl,--disable-new-dtags"
    )

    # Windows下隐藏黑框控制台
    if(WIN32)
        set_target_properties(CourtMapViewer PROPERTIES
            WIN32_EXECUTABLE TRUE
        )
    endif()
else()
    message(STATUS "GUI module skipped (Qt not found)")
endif()

# ============================================================
# 安装部署规则（打包发布用）
# ============================================================
install(TARGETS gonxun_core gonxun_vision
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# 拷贝配置文件
install(FILES config/config.yaml
    DESTINATION etc/gonxun
)

# 拷贝头文件（给二次开发用）
install(DIRECTORY core/include/
    DESTINATION include/gonxun
    FILES_MATCHING PATTERN "*.hpp"
)

install(DIRECTORY config/include/
    DESTINATION include/gonxun
    FILES_MATCHING PATTERN "*.hpp"
)

# 安装GUI程序
if(QT_AVAILABLE)
    install(TARGETS CourtMapViewer DESTINATION bin)
endif()

# ============================================================
# 最后打印项目构建信息（方便你排查环境问题）
# ============================================================
message(STATUS "")
message(STATUS "========== 构建配置摘要 ==========")
message(STATUS "项目: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
message(STATUS "依赖状态:")
message(STATUS "  OpenCV:    ${OpenCV_VERSION} (必需)")
message(STATUS "  yaml-cpp:  ${yaml-cpp_VERSION} (可选)")
message(STATUS "  Qt:        ${QT_VERSION_MAJOR} (可选, GUI)")
message(STATUS "  CUDA:      ${CUDA_VERSION} (可选, Vision)")
message(STATUS "  TensorRT:  ${TENSORRT_AVAILABLE} (可选, Vision)")
message(STATUS "")
message(STATUS "模块:")
message(STATUS "  core:      ✓ (共享核心)")
message(STATUS "  vision:    ✓ (视觉系统)")
message(STATUS "  gui:       ${QT_AVAILABLE} (用户界面)")
message(STATUS "")
message(STATUS "输出目录: ${CMAKE_BINARY_DIR}")
message(STATUS "====================================")
message(STATUS "")