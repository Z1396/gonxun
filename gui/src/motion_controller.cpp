/// @file motion_controller.cpp
/// @brief 四轮底盘运动控制器实现，包含格子路径分解为步进指令、
///        指令队列逐条发送与超时重传、以及下位机应答帧解析。

#include "motion_controller.hpp"
#include "courtmapwidget.hpp"
#include "config_loader.hpp"

#include <QMetaObject>

/// @brief 构造运动控制器：初始化当前指令状态，启动超时检查定时器(50ms周期)。
MotionController::MotionController(SerialComm& serial_comm,
                                     CourtMapWidget& map_widget,
                                     QObject* parent) noexcept
    : QObject(parent)
    , serial_comm_(serial_comm)
    , map_widget_(map_widget)
{
    // 从配置文件加载运动参数
    const auto& cfg = gonxun::ConfigLoader::instance().config();
    default_speed_ = static_cast<uint16_t>(cfg.motion.default_speed);
    default_accel_ = static_cast<uint16_t>(cfg.motion.default_accel);
    max_retries_ = cfg.motion.max_retries;
    cmd_timeout_ms_ = cfg.motion.command_timeout;

    current_cmd_.state = gonxun::MotionCmdState::PENDING;
    current_cmd_.retry_count = 0;
    current_cmd_.seq_num = 0;

    timeout_timer_ = new QTimer(this);
    timeout_timer_->setInterval(50);
    connect(timeout_timer_, &QTimer::timeout, this, &MotionController::on_timeout_check);
    timeout_timer_->start();
} 

// ==== 路径分解 ====

/// @brief 将格子路径分解为步进帧序列。
///        对每对相邻格子调用calc_move_between_grids()计算方向与步数，
///        步数为0的跳过，否则构建步进移动帧。
/// @param grid_path 格子坐标路径 [(x0,y0), (x1,y1), ...]
/// @param start_angle 起始朝向角度（当前未使用，保留扩展）
/// @return 步进帧列表
QVector<gonxun::MotionFrame> MotionController::decompose_grid_path(
    const QVector<QPair<int, int>>& grid_path, int start_angle)
{
    QVector<gonxun::MotionFrame> frames;

    if (grid_path.size() < 2) {
        return frames;
    }

    for (int i = 0; i < grid_path.size() - 1; ++i) {
        int from_x = grid_path[i].first;
        int from_y = grid_path[i].second;
        int to_x = grid_path[i + 1].first;
        int to_y = grid_path[i + 1].second;

        auto [direction, steps] = calc_move_between_grids(from_x, from_y, to_x, to_y);

        if (steps == 0) continue;

        // 使用简化步进命令（速度和加速度由下位机内部配置）
        auto frame = gonxun::build_step_move_simple_frame(direction, steps);
        frames.append(frame);
    }

    return frames;
}

/// @brief 计算从一个格子到相邻格子的移动方向与步数。
///        赛场坐标系下：X增大→DIR_LEFT（向赛场左侧移动），
///        Y增大→DIR_DOWN，Y减小→DIR_UP。
/// @note 方向映射基于赛场坐标系而非格子坐标系，与下位机协议一致。
/// @param from_x 起点格子X
/// @param from_y 起点格子Y
/// @param to_x 终点格子X
/// @param to_y 终点格子Y
/// @return (方向编码, 步数)
QPair<uint8_t, uint8_t> MotionController::calc_move_between_grids(
    int from_x, int from_y, int to_x, int to_y)
{
    int dx = to_x - from_x;
    int dy = to_y - from_y;

    if (dx > 0) {
        return {gonxun::DIR_LEFT, static_cast<uint8_t>(dx)};
    } else if (dx < 0) {
        return {gonxun::DIR_RIGHT, static_cast<uint8_t>(-dx)};
    } else if (dy > 0) {
        return {gonxun::DIR_DOWN, static_cast<uint8_t>(dy)};
    } else if (dy < 0) {
        return {gonxun::DIR_UP, static_cast<uint8_t>(-dy)};
    }

    return {gonxun::DIR_LEFT, 0};
}

// ==== 运动控制 ====

/// @brief 执行格子路径：清空队列，分解路径为步进帧，逐条入队并发送。
/// @param grid_path 格子坐标路径
/// @param start_angle 起始朝向角度
void MotionController::execute_grid_path(
    const QVector<QPair<int, int>>& grid_path, int start_angle)
{
    clear_queue();

    auto frames = decompose_grid_path(grid_path, start_angle);

    for (int i = 0; i < frames.size(); ++i) {
        gonxun::MotionCmd cmd;
        cmd.frame = frames[i];
        cmd.state = gonxun::MotionCmdState::PENDING;
        cmd.seq_num = ++seq_num_;
        cmd.retry_count = 0;
        cmd.description = QString("步进移动#%1").arg(cmd.seq_num);
        cmd_queue_.enqueue(cmd);
    }

    send_next_command();
}

/// @brief 发送步进移动指令：构建帧并入队，空闲时立即发送。
/// @param direction 方向编码
/// @param steps 步数
void MotionController::send_step_move(uint8_t direction, uint8_t steps)
{
    // 使用简化步进命令（速度和加速度由下位机内部配置）
    auto frame = gonxun::build_step_move_simple_frame(direction, steps);

    gonxun::MotionCmd cmd;
    cmd.frame = frame;
    cmd.state = gonxun::MotionCmdState::PENDING;
    cmd.seq_num = ++seq_num_;
    cmd.retry_count = 0;
    cmd.description = QString("步进移动:方向=%1 步数=%2").arg(direction).arg(steps);

    cmd_queue_.enqueue(cmd);

    if (!waiting_ack_) {
        send_next_command();
    }
}

/// @brief 发送绝对位置移动指令：构建帧并入队，空闲时立即发送。
/// @param target_x 目标X(mm)
/// @param target_y 目标Y(mm)
/// @param grid_x 格子X
/// @param grid_y 格子Y
void MotionController::send_position_move(uint16_t target_x, uint16_t target_y,
                                          uint8_t grid_x, uint8_t grid_y)
{
    auto frame = gonxun::build_position_move_frame(target_x, target_y, default_speed_, grid_x, grid_y);

    gonxun::MotionCmd cmd;
    cmd.frame = frame;
    cmd.state = gonxun::MotionCmdState::PENDING;
    cmd.seq_num = ++seq_num_;
    cmd.retry_count = 0;
    cmd.description = QString("位置移动:(%1,%2)").arg(target_x).arg(target_y);

    cmd_queue_.enqueue(cmd);

    if (!waiting_ack_) {
        send_next_command();
    }
}

/// @brief 发送停止指令（立即，绕过队列）。
void MotionController::send_stop()
{
    auto frame = gonxun::build_stop_frame();
    transmit_frame(frame);
    emit command_sent("停止");
}

/// @brief 发送紧急停止指令（立即，绕过队列）。
void MotionController::send_emergency()
{
    auto frame = gonxun::build_emergency_frame();
    transmit_frame(frame);
    emit command_sent("紧急停止");
}

/// @brief 发送速度设置指令（立即，绕过队列）。
/// @param speed 目标速度(mm/s)
void MotionController::send_set_speed(uint16_t speed)
{
    auto frame = gonxun::build_set_speed_frame(speed);
    transmit_frame(frame);
    emit command_sent(QString("设置速度:%1mm/s").arg(speed));
}

/// @brief 查询下位机状态报告（立即发送）。
void MotionController::query_status()
{
    auto frame = gonxun::build_query_status_frame();
    transmit_frame(frame);
}

// ==== 队列管理 ====

/// @brief 清空指令队列，重置等待应答状态与当前指令。
void MotionController::clear_queue()
{
    cmd_queue_.clear();
    waiting_ack_ = false;
    current_cmd_.state = gonxun::MotionCmdState::PENDING;
}

/// @brief 从队列取出下一条指令发送。
///        队列为空时清除等待标志并发射all_commands_completed()。
void MotionController::send_next_command()
{
    if (cmd_queue_.isEmpty()) {
        waiting_ack_ = false;
        emit all_commands_completed();
        return;
    }

    current_cmd_ = cmd_queue_.dequeue();
    current_cmd_.state = gonxun::MotionCmdState::SENT;
    current_cmd_.retry_count = 0;
    waiting_ack_ = true;

    cmd_elapsed_timer_.start();

    transmit_frame(current_cmd_.frame);
    emit command_sent(current_cmd_.description);
}

/// @brief 通过串口发送帧数据。
/// @param frame 待发送的运动协议帧
void MotionController::transmit_frame(const gonxun::MotionFrame& frame)
{
    auto bytes = frame.to_bytes();
    serial_comm_.send_raw_frame(bytes);
}

// ==== 超时与重传 ====

/// @brief 超时检查定时器回调(50ms周期)：检查当前指令是否超时。
///        超时且未超出重试次数→重传；超出→标记TIMEOUT并推进队列。
void MotionController::on_timeout_check()
{
    if (!waiting_ack_) return;
    if (current_cmd_.state != gonxun::MotionCmdState::SENT) return;

    if (cmd_elapsed_timer_.elapsed() > cmd_timeout_ms_) {
        if (current_cmd_.retry_count < max_retries_) {
            // 重传：递增重试计数并重新发送
            current_cmd_.retry_count++;
            cmd_elapsed_timer_.restart();

            transmit_frame(current_cmd_.frame);
            emit command_sent(QString("%1 (重试%2)")
                .arg(current_cmd_.description)
                .arg(current_cmd_.retry_count));
        } else {
            // 重试耗尽：标记超时，发射错误信号，推进下一条
            current_cmd_.state = gonxun::MotionCmdState::TIMEOUT;
            waiting_ack_ = false;

            emit command_timeout(current_cmd_.description);
            emit motion_error(QString("指令超时: %1 (已重试%2次)")
                .arg(current_cmd_.description)
                .arg(max_retries_));

            send_next_command();
        }
    }
}

/// @brief 手动重传当前指令（递增重试计数）。
/// @note 当前仅递增计数，未做最大次数检查，由on_timeout_check()负责上限判定。
void MotionController::retry_current_command()
{
    if (current_cmd_.retry_count < max_retries_) {
        current_cmd_.retry_count++;
        cmd_elapsed_timer_.restart();
        transmit_frame(current_cmd_.frame);
    }
}

// ==== 接收处理 ====

/// @brief 解析下位机返回帧数据，根据命令码分发处理：
///        CMD_ACK→确认当前指令，CMD_STATUS_REPORT→更新状态报告，CMD_ERROR→报告错误。
/// @param data 原始字节数据（至少3字节：帧头+命令码+...）
void MotionController::process_received_frame(const std::vector<uint8_t>& data)
{
    if (data.size() < 3) return;

    uint8_t cmd = data[1];

    switch (cmd) {
    case gonxun::CMD_ACK: {
        if (waiting_ack_) {
            current_cmd_.state = gonxun::MotionCmdState::ACKED;
            waiting_ack_ = false;

            // 解析ACK码：ACK_OK→成功，其他→NACK
            uint8_t ack_code = (data.size() > 3) ? data[3] : gonxun::ACK_OK;
            if (ack_code == gonxun::ACK_OK) {
                emit command_acked(current_cmd_.description);
                send_next_command();
            } else {
                emit command_nacked(current_cmd_.description, ack_code);
                send_next_command();
            }
        }
        break;
    }

    case gonxun::CMD_STATUS_REPORT: {
        // 解析12字节状态报告数据
        if (data.size() >= 15) {
            gonxun::StatusReportData status;
            std::memcpy(&status, &data[3], sizeof(status));
            last_status_ = status;
            emit status_received(status);
        }
        break;
    }

    case gonxun::CMD_ERROR: {
        uint8_t error_code = (data.size() > 3) ? data[3] : 0;
        emit motion_error(QString("下位机错误: 0x%1").arg(error_code, 2, 16, QChar('0')));
        break;
    }

    default:
        break;
    }
}
