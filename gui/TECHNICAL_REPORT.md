# 项目技术分析报告

## 一、项目当前状态评估

### 1.1 项目概述
- **项目类型**: 机器人竞赛视觉识别系统
- **核心功能**: 物料识别、数字识别、二维码识别、同心圆检测
- **目标平台**: Windows (开发) + Jetson Nano (部署)
- **技术栈**: Python + OpenCV + YOLOv8 + TensorRT

### 1.2 已实现功能

| 模块 | 功能 | 状态 | 关键文件 |
|------|------|------|---------|
| 摄像头管理 | 多路摄像头、曝光控制 | ✅ 完成 | vision/camera_manager.py |
| YOLO检测器 | 6色物料识别 | ✅ 完成 | vision/yolo_detector.py |
| 颜色检测 | HSV颜色识别 | ✅ 完成 | vision/color_detector.py |
| 二维码检测 | QR码识别 | ✅ 完成 | vision/qr_detector.py |
| 同心圆检测 | 环形图案检测 | ✅ 完成 | vision/ring_detector.py |
| 数字识别 | CNN数字识别（1/2/3） | ⚠️ 需重训 | tests/test_digit_cnn.py |
| 串口通信 | 与下位机通信 | ✅ 完成 | vision/serial_comm.py |
| 系统调度 | 主流程控制 | ✅ 完成 | vision/system.py |
| YOLO训练管线 | 数据集管理与训练 | ✅ 完成 | yolo_pipeline/ |
| TensorRT推理 | Jetson加速 | ✅ 完成 | vision/yolo_tensorrt_detector.py |

### 1.3 项目目录结构

```
项目根目录/
├── vision/                    # 核心视觉模块
│   ├── camera_manager.py      # 摄像头管理
│   ├── yolo_detector.py       # YOLO检测（PyTorch）
│   ├── yolo_tensorrt_detector.py  # TensorRT检测（Jetson）
│   ├── color_detector.py      # 颜色检测
│   ├── qr_detector.py         # 二维码检测
│   ├── ring_detector.py       # 同心圆检测
│   ├── serial_comm.py         # 串口通信
│   ├── system.py              # 系统主流程
│   └── utils.py               # 工具函数
├── yolo_pipeline/             # YOLO训练管线
│   ├── train_model.py         # 训练脚本
│   ├── split_val.py           # 验证集划分
│   ├── config.yaml            # 配置文件
│   ├── inference.py           # 推理脚本
│   └── runs/                  # 训练输出
├── yolo_dataset/              # 数据集
│   ├── data.yaml              # 数据集配置
│   ├── images/train/          # 训练图片
│   └── labels/train/          # 标注文件
├── tests/                     # 测试脚本
│   ├── collect_digits.py      # 数字采集
│   ├── collect_materials.py   # 物料采集
│   ├── test_jetson.py         # Jetson测试
│   └── test_ring_detector.py  # 同心圆测试
├── gui/                       # Qt GUI界面（新增）
│   ├── CMakeLists.txt         # CMake构建
│   ├── include/               # 头文件
│   └── src/                   # 源文件
├── config.py                  # 主配置
├── vision_system.py           # 主入口
├── requirements.txt           # Python依赖
└── setup_jetson.sh            # Jetson部署脚本
```

---

## 二、代码结构优化方案

### 2.1 已识别的冗余文件

| 文件 | 原因 | 建议 |
|------|------|------|
| vision/fusion_detector.py | 融合检测，主流程未使用 | 保留备用 |
| vision/material_recognizer.py | YOLO+颜色融合，主流程用yolo_detector | 保留备用 |
| vision/precise_locator.py | 亚精确定位，仅融合检测使用 | 保留备用 |
| vision/region_extractor.py | ROI提取，仅融合检测使用 | 保留备用 |
| vision/visualization.py | 可视化，仅融合检测使用 | 保留备用 |
| vision/material_visualizer.py | 物料可视化，主流程未使用 | 保留备用 |
| vision/jetson_optimizer.py | Jetson性能优化，实验性 | 保留备用 |
| vision/kalman_filter.py | 卡尔曼滤波，目标跟踪用 | 保留备用 |
| vision/obstacle_detector.py | 障碍物检测，未使用 | 保留备用 |
| vision/task_display.py | 任务显示，未使用 | 保留备用 |
| install_pytorch_jetson.sh | 安装失败，已废弃 | ❌ 删除 |
| test_*.py (根目录) | 临时测试文件 | ❌ 删除 |
| vscode-old.deb | 安装包，已安装 | ❌ 删除 |
| vscode-arm64.deb | 安装失败 | ❌ 删除 |
| _cleanup_backup_20260628/ | 上次清理备份 | ❌ 删除（确认无误后） |

### 2.2 目录结构优化建议

```
优化后结构:
project/
├── src/                       # Python源码
│   ├── core/                  # 核心模块
│   │   ├── camera_manager.py
│   │   ├── yolo_detector.py
│   │   ├── color_detector.py
│   │   ├── qr_detector.py
│   │   ├── ring_detector.py
│   │   └── serial_comm.py
│   ├── system/                # 系统调度
│   │   ├── system.py
│   │   └── config.py
│   └── utils/                 # 工具函数
│       └── utils.py
├── gui/                       # Qt GUI界面
│   ├── CMakeLists.txt
│   ├── include/
│   └── src/
├── training/                  # 训练相关
│   ├── yolo_pipeline/
│   └── dataset/
├── tests/                     # 测试脚本
├── scripts/                   # 部署脚本
│   ├── setup_jetson.sh
│   ├── start_jetson.sh
│   └── config_jetson.py
├── docs/                      # 文档
│   ├── JETSON_MIGRATION_GUIDE.md
│   └── RETRAIN_GUIDE.md
└── vision_system.py           # 主入口
```

---

## 三、图形用户界面 (GUI) 设计方案

### 3.1 技术选型

- **框架**: Qt 5 (Widgets)
- **构建系统**: CMake
- **编程语言**: C++
- **图像库**: OpenCV 3.x+
- **目标设备**: 触摸屏 (1024x768 或更高)

### 3.2 界面架构

采用多页面堆叠式设计 (QStackedWidget)，适合触摸屏操作：

```
主页 (HomePage)
  ├── 开始任务 → 控制页
  ├── 摄像头 → 摄像头页
  ├── 赛场地图 → 地图页
  └── 设置 → 设置页

控制页 (ControlPage)
  ├── 赛场地图（可交互）
  ├── 任务按钮（5个任务）
  └── 紧急停止

摄像头页 (CameraPage)
  ├── 实时画面
  ├── 开启/停止控制
  ├── 摄像头选择
  ├── 曝光调节
  └── 截图功能

地图页 (MapPage)
  ├── 完整赛场地图
  ├── 机器人位置
  ├── 任务进度
  └── 目标点标记

设置页 (SettingsPage)
  ├── 摄像头参数
  ├── 识别阈值
  ├── 串口配置
  ├── 颜色校准
  ├── 触摸屏校准
  └── 关于系统
```

### 3.3 触摸屏设计规范

| 参数 | 推荐值 | 说明 |
|------|--------|------|
| 按钮最小尺寸 | 60x60 px | 手指触摸区域 |
| 按钮间距 | 10-20 px | 防止误触 |
| 字体大小 | 16-24 px | 清晰可读 |
| 图标大小 | 32-48 px | 直观识别 |
| 页面切换 | 滑动/按钮 | 两种方式都支持 |
| 反馈效果 | 颜色变化+震动 | 确认操作 |

### 3.4 赛场地图设计

**地图元素**:
- 原料区 (顶部，黄色背景)
- 暂存区 (左侧，绿色背景)
- 粗加工区 (底部，白色背景)
- 启停区1 (右上，深蓝色)
- 启停区2 (右下，深蓝色)
- 二维码板 (右侧竖条)
- 原料盘 (圆形，带渐变)
- 暂存位/加工位 (黑色圆点)
- 机器人位置 (红色圆形，渐变)
- 任务进度条 (中央，56%)
- 尺寸标注 (四周)

**坐标系统**:
- 地图尺寸: 2400 x 2400 (单位: mm)
- 自动缩放适配窗口
- 支持坐标转换 (widget ↔ map)

---

## 四、触摸屏功能实现方案

### 4.1 Qt 触摸事件处理机制

**事件类型**:
- `QEvent::TouchBegin` - 触摸开始
- `QEvent::TouchUpdate` - 触摸移动
- `QEvent::TouchEnd` - 触摸结束
- `QTouchEvent` - 触摸事件类
- `QTouchEvent::TouchPoint` - 触摸点信息

**关键属性**:
- `Qt::WA_AcceptTouchEvents` - 接受触摸事件属性
- `touchPoint.id()` - 触摸点唯一标识
- `touchPoint.position()` - 触摸点坐标
- `touchPoint.state()` - 触摸点状态

### 4.2 TouchHandler 类设计

**功能模块**:

| 功能 | 说明 | 参数 |
|------|------|------|
| 单点触摸 | 按下/移动/释放 | touchPressed/Moved/Released |
| 点击检测 | 短按识别 | Tap (默认20px, 200ms) |
| 双击检测 | 快速双击 | DoubleTap (默认300ms间隔) |
| 长按检测 | 长按识别 | LongPress (默认800ms) |
| 滑动检测 | 四方向滑动 | SwipeLeft/Right/Up/Down |
| 缩放手势 | 双指捏合 | PinchIn/Out |
| 旋转手势 | 双指旋转 | Rotate |

**可调参数**:
```cpp
setTapRadius(int radius)              // 点击判定半径 (默认20px)
setTapDelay(int ms)                   // 点击延迟 (默认200ms)
setLongPressDuration(int ms)          // 长按时长 (默认800ms)
setSwipeDistance(int dist)            // 滑动距离阈值 (默认50px)
setDoubleTapInterval(int ms)          // 双击间隔 (默认300ms)
```

### 4.3 赛场地图触摸交互

**点击区域检测**:
```
用户点击 → widget坐标 → map坐标 → 遍历区域 → 返回区域名
         ↑坐标转换    ↑几何包含
```

**支持的交互**:
- 点击区域 → 高亮 + 显示信息
- 点击地图空白 → 设置目标点
- 双指缩放 → 地图缩放（预留）
- 滑动 → 地图平移（预留）

### 4.4 性能优化建议

| 优化点 | 方法 | 预期效果 |
|--------|------|---------|
| 减少重绘 | 使用 update() 而非 repaint() | 减少闪烁 |
| 双缓冲 | Qt自动启用，无需额外代码 | 平滑绘制 |
| 简化绘制 | 缓存静态元素为 QPixmap | 提升 30-50% |
| 触摸响应 | 事件处理 < 16ms | 60fps 流畅 |
| 手势检测 | 后台线程/定时器 | 不阻塞主线程 |
| 坐标转换 | 预计算缩放因子 | 减少重复计算 |

---

## 五、系统集成测试方案

### 5.1 测试环境配置

**硬件环境**:
- 触摸屏: 10寸以上电容屏 (1024x768+)
- 摄像头: USB摄像头 x2
- 开发板: Jetson Nano / PC (Windows)

**软件环境**:
- Qt 5.12+ (Widgets + Quick)
- OpenCV 3.x / 4.x
- CMake 3.10+
- 编译器: MSVC / GCC

### 5.2 测试用例

#### 5.2.1 单点触摸测试

| 用例ID | 测试项 | 操作 | 预期结果 |
|--------|--------|------|---------|
| TC-T01 | 点击按钮 | 点击"开始任务" | 页面跳转至控制页 |
| TC-T02 | 点击地图区域 | 点击"原料区" | 区域高亮 + 状态提示 |
| TC-T03 | 点击地图空白 | 点击地图中间 | 添加目标点标记 |
| TC-T04 | 点击边缘 | 点击按钮边缘 | 正常响应 |
| TC-T05 | 快速点击 | 快速点击多次 | 无卡顿、不崩溃 |

#### 5.2.2 多点触摸测试

| 用例ID | 测试项 | 操作 | 预期结果 |
|--------|--------|------|---------|
| TC-M01 | 双指触摸 | 两指同时按下 | 识别为2个触摸点 |
| TC-M02 | 双指捏合 | 两指靠拢/分开 | 触发缩放手势 |
| TC-M03 | 双指旋转 | 两指旋转 | 触发旋转手势 |
| TC-M04 | 多指触摸 | 3指以上按下 | 稳定不崩溃 |

#### 5.2.3 手势测试

| 用例ID | 测试项 | 操作 | 预期结果 |
|--------|--------|------|---------|
| TC-G01 | 左滑 | 从右向左滑动 > 50px | 触发 SwipeLeft |
| TC-G02 | 右滑 | 从左向右滑动 > 50px | 触发 SwipeRight |
| TC-G03 | 上滑 | 从下向上滑动 > 50px | 触发 SwipeUp |
| TC-G04 | 下滑 | 从上向下滑动 > 50px | 触发 SwipeDown |
| TC-G05 | 双击 | 快速点击两次同一位置 | 触发 DoubleTap |
| TC-G06 | 长按 | 按住 800ms 不动 | 触发 LongPress |

#### 5.2.4 摄像头测试

| 用例ID | 测试项 | 操作 | 预期结果 |
|--------|--------|------|---------|
| TC-C01 | 摄像头开启 | 点击"开启"按钮 | 显示实时画面 |
| TC-C02 | 摄像头切换 | 选择摄像头1 | 切换到摄像头1 |
| TC-C03 | 曝光调节 | 拖动曝光滑块 | 画面亮度变化 |
| TC-C04 | 截图功能 | 点击"截图"按钮 | 保存图片 |
| TC-C05 | FPS显示 | 运行30秒 | FPS稳定，画面流畅 |

#### 5.2.5 地图绘制测试

| 用例ID | 测试项 | 操作 | 预期结果 |
|--------|--------|------|---------|
| TC-MP01 | 地图显示 | 进入地图页 | 完整绘制所有元素 |
| TC-MP02 | 窗口缩放 | 调整窗口大小 | 地图自适应 |
| TC-MP03 | 机器人移动 | 更新机器人位置 | 平滑移动 |
| TC-MP04 | 进度更新 | 改变进度值 | 进度条实时更新 |
| TC-MP05 | 目标点标记 | 添加目标点 | 正确显示标记 |

#### 5.2.6 系统集成测试

| 用例ID | 测试项 | 操作 | 预期结果 |
|--------|--------|------|---------|
| TC-S01 | 完整流程 | 主页→控制→开始→结束 | 流程顺畅 |
| TC-S02 | 内存泄漏 | 运行2小时 | 内存稳定增长 < 50MB |
| TC-S03 | 长时间运行 | 连续运行24小时 | 无崩溃、无异常 |
| TC-S04 | 快速切换 | 快速切换页面 | 无延迟、无闪退 |
| TC-S05 | 异常恢复 | 摄像头断开重连 | 自动恢复 |

### 5.3 性能指标

| 指标 | 目标值 | 测试方法 |
|------|--------|---------|
| 启动时间 | < 3秒 | 计时从启动到主界面显示 |
| 页面切换 | < 200ms | 计时页面切换动画 |
| 触摸响应 | < 50ms | 从触摸到界面反馈 |
| 地图绘制 | < 16ms (60fps) | QElapsedTimer 测量 paintEvent |
| 摄像头延迟 | < 100ms | 对比实物与画面 |
| 内存占用 | < 200MB | 任务管理器/top 查看 |
| CPU占用 | < 30% (空闲) | 任务管理器/top 查看 |

---

## 六、编译与运行指南

### 6.1 编译步骤 (Windows)

```powershell
# 前提：已安装 Qt5 + OpenCV + CMake

cd gui
mkdir build
cd build

# 配置
cmake .. -G "Visual Studio 16 2019" -A x64 ^
  -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019_64" ^
  -DOpenCV_DIR="C:/opencv/build"

# 编译
cmake --build . --config Release

# 运行
./Release/VisionGUI.exe
```

### 6.2 编译步骤 (Linux/Jetson)

```bash
cd gui
mkdir build
cd build

# 配置
cmake .. \
  -DCMAKE_PREFIX_PATH="/usr/lib/aarch64-linux-gnu/cmake/Qt5" \
  -DOpenCV_DIR="/usr/share/OpenCV"

# 编译
make -j4

# 运行
./VisionGUI
```

### 6.3 依赖说明

- **Qt5 Core**: 核心功能
- **Qt5 Gui**: 图形界面
- **Qt5 Widgets**: 控件库
- **Qt5 Quick/Qml**: QML支持（可选）
- **OpenCV**: 图像处理 + 摄像头
- **CMake**: 构建系统

---

## 七、关键文件清单

| 文件 | 路径 | 说明 |
|------|------|------|
| 主窗口 | gui/include/mainwindow.h | 主界面逻辑 |
| 赛场地图 | gui/include/courtmapwidget.h | 地图绘制与交互 |
| 摄像头 | gui/include/camerawidget.h | 摄像头显示与控制 |
| 触摸处理 | gui/include/touchhandler.h | 触摸与手势识别 |
| 构建脚本 | gui/CMakeLists.txt | CMake构建配置 |

---

## 八、后续优化方向

1. **QML 界面**: 使用 Qt Quick 实现更流畅的动画
2. **3D 地图**: 使用 Qt 3D 显示赛场三维视图
3. **视频流优化**: 使用 GPU 加速视频解码
4. **语音反馈**: 集成语音提示功能
5. **数据记录**: 运行日志与性能统计
6. **远程控制**: 网络远程监控功能
