/**
 * @file task_state_machine.hpp
 * @brief 任务状态机，管理机器人从标记到完成的全流程。
 *
 * 定义 16 种任务状态与 15 种事件，通过事件驱动实现状态转移，
 * 支持多轮循环取放料、QR 扫描、紧急停止等流程。
 * 提供状态变更回调、进度更新回调和路径请求回调。
 */

#pragma once

#include "common_types.hpp"

#include <chrono>
#include <functional>
#include <string>

namespace gonxun {

// ==== 任务状态枚举 ====

/**
 * @brief 任务状态枚举，描述机器人任务的完整生命周期。
 *
 * 状态流转：IDLE → MARKING → READY → MOVING_TO_QR → SCANNING_QR →
 * QR_DONE → MOVING_TO_MATERIAL → PICKING_MATERIAL → MATERIAL_DONE →
 * MOVING_TO_PROCESS → PLACING_MATERIAL → PROCESS_DONE →
 * MOVING_TO_BUFFER → BUFFER_DONE → (CYCLE_REPEAT | RETURNING) → COMPLETED
 */
enum class TaskState {
    IDLE,               ///< 空闲，等待标记启动
    MARKING,            ///< 标记中，机器人初始位置标记
    READY,              ///< 就绪，等待任务开始
    MOVING_TO_QR,       ///< 前往 QR 扫码区
    SCANNING_QR,        ///< 扫描 QR 码中
    QR_DONE,            ///< QR 扫码完成
    MOVING_TO_MATERIAL, ///< 前往物料区
    PICKING_MATERIAL,   ///< 取料中
    MATERIAL_DONE,      ///< 取料完成
    MOVING_TO_PROCESS,  ///< 前往粗加工区
    PLACING_MATERIAL,   ///< 放料中
    PROCESS_DONE,       ///< 放料完成
    MOVING_TO_BUFFER,   ///< 前往暂存区
    BUFFER_DONE,        ///< 暂存完成
    CYCLE_REPEAT,       ///< 准备下一轮循环
    RETURNING,          ///< 返回启停区
    COMPLETED,          ///< 任务完成
    ERROR               ///< 异常状态
};

// ==== 任务事件枚举 ====

/**
 * @brief 任务事件枚举，触发状态转移的输入事件。
 */
enum class TaskEvent {
    START_MARKING,    ///< 启动标记
    MARKING_DONE,     ///< 标记完成
    START_MISSION,    ///< 开始任务
    REACHED_QR,       ///< 到达 QR 区
    QR_SCANNED,       ///< QR 扫描完成
    REACHED_MATERIAL, ///< 到达物料区
    MATERIAL_PICKED,  ///< 取料完成
    REACHED_PROCESS,  ///< 到达粗加工区
    MATERIAL_PLACED,  ///< 放料完成
    REACHED_BUFFER,   ///< 到达暂存区
    BUFFER_DONE,      ///< 暂存完成
    CYCLE_START,      ///< 开始新一轮循环
    REACHED_START,    ///< 返回启停区
    ALL_DONE,         ///< 全部任务完成
    ERROR_OCCURRED,   ///< 发生错误
    RESET,            ///< 重置状态机
    EMERGENCY_STOP    ///< 紧急停止
};

// ==== 任务进度信息 ====

/**
 * @brief 任务进度信息，供 UI 显示和日志记录。
 */
struct TaskProgress {
    TaskState current_state;     ///< 当前状态
    int current_cycle;           ///< 当前循环轮次（从 1 开始）
    int total_cycles;            ///< 总循环轮次
    int materials_picked;        ///< 已取料数量
    int materials_placed;        ///< 已放料数量
    int total_materials;         ///< 单轮总物料数
    std::string task_code;       ///< 任务码
    std::string state_description;///< 状态中文描述
    std::string error_message;   ///< 错误信息
};

// ==== 回调函数类型 ====

/// 状态变更回调，参数为 (旧状态, 新状态)
using StateChangeCallback = std::function<void(TaskState old_state, TaskState new_state)>;
/// 进度更新回调，参数为当前进度快照
using ProgressUpdateCallback = std::function<void(const TaskProgress&)>;
/// 路径请求回调，参数为 (起点, 终点)，返回规划路径
using PathRequestCallback = std::function<Path(const Point& start, const Point& goal)>;

// ==== 任务状态机 ====

/**
 * @brief 任务状态机，事件驱动的状态转移管理。
 *
 * 每个 TaskState 对应一个 handle_xxx_event 方法，根据 TaskEvent 决定转移目标。
 * 支持回调通知外部状态变更和进度更新，可配置循环轮次与物料数。
 */
class TaskStateMachine {
public:
    TaskStateMachine();
    ~TaskStateMachine() = default;

    /**
     * @brief 处理事件，执行状态转移。
     * @param event 输入事件
     * @return 转移后的当前状态
     */
    [[nodiscard]] TaskState handle_event(TaskEvent event);

    /// 获取当前状态
    [[nodiscard]] TaskState get_current_state() const { return current_state_; }
    /// 获取进度信息
    [[nodiscard]] const TaskProgress& get_progress() const { return progress_; }
    /**
     * @brief 将状态枚举转换为中文描述。
     * @param state 任务状态
     * @return 中文状态名称
     */
    [[nodiscard]] static std::string state_to_string(TaskState state);
    /// 判断状态机是否处于活跃状态（非 IDLE/COMPLETED/ERROR）
    [[nodiscard]] bool is_active() const;
    /// 判断是否处于异常状态
    [[nodiscard]] bool is_error() const { return current_state_ == TaskState::ERROR; }

    /// 设置总循环轮次
    void set_total_cycles(int cycles) { progress_.total_cycles = cycles; }
    /// 设置单轮总物料数
    void set_total_materials(int count) { progress_.total_materials = count; }
    /// 设置任务码
    void set_task_code(const std::string& code) { progress_.task_code = code; }
    /// 设置错误信息
    void set_error(const std::string& msg) { progress_.error_message = msg; }

    /// 注册状态变更回调
    void on_state_change(StateChangeCallback cb) { state_change_cb_ = std::move(cb); }
    /// 注册进度更新回调
    void on_progress_update(ProgressUpdateCallback cb) { progress_cb_ = std::move(cb); }
    /// 注册路径请求回调
    void on_path_request(PathRequestCallback cb) { path_request_cb_ = std::move(cb); }

    /// 重置状态机到 IDLE
    void reset();

private:
    /// 转移到新状态并触发回调
    void transition_to(TaskState new_state);
    /// 通知进度更新
    void notify_progress();
    /// 获取当前目标点（占位，返回原点）
    [[nodiscard]] Point get_current_target() const;

    // ---- 各状态的 event handler ----
    void handle_idle_event(TaskEvent event);
    void handle_marking_event(TaskEvent event);
    void handle_ready_event(TaskEvent event);
    void handle_moving_to_qr_event(TaskEvent event);
    void handle_scanning_qr_event(TaskEvent event);
    void handle_moving_to_material_event(TaskEvent event);
    void handle_picking_material_event(TaskEvent event);
    void handle_moving_to_process_event(TaskEvent event);
    void handle_placing_material_event(TaskEvent event);
    void handle_moving_to_buffer_event(TaskEvent event);
    void handle_buffer_done_event(TaskEvent event);
    void handle_cycle_repeat_event(TaskEvent event);
    void handle_returning_event(TaskEvent event);
    void handle_error_event(TaskEvent event);

    TaskState current_state_;              ///< 当前状态
    TaskProgress progress_;                ///< 进度信息
    StateChangeCallback state_change_cb_;  ///< 状态变更回调
    ProgressUpdateCallback progress_cb_;   ///< 进度更新回调
    PathRequestCallback path_request_cb_;  ///< 路径请求回调
};

} // namespace gonxun
