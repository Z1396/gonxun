# 串口通信协议重构方案

## Context

当前项目存在两套互不兼容的串口帧：视觉模块用 15 字节帧（`serial_comm.hpp`），运动控制用 17 字节帧（`motion_protocol.hpp`）。两者共用同一物理串口但接收线程只解析视觉帧，导致运动 ACK 完全断路——`MotionController::process_received_frame()` 从未被调用，每条指令必然超时失败。同时存在两个 `SerialComm` 实例（`MainWindow` 与 `VisionSystem` 各自构造）争抢同一串口 fd，真实模式下第二个 `open()` 失败回退 mock。

用户给出新的统一协议规范，将收发简化为单一帧格式，并明确交互语义为严格 stop-and-wait（发一段→等 move_done→发下一段）。本次重构目标是按新协议彻底重写通信层，修复 ACK 断路与双实例问题，并将仿真动画与下位机段进度对齐。

## 新协议定稿

### 发送帧（上位机→下位机，12 字节）

| 偏移 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0 | header | u8 | 0x66 |
| 1 | mode | u8 | 1=路径规划, 2=定位/视觉坐标上传 |
| 2 | angle | u8 | 0/90/180/270 |
| 3-4 | steps | i16 大端 | 带符号步数，正=前进，负=后退 |
| 5-6 | x | u16 大端 | mm，mode=2 时为物料坐标 |
| 7-8 | y | u16 大端 | mm，mode=2 时为物料坐标 |
| 9 | grab | u8 | 0/1，1=触发抓取 |
| 10 | checksum | u8 | bytes[1..9] 累加和 & 0xFF |
| 11 | tail | u8 | 0x77 |

### 接收帧（下位机→上位机，6 字节）

| 偏移 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0 | header | u8 | 0x66 |
| 1 | match_start | u8 | 0/1，比赛开始后锁存为 1 直到程序退出 |
| 2 | move_done | u8 | 0/1，每完成一段路径触发一次（边沿） |
| 3 | grab_done | u8 | 0/1，每次抓取完成触发（边沿） |
| 4 | checksum | u8 | bytes[1..3] 累加和 & 0xFF |
| 5 | tail | u8 | 0x77 |

### 不加序列号的理由

严格 stop-and-wait（发一帧等一个 done 信号）天然有序，seq 冗余。代价是丢失 done 信号时无法去重——用看门狗超时（默认 5s）报错中止，**不重发**（重发会导致下位机二次移动）。下位机端建议对 done 信号冗余发送 3 次（间隔 5ms）提高可靠性。

## 系统流程（用户确认）

1. 用户启动程序 → 标记障碍物 → 选择启停区 → 点"开始"按钮（启动视觉系统）
3. 下位机发 `match_start=1` → MainWindow 收到 → 若已满足（启停区已选 + 障碍物已得）则自动启动仿真
4. 仿真 BFS 规划路径，按方向分段，依次发送 `move_frame(angle, steps)`
5. 每收到一个 `move_done` → 发送下一段；动画按段推进机器人位置
6. 全段完成（到达二维码区扫描二维码得到任务码/物料区/粗加工区/暂存区）→ 从 VisionSystem 取物料坐标 → 发 `locate_frame(x, y, grab=1)` 触发机械臂抓取
7. 收到 `grab_done` → 推进到下一个导航段

## 文件改动清单

### 1. `core/include/motion_protocol.hpp` — 整文件重写

**删除**：旧 `MotionFrame`/`StepMoveData`/`PositionMoveData`/`StatusReportData`、`ADDR_*`/`CMD_*`/`MODE_HORIZON/VERTICAL`/`DIR_*`/`ACK_*` 常量、所有 `build_*_frame` 旧函数、`angle_to_direction`/`direction_to_angle`。

**新增**：
- 常量 `FRAME_HEADER=0x66`、`FRAME_TAIL=0x77`、`CMD_FRAME_LEN=12`、`FB_FRAME_LEN=6`、`ANGLE_0/90/180/270`
- 枚举 `enum class FrameMode : uint8_t { Path=1, Locate=2 }`
- `struct CommandFrame`（12 字节，`#pragma pack(push,1)`）：含 `update_checksum()`、`verify_checksum()`、`to_bytes()`，及大端访问器 `set_steps/get_steps/set_x/get_x/set_y/get_y`
- `struct FeedbackFrame`（6 字节）：含 `verify_checksum()`
- `struct MoveSegment { uint8_t angle; int16_t steps; }`
- 构建函数：`build_move_frame(angle, steps)`、`build_locate_frame(x, y, grab)`、`build_grab_frame()`
- 解析函数：`parse_feedback(const uint8_t*, size_t) -> std::optional<FeedbackFrame>`
- 路径分段：`segment_grid_path(const QVector<QPair<int,int>>&) -> QVector<MoveSegment>`（连续同方向合并），匿名命名空间内辅助 `grid_delta_to_move(dx, dy) -> std::pair<uint8_t,int16_t>`，约定沿用现有 `calc_move_between_grids` 方向映射

### 2. `vision_cpp/include/serial_comm.hpp` + `serial_comm.cpp` — 大幅改造

**删除**：`std::atomic<uint8_t> unit`/`unit_target`、`MODE_COLOR/RING/DOCK/QR` 视觉常量、`CMD_COLOR/RING/DOCK/QR`、旧帧索引常量、`send_coordinates`/`send_qr_data`/`build_frame`、`mock_cycle_units_`/`mock_cycle_idx_`、15 字节 `send_` 缓冲。

**新增**：
- 三个发送接口：`send_move_frame(angle, steps)`、`send_locate_frame(x, y, grab)`、`send_grab_frame()`（内部调 `build_*_frame` → `transmit`）
- 三个回调 setter：`set_match_start_callback(std::function<void(bool)>)`、`set_move_done_callback(std::function<void()>)`、`set_grab_done_callback(std::function<void()>)`
- 接收缓冲 `std::deque<uint8_t> rx_buf_` 与状态机式帧同步（扫 0x66 → 凑够 6 字节 → `parse_feedback` → 校验失败丢首字节 resync）
- `match_started_` 锁存标志（仅 `match_start==1 && !match_started_` 时触发回调一次）

**改造 `process_real`**：6 字节帧同步解析；解析成功后按字段派发回调。
**改造 `process_mock`**：启动 500ms 后发 `match_start=1` 锁存；记录每次 `send_move_frame` 的预期完成时间（`300ms + steps*50ms`）后发一帧 `move_done=1`；每次 `send_grab_frame` 或 `send_locate_frame(*,*,1)` 后 800ms 发 `grab_done=1`。让 stop-and-wait 队列机制在无硬件下可调试。

### 3. `gui/include/motion_controller.hpp` + `motion_controller.cpp` — 事件驱动重写

**删除**：`MotionCmdState`/`MotionCmd`、`QTimer* timeout_timer_`、`on_timeout_check`、`QElapsedTimer cmd_elapsed_timer_`、`waiting_ack_`、`retry_current_command`、`process_received_frame`、`send_step_move`/`send_position_move`/`send_stop`/`send_emergency`/`send_set_speed`/`query_status`、`last_status_`/`status_received`/`command_acked`/`command_nacked`/`command_timeout`、`seq_num_`/`current_seq_num`、`default_speed_`/`default_accel_`/`max_retries_`/`cmd_timeout_ms_`、`calc_move_between_grids`（移入 motion_protocol 的 `grid_delta_to_move`）。

**新增**：
- 队列类型改为 `QQueue<MoveSegment>`，成员 `current_segment_idx_`
- 看门狗 `QTimer* watchdog_timer_`（默认 5000ms，单次触发，超时 `emit motion_error` 并 `clear_queue`，不重发）
- slots `on_move_done()`、`on_grab_done()`（被 SerialComm 回调经 `QMetaObject::invokeMethod(this, "slot", Qt::QueuedConnection)` 跨线程调用）
- 构造时向 `serial_comm_` 注册三个回调
- `execute_grid_path(grid_path, start_angle)`：`clear_queue()` → 调 `segment_grid_path` → 入队 → `send_next_segment()`
- `send_next_segment()`：队空则 `emit path_completed()`；否则取队首调 `serial_comm_.send_move_frame`，重启看门狗，`emit segment_sent(idx)`
- `on_move_done()`：停看门狗，`emit segment_completed(idx++)`，`send_next_segment()`
- `send_grab()`：调 `serial_comm_.send_grab_frame()`，重启看门狗
- `on_grab_done()`：停看门狗，`emit grab_completed()`
- 新信号：`segment_sent(int)`、`segment_completed(int)`、`path_completed()`、`grab_completed()`
- 保留：`clear_queue`、`queue_size`、`is_busy`、`execute_grid_path`、`motion_error`

### 4. `gui/include/simulation_controller.hpp` + `simulation_controller.cpp` — 段级 pace

**`SimPhase` 增项**：`WAITING_MOVE_DONE`、`WAITING_GRAB_DONE`。

**改造 `plan_current_segment`**：BFS 求路径 → 调 `motion_controller_->execute_grid_path(grid_seq, 0)` → 进 `WAITING_MOVE_DONE`（不再用 50ms 定时器自由跑全程）。

**新增 slots**：`on_segment_completed(int idx)`（把视觉机器人跳到该段终点，调 `map_widget_.set_robot_pos`）、`on_path_completed()`（判断当前目标区域是否需要抓取：物料区/粗加工区 → 发 `motion_controller_->send_grab()` 进 `WAITING_GRAB_DONE`；其他 → 进 DWELLING）、`on_grab_completed()`（进 DWELLING）、`on_motion_error()`（调 `stop`）。

**移除/弱化** `on_animation_tick` 自由动画逻辑（保留 `anim_timer_` 仅用于 DWELLING 倒计时显示）。

**信号连接**（在 SimulationController 构造或 MainWindow 组装时）：
- `segment_completed` → `on_segment_completed`
- `path_completed` → `on_path_completed`
- `grab_completed` → `on_grab_completed`
- `motion_error` → `on_motion_error`

### 5. `gui/include/mainwindow.hpp` + `mainwindow.cpp` — 实例统一与比赛开始触发

**实例统一**：
- 构造函数新增 `SerialComm& serial_comm` 参数，存 `SerialComm& serial_comm_` 成员
- 删除 `mainwindow.cpp:184` 的 `new SerialComm(...)` 与析构 `delete serial_comm_`
- 删除 `serial_comm_->start()`（改由 main.cpp 调用）

**比赛开始触发**：
- 构造时向 `serial_comm_` 注册 `set_match_start_callback`，回调内 `QMetaObject::invokeMethod(this, "on_match_started", Qt::QueuedConnection)`
- 新增 slot `on_match_started()`：置 `match_started_=true` 并调 `try_auto_start_mission()`
- 新增 `try_auto_start_mission()`：若 `has_start_zone_ && has_task_code_ && match_started_` → `sim_controller_->start(task_code_)`
- 启停区选择回调与 QR 扫码回调分别置 `has_start_zone_=true` / `has_task_code_=true`，并调 `try_auto_start_mission()`
- 新增信号 `match_started()`

**视觉定位上传编排**（到达区域后取坐标发 locate 帧）：
- `on_path_completed` 或 SimulationController 的 `on_path_completed` 中，若当前目标是物料区/粗加工区/暂存区 → 从 `vision_system_.material_coords()` 取下一个物料坐标 → `serial_comm_.send_locate_frame(x, y, 1)`（grab=1 触发抓取）→ 进 `WAITING_GRAB_DONE`
- 收 `grab_completed` → 若还有物料坐标则发下一个；列表空 → 推进状态机
- 该编排逻辑可内嵌为 MainWindow 私有方法 `dispatch_next_material_grab()`

### 6. `vision_cpp/include/vision_system.hpp` + `vision_system.cpp` — 视觉定位存储化

**改造**：
- `serial_comm` 成员改为 `SerialComm& serial_comm`，构造函数加 `SerialComm&` 参数（与 cfg 一起）
- 删除 `send_coordinates`/`send_qr_data` 调用
- `process_color`/`process_ring`/`process_dock` 检测结果**存入** `std::vector<std::pair<uint16_t,uint16_t>> last_material_coords_`
- 新增 `[[nodiscard]] const std::vector<std::pair<uint16_t,uint16_t>>& material_coords() const noexcept` 与 `void clear_material_coords() noexcept`
- 新增 `void set_vision_mode(uint8_t mode) noexcept` 与 `[[nodiscard]] uint8_t current_vision_mode() const noexcept`（封装 `override_unit_`）
- `process_frame` 中 `unit<0` 分支改读 `current_vision_mode()`
- QR 结果仅走 `qr_callback_`（已接 MainWindow），不再发串口

### 7. `core/src/vision_controller.cpp` — 模式读取适配

- 第 77 行 `serial_comm.unit.load()` 改为 `vision_system_->current_vision_mode()`

### 8. `main.cpp` — 实例注入

- 在 `VisionSystem vision_system(cfg)` 之前构造 `SerialComm serial_comm(cfg.serial.mock, cfg.serial.port, cfg.serial.baudrate)`
- 改为 `VisionSystem vision_system(cfg, serial_comm)`
- 改为 `MainWindow window(serial_comm)`
- 调用 `serial_comm.start()` 一次

### 9. `CMakeLists.txt`

无需改动（SerialComm 源已在 gonxun_vision 中编译，MotionController 在 gonxun_gui 中）。

## 实施顺序（渐进式可编译验证）

1. **motion_protocol.hpp 重写**：纯头文件 + 纯函数，独立编译验证
2. **serial_comm.{hpp,cpp} 改造**：新帧收发 + mock 生成 done 信号
3. **motion_controller.{hpp,cpp} 重写**：事件驱动 + 段队列 + 看门狗
4. **simulation_controller 适配**：段级 pace + 抓取分支
5. **vision_system 适配**：serial_comm 引用 + 物料坐标存储
6. **mainwindow + main.cpp 适配**：实例统一 + 比赛开始触发 + 抓取编排
7. **vision_controller.cpp 一行改动**：模式读取适配

每步完成后单独编译对应模块验证。

## 验证方案

### 模拟模式端到端测试

1. `config.yaml` 中 `serial.mock: true`
2. 启动程序 → 标记障碍物 → 选启停区 → 点开始
3. 观察 SerialComm mock：500ms 后发 `match_start=1`
4. 扫码获取 task_code → 仿真应自动启动
5. 观察 MotionController 日志：依次发送每段 `move_frame`，每段后收到 mock 的 `move_done`，发送下一段
6. 到达物料区 → MainWindow 调 `send_locate_frame(x, y, 1)` → 800ms 后收到 `grab_done` → 推进
7. 全程无看门狗超时

### 纯函数单测（可选，推荐）

- `build_*_frame` ↔ `parse_feedback` round-trip
- `segment_grid_path` 对典型路径（同向合并、转向断段、空路径、单格路径）
- 大端编码正确性、checksum 覆盖范围

### 真机调试要点

- `#ifdef GONXUN_DEBUG_SERIAL` 打印每帧十六进制
- 看门狗阈值可配置（`config.yaml` 新增 `motion.watchdog_ms: 5000`）
- 真机若 move_done 丢失，先排查下位机是否冗余发送 3 次

## 风险与权衡

- **不加 seq 的代价**：done 信号丢失靠看门狗报错中止，不能自动恢复。需下位机端配合冗余发送。
- **段级 pace 改动**：SimulationController 动画逻辑改动较大，需仔细处理 DWELLING 与段完成的时序。
- **视觉定位存储化**：VisionSystem 职责收窄（不再直接发串口），MainWindow 承担编排。这是必要的，因为 mode=2 上传需要与 grab 流节奏联动。
- **比赛开始自动启动**：用户已确认流程，但需注意 `try_auto_start_mission` 的三个条件缺一不可，避免误启动。
