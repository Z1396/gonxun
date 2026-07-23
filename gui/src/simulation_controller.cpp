/// @file simulation_controller.cpp
/// @brief 仿真控制器实现，包含5×5格子BFS路径搜索、动画播放与状态机推进。
///        每段导航使用BFS在格子图上搜索避开障碍物的最短路径，
///        找到后将路径转为赛场坐标序列驱动动画，同时通过串口下发步进指令。

#include "simulation_controller.hpp"

#include <QDateTime>
#include <QQueue>
#include <cmath>
#include <iostream>

/// @brief 构造仿真控制器：初始化高精度动画定时器，绑定状态机回调
///        以自动更新数据面板的状态与进度显示。
SimulationController::SimulationController(CourtMapWidget& map_widget,
                                           DataPanelWidget* data_panel,
                                           MotionController* motion_controller,
                                           QObject* parent) noexcept
    : QObject(parent)
    , map_widget_(map_widget)
    , data_panel_(data_panel)
    , motion_controller_(motion_controller)
{
    // ==== 动画定时器初始化 ====
    anim_timer_ = new QTimer(this);
    anim_timer_->setTimerType(Qt::PreciseTimer);
    connect(anim_timer_, &QTimer::timeout, this, &SimulationController::on_animation_tick);

    // ==== 状态机回调绑定：状态变更时更新面板 ====
    state_machine_.on_state_change([this](gonxun::TaskState old_state, gonxun::TaskState new_state) {
        Q_UNUSED(old_state)
        if (data_panel_) {
            data_panel_->update_task_state(
                QString::fromStdString(gonxun::TaskStateMachine::state_to_string(new_state)));
        }
    });

    // 状态机回调绑定：进度更新时刷新面板
    state_machine_.on_progress_update([this](const gonxun::TaskProgress& p) {
        if (data_panel_) {
            data_panel_->update_task_progress(p.current_cycle, p.total_cycles,
                                              p.materials_picked, p.materials_placed,
                                              p.total_materials);
            data_panel_->update_task_code(QString::fromStdString(p.task_code));
        }
    });
}

/// @brief 启动仿真：检查前置条件，初始化导航序列与状态机，开始第一段。
/// @param task_code 任务编码字符串
/// @return true 启动成功；false 已在运行/未选择启停区/机器人不可见
bool SimulationController::start(const QString& task_code)
{
    if (running_) {
        return false;
    }

    // 前置条件：必须选择启停区
    if (map_widget_.selected_start_zone() < 0) {
        phase_ = SimPhase::ERROR;
        emit phase_changed(phase_, "错误：未选择启停区");
        return false;
    }

    // 前置条件：机器人必须可见（有初始位置）
    if (!map_widget_.is_robot_visible()) {
        return false;
    }

    // ==== 初始化仿真状态 ====
    task_code_ = task_code;
    running_ = true;
    current_segment_ = 0;
    current_cycle_ = 0;
    total_distance_ = 0.0;
    total_steps_ = 0;
    total_timer_.start();

    nav_sequence_ = build_nav_sequence();

    // ==== 初始化状态机：快速推进到Mission状态 ====
    state_machine_.set_task_code(task_code.toStdString());
    state_machine_.set_total_cycles(total_cycles_);
    state_machine_.set_total_materials(3);
    state_machine_.reset();
    (void)state_machine_.handle_event(gonxun::TaskEvent::START_MARKING);
    (void)state_machine_.handle_event(gonxun::TaskEvent::MARKING_DONE);
    (void)state_machine_.handle_event(gonxun::TaskEvent::START_MISSION);

    if (data_panel_) {
        data_panel_->update_task_code(task_code);
        data_panel_->update_task_state("仿真开始");
    }

    emit simulation_started();

    start_next_segment();
    return true;
}

/// @brief 停止仿真：重置所有运行状态，停止动画定时器，发射失败完成信号。
void SimulationController::stop() noexcept
{
    running_ = false;
    anim_timer_->stop();
    phase_ = SimPhase::IDLE;
    current_segment_ = 0;
    current_path_idx_ = 0;
    emit simulation_finished(false);
}

/// @brief 构建导航序列：每循环包含扫码区→原料区→粗加工区→暂存区，
///        末尾添加返回启停区。启停区位置由用户选择决定。
/// @return 完整导航目标列表
QVector<SimulationController::NavTarget> SimulationController::build_nav_sequence() const
{
    QVector<NavTarget> seq;

    for (int cycle = 0; cycle < total_cycles_; ++cycle) {
        seq.append({"前往扫码区", 0, 2});
        seq.append({"前往原料区", 2, 0});
        seq.append({"前往粗加工区", 2, 4});
        seq.append({"前往暂存区", 4, 2});
    }

    int start_x = 0;
    int start_y = (map_widget_.selected_start_zone() == 0) ? 0 : 4;
    seq.append({"返回启停区", start_x, start_y});

    return seq;
}

/// @brief 启动下一段导航：检查是否全部完成，否则设阶段PLANNING并规划路径。
void SimulationController::start_next_segment()
{
    if (current_segment_ >= nav_sequence_.size()) {
        on_all_complete();
        return;
    }

    const NavTarget& target = nav_sequence_[current_segment_];

    phase_ = SimPhase::PLANNING;
    emit phase_changed(phase_, target.name);

    if (data_panel_) {
        data_panel_->update_task_state(target.name);
    }

    plan_current_segment();
}

/// @brief 为当前段执行BFS路径规划。
///        在5×5格子图上从当前位置搜索到目标位置的最短路径，
///        避开已标记障碍物和固定障碍物（黄色方块和启停区）。
///        路径找到后转为赛场坐标并下发给运动控制器，然后启动动画。
///        若路径被阻断则报错停止。
void SimulationController::plan_current_segment()
{
    const NavTarget& target = nav_sequence_[current_segment_];
    QPointF current_pos = map_widget_.robot_pos();

    // 赛场坐标 → 5×5格子坐标
    auto current_cell = map_widget_.field_to_grid5(static_cast<int>(current_pos.x()),
                                                      static_cast<int>(current_pos.y()));

    int current_grid_x = current_cell.grid_x;
    int current_grid_y = current_cell.grid_y;
    int target_grid_x = target.grid_x;
    int target_grid_y = target.grid_y;

    // 起点与终点重合，直接完成当前段
    if (current_grid_x == target_grid_x && current_grid_y == target_grid_y) {
        on_segment_complete();
        return;
    }

    // ==== 5×5格子BFS路径搜索 ====
    const int GRID_SIZE = 5;
    bool visited[GRID_SIZE][GRID_SIZE] = {};
    int parent_x[GRID_SIZE][GRID_SIZE] = {};
    int parent_y[GRID_SIZE][GRID_SIZE] = {};
    for (int y = 0; y < GRID_SIZE; ++y)
        for (int x = 0; x < GRID_SIZE; ++x) {
            parent_x[y][x] = -1;
            parent_y[y][x] = -1;
        }

    // 四连通方向：右、左、下、上
    const int dx[] = {1, -1, 0, 0};
    const int dy[] = {0, 0, 1, -1};

    // 返回启停区时允许进入启停区格子
    bool is_return_to_start = target.name.contains("启停区");
    // 获取选中的启停区Y坐标（0=右上角，4=右下角）
    int selected_start_y = (map_widget_.selected_start_zone() == 0) ? 0 : 4;

    // 可进入判定：边界内 + 无障碍
    // 规则：离开启停区后不能进入任何启停区；返回时只能进入选中的启停区
    auto can_enter = [&](int gx, int gy) -> bool {
        if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE) return false;

        // 启停区格子 (x=0, y=0 或 y=4)
        bool is_start_zone_cell = (gx == 0 && (gy == 0 || gy == 4));

        if (is_start_zone_cell) {
            // 返回启停区时，只允许进入选中的启停区
            if (is_return_to_start && gx == 0 && gy == selected_start_y) {
                return true;
            }
            // 其他情况禁止进入启停区
            return false;
        }

        // 非启停区格子：检查障碍物
        if (map_widget_.has_obstacle_in_cell(gx, gy)) return false;
        return true;
    };

    // BFS搜索
    QQueue<QPair<int, int>> queue;
    queue.enqueue({current_grid_x, current_grid_y});
    visited[current_grid_y][current_grid_x] = true;

    bool found = false;
    while (!queue.isEmpty()) {
        auto [cx, cy] = queue.dequeue();
        if (cx == target_grid_x && cy == target_grid_y) {
            found = true;
            break;
        }
        for (int d = 0; d < 4; ++d) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            if (can_enter(nx, ny) && !visited[ny][nx]) {
                visited[ny][nx] = true;
                parent_x[ny][nx] = cx;
                parent_y[ny][nx] = cy;
                queue.enqueue({nx, ny});
            }
        }
    }

    // 路径不可达时报错停止
    if (!found) {
        phase_ = SimPhase::ERROR;
        (void)state_machine_.handle_event(gonxun::TaskEvent::ERROR_OCCURRED);
        emit phase_changed(phase_, QString("无法到达: %1（路径被阻断）").arg(target.name));
        stop();
        return;
    }

    // 回溯重建格子路径序列
    QVector<QPair<int, int>> grid_seq;
    int px = target_grid_x, py = target_grid_y;
    while (px != -1 && py != -1) {
        grid_seq.prepend({px, py});
        int ox = parent_x[py][px];
        int oy = parent_y[py][px];
        px = ox;
        py = oy;
    }

    // 格子坐标 → 赛场坐标路径
    QVector<QPointF> grid_path;
    for (const auto& [gx, gy] : grid_seq) {
        grid_path.append(map_widget_.get_cell_center(gx, gy));
    }

    // 在地图上显示路径
    map_widget_.set_path(grid_path);

    // 保存当前路径供动画使用
    current_path_.clear();
    for (const QPointF& pt : grid_path) {
        current_path_.push_back({static_cast<int>(pt.x()), static_cast<int>(pt.y())});
    }

    // ==== 通过串口发送步进移动指令 ====
    if (motion_controller_) {
        motion_controller_->execute_grid_path(grid_seq, 0);
    }

    start_moving();
}

/// @brief 切换至MOVING阶段，重置动画索引并启动定时器。
void SimulationController::start_moving()
{
    phase_ = SimPhase::MOVING;
    current_path_idx_ = 0;
    emit phase_changed(phase_, "移动中");
    anim_timer_->start(anim_interval_);
}

/// @brief 切换至DWELLING阶段：停止动画，延迟dwell_time_后推进到下一段。
void SimulationController::start_dwelling()
{
    phase_ = SimPhase::DWELLING;
    anim_timer_->stop();
    emit phase_changed(phase_, "到达目标");

    const NavTarget& target = nav_sequence_[current_segment_];
    if (data_panel_) {
        data_panel_->update_task_state("到达" + target.name);
    }

    // 驻留延迟后自动推进
    QTimer::singleShot(dwell_time_, this, [this]() {
        on_segment_complete();
    });
}

/// @brief 动画定时器回调：将机器人沿当前路径前进一步，
///        计算移动方向角度并更新地图与数据面板。路径走完转入驻留。
void SimulationController::on_animation_tick()
{
    // 路径已走完，进入驻留
    if (current_path_idx_ >= static_cast<int>(current_path_.size())) {
        start_dwelling();
        return;
    }

    const auto& pt = current_path_[current_path_idx_];

    // 计算移动方向角度
    QPointF cur_pos = map_widget_.robot_pos();
    double dx = pt.x - cur_pos.x();
    double dy = pt.y - cur_pos.y();
    double angle = std::atan2(dy, dx) * 180.0 / M_PI;
    if (angle < 0) angle += 360.0;

    // 累计统计
    total_distance_ += std::sqrt(dx*dx + dy*dy);
    total_steps_++;

    // 更新地图与面板
    map_widget_.set_robot_pos(QPointF(pt.x, pt.y), angle);

    if (data_panel_) {
        data_panel_->update_robot_pose(pt.x, pt.y, angle);
    }

    current_path_idx_++;
}

/// @brief 当前导航段完成处理：根据目标名称推进状态机事件，
///        清除地图路径显示，递增段索引，启动下一段或完成全部。
void SimulationController::on_segment_complete()
{
    const NavTarget& target = nav_sequence_[current_segment_];

    // 根据目标区域推进状态机（返回值为转移后状态，此处不使用）
    if (target.name.contains("扫码区")) {
        (void)state_machine_.handle_event(gonxun::TaskEvent::REACHED_QR);
        (void)state_machine_.handle_event(gonxun::TaskEvent::QR_SCANNED);
    } else if (target.name.contains("原料区")) {
        (void)state_machine_.handle_event(gonxun::TaskEvent::REACHED_MATERIAL);
        (void)state_machine_.handle_event(gonxun::TaskEvent::MATERIAL_PICKED);
    } else if (target.name.contains("粗加工")) {
        (void)state_machine_.handle_event(gonxun::TaskEvent::REACHED_PROCESS);
        (void)state_machine_.handle_event(gonxun::TaskEvent::MATERIAL_PLACED);
    } else if (target.name.contains("暂存区")) {
        (void)state_machine_.handle_event(gonxun::TaskEvent::REACHED_BUFFER);
    } else if (target.name.contains("启停区")) {
        (void)state_machine_.handle_event(gonxun::TaskEvent::REACHED_START);
        (void)state_machine_.handle_event(gonxun::TaskEvent::ALL_DONE);
    }

    map_widget_.clear_path();
    current_segment_++;

    if (current_segment_ >= nav_sequence_.size()) {
        on_all_complete();
        return;
    }

    start_next_segment();
}

/// @brief 全部导航段完成：停止仿真，设阶段COMPLETED，发射成功完成信号。
void SimulationController::on_all_complete()
{
    running_ = false;
    phase_ = SimPhase::COMPLETED;
    anim_timer_->stop();

    if (data_panel_) {
        data_panel_->update_task_state("任务完成");
    }

    emit phase_changed(phase_, "全部完成");
    emit simulation_finished(true);
}
