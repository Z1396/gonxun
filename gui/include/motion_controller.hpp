/// @file motion_controller.hpp
/// @brief 四轮底盘运动控制器，负责格子路径分解为步进指令、指令队列管理
///        与超时重传机制。通过 SerialComm 将 MotionFrame 下发至下位机，
///        并解析 ACK/NACK/状态报告等应答帧。

#pragma once

#include "motion_protocol.hpp"
#include "serial_comm.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QQueue>
#include <QTimer>

class CourtMapWidget;

namespace gonxun {

/// @brief 运动指令状态枚举，描述单条指令在生命周期中的状态。
enum class MotionCmdState {
    PENDING,    ///< 已入队等待发送
    SENT,       ///< 已发送等待应答
    ACKED,      ///< 已收到正确应答
    TIMEOUT,    ///< 超时未收到应答
    NACKED,     ///< 收到否定应答
    ERROR       ///< 其他错误
};

/// @brief 运动指令结构，封装帧数据、状态与重试信息。
struct MotionCmd {
    MotionFrame frame;          ///< 运动协议帧
    MotionCmdState state;       ///< 指令当前状态
    uint8_t seq_num;            ///< 序列号（递增，用于追踪）
    int retry_count;            ///< 已重试次数
    QString description;        ///< 人类可读的指令描述
};

} // namespace gonxun

/// @brief 四轮底盘运动控制器，管理指令队列与串口通信。
///
/// 核心流程：execute_grid_path() 将格子路径分解为步进帧序列 →
/// 入队 → send_next_command() 逐条发送 → 等待ACK → 自动发送下一条。
/// 超时未收到ACK时自动重传（最多max_retries_次），超出则报告超时错误。
/// 紧急指令（stop/emergency）直接绕过队列立即发送。
class MotionController : public QObject {
    Q_OBJECT

public:
    /// @brief 构造运动控制器，初始化超时检查定时器。
    /// @param serial_comm 串口通信实例（必须存在）
    /// @param map_widget 地图控件（用于坐标查询，必须存在）
    /// @param parent 父对象
    explicit MotionController(SerialComm& serial_comm,
                               CourtMapWidget& map_widget,
                               QObject* parent = nullptr) noexcept;
    ~MotionController() override = default;

    // ==== 配置 ====

    /// @brief 设置最大重试次数，超时后重传不超过此次数。
    /// @param retries 重试次数，默认3
    void set_max_retries(int retries) noexcept { max_retries_ = retries; }

    /// @brief 设置指令超时时间。
    /// @param ms 超时毫秒数，默认500
    void set_command_timeout(int ms) noexcept { cmd_timeout_ms_ = ms; }

    /// @brief 设置默认移动速度。
    /// @param speed 速度值(mm/s)，默认300
    void set_default_speed(uint16_t speed) noexcept { default_speed_ = speed; }

    /// @brief 设置默认加速度。
    /// @param accel 加速度值(mm/s²)，默认500
    void set_default_accel(uint16_t accel) noexcept { default_accel_ = accel; }

    // ==== 运动控制 ====

    /// @brief 执行格子路径：分解为步进帧序列并入队发送。
    /// @param grid_path 格子坐标路径 [(x0,y0), (x1,y1), ...]
    /// @param start_angle 起始朝向角度(°)，当前未使用
    void execute_grid_path(const QVector<QPair<int, int>>& grid_path, int start_angle = 0);

    /// @brief 发送步进移动指令（方向+步数）。
    /// @param direction 方向编码（DIR_LEFT/RIGHT/UP/DOWN）
    /// @param steps 步数，默认1
    void send_step_move(uint8_t direction, uint8_t steps = 1);

    /// @brief 发送绝对位置移动指令。
    /// @param target_x 目标X坐标(mm)
    /// @param target_y 目标Y坐标(mm)
    /// @param grid_x 格子X坐标
    /// @param grid_y 格子Y坐标
    void send_position_move(uint16_t target_x, uint16_t target_y,
                            uint8_t grid_x = 0, uint8_t grid_y = 0);

    /// @brief 发送停止指令（立即，不入队）。
    void send_stop();

    /// @brief 发送紧急停止指令（立即，不入队）。
    void send_emergency();

    /// @brief 发送速度设置指令（立即，不入队）。
    /// @param speed 目标速度(mm/s)
    void send_set_speed(uint16_t speed);

    /// @brief 查询下位机状态报告（立即发送）。
    void query_status();

    // ==== 队列管理 ====

    /// @brief 清空指令队列并重置当前指令状态。
    void clear_queue();

    /// @brief 获取队列中待发送指令数量。
    /// @return 队列长度
    [[nodiscard]] int queue_size() const noexcept { return cmd_queue_.size(); }

    /// @brief 查询控制器是否正在等待应答。
    /// @return true 当前指令已发送等待ACK
    [[nodiscard]] bool is_busy() const noexcept { return current_cmd_.state == gonxun::MotionCmdState::SENT; }

    // ==== 状态查询 ====

    /// @brief 获取最近一次状态报告数据。
    /// @return 状态报告常引用
    [[nodiscard]] const gonxun::StatusReportData& last_status() const noexcept { return last_status_; }

    /// @brief 获取当前序列号。
    /// @return 序列号
    [[nodiscard]] uint8_t current_seq_num() const noexcept { return seq_num_; }

signals:
    /// @brief 指令发送时发射。
    /// @param description 指令描述
    void command_sent(const QString& description);

    /// @brief 收到正确应答时发射。
    /// @param description 指令描述
    void command_acked(const QString& description);

    /// @brief 收到否定应答时发射。
    /// @param description 指令描述
    /// @param reason NACK原因码
    void command_nacked(const QString& description, uint8_t reason);

    /// @brief 指令超时（重试耗尽）时发射。
    /// @param description 指令描述
    void command_timeout(const QString& description);

    /// @brief 收到下位机状态报告时发射。
    /// @param status 状态报告数据
    void status_received(const gonxun::StatusReportData& status);

    /// @brief 队列中所有指令执行完毕时发射。
    void all_commands_completed();

    /// @brief 运动错误时发射（超时/下位机错误等）。
    /// @param error 错误描述
    void motion_error(const QString& error);

private slots:
    /// @brief 超时检查定时器回调，每50ms检查当前指令是否超时。
    void on_timeout_check();

private:
    /// @brief 从队列取出下一条指令发送，队列为空时发射all_commands_completed()。
    void send_next_command();

    /// @brief 通过串口发送帧数据。
    /// @param frame 待发送的运动协议帧
    void transmit_frame(const gonxun::MotionFrame& frame);

    /// @brief 解析下位机返回的帧数据，处理ACK/NACK/状态报告/错误。
    /// @param data 原始字节数据
    void process_received_frame(const std::vector<uint8_t>& data);

    /// @brief 重传当前指令（递增retry_count_），超出最大次数时放弃。
    void retry_current_command();

    /// @brief 将格子路径分解为步进帧序列。
    ///        相邻格子对通过calc_move_between_grids()计算方向与步数。
    /// @param grid_path 格子坐标路径
    /// @param start_angle 起始朝向角度
    /// @return 步进帧列表
    [[nodiscard]] QVector<gonxun::MotionFrame> decompose_grid_path(
        const QVector<QPair<int, int>>& grid_path, int start_angle);

    /// @brief 计算从一格到相邻格的移动方向与步数。
    /// @param from_x 起点格子X
    /// @param from_y 起点格子Y
    /// @param to_x 终点格子X
    /// @param to_y 终点格子Y
    /// @return (方向编码, 步数)，步数为0表示无需移动
    [[nodiscard]] QPair<uint8_t, uint8_t> calc_move_between_grids(
        int from_x, int from_y, int to_x, int to_y);

    // ==== 成员变量 ====
    SerialComm& serial_comm_;           ///< 串口通信实例
    CourtMapWidget& map_widget_;        ///< 地图控件

    QQueue<gonxun::MotionCmd> cmd_queue_;      ///< 待发送指令队列
    gonxun::MotionCmd current_cmd_;             ///< 当前正在执行的指令
    uint8_t seq_num_ = 0;                       ///< 全局递增序列号

    int max_retries_ = 3;               ///< 最大重试次数
    int cmd_timeout_ms_ = 500;          ///< 指令超时时间(ms)
    uint16_t default_speed_ = 300;      ///< 默认移动速度(mm/s)
    uint16_t default_accel_ = 500;      ///< 默认加速度(mm/s²)

    QTimer* timeout_timer_;             ///< 超时检查定时器（50ms周期）
    QElapsedTimer cmd_elapsed_timer_;   ///< 当前指令已发送耗时计时器

    gonxun::StatusReportData last_status_{};    ///< 最近一次状态报告
    bool waiting_ack_ = false;                  ///< 是否正在等待应答
};
