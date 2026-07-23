/// @file simulation_controller.cpp
/// @brief 仿真控制器实现，包含5×5格子BFS路径搜索与段级 pace 动画。
///        每段导航使用BFS在格子图上搜索避开障碍物的最短路径，
///        路径按方向分段，每段通过串口下发 move_frame，收到 move_done 后推进机器人位置。
///        到达物料区/粗加工区后自动发送 grab_frame 触发抓取。

#include "simulation_controller.hpp"

#include <QQueue>
#include <cmath>
#include <iostream>

/// @brief 构造仿真控制器：初始化驻留定时器，绑定状态机回调。
SimulationController::SimulationController(CourtMapWidget& map_widget,
                                           MotionController* motion_controller,
                                           QObject* parent) noexcept
    : QObject(parent)
    , map_widget_(map_widget)
    , motion_controller_(motion_controller)
{
    // 驻留定时器：单次触发，到达目标后等待
    dwell_timer_ = new QTimer(this);
    dwell_timer_->setSingleShot(true);
    connect(dwell_timer_, &QTimer::timeout, this, [this]() { on_segment_complete(); });

    // 状态机回调绑定：状态变更（暂不更新面板）
    state_machine_.on_state_change([this](gonxun::TaskState old_state, gonxun::TaskState new_state) {
        Q_UNUSED(old_state)
        Q_UNUSED(new_state)
    });

    state_machine_.on_progress_update([this](const gonxun::TaskProgress& p) {
        Q_UNUSED(p)
    });

    // 绑定运动控制器信号（段级 pace 驱动）
    if (motion_controller_) {
        connect(motion_controller_, &MotionController::segment_completed,
                this, &SimulationController::on_segment_completed);
        connect(motion_controller_, &MotionController::path_completed,
                this, &SimulationController::on_path_completed);
        connect(motion_controller_, &MotionController::grab_completed,
                this, &SimulationController::on_grab_completed);
        connect(motion_controller_, &MotionController::motion_error,
                this, &SimulationController::on_motion_error);
    }
}

/// @brief 启动仿真：检查前置条件，初始化导航序列与状态机，开始第一段。
bool SimulationController::start(const QString& task_code)
{
    if (running_) return false;

    if (map_widget_.selected_start_zone() < 0) {
        phase_ = SimPhase::ERROR;
        emit phase_changed(phase_, "错误：未选择启停区");
        return false;
    }
    if (!map_widget_.is_robot_visible()) return false;

    task_code_ = task_code;
    running_ = true;
    current_segment_ = 0;
    current_cycle_ = 0;
    total_distance_ = 0.0;
    total_steps_ = 0;
    total_timer_.start();

    nav_sequence_ = build_nav_sequence();

    // 状态机快速推进到 Mission 状态
    state_machine_.set_task_code(task_code.toStdString());
    state_machine_.set_total_cycles(total_cycles_);
    state_machine_.set_total_materials(3);
    state_machine_.reset();
    (void)state_machine_.handle_event(gonxun::TaskEvent::START_MARKING);
    (void)state_machine_.handle_event(gonxun::TaskEvent::MARKING_DONE);
    (void)state_machine_.handle_event(gonxun::TaskEvent::START_MISSION);

    emit simulation_started();
    start_next_segment();
    return true;
}

/// @brief 停止仿真：重置所有运行状态，停止定时器，发射失败完成信号。
void SimulationController::stop() noexcept
{
    running_ = false;
    if (dwell_timer_) dwell_timer_->stop();
    if (motion_controller_) motion_controller_->clear_queue();
    phase_ = SimPhase::IDLE;
    current_segment_ = 0;
    emit simulation_finished(false);
}

/// @brief 构建导航序列：每循环包含扫码区→原料区→粗加工区→暂存区，
///        末尾添加返回启停区。
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

    plan_current_segment();
}

/// @brief 为当前段执行BFS路径规划，找到路径后按方向分段下发。
void SimulationController::plan_current_segment()
{
    const NavTarget& target = nav_sequence_[current_segment_];
    QPointF current_pos = map_widget_.robot_pos();

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

    const int dx[] = {1, -1, 0, 0};
    const int dy[] = {0, 0, 1, -1};

    bool is_return_to_start = target.name.contains("启停区");
    int selected_start_y = (map_widget_.selected_start_zone() == 0) ? 0 : 4;

    auto can_enter = [&](int gx, int gy) -> bool {
        if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE) return false;
        bool is_start_zone_cell = (gx == 0 && (gy == 0 || gy == 4));
        if (is_start_zone_cell) {
            if (is_return_to_start && gx == 0 && gy == selected_start_y) {
                return true;
            }
            return false;
        }
        if (map_widget_.has_obstacle_in_cell(gx, gy)) return false;
        return true;
    };

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

    // 计算每段终点格子坐标（供 on_segment_completed 推进机器人位置）
    // 按方向变化切分 grid_seq，每段终点 = 方向改变前的最后一个格子
    segment_endpoints_.clear();
    if (grid_seq.size() >= 2) {
        int prev_dx = grid_seq[1].first - grid_seq[0].first;
        int prev_dy = grid_seq[1].second - grid_seq[0].second;
        for (int i = 2; i < grid_seq.size(); ++i) {
            int cur_dx = grid_seq[i].first - grid_seq[i-1].first;
            int cur_dy = grid_seq[i].second - grid_seq[i-1].second;
            if (!gonxun::same_direction(prev_dx, prev_dy, cur_dx, cur_dy)) {
                segment_endpoints_.append(grid_seq[i-1]);
                prev_dx = cur_dx;
                prev_dy = cur_dy;
            }
        }
        segment_endpoints_.append(grid_seq.last());
    }

    // 在地图上显示完整路径
    QVector<QPointF> grid_path;
    for (const auto& [gx, gy] : grid_seq) {
        grid_path.append(map_widget_.get_cell_center(gx, gy));
    }
    map_widget_.set_path(grid_path);

    // 通过串口下发步进移动指令（MotionController 会按段 stop-and-wait）
    if (motion_controller_) {
        motion_controller_->execute_grid_path(grid_seq, 0);
    }

    phase_ = SimPhase::WAITING_MOVE_DONE;
    emit phase_changed(phase_, "等待移动完成");
}

/// @brief 收到一段移动完成：把机器人跳到该段终点，箭头指向协议朝向。
void SimulationController::on_segment_completed(int seg_idx)
{
    if (seg_idx < 0 || seg_idx >= segment_endpoints_.size()) return;

    const auto& [gx, gy] = segment_endpoints_[seg_idx];
    QPointF target_pos = map_widget_.get_cell_center(gx, gy);
    QPointF cur_pos = map_widget_.robot_pos();
    double dx = target_pos.x() - cur_pos.x();
    double dy = target_pos.y() - cur_pos.y();
    double distance = std::sqrt(dx*dx + dy*dy);
    total_distance_ += distance;
    total_steps_++;

    // 协议朝向直接作为 GUI 角度：0=左, 90=下, 180=右, 270=上
    double gui_angle = motion_controller_ ? static_cast<double>(motion_controller_->heading()) : 90.0;
    map_widget_.set_robot_pos(target_pos, gui_angle);
}

/// @brief 整段路径全部完成：判断是否需要抓取。
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

/// @brief 抓取完成：进入驻留。
void SimulationController::on_grab_completed()
{
    if (!running_) return;
    start_dwelling();
}

/// @brief 运动错误：停止仿真。
void SimulationController::on_motion_error(const QString& err)
{
    phase_ = SimPhase::ERROR;
    emit phase_changed(phase_, QString("运动错误: %1").arg(err));
    stop();
}

/// @brief 判断当前导航目标是否需要抓取（原料区/粗加工区）。
bool SimulationController::current_target_needs_grab() const
{
    if (current_segment_ >= nav_sequence_.size()) return false;
    const QString& name = nav_sequence_[current_segment_].name;
    return name.contains("原料区") || name.contains("粗加工");
}

/// @brief 切换至DWELLING阶段：延迟dwell_time_后推进到下一段。
void SimulationController::start_dwelling()
{
    phase_ = SimPhase::DWELLING;
    emit phase_changed(phase_, "到达目标");
    dwell_timer_->start(dwell_time_);
}

/// @brief 当前导航段完成处理：推进状态机事件、清除路径、递增段索引。
void SimulationController::on_segment_complete()
{
    const NavTarget& target = nav_sequence_[current_segment_];

    if (target.name.contains("扫码区")) {
        (void)state_machine_.handle_event(gonxun::TaskEvent::REACHED_QR);
        (void)state_machine_.handle_event(gonxun::TaskEvent::QR_SCANNED);
        map_widget_.set_task_code(task_code_);
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
    if (dwell_timer_) dwell_timer_->stop();
    emit phase_changed(phase_, "全部完成");
    emit simulation_finished(true);
}
