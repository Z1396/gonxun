/**
 * @file task_state_machine.hpp
 * @brief 任务状态机模块 - 统筹机器人完整工作流程
 * 
 * @details 本模块实现了基于状态模式的任务流程控制器，用于管理机器人从启动到完成的所有状态转换。
 *          涵盖完整工作流程：标记 → 扫码 → 取料 → 放料 → 暂存 → 循环 → 返回。
 * 
 *          状态机设计原则：
 *          - 单一职责：每个状态只处理特定阶段的任务
 *          - 明确转换：状态之间通过事件触发，转换逻辑清晰
 *          - 可扩展性：新增状态或事件不影响现有代码
 *          - 错误隔离：异常状态独立处理，不影响正常流程
 * 
 *          状态转换图：
 *          @code
 *          IDLE → MARKING → READY
 *             ↓
 *          MOVING_TO_QR → SCANNING_QR → QR_DONE
 *             ↓
 *          MOVING_TO_MATERIAL → PICKING_MATERIAL → MATERIAL_DONE
 *             ↓
 *          MOVING_TO_PROCESS → PLACING_MATERIAL → PROCESS_DONE
 *             ↓
 *          MOVING_TO_BUFFER → BUFFER_DONE
 *             ↓
 *          (循环) → CYCLE_REPEAT → ...
 *             ↓
 *          RETURNING → COMPLETED
 *          @endcode
 * 
 * @author gonxun 开发团队
 * @version 1.0
 * @date 2026-07-15
 * 
 * @note 使用示例：
 *       @code
 *       gonxun::TaskStateMachine stateMachine;
 *       
 *       // 注册回调
 *       stateMachine.onStateChange([](auto oldState, auto newState) {
 *           std::cout << "状态变更: " << gonxun::TaskStateMachine::stateToString(oldState)
 *                     << " -> " << gonxun::TaskStateMachine::stateToString(newState) << std::endl;
 *       });
 *       
 *       // 设置参数
 *       stateMachine.setTotalCycles(3);
 *       stateMachine.setTaskCode("312");
 *       
 *       // 触发事件
 *       stateMachine.handleEvent(gonxun::TaskEvent::START_MARKING);
 *       stateMachine.handleEvent(gonxun::TaskEvent::MARKING_DONE);
 *       stateMachine.handleEvent(gonxun::TaskEvent::START_MISSION);
 *       @endcode
 * 
 * @see AStarPlanner 路径规划器
 * @see GridPlanner 网格规划器
 * 
 * @copyright 工创赛2025智能物流搬运系统
 */

#pragma once

#include <string>
#include <functional>
#include <chrono>
#include "astar_planner.hpp"

namespace gonxun {

// ============================================================================
// 任务状态枚举定义
// ============================================================================

/**
 * @enum TaskState
 * @brief 任务状态枚举
 * 
 * 定义任务状态机的所有可能状态，每个状态对应机器人的一个工作阶段。
 * 
 * @note 状态转换规则：
 *       - 正常流程：遵循状态转换图
 *       - 异常处理：可从任意状态转换到 ERROR
 *       - 重置：从任意状态转换到 IDLE
 */
enum class TaskState {
    IDLE,               ///< 空闲状态 - 初始状态，等待用户启动
    MARKING,            ///< 标记阶段 - GUI 上标记障碍物和启停区
    READY,              ///< 准备就绪 - 标记完成，等待出发指令
    MOVING_TO_QR,       ///< 前往扫码区 - 移动到二维码区域
    SCANNING_QR,        ///< 扫码中 - 识别任务码
    QR_DONE,            ///< 扫码完成 - 获取任务码，准备取料
    MOVING_TO_MATERIAL, ///< 前往物料区 - 移动到原料区
    PICKING_MATERIAL,   ///< 取料中 - 夹取物料
    MATERIAL_DONE,      ///< 取料完成 - 准备前往粗加工区
    MOVING_TO_PROCESS,  ///< 前往粗加工区 - 搬运物料到加工台
    PLACING_MATERIAL,   ///< 放料中 - 放置物料到指定槽位
    PROCESS_DONE,       ///< 放料完成 - 准备前往暂存区或取下一个物料
    MOVING_TO_BUFFER,   ///< 前往暂存区 - 移动到暂存位置
    BUFFER_DONE,        ///< 暂存完成 - 本轮循环结束
    CYCLE_REPEAT,       ///< 循环重复 - 判断是否需要下一轮
    RETURNING,          ///< 返回启停区 - 所有循环完成后返回
    COMPLETED,          ///< 任务完成 - 终止状态
    ERROR               ///< 异常状态 - 错误处理
};

// ============================================================================
// 任务事件枚举定义
// ============================================================================

/**
 * @enum TaskEvent
 * @brief 任务事件枚举
 * 
 * 定义触发状态转换的所有事件类型。事件由外部系统（GUI、视觉、传感器）产生，
 * 状态机根据当前状态和事件类型决定转换到哪个新状态。
 * 
 * @note 事件触发时机：
 *       - GUI 操作：START_MARKING, MARKING_DONE, START_MISSION, EMERGENCY_STOP
 *       - 机器人到达：REACHED_QR, REACHED_MATERIAL, REACHED_PROCESS, REACHED_BUFFER, REACHED_START
 *       - 动作完成：QR_SCANNED, MATERIAL_PICKED, MATERIAL_PLACED, BUFFER_DONE
 *       - 流程控制：CYCLE_START, ALL_DONE, ERROR_OCCURRED, RESET
 */
enum class TaskEvent {
    START_MARKING,        ///< 开始标记 - 用户点击"标记障碍物"按钮
    MARKING_DONE,         ///< 标记完成 - 用户完成所有标记操作
    START_MISSION,        ///< 开始任务 - 用户点击"仿真"或"启动"按钮
    REACHED_QR,           ///< 到达扫码区 - 机器人到达二维码区域
    QR_SCANNED,           ///< 扫码完成 - 视觉系统识别到任务码
    REACHED_MATERIAL,     ///< 到达物料区 - 机器人到达原料区
    MATERIAL_PICKED,      ///< 取料完成 - 机器人夹取物料成功
    REACHED_PROCESS,      ///< 到达粗加工区 - 机器人到达加工台
    MATERIAL_PLACED,      ///< 放料完成 - 机器人放置物料成功
    REACHED_BUFFER,       ///< 到达暂存区 - 机器人到达暂存位置
    BUFFER_DONE,          ///< 暂存完成 - 本轮循环所有操作完成
    CYCLE_START,          ///< 开始新一轮 - 进入下一个循环周期
    REACHED_START,        ///< 返回启停区 - 机器人回到起始位置
    ALL_DONE,             ///< 全部完成 - 所有任务循环结束
    ERROR_OCCURRED,       ///< 异常发生 - 检测到错误（路径规划失败、硬件故障等）
    RESET,                ///< 重置 - 手动重置状态机到初始状态
    EMERGENCY_STOP        ///< 急停 - 紧急停止所有操作
};

// ============================================================================
// 任务进度信息结构体
// ============================================================================

/**
 * @struct TaskProgress
 * @brief 任务进度信息结构体
 * 
 * 存储任务执行的实时进度数据，用于 GUI 显示和日志记录。
 * 
 * @note 该结构体通过回调函数 onProgressUpdate() 定期更新到 GUI。
 */
struct TaskProgress {
    TaskState currentState;        ///< 当前状态
    int currentCycle;              ///< 当前循环次数（第几轮），范围 [1, totalCycles]
    int totalCycles;               ///< 总循环次数，由配置或任务码决定
    int materialsPicked;           ///< 已取物料数，范围 [0, totalMaterials]
    int materialsPlaced;           ///< 已放物料数，范围 [0, totalMaterials]
    int totalMaterials;            ///< 总物料数，通常为 3
    std::string taskCode;          ///< 任务码（扫码获得），格式为 3 位数字，如 "312"
    std::string stateDescription;  ///< 状态描述文字，用于 GUI 显示
    std::string errorMessage;      ///< 错误信息，仅在 ERROR 状态时有效
};

// ============================================================================
// 回调函数类型定义
// ============================================================================

/**
 * @brief 状态变更回调函数类型
 * 
 * 当状态机从某个状态转换到另一个状态时触发。
 * 
 * @param oldState 转换前的状态
 * @param newState 转换后的状态
 * 
 * @par 使用示例：
 * @code
 * stateMachine.onStateChange([](gonxun::TaskState oldState, gonxun::TaskState newState) {
 *     std::cout << "状态变更: " 
 *               << gonxun::TaskStateMachine::stateToString(oldState) << " -> "
 *               << gonxun::TaskStateMachine::stateToString(newState) << std::endl;
 * });
 * @endcode
 */
using StateChangeCallback = std::function<void(TaskState oldState, TaskState newState)>;

/**
 * @brief 进度更新回调函数类型
 * 
 * 当任务进度信息发生变化时触发，用于更新 GUI 显示。
 * 
 * @param progress 当前进度信息
 */
using ProgressUpdateCallback = std::function<void(const TaskProgress&)>;

/**
 * @brief 路径请求回调函数类型
 * 
 * 当状态机需要规划路径时调用，由外部提供路径规划功能。
 * 
 * @param start 起点（毫米坐标）
 * @param goal 终点（毫米坐标）
 * 
 * @return 路径点数组，空数组表示规划失败
 * 
 * @note 此回调允许状态机与具体的路径规划算法解耦，
 *       可在运行时动态替换不同的规划器。
 */
using PathRequestCallback = std::function<Path(const Point& start, const Point& goal)>;

// ============================================================================
// 任务状态机类
// ============================================================================

/**
 * @class TaskStateMachine
 * @brief 任务状态机 - 管理机器人任务执行流程
 * 
 * @details 本类实现了有限状态机（FSM），用于协调机器人的完整工作流程。
 *          状态机负责：
 *          - 管理状态转换逻辑
 *          - 维护任务进度信息
 *          - 通过回调通知外部系统
 *          - 处理异常和错误情况
 * 
 *          状态机特点：
 *          - 单线程模型：状态转换在调用线程执行，无内部锁
 *          - 事件驱动：通过 handleEvent() 触发状态转换
 *          - 回调机制：通过注册回调函数通知状态变更和进度更新
 *          - 线程安全：回调函数在调用线程执行，需用户保证线程安全
 * 
 * @see TaskState 状态枚举
 * @see TaskEvent 事件枚举
 */
class TaskStateMachine {
public:
    // ========================================================================
    // 构造函数与析构函数
    // ========================================================================
    
    /**
     * @brief 构造函数
     * 
     * 初始化状态机到 IDLE 状态，设置默认参数：
     * - totalCycles = 1
     * - totalMaterials = 3
     * - taskCode = ""
     */
    TaskStateMachine();
    
    /**
     * @brief 析构函数
     * 
     * 默认析构，无特殊清理操作。
     */
    ~TaskStateMachine() = default;
    
    // ========================================================================
    // 核心接口：事件处理
    // ========================================================================
    
    /**
     * @brief 处理事件（触发状态转换）
     * 
     * 根据当前状态和事件类型，执行状态转换逻辑。
     * 转换成功后会触发状态变更回调和进度更新回调。
     * 
     * @param event 任务事件
     * 
     * @return 转换后的新状态
     *         - 如果事件有效且转换成功，返回新状态
     *         - 如果事件在当前状态下无效，返回当前状态（不转换）
     * 
     * @par 状态转换规则：
     *      - IDLE + START_MARKING → MARKING
     *      - MARKING + MARKING_DONE → READY
     *      - READY + START_MISSION → MOVING_TO_QR
     *      - MOVING_TO_QR + REACHED_QR → SCANNING_QR
     *      - SCANNING_QR + QR_SCANNED → QR_DONE
     *      - QR_DONE + (自动) → MOVING_TO_MATERIAL
     *      - MOVING_TO_MATERIAL + REACHED_MATERIAL → PICKING_MATERIAL
     *      - PICKING_MATERIAL + MATERIAL_PICKED → MATERIAL_DONE
     *      - MATERIAL_DONE + (判断) → MOVING_TO_PROCESS 或 MOVING_TO_MATERIAL
     *      - MOVING_TO_PROCESS + REACHED_PROCESS → PLACING_MATERIAL
     *      - PLACING_MATERIAL + MATERIAL_PLACED → PROCESS_DONE
     *      - PROCESS_DONE + (判断) → MOVING_TO_BUFFER 或 MOVING_TO_MATERIAL
     *      - MOVING_TO_BUFFER + REACHED_BUFFER → BUFFER_DONE
     *      - BUFFER_DONE + (判断) → CYCLE_REPEAT 或 RETURNING
     *      - CYCLE_REPEAT + (自动) → MOVING_TO_QR
     *      - RETURNING + REACHED_START → COMPLETED
     *      - 任意状态 + ERROR_OCCURRED → ERROR
     *      - 任意状态 + RESET → IDLE
     *      - 任意状态 + EMERGENCY_STOP → ERROR
     * 
     * @note 线程安全：此函数在调用线程执行，无内部锁。
     *       如果多线程调用，需外部加锁保护。
     */
    TaskState handleEvent(TaskEvent event);
    
    // ========================================================================
    // 状态查询接口
    // ========================================================================
    
    /**
     * @brief 获取当前状态
     * 
     * @return 当前状态枚举值
     */
    TaskState getCurrentState() const { return m_currentState; }
    
    /**
     * @brief 获取进度信息
     * 
     * 返回包含当前状态、循环次数、物料数、任务码等完整进度数据。
     * 
     * @return 进度信息结构体的常量引用
     * 
     * @note 返回引用，避免拷贝开销。引用在下次状态转换前有效。
     */
    const TaskProgress& getProgress() const { return m_progress; }
    
    /**
     * @brief 获取状态描述文字
     * 
     * 将状态枚举转换为可读的中文描述字符串，用于 GUI 显示和日志输出。
     * 
     * @param state 任务状态枚举值
     * 
     * @return 状态描述字符串（UTF-8 编码）
     *         - 如果状态有效，返回对应的中文名称
     *         - 如果状态无效，返回 "未知状态"
     * 
     * @par 示例：
     * @code
     * std::string desc = TaskStateMachine::stateToString(TaskState::MOVING_TO_QR);
     * // desc = "前往扫码区"
     * @endcode
     */
    static std::string stateToString(TaskState state);
    
    /**
     * @brief 检查是否处于可执行状态
     * 
     * 判断状态机是否正在执行任务（非空闲、非完成、非错误）。
     * 
     * @return true：正在执行任务
     *         false：处于 IDLE、COMPLETED 或 ERROR 状态
     */
    bool isActive() const;
    
    /**
     * @brief 检查是否处于异常状态
     * 
     * @return true：当前状态为 ERROR
     *         false：其他状态
     */
    bool isError() const { return m_currentState == TaskState::ERROR; }
    
    // ========================================================================
    // 参数设置接口
    // ========================================================================
    
    /**
     * @brief 设置总循环次数
     * 
     * 指定任务需要执行的循环周期数。通常为 1-3 次。
     * 
     * @param cycles 循环次数（正整数）
     * 
     * @note 可以在任务执行过程中动态修改，影响后续循环判断。
     */
    void setTotalCycles(int cycles) { m_progress.totalCycles = cycles; }
    
    /**
     * @brief 设置总物料数
     * 
     * 指定每个循环周期需要搬运的物料数量。比赛规则为 3 个。
     * 
     * @param count 物料数量（正整数）
     */
    void setTotalMaterials(int count) { m_progress.totalMaterials = count; }
    
    /**
     * @brief 设置任务码
     * 
     * 设置从二维码获取的任务码，决定物料放置顺序。
     * 
     * @param code 任务码（3 位数字字符串，如 "123"、"312"、"213"）
     * 
     * @par 任务码含义：
     *      - "123"：物料按顺序放置到槽位 1、2、3
     *      - "312"：物料按顺序放置到槽位 3、1、2
     *      - "213"：物料按顺序放置到槽位 2、1、3
     */
    void setTaskCode(const std::string& code) { m_progress.taskCode = code; }
    
    /**
     * @brief 设置错误信息
     * 
     * 在进入 ERROR 状态时设置错误描述，用于 GUI 显示和日志记录。
     * 
     * @param msg 错误描述字符串
     */
    void setError(const std::string& msg) { m_progress.errorMessage = msg; }
    
    // ========================================================================
    // 回调注册接口
    // ========================================================================
    
    /**
     * @brief 注册状态变更回调
     * 
     * 当状态机从一个状态转换到另一个状态时，会调用此回调。
     * 典型用途：更新 GUI 状态显示、记录状态日志。
     * 
     * @param cb 回调函数对象
     * 
     * @note 回调函数在 handleEvent() 调用线程执行，需保证线程安全。
     *       只能注册一个回调，多次调用会覆盖之前的回调。
     */
    void onStateChange(StateChangeCallback cb) { m_stateChangeCb = std::move(cb); }
    
    /**
     * @brief 注册进度更新回调
     * 
     * 当任务进度信息发生变化时，会调用此回调。
     * 典型用途：更新 GUI 进度条、显示任务码、显示循环次数。
     * 
     * @param cb 回调函数对象
     */
    void onProgressUpdate(ProgressUpdateCallback cb) { m_progressCb = std::move(cb); }
    
    /**
     * @brief 注册路径请求回调
     * 
     * 当状态机需要规划路径时，会调用此回调获取路径。
     * 典型用途：调用 AStarPlanner 或 GridPlanner 进行路径规划。
     * 
     * @param cb 回调函数对象
     * 
     * @par 使用示例：
     * @code
     * gonxun::AStarPlanner planner;
     * stateMachine.onPathRequest([&planner](const gonxun::Point& start, const gonxun::Point& goal) {
     *     return planner.plan(start, goal);
     * });
     * @endcode
     */
    void onPathRequest(PathRequestCallback cb) { m_pathRequestCb = std::move(cb); }
    
    // ========================================================================
    // 控制接口
    // ========================================================================
    
    /**
     * @brief 重置状态机到 IDLE
     * 
     * 清空所有进度信息，将状态机恢复到初始状态。
     * 
     * @note 重置后会触发状态变更回调（任意状态 → IDLE）。
     */
    void reset();
    
private:
    // ========================================================================
    // 内部辅助方法
    // ========================================================================
    
    /**
     * @brief 执行状态转换
     * 
     * 更新内部状态，触发状态变更回调和进度更新回调。
     * 
     * @param newState 新状态
     */
    void transitionTo(TaskState newState);
    
    /**
     * @brief 通知进度更新
     * 
     * 调用进度更新回调，将当前进度信息发送给外部系统。
     */
    void notifyProgress();
    
    /**
     * @brief 获取当前目标点
     * 
     * 根据当前状态计算机器人应该前往的目标位置。
     * 用于路径规划。
     * 
     * @return 目标点坐标（毫米）
     */
    Point getCurrentTarget() const;
    
    // ========================================================================
    // 成员变量
    // ========================================================================
    
    /**
     * @brief 当前状态
     * 
     * 状态机的核心状态变量，初始值为 IDLE。
     */
    TaskState m_currentState;
    
    /**
     * @brief 任务进度信息
     * 
     * 存储当前循环次数、物料数、任务码等进度数据。
     */
    TaskProgress m_progress;
    
    /**
     * @brief 状态变更回调函数
     * 
     * 可选回调，默认为空函数。通过 onStateChange() 注册。
     */
    StateChangeCallback m_stateChangeCb;
    
    /**
     * @brief 进度更新回调函数
     * 
     * 可选回调，默认为空函数。通过 onProgressUpdate() 注册。
     */
    ProgressUpdateCallback m_progressCb;
    
    /**
     * @brief 路径请求回调函数
     * 
     * 可选回调，默认为空函数。通过 onPathRequest() 注册。
     */
    PathRequestCallback m_pathRequestCb;
};

} // namespace gonxun