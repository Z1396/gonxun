# 添加视觉模式切换按钮

## Context

用户调试时需要手动切换视觉系统的工作模式（物料/圆环/二维码），目前模式只能由串口下位机下发控制。需要在 GUI 左侧工具栏添加三个模式按钮，点击即切换，方便离线调试。

## 修改清单

### 1. VisionSystem 增加模式覆写机制

**文件**: `vision_cpp/include/vision_system.hpp`

添加两个原子变量：
- `std::atomic<bool> manual_mode_{false}` — 是否手动模式
- `std::atomic<uint8_t> override_unit_{MODE_IDLE}` — 手动模式下的工作模式

公开访问（与 `serial_comm` 同级），无需 getter。

### 2. VisionWorker::run() 使用模式覆写

**文件**: `core/src/vision_controller.cpp`

将当前的：
```cpp
int current_unit = vision_system_->serial_comm.unit.load(std::memory_order_relaxed);
```
改为：
```cpp
int current_unit = vision_system_->manual_mode_.load(std::memory_order_relaxed)
    ? vision_system_->override_unit_.load(std::memory_order_relaxed)
    : vision_system_->serial_comm.unit.load(std::memory_order_relaxed);
```

### 3. MainWindow 添加三个模式按钮

**文件**: `gui/include/mainwindow.hpp`, `gui/src/mainwindow.cpp`

- 新增成员：`QPushButton *color_btn_`, `QPushButton *ring_btn_`, `QPushButton *qr_btn_`
- 新增信号：`void mode_switch_requested(int mode, bool manual)`
- 新增槽函数：`void on_mode_button_clicked()`
- 按钮样式复用现有 `button_style()` 模板
- 按钮固定尺寸 60×36，与现有按钮一致
- 互斥逻辑：点击任一按钮，取消其他两个按钮的选中状态；再次点击当前按钮则取消选中（回到串口自动模式）

工具栏顺序：开始 → 物料 → 圆环 → 二维码 → 障碍物 → 启停区 → 仿真 → [stretch] → 关闭

按钮配色：
- 物料：橙 (#e67e22 / #d35400 / #e74c3c)
- 圆环：蓝 (#2980b9 / #2471a3 / #e74c3c)
- 二维码：青 (#16a085 / #138d75 / #e74c3c)

### 4. main.cpp 连接模式切换信号

**文件**: `main.cpp`

```cpp
QObject::connect(&window, &MainWindow::mode_switch_requested,
    [&vision_system](int mode, bool manual) {
        vision_system.manual_mode_.store(manual, std::memory_order_relaxed);
        vision_system.override_unit_.store(static_cast<uint8_t>(mode), std::memory_order_relaxed);
    });
```

## 验证

1. 编译通过
2. 启动程序 → 点击"开始" → 点击"物料"按钮 → 主摄像头画面应显示 Mode: COLOR
3. 点击"圆环"按钮 → 画面切换为 Mode: RING
4. 点击"二维码"按钮 → 画面切换为 Mode: QR，扫码摄像头窗口激活
5. 再次点击"二维码"按钮 → 取消选中，回到串口自动模式（Mode: IDLE 或串口下发值）
