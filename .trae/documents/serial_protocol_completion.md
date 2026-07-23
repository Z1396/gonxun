# 串口协议重构 — 收尾实施计划

## 摘要

串口协议重构已完成主体（motion_protocol / serial_comm / motion_controller / simulation_controller / vision_system / vision_controller 均已按新协议改写并通过单模块编译）。本计划仅覆盖**尚未完成**的收尾工作：`main.cpp` 实例注入、`mainwindow.cpp` 比赛开始触发与自动启动、到达抓取目标后发 `mode=2` 视觉定位帧的编排，以及全量编译验证。

依据：用户已确认的总体方案见 `serial_protocol_refactor.md`，本文件是其执行收尾。

## 当前状态分析（已读源码确认）

| 文件 | 状态 | 说明 |
|------|------|------|
| `core/include/motion_protocol.hpp` | ✅ 完成 | CommandFrame(12B)/FeedbackFrame(6B)、build/parse、segment_grid_path |
| `vision_cpp/include/serial_comm.hpp` + `.cpp` | ✅ 完成 | send_move/locate/grab_frame、三回调、6B 帧同步、mock done 生成 |
| `gui/include/motion_controller.hpp` + `.cpp` | ✅ 完成 | 段队列、send_grab、看门狗、on_move_done/on_grab_done |
| `gui/include/simulation_controller.hpp` + `.cpp` | ✅ 完成 | 段级 pace、on_path_completed 调 send_grab、on_grab_completed→dwell |
| `vision_cpp/include/vision_system.hpp` + `.cpp` | ✅ 完成 | `SerialComm&` 引用、material_coords()、set_vision_mode()、不注册任何串口回调 |
| `core/src/vision_controller.cpp` | ✅ 完成 | current_vision_mode()、VISION_QR |
| `gui/include/mainwindow.hpp` | ✅ 完成 | 构造签名、on_match_started、try_auto_start_mission、状态标志成员 |
| `gui/src/mainwindow.cpp` | ⚠️ 部分 | 构造已更新、match_start 回调已注册、on_mode_button_clicked 已用 VISION_*；**缺**：on_match_started/try_auto_start_mission 实现、on_start_zone_selected/on_qr_code_scanned 置标志、抓取编排 |
| `main.cpp` | ❌ 未改 | 仍 `VisionSystem vision_system(cfg);` + `MainWindow window;`，无 SerialComm 构造与 start() |

**关键约束**（已验证）：
- VisionSystem 构造函数不注册任何串口回调，仅持有 `SerialComm&`；三回调分别由 MainWindow(match_start) 与 MotionController(move_done/grab_done) 注册，无覆盖冲突。
- `send_locate_frame` 的 mock 已就绪：`grab==1` 时 800ms 后发 `grab_done`（serial_comm.cpp:160-163）。
- 构建目标 `CourtMapViewer`，build/ 已有 ninja 缓存。

## 待实施改动

### 改动 1：`main.cpp` — 单实例注入

**目标**：在 main 中统一构造唯一 `SerialComm`，注入 VisionSystem 与 MainWindow，并启动接收线程。

**具体修改**（main.cpp:80-99 区域）：

```cpp
// 5. 构造唯一串口实例（与 VisionSystem/MainWindow 共享）
SerialComm serial_comm(cfg.serial.mock, cfg.serial.port, cfg.serial.baudrate);

// 6. 视觉系统（注入 serial_comm）
VisionSystem vision_system(cfg, serial_comm);

// 7. GUI 主窗口（注入 serial_comm + vision_system）
MainWindow window(serial_comm, vision_system);
window.show();

// 注册二维码扫描回调（不变）
vision_system.set_qr_callback([&window](const std::string& qr_data) {
    window.qr_code_scanned(QString::fromStdString(qr_data));
});

gonxun::VisionController controller(&vision_system);

// UI↔视觉信号槽绑定（不变，保持现有 4 个 connect）

// 8. 启动串口接收线程（在所有回调注册完成后）
serial_comm.start();
```

**删除**：无（原代码无需删除大块，只是替换两行构造 + 加 start）。
**保留**：命令行 `--mock-serial`/`--serial-port` 覆盖逻辑、qr_callback、4 个 QObject::connect。

### 改动 2：`gui/src/mainwindow.cpp` — 比赛开始触发 + 自动启动

**2a. 实现 `on_match_started()`**（新增方法体）：
```cpp
void MainWindow::on_match_started()
{
    if (match_started_) return;  // 锁存，仅触发一次
    match_started_ = true;
    std::cout << "[MainWindow] 收到比赛开始信号" << std::endl;
    emit match_started();
    try_auto_start_mission();
}
```

**2b. 实现 `try_auto_start_mission()`**（新增方法体）：
```cpp
void MainWindow::try_auto_start_mission()
{
    if (sim_controller_->is_running()) return;
    if (!has_start_zone_ || !has_task_code_ || !match_started_) return;
    std::cout << "[MainWindow] 三条件满足，自动启动任务: "
              << task_code_.toStdString() << std::endl;
    if (sim_controller_->start(task_code_)) {
        sim_btn_->setText("停止仿真");
        sim_btn_->setChecked(true);
    }
}
```

**2c. 更新 `on_start_zone_selected()`**（在末尾置标志 + 尝试自动启动）：
```cpp
void MainWindow::on_start_zone_selected(int zone_index, const QString &zone_name)
{
    Q_UNUSED(zone_index)
    Q_UNUSED(zone_name)
    select_start_btn_->setChecked(false);
    select_start_btn_->setText("启停区");
    court_map_->set_start_zone_selectable(false);

    has_start_zone_ = true;          // 新增
    try_auto_start_mission();        // 新增
}
```

**2d. 更新 `on_qr_code_scanned()`**（置标志 + 缓存任务码 + 尝试自动启动）：
```cpp
void MainWindow::on_qr_code_scanned(const QString& task_code)
{
    std::cout << "[MainWindow] 收到二维码扫描信号: " << task_code.toStdString() << std::endl;
    court_map_->set_task_code(task_code);

    has_task_code_ = true;           // 新增
    task_code_ = task_code;          // 新增：缓存供自动启动
    try_auto_start_mission();        // 新增
}
```

### 改动 3：`gui/include/motion_controller.hpp` + `.cpp` — 新增 send_locate

**原因**：用户要求到达抓取目标后发 `mode=2` 视觉定位帧（携带物料坐标 + grab=1），而非纯 grab 帧。MotionController 已封装 grab 的看门狗与 grab_in_progress_ 状态，新增 send_locate 复用同一套机制。

**motion_controller.hpp**（在 `send_grab()` 声明后新增）：
```cpp
/// @brief 发送视觉定位帧（mode=Locate, grab=1），等待 grab_done。
/// @param x 物料 X 坐标 (mm)
/// @param y 物料 Y 坐标 (mm)
/// @param grab 抓取指令 0/1
void send_locate(uint16_t x, uint16_t y, uint8_t grab);
```

**motion_controller.cpp**（在 `send_grab()` 实现后新增）：
```cpp
/// @brief 发送视觉定位帧，等待 grab_done（复用 grab 看门狗机制）。
void MotionController::send_locate(uint16_t x, uint16_t y, uint8_t grab)
{
    grab_in_progress_ = true;
    waiting_done_ = true;
    serial_comm_.send_locate_frame(x, y, grab);
    start_watchdog();
}
```
`on_grab_done()` 无需改动（已按 `grab_in_progress_` 派发，send_locate 与 send_grab 共用同一等待路径）。

### 改动 4：`gui/include/simulation_controller.hpp` + `.cpp` — 物料坐标提供者

**设计**：SimulationController 不直接依赖 VisionSystem（避免 gui↔vision_cpp 耦合），改为持有一个坐标提供者回调，由 MainWindow 在构造时注入（从 `vision_system_->material_coords()` 取首坐标）。

**simulation_controller.hpp**（新增，include 区加 `<functional>` `<optional>` `<utility>`）：
```cpp
/// @brief 物料坐标提供者：返回下一个待抓取物料坐标，无则 nullopt
using CoordProvider = std::function<std::optional<std::pair<uint16_t, uint16_t>>()>;

/// @brief 设置物料坐标提供者（由 MainWindow 注入，读取 VisionSystem 缓存）
void set_material_coord_provider(CoordProvider p) noexcept {
    material_coord_provider_ = std::move(p);
}
```
private 区新增成员：
```cpp
CoordProvider material_coord_provider_;  ///< 物料坐标提供者（可为空）
```

**simulation_controller.cpp — `on_path_completed()` 改造**：
```cpp
void SimulationController::on_path_completed()
{
    if (!running_) return;

    if (current_target_needs_grab() && motion_controller_) {
        phase_ = SimPhase::WAITING_GRAB_DONE;
        emit phase_changed(phase_, "抓取中");

        // 取视觉物料坐标；有则发 mode=2 定位帧，无则退化为纯抓取
        auto coord = material_coord_provider_ ? material_coord_provider_() : std::nullopt;
        if (coord) {
            motion_controller_->send_locate(coord->first, coord->second, 1);
        } else {
            motion_controller_->send_grab();
        }
    } else {
        start_dwelling();
    }
}
```

### 改动 5：`gui/src/mainwindow.cpp` — 注入坐标提供者

在 MainWindow 构造函数中（sim_controller_ 创建之后、信号槽绑定之前）注入：
```cpp
// 注入物料坐标提供者：到达抓取目标时取首坐标发 mode=2 定位帧
sim_controller_->set_material_coord_provider([this]() -> std::optional<std::pair<uint16_t, uint16_t>> {
    const auto& coords = vision_system_->material_coords();
    if (coords.empty()) return std::nullopt;
    return coords.front();
});
```
**线程安全说明**：`on_path_completed` 经 Qt 信号在 GUI 线程触发，lambda 亦在 GUI 线程读 `material_coords()`；VisionSystem 工作线程写该 vector 无锁，存在轻度数据竞争，但抓取时序为低频单次读取，可接受（如需严格安全后续可加 mutex）。

## 假设与决策

1. **单物料抓取**：每次到达物料区/粗加工区只发一帧 locate（取首坐标），不做多物料循环。多物料循环为后续增强。
2. **无坐标退化**：视觉未检测到物料时退化为 `send_grab()`（纯 mode=Path grab 帧，协议合法），保证流程不卡死。
3. **match_start 锁存**：`on_match_started` 内 `match_started_` 守卫，仅触发一次自动启动尝试。
4. **manual 仿真按钮保留**：`on_sim_button_clicked` 仍可用固定任务码手动启动（调试用），与自动启动互不冲突（`is_running()` 守卫）。
5. **serial_comm.start() 时机**：在 MainWindow 构造完成（match_start 回调已注册）后、app.exec() 前调用；mock 模式 500ms 后才发 match_start，时序安全。
6. **不改动 CMakeLists**：所有源文件已在现有 target 中，无需调整构建配置。

## 验证步骤

### 全量编译
```bash
cd /home/pldx/Desktop/gonxun/build && cmake .. && ninja CourtMapViewer
```
修复所有编译错误/警告（关注：未使用 include、初始化顺序、`[[nodiscard]]` 忽略）。

### mock 端到端（config.yaml: serial.mock=true）
1. 启动 → 标记障碍物 → 选启停区（`has_start_zone_=true`）
2. 点"开始"启动视觉 → 选"物料"模式触发 VISION_COLOR → `material_coords_` 填充
3. mock 串口 500ms 后发 `match_start=1` → `on_match_started` → `try_auto_start_mission`
4. （若无真实二维码）手动确认 `try_auto_start_mission` 在 `has_task_code_` 缺失时不误启动；扫码后补齐条件则自动启动
5. 仿真启动 → 依次发 move_frame → 每段收到 mock move_done → 发下一段
6. 到达物料区 → `send_locate(x, y, 1)` → 800ms 后 grab_done → dwell → 下一段
7. 全程无看门狗超时（5s）

### 真机要点
- `SERIAL_MOCK=False`，确认 `cfg.serial.port` 与实际设备匹配
- 看门狗超时可后续加 `motion.watchdog_ms` 配置项（本次不实现）
