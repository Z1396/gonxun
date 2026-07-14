/**
 * 任务状态机模块
 * 统筹机器人完整工作流程：标记→扫码→取料→放料→暂存→循环→返回
 */
#pragma once

#include <string>
#include <functional>
#include <chrono>
#include "astar_planner.hpp"

namespace gonxun {

// 任务状态枚举
enum class TaskState {
    IDLE,               // 空闲等待
    MARKING,            // GUI 标记阶段（障碍物+启停区）
    READY,              // 标记完成，准备出发
    MOVING_TO_QR,       // 前往扫码区
    SCANNING_QR,        // 扫码中
    QR_DONE,            // 扫码完成
    MOVING_TO_MATERIAL, // 前往物料区
    PICKING_MATERIAL,   // 取料中
    MATERIAL_DONE,      // 取料完成
    MOVING_TO_PROCESS,  // 前往粗加工区
    PLACING_MATERIAL,   // 放料中
    PROCESS_DONE,       // 放料完成
    MOVING_TO_BUFFER,   // 前往暂存区
    BUFFER_DONE,        // 暂存完成
    CYCLE_REPEAT,       // 循环重复
    RETURNING,          // 返回启停区
    COMPLETED,          // 任务全部完成
    ERROR               // 异常状态
};

// 任务事件枚举（触发状态转换的信号）
enum class TaskEvent {
    START_MARKING,        // 开始标记
    MARKING_DONE,         // 标记完成
    START_MISSION,        // 开始任务（出发）
    REACHED_QR,           // 到达扫码区
    QR_SCANNED,           // 扫码完成
    REACHED_MATERIAL,     // 到达物料区
    MATERIAL_PICKED,      // 取料完成
    REACHED_PROCESS,      // 到达粗加工区
    MATERIAL_PLACED,      // 放料完成
    REACHED_BUFFER,       // 到达暂存区
    BUFFER_DONE,          // 暂存完成
    CYCLE_START,          // 开始新一轮循环
    REACHED_START,        // 返回启停区
    ALL_DONE,             // 全部完成
    ERROR_OCCURRED,       // 异常发生
    RESET,                // 重置
    EMERGENCY_STOP        // 急停
};

// 流程进度信息
struct TaskProgress {
    TaskState currentState;        // 当前状态
    int currentCycle;              // 当前循环次数（第几轮）
    int totalCycles;               // 总循环次数
    int materialsPicked;           // 已取物料数
    int materialsPlaced;           // 已放物料数
    int totalMaterials;            // 总物料数
    std::string taskCode;          // 任务码（扫码获得）
    std::string stateDescription;  // 状态描述文字
    std::string errorMessage;      // 错误信息
};

// 状态变更回调函数类型
using StateChangeCallback = std::function<void(TaskState oldState, TaskState newState)>;
// 进度更新回调
using ProgressUpdateCallback = std::function<void(const TaskProgress&)>;
// 路径请求回调（状态机请求路径规划）
using PathRequestCallback = std::function<Path(const Point& start, const Point& goal)>;

/**
 * 任务状态机
 */
class TaskStateMachine {
public:
    TaskStateMachine();
    ~TaskStateMachine() = default;

    /**
     * 处理事件（触发状态转换）
     * @param event 任务事件
     * @return 转换后的新状态
     */
    TaskState handleEvent(TaskEvent event);

    /**
     * 获取当前状态
     */
    TaskState getCurrentState() const { return m_currentState; }

    /**
     * 获取进度信息
     */
    const TaskProgress& getProgress() const { return m_progress; }

    /**
     * 设置总循环次数
     */
    void setTotalCycles(int cycles) { m_progress.totalCycles = cycles; }

    /**
     * 设置总物料数
     */
    void setTotalMaterials(int count) { m_progress.totalMaterials = count; }

    /**
     * 设置任务码
     */
    void setTaskCode(const std::string& code) { m_progress.taskCode = code; }

    /**
     * 设置错误信息
     */
    void setError(const std::string& msg) { m_progress.errorMessage = msg; }

    /**
     * 注册状态变更回调
     */
    void onStateChange(StateChangeCallback cb) { m_stateChangeCb = std::move(cb); }

    /**
     * 注册进度更新回调
     */
    void onProgressUpdate(ProgressUpdateCallback cb) { m_progressCb = std::move(cb); }

    /**
     * 注册路径请求回调
     */
    void onPathRequest(PathRequestCallback cb) { m_pathRequestCb = std::move(cb); }

    /**
     * 获取状态描述文字
     */
    static std::string stateToString(TaskState state);

    /**
     * 检查是否处于可执行状态（非IDLE/ERROR/COMPLETED）
     */
    bool isActive() const;

    /**
     * 检查是否处于异常状态
     */
    bool isError() const { return m_currentState == TaskState::ERROR; }

    /**
     * 重置状态机到 IDLE
     */
    void reset();

private:
    // 执行状态转换
    void transitionTo(TaskState newState);
    // 通知进度更新
    void notifyProgress();
    // 获取当前目标点
    Point getCurrentTarget() const;

    TaskState m_currentState;
    TaskProgress m_progress;

    StateChangeCallback m_stateChangeCb;
    ProgressUpdateCallback m_progressCb;
    PathRequestCallback m_pathRequestCb;
};

} // namespace gonxun