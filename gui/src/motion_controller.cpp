/// @file motion_controller.cpp
/// @brief 底盘运动控制器实现：BFS 路径分段、事件驱动段队列、看门狗超时保护。

#include "motion_controller.hpp"
#include "courtmapwidget.hpp"

#include <QMetaObject>

/// @brief 构造运动控制器：注册串口回调，初始化看门狗定时器。
MotionController::MotionController(SerialComm& serial_comm,
                                     CourtMapWidget& map_widget,
                                     QObject* parent) noexcept
    : QObject(parent),
      serial_comm_(serial_comm),
      map_widget_(map_widget)
{
    // 注册串口回调：跨线程 marshal 到本对象线程（QueuedConnection）
    serial_comm_.set_move_done_callback([this]() {
        QMetaObject::invokeMethod(this, "on_move_done", Qt::QueuedConnection);
    });
    // 注册串口回调：跨线程 marshal 到本对象线程（QueuedConnection）
    serial_comm_.set_grab_done_callback([this]() {
        QMetaObject::invokeMethod(this, "on_grab_done", Qt::QueuedConnection);
    });

    // 看门狗：单次触发，超时即报错中止
    watchdog_timer_ = new QTimer(this);
    watchdog_timer_->setSingleShot(true);
    watchdog_timer_->setInterval(watchdog_ms_);
    connect(watchdog_timer_, &QTimer::timeout, this, &MotionController::on_watchdog_timeout);
}

// ==== 路径执行 ====

/// @brief 执行格子路径：按方向分段并入队，发送第一段。
void MotionController::execute_grid_path(
    const QVector<QPair<int, int>>& grid_path, int /*start_angle*/)
{
    clear_queue();
    current_heading_ = 90;  // 每次任务开始重置为初始朝向（向下）

    auto segments = gonxun::segment_grid_path(grid_path);
    for (const auto& seg : segments) {
        segment_queue_.enqueue(seg);
    }

    send_next_segment();
}

/// @brief 发送纯抓取指令，等待 grab_done。
void MotionController::send_grab()
{
    grab_in_progress_ = true;
    waiting_done_ = true;
    serial_comm_.send_grab_frame();
    start_watchdog();
}

/// @brief 发送视觉定位帧，等待 grab_done（复用 grab 看门狗机制）。
void MotionController::send_locate(uint16_t x, uint16_t y, uint8_t grab)
{
    grab_in_progress_ = true;
    waiting_done_ = true;
    serial_comm_.send_locate_frame(x, y, grab);
    start_watchdog();
}

// ==== 队列管理 ====

/// @brief 清空段队列并重置状态。
void MotionController::clear_queue()
{
    segment_queue_.clear();
    current_seg_idx_ = 0;
    waiting_done_ = false;
    grab_in_progress_ = false;
    stop_watchdog();
}

/// @brief 取下一段发送；队列为空则发射 path_completed()。
void MotionController::send_next_segment()
{
    if (segment_queue_.isEmpty()) {
        waiting_done_ = false;
        emit path_completed();
        return;
    }

    gonxun::MoveSegment seg = segment_queue_.dequeue();
    waiting_done_ = true;
    grab_in_progress_ = false;

    // 后退优化：根据当前朝向决定前进/后退，并转换格子数→步进电机步数
    auto [angle, motor_steps] = optimize_move(seg.angle, seg.steps);

    serial_comm_.send_move_frame(angle, motor_steps);
    start_watchdog();

    emit segment_sent(current_seg_idx_, angle, motor_steps);
}

/// @brief 后退优化：对比当前朝向与目标移动方向。
/// @return <实际发送角度, 步进电机步数>
std::pair<uint16_t, int16_t> MotionController::optimize_move(
    uint16_t target_angle, int16_t grid_steps) noexcept
{
    int16_t motor_steps = static_cast<int16_t>(grid_steps * steps_per_grid_);

    // 计算方向差：当前朝向 vs 目标移动方向
    int diff = (static_cast<int>(target_angle) - static_cast<int>(current_heading_) + 360) % 360;

    if (diff == 0) {
        // 方向相同，保持朝向前进
        return {current_heading_, motor_steps};
    } else if (diff == 180) {
        // 方向相反，保持朝向后退（不更新 current_heading_）
        return {current_heading_, -motor_steps};
    } else {
        // 其他方向（90°/270°），转向后前进，更新朝向
        current_heading_ = target_angle;
        return {target_angle, motor_steps};
    }
}

// ==== 事件回调 ====

/// @brief 收到 move_done：停看门狗，发射 segment_completed，发下一段。
void MotionController::on_move_done()
{
    if (!waiting_done_ || grab_in_progress_) return;

    stop_watchdog();
    emit segment_completed(current_seg_idx_);
    current_seg_idx_++;
    send_next_segment();
}

/// @brief 收到 grab_done：停看门狗，发射 grab_completed。
void MotionController::on_grab_done()
{
    if (!grab_in_progress_) return;

    stop_watchdog();
    grab_in_progress_ = false;
    waiting_done_ = false;
    emit grab_completed();
}

// ==== 看门狗 ====

/// @brief 启动看门狗定时器。
void MotionController::start_watchdog()
{
    watchdog_timer_->setInterval(watchdog_ms_);
    watchdog_timer_->start();
}

/// @brief 停止看门狗定时器。
void MotionController::stop_watchdog()
{
    watchdog_timer_->stop();
}

/// @brief 看门狗超时：报错中止，不重发（避免下位机二次移动）。
void MotionController::on_watchdog_timeout()
{
    QString err;
    if (grab_in_progress_) {
        err = QString("抓取看门狗超时（%1ms 无 grab_done）").arg(watchdog_ms_);
    } else {
        err = QString("移动看门狗超时（%1ms 无 move_done）").arg(watchdog_ms_);
    }
    waiting_done_ = false;
    grab_in_progress_ = false;
    emit motion_error(err);
    clear_queue();
}
