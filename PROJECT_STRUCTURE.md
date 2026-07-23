# Gonxun 智能物流搬运系统 - 项目结构文档

## 目录

1. [项目概述](#项目概述)
2. [目录结构](#目录结构)
3. [构建系统](#构建系统)
4. [核心模块详解](#核心模块详解)
5. [配置文件](#配置文件)
6. [数据流与架构](#数据流与架构)
7. [开发与调试](#开发与调试)

---

## 项目概述

**Gonxun** 是工创赛 2025 智能物流搬运系统，基于 C++17 + Qt5 开发，运行在 Jetson Nano B01 (Ubuntu 18.04) 上。系统集成了机器视觉、路径规划、运动控制和人机交互，实现机器人在 2400×2400mm 赛场内的物料搬运任务。

### 核心特性
- 双摄像头视觉系统（主相机 + 扫码相机）
- YOLO 目标检测 + 颜色识别 + 二维码识别
- 卡尔曼滤波坐标平滑
- 5×5 网格 BFS 路径规划器
- 任务状态机（16 种状态 + 15 种事件）
- Qt GUI 赛场地图可视化
- 串口通信（STM32 下位机）

---

## 目录结构

```
gonxun/
├── main.cpp                      # 程序入口
├── CMakeLists.txt                # 顶层构建配置
├── AGENTS.md                     # 代码风格规范
├── PROJECT_STRUCTURE.md          # 本文档
│
├── config/                       # 配置模块
│   ├── config.yaml               # 系统配置文件（YAML）
│   ├── include/
│   │   └── config_loader.hpp     # 配置加载器（单例模式）
│   └── src/
│       └── config_loader.cpp     # 配置加载实现
│
├── core/                         # 核心算法模块（动态库 gonxun_core）
│   ├── include/
│   │   ├── app_signals.hpp       # 系统信号处理
│   │   ├── common_types.hpp      # 通用数据类型（Point/Path/ObstacleRect）
│   │   ├── field_constants.hpp   # 场地常量定义
│   │   ├── grid5_path_planner.hpp # 5×5 网格路径规划器
│   │   ├── motion_protocol.hpp   # 运动协议帧定义
│   │   ├── task_state_machine.hpp # 任务状态机
│   │   └── vision_controller.hpp # 视觉线程控制器
│   └── src/
│       ├── app_signals.cpp
│       ├── grid5_path_planner.cpp
│       ├── task_state_machine.cpp
│       └── vision_controller.cpp
│
├── vision_cpp/                   # 视觉算法模块（静态库 gonxun_vision）
│   ├── include/
│   │   ├── camera_manager.hpp    # 双摄像头管理器
│   │   ├── color_detector.hpp    # 颜色检测器
│   │   ├── config.hpp            # 视觉模块常量配置
│   │   ├── jetson_optimizer.hpp  # Jetson 优化
│   │   ├── kalman_filter.hpp     # 卡尔曼滤波器
│   │   ├── obstacle_detector.hpp # 障碍物检测器
│   │   ├── qr_detector.hpp       # 二维码检测器
│   │   ├── ring_detector.hpp     # 色环检测器
│   │   ├── serial_comm.hpp       # 串口通信
│   │   ├── task_display.hpp      # 任务信息绘制
│   │   ├── utils.hpp             # 工具函数
│   │   ├── vision_system.hpp     # 视觉系统总控
│   │   └── yolo_detector.hpp     # YOLO 检测器
│   └── src/
│       ├── camera_manager.cpp
│       ├── color_detector.cpp
│       ├── jetson_optimizer.cpp
│       ├── kalman_filter.cpp
│       ├── obstacle_detector.cpp
│       ├── qr_detector.cpp
│       ├── ring_detector.cpp
│       ├── serial_comm.cpp
│       ├── task_display.cpp
│       ├── utils.cpp
│       ├── vision_system.cpp
│       └── yolo_detector.cpp
│
├── gui/                          # GUI 模块（可执行程序 CourtMapViewer）
│   ├── include/
│   │   ├── courtmapwidget.hpp    # 赛场地图绘制控件
│   │   ├── mainwindow.hpp        # 主窗口
│   │   ├── motion_controller.hpp # 运动控制器
│   │   ├── simulation_controller.hpp # 仿真控制器
│   │   └── touchhandler.hpp      # 触摸屏事件处理
│   └── src/
│       ├── courtmapwidget.cpp
│       ├── mainwindow.cpp
│       ├── motion_controller.cpp
│       ├── simulation_controller.cpp
│       └── touchhandler.cpp
│
├── yolo_dataset/                 # YOLO 数据集
│   ├── data.yaml                 # 数据集配置
│   ├── images/
│   │   ├── train/                # 训练图像
│   │   └── val/                  # 验证图像
│   └── labels/
│       ├── train/                # 训练标签
│       └── val/                  # 验证标签
│
├── yolo_pipeline/                # YOLO 训练流水线（Python）
│   ├── config.yaml               # 训练配置
│   ├── raw_images/               # 原始图像
│   └── runs/detect/runs/
│       └── material_detection-4/
│           ├── args.yaml          # 训练参数
│           └── weights/           # 模型权重
│
├── logs/                         # 日志目录
└── .vscode/                      # VS Code 配置
    └── settings.json
```

---

## 构建系统

### CMake 构建配置

**文件**：[CMakeLists.txt](file:///home/pldx/Desktop/gonxun/CMakeLists.txt)

#### 构建命令
```bash
mkdir build && cd build
cmake ..
make -j4
```

#### 输出目录
- 可执行文件：`build/bin/`
- 库文件：`build/lib/`

#### 三模块架构

| 模块 | 类型 | 目标名 | 说明 |
|------|------|--------|------|
| Core | 动态库 (.so) | `gonxun_core` | 路径规划、状态机、信号处理 |
| Vision | 静态库 (.a) | `gonxun_vision` | 相机、视觉算法、串口 |
| GUI | 可执行文件 | `CourtMapViewer` | Qt 赛场地图界面 |

#### 依赖库检测

| 依赖 | 必需 | 说明 |
|------|------|------|
| OpenCV 4.x | ✅ 是 | 核心视觉库 |
| Threads | ✅ 是 | 多线程支持 |
| yaml-cpp | ❌ 可选 | YAML 配置解析 |
| Qt5/Qt6 | ❌ 可选 | GUI 界面 |
| CUDA + TensorRT | ❌ 可选 | Jetson GPU 加速 |

---

## 核心模块详解

### 1. 程序入口

**文件**：[main.cpp](file:///home/pldx/Desktop/gonxun/main.cpp)

#### 核心职责
- 注册系统信号处理器（SIGINT/SIGTERM 优雅退出）
- 加载 YAML 全局配置
- 初始化 Qt 应用实例
- 实例化视觉系统（VisionSystem）
- 创建主窗口（MainWindow）
- 绑定 UI 信号 ↔ 视觉线程
- 启动 Qt 事件循环

#### 启动流水线
```
main()
  ├── setup_signal_handlers()    // 信号钩子
  ├── ConfigLoader::load()       // 加载配置
  ├── QApplication               // Qt 实例
  ├── VisionSystem               // 视觉系统
  ├── MainWindow                 // GUI 窗口
  ├── VisionController           // 视觉线程管理
  └── app.exec()                 // 事件循环
```

#### 关键依赖
- `mainwindow.hpp` - UI 主窗口
- `vision_system.hpp` - 视觉总控
- `config_loader.hpp` - 配置加载
- `app_signals.hpp` - 信号处理
- `vision_controller.hpp` - 线程控制

---

### 2. 配置模块

**位置**：`config/`

#### config_loader.hpp / config_loader.cpp

**文件**：[config/include/config_loader.hpp](file:///home/pldx/Desktop/gonxun/config/include/config_loader.hpp)

##### 核心职责
- Meyer's Singleton 单例模式
- 从 YAML 文件加载系统配置
- yaml-cpp 不可用时降级为默认值
- 集中管理所有子系统配置

##### Config 结构体
```cpp
struct Config {
    struct { ... } logging;          // 日志配置
    struct { ... } serial;           // 串口配置
    struct { ... } motion;           // 运动控制配置
    struct {
        struct { ... } main;         // 主相机配置
        struct { ... } qr;           // 扫码相机配置
        struct { ... } params;       // 摄像头参数
    } camera;
    struct { ... } color_detection;  // 颜色检测配置
    struct { ... } kalman_filter;    // 卡尔曼滤波配置
    struct { ... } field;            // 场地配置
    struct { ... } yolo;             // YOLO 模型配置
    struct { ... } system;           // 系统信息
};
```

##### 摄像头参数（新增）
```yaml
camera:
  params:
    buffer_size: 1        # 缓冲区大小（帧数）
    auto_exposure: 1      # 自动曝光 (0=关闭, 1=开启)
    exposure: 120         # 曝光值
    gain: 15              # 增益值
```

##### 关键方法
- `load(path)` - 加载配置文件
- `config()` - 获取配置引用
- `is_loaded()` - 是否已加载

---

### 3. 核心模块 (gonxun_core)

**位置**：`core/`

#### 3.1 app_signals.hpp / app_signals.cpp

**文件**：[core/include/app_signals.hpp](file:///home/pldx/Desktop/gonxun/core/include/app_signals.hpp)

##### 核心职责
- 捕获 SIGINT (Ctrl+C) 和 SIGTERM
- 设置全局退出标志 `g_running`
- 支持程序优雅退出

##### 关键函数
- `setup_signal_handlers()` - 注册信号处理器
- `request_exit()` - 请求程序退出
- `is_exit_requested()` - 查询退出标志

---

#### 3.2 field_constants.hpp

**文件**：[core/include/field_constants.hpp](file:///home/pldx/Desktop/gonxun/core/include/field_constants.hpp)

##### 核心职责
- 定义赛场物理常量
- 所有尺寸单位：毫米 (mm)

##### 关键常量
- `FIELD_SIZE_MM = 2400` - 场地边长
- 机器人尺寸、启停区位置等

---

#### 3.3 common_types.hpp

**文件**：[core/include/common_types.hpp](file:///home/pldx/Desktop/gonxun/core/include/common_types.hpp)

##### 核心职责
- 项目通用数据类型定义
- 供路径规划、任务状态机、视觉系统等模块统一引用

##### 数据结构
```cpp
struct Point { int x, y; };           // 二维点
struct ObstacleRect { x, y, w, h };   // 矩形障碍物
using Path = std::vector<Point>;      // 路径类型
constexpr int GRID_RESOLUTION_MM = 50;
constexpr int GRID_SIZE = 48;
```

---

#### 3.4 grid5_path_planner.hpp / grid5_path_planner.cpp

**文件**：[core/include/grid5_path_planner.hpp](file:///home/pldx/Desktop/gonxun/core/include/grid5_path_planner.hpp)

##### 核心职责
- 5×5 粗粒度网格路径决策
- 陀螺仪角度融合
- 生成横平竖直的移动指令序列
- 适用于下位机全向移动底盘

##### 数据结构
```cpp
struct Grid5Coord { int x, y; };       // 5×5 网格坐标
struct Move5Vector { int dx, dy; };    // 移动向量
struct Move5Command {                  // 单步移动指令
    int turn_angle;                    // 转向角度
    int target_angle;                  // 目标朝向
    int dx, dy, steps;                 // 移动参数
    std::string direction_name;        // 方向名称
};
```

##### 关键方法
- `plan(current, goal, angle, prioritize_x)` - 路径规划
- `calculate_vector()` - 计算移动向量
- `build_x_commands()` / `build_y_commands()` - 生成指令

##### 朝向定义
- 0° = 左
- 90° = 下
- 180° = 右
- 270° = 上

---

#### 3.5 task_state_machine.hpp / task_state_machine.cpp

**文件**：[core/include/task_state_machine.hpp](file:///home/pldx/Desktop/gonxun/core/include/task_state_machine.hpp)

##### 核心职责
- 管理机器人任务全生命周期
- 16 种状态 + 15 种事件
- 事件驱动状态转移
- 支持多轮循环取放料

##### 任务状态 (16 种)
| 状态 | 说明 |
|------|------|
| IDLE | 空闲等待 |
| MARKING | 标记中 |
| READY | 就绪 |
| MOVING_TO_QR | 前往扫码区 |
| SCANNING_QR | 扫描二维码 |
| QR_DONE | 扫码完成 |
| MOVING_TO_MATERIAL | 前往物料区 |
| PICKING_MATERIAL | 取料中 |
| MATERIAL_DONE | 取料完成 |
| MOVING_TO_PROCESS | 前往粗加工区 |
| PLACING_MATERIAL | 放料中 |
| PROCESS_DONE | 放料完成 |
| MOVING_TO_BUFFER | 前往暂存区 |
| BUFFER_DONE | 暂存完成 |
| CYCLE_REPEAT | 准备下一轮 |
| RETURNING | 返回启停区 |
| COMPLETED | 任务完成 |
| ERROR | 异常状态 |

##### 任务事件 (15 种)
```
START_MARKING, MARKING_DONE, START_MISSION,
REACHED_QR, QR_SCANNED, REACHED_MATERIAL,
MATERIAL_PICKED, REACHED_PROCESS, MATERIAL_PLACED,
REACHED_BUFFER, BUFFER_DONE, CYCLE_START,
REACHED_START, ALL_DONE, ERROR_OCCURRED,
RESET, EMERGENCY_STOP
```

##### 回调机制
- `StateChangeCallback` - 状态变更通知
- `ProgressUpdateCallback` - 进度更新
- `PathRequestCallback` - 路径规划请求

##### 关键方法
- `handle_event(event)` - 处理事件，执行状态转移
- `get_current_state()` - 获取当前状态
- `get_progress()` - 获取进度信息
- `reset()` - 重置状态机

---

#### 3.6 vision_controller.hpp / vision_controller.cpp

**文件**：[core/include/vision_controller.hpp](file:///home/pldx/Desktop/gonxun/core/include/vision_controller.hpp)

##### 核心职责
- 管理视觉处理线程的启停
- Qt QThread 线程模型
- 线程安全：atomic<bool> 运行标志
- 析构自动停止线程

##### 类结构
```
VisionController (QObject)
  ├── VisionWorker (QObject)   // 工作线程对象
  │   └── run()                // 视觉循环
  └── QThread                  // 线程载体
```

##### 视觉循环
```
while (running):
  camera.read_main()  → 读取帧
  process_frame()     → 处理图像
  (30ms 延迟)
```

##### 关键方法
- `start()` - 启动视觉线程
- `stop()` - 停止并等待线程退出
- `on_finished()` - 线程完成清理

---

#### 3.7 motion_protocol.hpp

**文件**：[core/include/motion_protocol.hpp](file:///home/pldx/Desktop/gonxun/core/include/motion_protocol.hpp)

##### 核心职责
- 定义运动控制协议帧格式
- 上位机 → 下位机指令帧
- 下位机 → 上位机应答帧

##### 关键类型
- `MotionFrame` - 运动协议帧
- `MotionCmdType` - 指令类型枚举
- `StatusReportData` - 状态报告数据

---

### 4. 视觉模块 (gonxun_vision)

**位置**：`vision_cpp/`

#### 4.1 vision_system.hpp / vision_system.cpp

**文件**：[vision_cpp/include/vision_system.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/vision_system.hpp)

##### 核心职责
- 视觉系统顶层调度器
- 根据工作模式分发处理流程
- 整合所有视觉子模块
- 卡尔曼滤波坐标平滑
- 串口结果回传

##### 工作模式 (5 种)
| 模式 | 常量 | 功能 |
|------|------|------|
| 空闲 | MODE_IDLE (0) | 不处理 |
| 颜色识别 | MODE_COLOR (1) | 物料颜色检测 |
| 色环检测 | MODE_RING (3) | 三环定位 |
| 停靠对接 | MODE_DOCK (4) | 停靠位置检测 |
| 二维码 | MODE_QR (9) | 任务码扫描 |

##### 处理流程
```
process_frame(img, unit)
  └── switch (unit):
        ├── MODE_COLOR → process_color()    // 颜色识别
        ├── MODE_RING  → process_ring()     // 色环检测
        ├── MODE_DOCK  → process_dock()     // 停靠对接
        └── MODE_QR    → process_qr()       // 二维码扫描
```

##### 子模块成员
```cpp
SerialComm serial_comm;          // 串口通信
CameraManager camera;            // 双摄像头
QRDetector qr_detector;          // 二维码检测
TaskCodeParser task_parser;      // 任务码解析
YOLOv8Detector yolo_detector_;   // YOLO 检测
ThreeRingDetector three_ring_detector_;  // 三环检测
SixRingDetector six_ring_detector_;      // 六环检测
ObstacleDetector obstacle_detector_;     // 障碍物检测
TaskDisplay task_display_;               // 任务显示
std::array<KalmanFilter, 3> kalman_filters_;  // 3路卡尔曼
```

##### 颜色映射 (6 种颜色)
| 编号 | 颜色 | 名称 | BGR 值 |
|------|------|------|--------|
| 1 | 红 | red | (0, 0, 255) |
| 2 | 黄 | yellow | (0, 255, 255) |
| 3 | 蓝 | blue | (255, 0, 0) |
| 4 | 绿 | green | (0, 255, 0) |
| 5 | 黑 | black | (0, 0, 0) |
| 6 | 浅蓝 | light_blue | (255, 255, 0) |

##### 关键方法
- `process_frame(img, unit)` - 单帧处理入口
- `set_task_code(task_code)` - 设置任务码
- `set_current_batch(batch)` - 设置当前批次
- `set_qr_callback(callback)` - 注册扫码回调
- `detect_three_colors()` - 三色检测 + 滤波
- `filter_position(x, y, idx)` - 卡尔曼滤波

---

#### 4.2 camera_manager.hpp / camera_manager.cpp

**文件**：[vision_cpp/include/camera_manager.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/camera_manager.hpp)

##### 核心职责
- 双摄像头管理（主相机 + 扫码相机）
- V4L2 后端（Linux）
- 分辨率自动降级
- 自动重连机制
- 线程安全（双独立互斥锁）

##### 双摄像头
| 摄像头 | 用途 | 索引 |
|--------|------|------|
| 主相机 | 物料/色环识别 | main_index_ |
| 扫码相机 | 二维码识别 | qr_index_ |

##### 摄像头参数（可配置）
- `buffer_size` - 缓冲区大小（帧数）
- `auto_exposure` - 自动曝光开关
- `exposure` - 曝光值
- `gain` - 增益值

##### 故障处理
- 连续失败 30 次触发重连
- 最大重连次数：3 次
- 重连等待：1.0 秒

##### 关键方法
- `open()` - 打开双摄像头
- `close()` - 关闭并释放
- `read_main()` - 读取主相机帧
- `read_qr()` - 读取扫码相机帧
- `open_one()` - 打开单个摄像头
- `configure_capture()` - 配置相机参数
- `reconnect()` - 重连摄像头

---

#### 4.3 serial_comm.hpp / serial_comm.cpp

**文件**：[vision_cpp/include/serial_comm.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/serial_comm.hpp)

##### 核心职责
- 上位机 (Jetson) ↔ 下位机 (STM32) 串口通信
- 支持真实硬件串口 + 模拟模式
- 接收线程持续读取工作模式
- 发送帧按协议打包

##### 帧格式 (15 字节)
```
[0]      帧头 0x66
[1]      命令字节
[2..13]  数据域 (12 字节)
[14]     校验和 (cmd + data 累加和 & 0xFF)
[15]     帧尾 0x77
```

##### 工作模式（下位机下发）
| 模式 | 值 | 说明 |
|------|----|------|
| MODE_IDLE | 0 | 空闲 |
| MODE_COLOR | 1 | 颜色识别 |
| MODE_RING | 3 | 圆环检测 |
| MODE_DOCK | 4 | 对接停靠 |
| MODE_QR | 9 | 二维码扫描 |

##### 命令字节（上位机下发）
| 命令 | 值 | 说明 |
|------|----|------|
| CMD_COLOR | 0x01 | 颜色坐标 |
| CMD_RING | 0x03 | 圆环坐标 |
| CMD_DOCK | 0x04 | 对接坐标 |
| CMD_QR | 0x09 | QR 数据 |

##### 模拟模式
- 不连接真实硬件
- 周期切换工作模式（mock_cycle）
- 发送数据仅打印到 stdout

##### 关键方法
- `open()` - 打开串口
- `close()` - 关闭串口
- `start()` - 启动通信
- `send_coordinates(cmd, coords)` - 发送坐标
- `send_qr_data(qr_data)` - 发送二维码数据
- `send_raw_frame(frame)` - 发送原始帧

---

#### 4.4 kalman_filter.hpp / kalman_filter.cpp

**文件**：[vision_cpp/include/kalman_filter.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/kalman_filter.hpp)

##### 核心职责
- 二维卡尔曼滤波（X/Y 解耦）
- 坐标平滑，减少检测抖动
- 简化的恒等模型（只有位置，无速度）

##### 状态模型
- 状态向量：`[x, y]`
- 状态转移：恒等矩阵
- 观测模型：恒等矩阵

##### 参数
- `q_` - 过程噪声协方差 Q
- `r_` - 观测噪声协方差 R
- `x_` - 状态估计 [x, y]
- `p_` - 估计协方差 2×2

##### 关键方法
- `predict()` - 预测步骤（P = P + Q）
- `update(z)` - 更新步骤（融合观测值）
- `filter(z)` - 预测 + 更新一步完成
- `reset()` - 重置滤波器

---

#### 4.5 yolo_detector.hpp / yolo_detector.cpp

**文件**：[vision_cpp/include/yolo_detector.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/yolo_detector.hpp)

##### 核心职责
- YOLOv8 目标检测
- 检测指定颜色的物料中心
- 面积阈值过滤
- 支持 TorchScript / TensorRT 后端

##### 关键方法
- `detect_center(img, color, min_area, max_area)` - 检测目标中心
- `is_available()` - 模型是否可用

---

#### 4.6 color_detector.hpp / color_detector.cpp

**文件**：[vision_cpp/include/color_detector.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/color_detector.hpp)

##### 核心职责
- 基于颜色空间的目标检测
- HSV 阈值分割
- 轮廓提取与面积过滤
- 中心点计算

---

#### 4.7 ring_detector.hpp / ring_detector.cpp

**文件**：[vision_cpp/include/ring_detector.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/ring_detector.hpp)

##### 核心职责
- 色环定位检测
- ThreeRingDetector - 三色环检测
- SixRingDetector - 六色环检测

---

#### 4.8 qr_detector.hpp / qr_detector.cpp

**文件**：[vision_cpp/include/qr_detector.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/qr_detector.hpp)

##### 核心职责
- 二维码检测与解码
- 使用 OpenCV QRCodeDetector
- 支持双相机降级

##### 关键方法
- `detect(img)` - 检测并解码二维码
- 返回 std::optional<std::string>

---

#### 4.9 obstacle_detector.hpp / obstacle_detector.cpp

**文件**：[vision_cpp/include/obstacle_detector.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/obstacle_detector.hpp)

##### 核心职责
- 障碍物视觉检测
- 轮廓提取与分类

---

#### 4.10 task_display.hpp / task_display.cpp

**文件**：[vision_cpp/include/task_display.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/task_display.hpp)

##### 核心职责
- 任务信息在图像上的绘制
- 任务码显示
- 状态信息叠加

---

#### 4.11 config.hpp

**文件**：[vision_cpp/include/config.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/config.hpp)

##### 核心职责
- 视觉模块常量配置
- inline constexpr 跨编译单元共享
- 所有参数的默认值定义

##### 配置分类
```
config::
  ├── LOG_*              // 日志配置
  ├── SERIAL_*           // 串口配置
  ├── CAMERA_*           // 摄像头配置
  ├── COLOR_*            // 颜色检测配置
  ├── KALMAN_*           // 卡尔曼滤波配置
  ├── FIELD_*            // 场地配置
  └── YOLO_*             // YOLO 模型配置
```

##### 摄像头参数常量
```cpp
CAMERA_BUFFER_SIZE      // 缓冲区大小
CAMERA_AUTO_EXPOSURE    // 自动曝光
CAMERA_EXPOSURE         // 曝光值
CAMERA_GAIN             // 增益值
```

---

#### 4.12 utils.hpp / utils.cpp

**文件**：[vision_cpp/include/utils.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/utils.hpp)

##### 核心职责
- 通用工具函数
- 图像处理辅助函数
- 数学计算工具

---

#### 4.13 jetson_optimizer.hpp / jetson_optimizer.cpp

**文件**：[vision_cpp/include/jetson_optimizer.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/jetson_optimizer.hpp)

##### 核心职责
- Jetson Nano 专用优化
- GPU 加速配置
- TensorRT 推理优化

---

### 5. GUI 模块 (CourtMapViewer)

**位置**：`gui/`

#### 5.1 mainwindow.hpp / mainwindow.cpp

**文件**：[gui/include/mainwindow.hpp](file:///home/pldx/Desktop/gonxun/gui/include/mainwindow.hpp)

##### 核心职责
- 主窗口，整合所有 GUI 组件
- 左侧垂直工具栏 + 中央地图
- 按钮互斥逻辑（标记模式 vs 启停区选择）
- 状态栏状态显示
- 二维码扫描信号（线程安全）

##### 布局结构
```
MainWindow (无边框, 固定大小 900×700)
  └── QHBoxLayout (主布局)
        ├── QVBoxLayout (左侧工具栏, 宽度 60px)
        │     ├── 开始按钮 (绿色)
        │     ├── 障碍物按钮 (蓝色)
        │     ├── 启停区按钮 (绿色)
        │     ├── 仿真按钮 (紫色)
        │     ├── 状态标签
        │     ├── (拉伸)
        │     └── 关闭按钮 (红色)
        └── CourtMapWidget (地图, 占主要空间)
```

##### 按钮互斥规则
- 障碍物模式 ↔ 启停区选择 互斥
- 同一时间只能开启一种编辑模式

##### 信号
```cpp
qr_code_scanned(task_code)     // 二维码扫描完成
vision_start_requested()       // 请求启动视觉
vision_stop_requested()        // 请求停止视觉
```

##### 槽函数
```cpp
on_mark_button_clicked()           // 障碍物按钮
on_obstacle_toggled(id, marked)    // 障碍物标记变更
on_select_start_zone_clicked()     // 启停区按钮
on_start_zone_selected(idx, name)  // 启停区选中
on_start_button_clicked()          // 开始/停止按钮
on_sim_button_clicked()            // 仿真按钮
on_qr_code_scanned(task_code)      // 扫码完成
```

##### 状态栏优先级
1. 仿真运行中（紫色）
2. 视觉系统运行中（绿色）
3. 障碍物标记模式（红色）
4. 已选定启停区（绿色）
5. 默认空闲状态（灰色）

##### 关键成员
```cpp
CourtMapWidget *court_map_;        // 赛场地图
SimulationController *sim_controller_;  // 仿真控制器
SerialComm *serial_comm_;          // 串口通信
MotionController *motion_controller_;   // 运动控制器

QPushButton *start_btn_;           // 开始/停止按钮
QPushButton *mark_btn_;            // 障碍物按钮
QPushButton *select_start_btn_;    // 启停区按钮
QPushButton *sim_btn_;             // 仿真按钮
QLabel *status_label_;             // 状态标签
```

---

#### 5.2 courtmapwidget.hpp / courtmapwidget.cpp

**文件**：[gui/include/courtmapwidget.hpp](file:///home/pldx/Desktop/gonxun/gui/include/courtmapwidget.hpp)

##### 核心职责
- 赛场地图绘制控件
- 2400×2400mm 场地可视化
- 5×5 网格系统 (25 个单元格)
- 障碍物标记与启停区选择
- 机器人位姿与路径显示
- 任务码显示（地图顶部）

##### 数据结构
```cpp
struct CourtZone { name, rect, color, is_selected };     // 赛场区域
struct CourtCircle { center, outer_r, inner_r, ... };    // 同心圆
struct ObstacleRect { id, rect, is_marked };             // 障碍物
struct Grid5Cell { id, grid_x, grid_y, rect };           // 5×5 格子
```

##### 绘制层次 (19 层)
```
1.  draw_background()         // 白色背景 + 灰色赛场
2.  draw_outer_frame()        // 蓝色外框
3.  draw_grid5()              // 5×5 网格线
4.  draw_center_blocks()      // 4 个中心方块（浅黄色）
5.  draw_center_cross()       // 中心十字虚线
6.  draw_raw_material_area()  // 原料区（圆形托盘）
7.  draw_start_stop_zones()   // 启停区（蓝色/绿色）
8.  draw_buffer_area()        // 暂存区（左侧 3 圆）
9.  draw_rough_process_area() // 粗加工区（底部 3 圆）
10. draw_qr_board()           // 二维码板（右侧）
11. draw_obstacles()          // 障碍物（红色）
12. draw_path()               // 路径（虚线）
13. draw_robot()              // 机器人（方体 + 四轮 + 箭头）
14. draw_dimension_marks()    // 尺寸标注
15. draw_task_code()          // 任务码（顶部）
```

##### 交互模式
- `mark_mode_` - 障碍物标记模式
- `start_zone_selectable_` - 启停区可选模式

##### 坐标系统
- 赛场坐标：左上角 (0,0)，X 向右，Y 向下，单位 mm
- 控件坐标：Qt 像素坐标
- `map_to_widget()` / `widget_to_map()` - 坐标转换

##### 任务码显示
- 位置：地图顶部居中
- 背景：白色，无框
- 字体：Microsoft YaHei，42pt，粗体
- 内容：任务码字符串（如 "156+123+516+231"）

##### 缩放控制
- `scale_factor_` - 全局缩放因子（1.0 = 原始大小）
- `set_scale_factor()` - 设置缩放比例
- 影响所有地图元素等比例缩放

##### 关键方法
```cpp
// 模式设置
set_mark_mode(enabled)
set_start_zone_selectable(selectable)

// 查询
selected_start_zone()
selected_start_zone_name()
marked_count()

// 机器人
set_robot_pos(pos, angle)
set_robot_visible(visible)

// 路径
set_path(points)
clear_path()

// 任务码
set_task_code(task_code)

// 坐标
field_to_grid5(x, y)
get_cell_center(gx, gy)
has_obstacle_in_cell(gx, gy)
```

##### 固定区域坐标
- 启停区1：右上角 (2100, 0, 300, 300)
- 启停区2：右下角 (2100, 2100, 300, 300)
- 暂存区：左侧 (0, 975-1425)
- 粗加工区：底部 (910-1490, 2250-2400)
- 原料区：顶部 (1200, -70) 中心
- 二维码板：右侧 (2360, 1100-1300)

---

#### 5.3 simulation_controller.hpp / simulation_controller.cpp

**文件**：[gui/include/simulation_controller.hpp](file:///home/pldx/Desktop/gonxun/gui/include/simulation_controller.hpp)

##### 核心职责
- GUI 可视化仿真控制器
- 5×5 网格 BFS 路径规划
- 多段导航序列管理
- 动画播放与驻留等待
- 任务状态机推进

##### 仿真阶段 (6 种)
| 阶段 | 说明 |
|------|------|
| IDLE | 空闲 |
| PLANNING | 路径规划中 |
| MOVING | 移动中 |
| DWELLING | 到达驻留 |
| COMPLETED | 全部完成 |
| ERROR | 错误 |

##### 导航序列
```
扫码区 → 原料区 → 粗加工区 → 暂存区 → (循环) → 返回启停区
```

##### 动画参数
- `anim_interval_` - 帧间隔 (默认 50ms)
- `dwell_time_` - 驻留时间 (默认 1000ms)
- `total_cycles_` - 循环次数 (默认 1)

##### 信号
```cpp
simulation_started()              // 仿真启动
simulation_finished(success)      // 仿真结束
phase_changed(phase, description) // 阶段变更
```

##### 关键方法
```cpp
start(task_code)                  // 启动仿真
stop()                            // 停止仿真
is_running()                      // 是否运行
set_animation_interval(ms)        // 设置动画间隔
set_dwell_time(ms)                // 设置驻留时间
set_total_cycles(cycles)          // 设置循环次数
```

---

#### 5.4 motion_controller.hpp / motion_controller.cpp

**文件**：[gui/include/motion_controller.hpp](file:///home/pldx/Desktop/gonxun/gui/include/motion_controller.hpp)

##### 核心职责
- 四轮底盘运动控制器
- 格子路径分解为步进指令
- 指令队列管理
- 超时重传机制
- ACK/NACK 应答处理

##### 指令生命周期
```
PENDING → SENT → ACKED
              ↘ TIMEOUT → (重试)
              ↘ NACKED
              ↘ ERROR
```

##### 运动指令类型
- 步进移动（方向 + 步数）
- 绝对位置移动
- 停止 / 紧急停止
- 速度设置
- 状态查询

##### 关键方法
```cpp
execute_grid_path(grid_path, start_angle)  // 执行格子路径
send_step_move(direction, steps)           // 发送步进指令
send_position_move(x, y, gx, gy)          // 发送绝对位置
send_stop() / send_emergency()             // 停止/急停
clear_queue()                              // 清空队列
```

##### 信号
```cpp
command_sent(description)         // 指令已发送
command_acked(description)       // 收到应答
command_nacked(description, reason) // 收到否定应答
command_timeout(description)     // 指令超时
status_received(status)          // 状态报告
all_commands_completed()         // 全部完成
motion_error(error)              // 运动错误
```

---

#### 5.5 touchhandler.hpp / touchhandler.cpp

**文件**：[gui/include/touchhandler.hpp](file:///home/pldx/Desktop/gonxun/gui/include/touchhandler.hpp)

##### 核心职责
- 触摸屏事件处理
- 触摸点识别与转换
- 手势检测（点击、滑动等）

---

### 6. YOLO 数据集与训练

**位置**：`yolo_dataset/`

#### data.yaml
- 数据集配置文件
- 定义类别名称、路径

#### 目录结构
```
yolo_dataset/
  ├── data.yaml              # 数据集配置
  ├── images/
  │   ├── train/             # 训练图像
  │   └── val/               # 验证图像
  └── labels/
      ├── train/             # 训练标签 (YOLO 格式)
      └── val/               # 验证标签
```

---

### 7. YOLO 训练流水线

**位置**：`yolo_pipeline/`

#### config.yaml
- 训练超参数配置

#### 训练输出
```
yolo_pipeline/runs/detect/runs/material_detection-4/
  ├── args.yaml              # 训练参数
  └── weights/
      ├── best.pt            # 最佳权重 (PyTorch)
      └── best.torchscript   # TorchScript 格式 (C++ 推理)
```

---

## 配置文件

### config/config.yaml

**文件**：[config/config.yaml](file:///home/pldx/Desktop/gonxun/config/config.yaml)

#### 配置节

| 节 | 说明 |
|----|------|
| logging | 日志级别、格式 |
| serial | 串口设备、波特率、模拟模式 |
| motion | 运动速度、加速度、重传 |
| camera | 双相机索引、分辨率、参数 |
| color_detection | 颜色检测面积阈值 |
| kalman_filter | 卡尔曼滤波 Q/R 参数 |
| field | 场地尺寸、像素比例 |
| yolo | 模型路径、置信度、输入尺寸 |
| system | 版本、名称、描述 |

#### camera.params 节（摄像头参数）
```yaml
camera:
  params:
    buffer_size: 1        # 缓冲区帧数
    auto_exposure: 1      # 自动曝光开关
    exposure: 120         # 曝光值
    gain: 15              # 增益值
```

---

## 数据流与架构

### 整体架构图

```
┌─────────────────────────────────────────────────────────┐
│                     GUI 层 (Qt)                          │
│  ┌──────────┐  ┌───────────┐  ┌────────────────────┐  │
│  │ MainWindow│→│CourtMapWidget│ │SimulationController│  │
│  └─────┬────┘  └───────────┘  └─────────┬──────────┘  │
│        │                                  │              │
│        ▼                                  ▼              │
│  ┌──────────────────────────────────────────────────┐   │
│  │              MotionController                    │   │
│  └─────────────────────┬───────────────────────────┘   │
└────────────────────────┼───────────────────────────────┘
                         │ 串口指令
                         ▼
┌─────────────────────────────────────────────────────────┐
│                   视觉层 (OpenCV)                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │ VisionSystem │→ │ SerialComm   │→ │下位机 STM32   │ │
│  └──────┬───────┘  └──────────────┘  └──────────────┘ │
│         │                                              │
│    ┌────┴───────────────────────────────────┐          │
│    │                                        │          │
│    ▼                                        ▼          │
│ ┌───────────┐  ┌────────────┐  ┌───────────────────┐ │
│ │CameraMgr  │  │YOLO 检测   │  │KalmanFilter (×3)  │ │
│ └───────────┘  └────────────┘  └───────────────────┘ │
│                                        │               │
│                                        ▼               │
│                                 ┌───────────┐          │
│                                 │ 串口上报  │          │
│                                 └───────────┘          │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                   核心算法层                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │ Grid5 Planner│  │ BFS Search   │  │TaskStateMachine│ │
│  └──────────────┘  └──────────────┘  └──────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### 视觉处理数据流

```
相机 (V4L2)
    ↓ read_main()
原始图像 (BGR)
    ↓ process_frame(unit)
┌───────────────────────────────────┐
│ 工作模式分发 (switch)              │
│  ├── COLOR  → YOLO检测 → 3目标    │
│  ├── RING   → 三环检测 → 3目标    │
│  ├── DOCK   → 颜色检测 → 3目标    │
│  └── QR     → 扫码相机 → 字符串   │
└───────────────────────────────────┘
    ↓
卡尔曼滤波 (×3 路独立)
    ↓
坐标平滑结果
    ↓ send_coordinates()
串口帧打包 → 下位机
```

### 任务码显示数据流（线程安全）

```
视觉线程 (VisionWorker)
    ↓ 扫码成功
qr_callback_()  ← 回调函数 (工作线程中执行)
    ↓
window.qr_code_scanned(task_code)  ← Qt 信号
    ↓ Qt 信号槽机制 (自动转发到主线程)
on_qr_code_scanned(task_code)  ← 槽函数 (主线程执行)
    ↓
court_map_->set_task_code(task_code)
    ↓ update()
CourtMapWidget::draw_task_code()  ← GUI 绘制
```

### 仿真控制数据流

```
用户点击「仿真」按钮
    ↓ on_sim_button_clicked()
sim_controller_->start(task_code)
    ↓
build_nav_sequence()  → 构建导航序列
    ↓
循环:
  start_next_segment()
    ↓
  plan_current_segment()  → BFS 路径规划
    ↓
  start_moving()  → 启动动画定时器
    ↓
  on_animation_tick()  → 每帧前进一步
    ↓
  start_dwelling()  → 到达后驻留
    ↓
  on_segment_complete()  → 推进状态机
    ↓
on_all_complete()  → 仿真结束
```

---

## 开发与调试

### 编译命令
```bash
cd /home/pldx/Desktop/gonxun
rm -rf build && mkdir build && cd build
cmake ..
make -j4
```

### 运行程序
```bash
cd build/bin
./CourtMapViewer
```

### 代码风格
- 参考 [AGENTS.md](file:///home/pldx/Desktop/gonxun/AGENTS.md)
- snake_case 函数/变量
- PascalCase 类名
- 成员变量尾下划线
- `#pragma once` 头文件保护
- `.hpp` / `.cpp` 扩展名

### VS Code 配置
- clangd 语言服务器
- `--clang-tidy=false` 禁用 clang-tidy
- compile_commands.json 位于 build/

### 关键配置参数速查

| 参数 | 文件 | 位置 | 默认值 |
|------|------|------|--------|
| 窗口大小 | mainwindow.cpp | setFixedSize() | 900×700 |
| 按钮大小 | mainwindow.cpp | setFixedSize() | 60×36 |
| 任务码字体 | courtmapwidget.cpp | QFont() | 42pt |
| 顶部边距 | courtmapwidget.hpp | MARGIN_TOP | 180px |
| 地图缩放 | courtmapwidget.hpp | scale_factor_ | 1.0 |
| 相机曝光 | config.yaml | camera.params.exposure | 120 |
| 卡尔曼 Q | config.yaml | kalman_filter.Q | 1e-5 |
| 卡尔曼 R | config.yaml | kalman_filter.R | 1e-2 |

---

## 文件索引

### 头文件 (.hpp)

| 文件 | 模块 | 行数 | 核心类 |
|------|------|------|--------|
| [config_loader.hpp](file:///home/pldx/Desktop/gonxun/config/include/config_loader.hpp) | config | ~165 | ConfigLoader |
| [app_signals.hpp](file:///home/pldx/Desktop/gonxun/core/include/app_signals.hpp) | core | - | - |
| [common_types.hpp](file:///home/pldx/Desktop/gonxun/core/include/common_types.hpp) | core | - | - |
| [field_constants.hpp](file:///home/pldx/Desktop/gonxun/core/include/field_constants.hpp) | core | - | - |
| [grid5_path_planner.hpp](file:///home/pldx/Desktop/gonxun/core/include/grid5_path_planner.hpp) | core | ~115 | Grid5PathPlanner |
| [motion_protocol.hpp](file:///home/pldx/Desktop/gonxun/core/include/motion_protocol.hpp) | core | - | - |
| [task_state_machine.hpp](file:///home/pldx/Desktop/gonxun/core/include/task_state_machine.hpp) | core | ~185 | TaskStateMachine |
| [vision_controller.hpp](file:///home/pldx/Desktop/gonxun/core/include/vision_controller.hpp) | core | ~85 | VisionController |
| [camera_manager.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/camera_manager.hpp) | vision | ~125 | CameraManager |
| [color_detector.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/color_detector.hpp) | vision | - | - |
| [config.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/config.hpp) | vision | ~65 | config 命名空间 |
| [jetson_optimizer.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/jetson_optimizer.hpp) | vision | - | - |
| [kalman_filter.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/kalman_filter.hpp) | vision | - | KalmanFilter |
| [obstacle_detector.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/obstacle_detector.hpp) | vision | - | ObstacleDetector |
| [qr_detector.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/qr_detector.hpp) | vision | - | QRDetector |
| [ring_detector.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/ring_detector.hpp) | vision | - | RingDetectors |
| [serial_comm.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/serial_comm.hpp) | vision | ~150 | SerialComm |
| [task_display.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/task_display.hpp) | vision | - | TaskDisplay |
| [utils.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/utils.hpp) | vision | - | - |
| [vision_system.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/vision_system.hpp) | vision | ~170 | VisionSystem |
| [yolo_detector.hpp](file:///home/pldx/Desktop/gonxun/vision_cpp/include/yolo_detector.hpp) | vision | - | YOLOv8Detector |
| [courtmapwidget.hpp](file:///home/pldx/Desktop/gonxun/gui/include/courtmapwidget.hpp) | gui | ~255 | CourtMapWidget |
| [mainwindow.hpp](file:///home/pldx/Desktop/gonxun/gui/include/mainwindow.hpp) | gui | ~95 | MainWindow |
| [motion_controller.hpp](file:///home/pldx/Desktop/gonxun/gui/include/motion_controller.hpp) | gui | ~220 | MotionController |
| [simulation_controller.hpp](file:///home/pldx/Desktop/gonxun/gui/include/simulation_controller.hpp) | gui | ~145 | SimulationController |
| [touchhandler.hpp](file:///home/pldx/Desktop/gonxun/gui/include/touchhandler.hpp) | gui | - | TouchHandler |

### 源文件 (.cpp)

| 文件 | 模块 | 对应头文件 |
|------|------|------------|
| [config_loader.cpp](file:///home/pldx/Desktop/gonxun/config/src/config_loader.cpp) | config | config_loader.hpp |
| [app_signals.cpp](file:///home/pldx/Desktop/gonxun/core/src/app_signals.cpp) | core | app_signals.hpp |
| [grid5_path_planner.cpp](file:///home/pldx/Desktop/gonxun/core/src/grid5_path_planner.cpp) | core | grid5_path_planner.hpp |
| [task_state_machine.cpp](file:///home/pldx/Desktop/gonxun/core/src/task_state_machine.cpp) | core | task_state_machine.hpp |
| [vision_controller.cpp](file:///home/pldx/Desktop/gonxun/core/src/vision_controller.cpp) | core | vision_controller.hpp |
| [camera_manager.cpp](file:///home/pldx/Desktop/gonxun/vision_cpp/src/camera_manager.cpp) | vision | camera_manager.hpp |
| [color_detector.cpp](file:///home/pldx/Desktop/gonxun/vision_cpp/src/color_detector.cpp) | vision | color_detector.hpp |
| [jetson_optimizer.cpp](file:///home/pldx/Desktop/gonxun/vision_cpp/src/jetson_optimizer.cpp) | vision | jetson_optimizer.hpp |
| [kalman_filter.cpp](file:///home/pldx/Desktop/gonxun/vision_cpp/src/kalman_filter.cpp) | vision | kalman_filter.hpp |
| [obstacle_detector.cpp](file:///home/pldx/Desktop/gonxun/vision_cpp/src/obstacle_detector.cpp) | vision | obstacle_detector.hpp |
| [qr_detector.cpp](file:///home/pldx/Desktop/gonxun/vision_cpp/src/qr_detector.cpp) | vision | qr_detector.hpp |
| [ring_detector.cpp](file:///home/pldx/Desktop/gonxun/vision_cpp/src/ring_detector.cpp) | vision | ring_detector.hpp |
| [serial_comm.cpp](file:///home/pldx/Desktop/gonxun/vision_cpp/src/serial_comm.cpp) | vision | serial_comm.hpp |
| [task_display.cpp](file:///home/pldx/Desktop/gonxun/vision_cpp/src/task_display.cpp) | vision | task_display.hpp |
| [utils.cpp](file:///home/pldx/Desktop/gonxun/vision_cpp/src/utils.cpp) | vision | utils.hpp |
| [vision_system.cpp](file:///home/pldx/Desktop/gonxun/vision_cpp/src/vision_system.cpp) | vision | vision_system.hpp |
| [yolo_detector.cpp](file:///home/pldx/Desktop/gonxun/vision_cpp/src/yolo_detector.cpp) | vision | yolo_detector.hpp |
| [courtmapwidget.cpp](file:///home/pldx/Desktop/gonxun/gui/src/courtmapwidget.cpp) | gui | courtmapwidget.hpp |
| [mainwindow.cpp](file:///home/pldx/Desktop/gonxun/gui/src/mainwindow.cpp) | gui | mainwindow.hpp |
| [motion_controller.cpp](file:///home/pldx/Desktop/gonxun/gui/src/motion_controller.cpp) | gui | motion_controller.hpp |
| [simulation_controller.cpp](file:///home/pldx/Desktop/gonxun/gui/src/simulation_controller.cpp) | gui | simulation_controller.hpp |
| [touchhandler.cpp](file:///home/pldx/Desktop/gonxun/gui/src/touchhandler.cpp) | gui | touchhandler.hpp |

---

*文档生成时间：2026-07-21*
*项目版本：v3.0.0*
