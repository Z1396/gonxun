/**
 * @file task_state_machine.hpp
 * @brief 任务状态机，管理机器人从标记到完成的全流程。
 *
 * 使用 std::variant 表达互斥状态，符合 Talos 规范。
 * 每个状态是一个独立的 struct，可携带状态数据。
 */

#pragma once

#include "common_types.hpp"

#include <chrono>
#include <functional>
#include <string>
#include <variant>

namespace gonxun {

// ==== 任务状态（std::variant 表达互斥状态） ====

/// 空闲状态
struct Idle {};

/// 标记中状态
struct Marking {};

/// 就绪状态
struct Ready {};

/// 前往 QR 扫码区
struct MovingToQr {};

/// 扫描 QR 码中
struct ScanningQr {};

/// QR 扫码完成
struct QrDone {};

/// 前往物料区
struct MovingToMaterial {};

/// 取料中
struct PickingMaterial {
    int picked{0};  ///< 已取料数量
};

/// 取料完成
struct MaterialDone {};

/// 前往粗加工区
struct MovingToProcess {};

/// 放料中
struct PlacingMaterial {
    int placed{0};  ///< 已放料数量
};

/// 放料完成
struct ProcessDone {};

/// 前往暂存区
struct MovingToBuffer {};

/// 暂存完成
struct BufferDone {};

/// 准备下一轮循环
struct CycleRepeat {};

/// 返回启停区
struct Returning {};

/// 任务完成
struct Completed {};

/// 异常状态（携带错误信息）
struct Error {
    std::string message;  ///< 错误信息
};

/// 任务状态类型
using TaskState = std::variant<
    Idle, Marking, Ready,
    MovingToQr, ScanningQr, QrDone,
    MovingToMaterial, PickingMaterial, MaterialDone,
    MovingToProcess, PlacingMaterial, ProcessDone,
    MovingToBuffer, BufferDone, CycleRepeat,
    Returning, Completed, Error>;

// ==== 任务事件枚举 ====

/**
 * @brief 任务事件枚举，触发状态转移的输入事件。
 */
enum class TaskEvent {
    START_MARKING,
    MARKING_DONE,
    START_MISSION,
    REACHED_QR,
    QR_SCANNED,
    REACHED_MATERIAL,
    MATERIAL_PICKED,
    REACHED_PROCESS,
    MATERIAL_PLACED,
    REACHED_BUFFER,
    BUFFER_DONE,
    CYCLE_START,
    REACHED_START,
    ALL_DONE,
    ERROR_OCCURRED,
    RESET,
    EMERGENCY_STOP
};

// ==== 任务进度信息 ====

/**
 * @brief 任务进度信息，供 UI 显示和日志记录。
 */
struct TaskProgress {
    int current_cycle{0};           ///< 当前循环轮次
    int total_cycles{2};            ///< 总循环轮次
    int materials_picked{0};        ///< 已取料数量
    int materials_placed{0};        ///< 已放料数量
    int total_materials{3};         ///< 单轮总物料数
    std::string task_code;          ///< 任务码
    std::string state_description;  ///< 状态中文描述
};

// ==== 回调函数类型 ====

using StateChangeCallback = std::function<void(const TaskState& old_state, const TaskState& new_state)>;
using ProgressUpdateCallback = std::function<void(const TaskProgress&)>;

// ==== 任务状态机 ====

/**
 * @brief 任务状态机，事件驱动的状态转移管理。
 *
 * 使用 std::variant 表达互斥状态，通过 std::visit 穷尽处理状态转移。
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
    [[nodiscard]] const TaskState& get_current_state() const { return current_state_; }
    /// 获取进度信息
    [[nodiscard]] const TaskProgress& get_progress() const { return progress_; }

    /// 将状态转换为中文描述
    [[nodiscard]] static std::string state_to_string(const TaskState& state);

    /// 判断状态机是否处于活跃状态
    [[nodiscard]] bool is_active() const;

    /// 设置总循环轮次
    void set_total_cycles(int cycles) { progress_.total_cycles = cycles; }
    /// 设置单轮总物料数
    void set_total_materials(int count) { progress_.total_materials = count; }
    /// 设置任务码
    void set_task_code(const std::string& code) { progress_.task_code = code; }

    /// 注册状态变更回调
    void on_state_change(StateChangeCallback cb) { state_change_cb_ = std::move(cb); }
    /// 注册进度更新回调
    void on_progress_update(ProgressUpdateCallback cb) { progress_cb_ = std::move(cb); }

    /// 重置状态机到 IDLE
    void reset();

private:
    /// 转移到新状态并触发回调
    void transition_to(TaskState new_state);

    /// 通知进度更新
    void notify_progress();

    TaskState current_state_;              ///< 当前状态
    TaskProgress progress_;                ///< 进度信息
    StateChangeCallback state_change_cb_;  ///< 状态变更回调
    ProgressUpdateCallback progress_cb_;   ///< 进度更新回调
};

} // namespace gonxun