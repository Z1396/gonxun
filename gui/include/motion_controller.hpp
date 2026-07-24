/// @file motion_controller.hpp
/// @brief 底盘运动控制器，负责 BFS 路径分段、指令队列与 stop-and-wait 事件驱动。
///
/// 工作流程：execute_grid_path() 将格子路径按方向分段 → 入队 → send_next_segment()
/// 发送一段 move_frame → 等待下位机 move_done → 发下一段 → 全部完成发射 path_completed()。
/// 抓取通过 send_grab() 发送 grab_frame 并等待 grab_done。

#pragma once

#include "motion_protocol.hpp"
#include "serial_comm.hpp"

#include <QObject>
#include <QQueue>
#include <QTimer>

class CourtMapWidget;

/// @brief 底盘运动控制器，基于事件驱动（stop-and-wait）管理段队列与串口通信。
///
/// 核心交互：发送一段 move_frame → 阻塞等待 move_done → 发下一段。
/// 不做超时重传（重发会导致下位机二次移动），仅用看门狗超时报错中止。
class MotionController : public QObject {
    Q_OBJECT

public:
    /// @brief 构造运动控制器，注册串口回调并启动看门狗定时器。
    /// @param serial_comm 串口通信实例（必须存在）
    /// @param map_widget 地图控件（保留扩展用）
    /// @param parent 父对象
    explicit MotionController(gonxun::SerialComm& serial_comm,
                               CourtMapWidget& map_widget,
                               QObject* parent = nullptr) noexcept;
    ~MotionController() override = default;

    // ==== 运动控制 ====

    /// @brief 执行格子路径：按方向分段后入队，自动开始发送第一段。
    /// @param grid_path 格子坐标路径 [(x0,y0), (x1,y1), ...]
    /// @param start_angle 起始朝向角度（保留扩展，当前未使用）
    void execute_grid_path(const QVector<QPair<int, int>>& grid_path, int start_angle = 0);

    /// @brief 发送纯抓取指令，等待 grab_done。
    void send_grab();

    /// @brief 发送视觉定位帧（mode=Locate），等待 grab_done。
    /// @param x 物料 X 坐标 (mm)
    /// @param y 物料 Y 坐标 (mm)
    /// @param grab 抓取指令 0/1
    void send_locate(uint16_t x, uint16_t y, uint8_t grab);

    // ==== 队列管理 ====

    /// @brief 清空指令队列并重置当前段索引。
    void clear_queue();

    /// @brief 获取队列中待发送段数。
    [[nodiscard]] int queue_size() const noexcept { return segment_queue_.size(); }

    /// @brief 查询控制器是否正在等待 move_done/grab_done。
    [[nodiscard]] bool is_busy() const noexcept { return waiting_done_; }

    /// @brief 设置看门狗超时时间（毫秒）。
    /// @param ms 超时毫秒数，默认 5000
    void set_watchdog_timeout(int ms) noexcept { watchdog_ms_ = ms; }

    /// @brief 获取当前机器人朝向（协议角度 0/90/180/270）。
    [[nodiscard]] uint16_t heading() const noexcept { return current_heading_; }

signals:
    /// @brief 一段移动指令发送时发射。
    /// @param seg_idx 段索引
    /// @param angle 段角度 0/90/180/270（真实值）
    /// @param steps 段步数
    void segment_sent(int seg_idx, uint16_t angle, int16_t steps);

    /// @brief 一段移动完成（收到 move_done）时发射。
    /// @param seg_idx 段索引
    void segment_completed(int seg_idx);

    /// @brief 整条路径全部段完成时发射（触发后续抓取或驻留）。
    void path_completed();

    /// @brief 抓取完成（收到 grab_done）时发射。
    void grab_completed();

    /// @brief 运动错误（看门狗超时等）时发射。
    /// @param error 错误描述
    void motion_error(const QString& error);

private slots:
    /// @brief 收到 move_done 回调（由 SerialComm 跨线程 invoke 调用）。
    void on_move_done();
    /// @brief 收到 grab_done 回调（由 SerialComm 跨线程 invoke 调用）。
    void on_grab_done();
    /// @brief 看门狗超时检查：触发 motion_error 并清空队列。
    void on_watchdog_timeout();

private:
    /// @brief 从队列取出下一段发送。队列为空时发射 path_completed()。
    void send_next_segment();

    /// @brief 后退优化：根据当前朝向决定前进/后退。
    /// @param target_angle 目标移动方向（0/90/180/270）
    /// @param grid_steps 格子步数（正整数）
    /// @return <实际发送角度, 步进电机步数（可正可负）>
    [[nodiscard]] std::pair<uint16_t, int16_t> optimize_move(
        uint16_t target_angle, int16_t grid_steps) noexcept;

    /// @brief 启动看门狗定时器。
    void start_watchdog();
    /// @brief 停止看门狗定时器。
    void stop_watchdog();

    gonxun::SerialComm& serial_comm_;             ///< 串口通信实例
    CourtMapWidget& map_widget_;          ///< 地图控件（保留扩展）

    QQueue<gonxun::MoveSegment> segment_queue_;  ///< 待发送段队列
    int current_seg_idx_ = 0;                    ///< 当前段索引
    bool waiting_done_ = false;                  ///< 是否正在等待 move_done/grab_done
    bool grab_in_progress_ = false;              ///< 是否正在等待 grab_done

    QTimer* watchdog_timer_;              ///< 看门狗定时器（单次触发）
    int watchdog_ms_ = 5000;               ///< 看门狗超时（ms）

    uint16_t current_heading_ = 90;       ///< 机器人当前朝向（0/90/180/270）
    int steps_per_grid_ = 480;             ///< 每格对应步进电机步数
};
