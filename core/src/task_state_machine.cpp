/**
 * @file task_state_machine.cpp
 * @brief 任务状态机实现文件
 * 
 * @details 本文件实现了基于有限状态机（FSM）的任务管理功能。
 *          核心特性：
 *          - 状态定义：18个状态覆盖整个任务生命周期
 *          - 事件驱动：通过事件触发状态转换
 *          - 状态转换表：明确定义每个状态的事件响应
 *          - 进度跟踪：实时更新任务进度信息
 *          - 回调机制：支持状态变化和进度通知
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，实现基础状态机
 *       - 2025-02-15: 增加循环和进度跟踪
 *       
 * @note 状态机模式：
 *       - 状态：系统在某一时刻的状况
 *       - 事件：触发状态转换的外部输入
 *       - 转换：从当前状态到新状态的变化
 *       - 动作：状态转换时执行的操作
 *       
 * @note 状态流程：
 *       IDLE → MARKING → READY → MOVING_TO_QR → SCANNING_QR → QR_DONE
 *       → MOVING_TO_MATERIAL → PICKING_MATERIAL → MATERIAL_DONE
 *       → MOVING_TO_PROCESS → PLACING_MATERIAL → PROCESS_DONE
 *       → MOVING_TO_BUFFER → BUFFER_DONE → [CYCLE_REPEAT or RETURNING]
 *       → COMPLETED
 *       
 * @note 循环机制：
 *       - 默认循环 2 次
 *       - 每次循环重置物料计数
 *       - 达到循环次数后返回启停区
 *       
 * @note 异常处理：
 *       - ERROR 状态：可从任意状态进入
 *       - 恢复机制：通过 RESET 事件重置，或 START_MISSION 事件重启
 *       
 * @see task_state_machine.hpp
 */
#include "task_state_machine.hpp"
#include <iostream>

namespace gonxun {

/**
 * @brief 构造函数，初始化状态机
 * 
 * @details 设置初始状态为 IDLE，并初始化进度信息。
 */
TaskStateMachine::TaskStateMachine()
{
    m_currentState = TaskState::IDLE;  // 初始状态：空闲
    m_progress = {};
    m_progress.currentState = m_currentState;
    m_progress.currentCycle = 0;       // 当前循环次数
    m_progress.totalCycles = 2;        // 默认2轮循环
    m_progress.materialsPicked = 0;    // 已取物料数
    m_progress.materialsPlaced = 0;    // 已放物料数
    m_progress.totalMaterials = 3;     // 默认3个物料
    m_progress.stateDescription = stateToString(m_currentState);
}

/**
 * @brief 处理事件并执行状态转换
 * 
 * @details 根据当前状态和事件，执行相应的状态转换。
 *          使用状态转换表（switch-case嵌套）实现FSM逻辑。
 *          
 * @param event 触发事件
 *        - START_MARKING: 开始标记（启动任务）
 *        - MARKING_DONE: 标记完成
 *        - START_MISSION: 开始任务
 *        - REACHED_QR: 到达扫码区
 *        - QR_SCANNED: 扫码完成
 *        - REACHED_MATERIAL: 到达物料区
 *        - MATERIAL_PICKED: 取料完成
 *        - REACHED_PROCESS: 到达粗加工区
 *        - MATERIAL_PLACED: 放料完成
 *        - REACHED_BUFFER: 到达暂存区
 *        - REACHED_START: 返回启停区
 *        - CYCLE_START: 开始新循环
 *        - ALL_DONE: 任务完成
 *        - ERROR_OCCURRED: 异常发生
 *        - EMERGENCY_STOP: 紧急停止
 *        - RESET: 重置状态机
 *        
 * @return TaskState 转换后的新状态
 *         
 * @note 状态转换表（部分）：
 *       | 当前状态           | 事件              | 新状态               |
 *       |--------------------|-------------------|----------------------|
 *       | IDLE               | START_MARKING     | MARKING              |
 *       | MARKING            | MARKING_DONE      | READY                |
 *       | READY              | START_MISSION     | MOVING_TO_QR         |
 *       | MOVING_TO_QR       | REACHED_QR        | SCANNING_QR          |
 *       | SCANNING_QR        | QR_SCANNED        | QR_DONE              |
 *       | QR_DONE            | -                 | MOVING_TO_MATERIAL   |
 *       | MOVING_TO_MATERIAL | REACHED_MATERIAL  | PICKING_MATERIAL     |
 *       | PICKING_MATERIAL   | MATERIAL_PICKED   | MATERIAL_DONE        |
 *       | MATERIAL_DONE      | -                 | MOVING_TO_PROCESS    |
 *       | MOVING_TO_PROCESS  | REACHED_PROCESS   | PLACING_MATERIAL     |
 *       | PLACING_MATERIAL   | MATERIAL_PLACED   | PROCESS_DONE         |
 *       | PROCESS_DONE       | -                 | MOVING_TO_BUFFER     |
 *       | MOVING_TO_BUFFER   | REACHED_BUFFER    | BUFFER_DONE          |
 *       | BUFFER_DONE        | -                 | CYCLE_REPEAT/RETURNING|
 *       | CYCLE_REPEAT       | CYCLE_START       | MOVING_TO_QR         |
 *       | RETURNING          | REACHED_START     | COMPLETED            |
 *       | 任意状态           | ERROR_OCCURRED    | ERROR                |
 *       | 任意状态           | RESET             | IDLE                 |
 *       
 * @see transitionTo()
 */
TaskState TaskStateMachine::handleEvent(TaskEvent event)
{
    // 状态转换表：根据当前状态和事件执行转换
    switch (m_currentState) {
    // ===== 空闲状态 =====
    case TaskState::IDLE:
        switch (event) {
        case TaskEvent::START_MARKING:
            transitionTo(TaskState::MARKING);
            break;
        case TaskEvent::RESET:
            reset();
            break;
        default: break;
        }
        break;

    // ===== 标记阶段 =====
    case TaskState::MARKING:
        switch (event) {
        case TaskEvent::MARKING_DONE:
            transitionTo(TaskState::READY);
            break;
        case TaskEvent::RESET:
            reset();
            break;
        default: break;
        }
        break;

    // ===== 准备出发 =====
    case TaskState::READY:
        switch (event) {
        case TaskEvent::START_MISSION:
            m_progress.currentCycle = 1;
            transitionTo(TaskState::MOVING_TO_QR);
            break;
        case TaskEvent::RESET:
            reset();
            break;
        default: break;
        }
        break;

    // ===== 前往扫码区 =====
    case TaskState::MOVING_TO_QR:
        switch (event) {
        case TaskEvent::REACHED_QR:
            transitionTo(TaskState::SCANNING_QR);
            break;
        case TaskEvent::ERROR_OCCURRED:
            transitionTo(TaskState::ERROR);
            break;
        case TaskEvent::EMERGENCY_STOP:
            transitionTo(TaskState::ERROR);
            break;
        default: break;
        }
        break;

    // ===== 扫码中 =====
    case TaskState::SCANNING_QR:
        switch (event) {
        case TaskEvent::QR_SCANNED:
            transitionTo(TaskState::QR_DONE);
            break;
        case TaskEvent::ERROR_OCCURRED:
            transitionTo(TaskState::ERROR);
            break;
        default: break;
        }
        break;

    // ===== 扫码完成，前往物料区 =====
    case TaskState::QR_DONE:
        switch (event) {
        case TaskEvent::REACHED_MATERIAL:
            // QR_DONE 直接到物料区（实际可能省略中间移动状态）
            transitionTo(TaskState::MOVING_TO_MATERIAL);
            break;
        default:
            // 自动转换到移动状态
            transitionTo(TaskState::MOVING_TO_MATERIAL);
            break;
        }
        break;

    // ===== 前往物料区 =====
    case TaskState::MOVING_TO_MATERIAL:
        switch (event) {
        case TaskEvent::REACHED_MATERIAL:
            transitionTo(TaskState::PICKING_MATERIAL);
            break;
        case TaskEvent::ERROR_OCCURRED:
            transitionTo(TaskState::ERROR);
            break;
        default: break;
        }
        break;

    // ===== 取料中 =====
    case TaskState::PICKING_MATERIAL:
        switch (event) {
        case TaskEvent::MATERIAL_PICKED:
            m_progress.materialsPicked++;
            if (m_progress.materialsPicked >= m_progress.totalMaterials) {
                transitionTo(TaskState::MATERIAL_DONE);
            }
            break;
        case TaskEvent::ERROR_OCCURRED:
            transitionTo(TaskState::ERROR);
            break;
        default: break;
        }
        break;

    // ===== 取料完成，前往粗加工区 =====
    case TaskState::MATERIAL_DONE:
        transitionTo(TaskState::MOVING_TO_PROCESS);
        break;

    // ===== 前往粗加工区 =====
    case TaskState::MOVING_TO_PROCESS:
        switch (event) {
        case TaskEvent::REACHED_PROCESS:
            transitionTo(TaskState::PLACING_MATERIAL);
            break;
        case TaskEvent::ERROR_OCCURRED:
            transitionTo(TaskState::ERROR);
            break;
        default: break;
        }
        break;

    // ===== 放料中 =====
    case TaskState::PLACING_MATERIAL:
        switch (event) {
        case TaskEvent::MATERIAL_PLACED:
            m_progress.materialsPlaced++;
            if (m_progress.materialsPlaced >= m_progress.totalMaterials) {
                transitionTo(TaskState::PROCESS_DONE);
            }
            break;
        case TaskEvent::ERROR_OCCURRED:
            transitionTo(TaskState::ERROR);
            break;
        default: break;
        }
        break;

    // ===== 放料完成，前往暂存区 =====
    case TaskState::PROCESS_DONE:
        transitionTo(TaskState::MOVING_TO_BUFFER);
        break;

    // ===== 前往暂存区 =====
    case TaskState::MOVING_TO_BUFFER:
        switch (event) {
        case TaskEvent::REACHED_BUFFER:
            transitionTo(TaskState::BUFFER_DONE);
            break;
        case TaskEvent::ERROR_OCCURRED:
            transitionTo(TaskState::ERROR);
            break;
        default: break;
        }
        break;

    // ===== 暂存完成 =====
    case TaskState::BUFFER_DONE:
        // 检查是否需要循环
        if (m_progress.currentCycle < m_progress.totalCycles) {
            transitionTo(TaskState::CYCLE_REPEAT);
        } else {
            transitionTo(TaskState::RETURNING);
        }
        break;

    // ===== 循环重复 =====
    case TaskState::CYCLE_REPEAT:
        switch (event) {
        case TaskEvent::CYCLE_START:
            m_progress.currentCycle++;
            m_progress.materialsPicked = 0;
            m_progress.materialsPlaced = 0;
            transitionTo(TaskState::MOVING_TO_QR);
            break;
        default: break;
        }
        break;

    // ===== 返回启停区 =====
    case TaskState::RETURNING:
        switch (event) {
        case TaskEvent::REACHED_START:
            transitionTo(TaskState::COMPLETED);
            break;
        case TaskEvent::ERROR_OCCURRED:
            transitionTo(TaskState::ERROR);
            break;
        default: break;
        }
        break;

    // ===== 任务完成 =====
    case TaskState::COMPLETED:
        switch (event) {
        case TaskEvent::RESET:
            reset();
            break;
        default: break;
        }
        break;

    // ===== 异常状态 =====
    case TaskState::ERROR:
        switch (event) {
        case TaskEvent::RESET:
            reset();
            break;
        case TaskEvent::START_MISSION:
            // 从异常恢复，重新开始
            transitionTo(TaskState::READY);
            break;
        default: break;
        }
        break;
    }

    return m_currentState;
}

void TaskStateMachine::transitionTo(TaskState newState)
{
    if (newState == m_currentState) return;

    TaskState oldState = m_currentState;
    m_currentState = newState;
    m_progress.currentState = newState;
    m_progress.stateDescription = stateToString(newState);

    std::cout << "[StateMachine] " << stateToString(oldState)
              << " -> " << stateToString(newState) << std::endl;

    if (m_stateChangeCb) {
        m_stateChangeCb(oldState, newState);
    }
    notifyProgress();
}

void TaskStateMachine::notifyProgress()
{
    if (m_progressCb) {
        m_progressCb(m_progress);
    }
}

bool TaskStateMachine::isActive() const
{
    return m_currentState != TaskState::IDLE &&
           m_currentState != TaskState::COMPLETED &&
           m_currentState != TaskState::ERROR;
}

void TaskStateMachine::reset()
{
    m_currentState = TaskState::IDLE;
    m_progress = {};
    m_progress.currentState = TaskState::IDLE;
    m_progress.totalCycles = 2;
    m_progress.totalMaterials = 3;
    m_progress.stateDescription = stateToString(TaskState::IDLE);
    std::cout << "[StateMachine] 已重置" << std::endl;
    notifyProgress();
}

std::string TaskStateMachine::stateToString(TaskState state)
{
    switch (state) {
    case TaskState::IDLE:               return "空闲";
    case TaskState::MARKING:            return "标记中";
    case TaskState::READY:              return "就绪";
    case TaskState::MOVING_TO_QR:       return "前往扫码区";
    case TaskState::SCANNING_QR:        return "扫码中";
    case TaskState::QR_DONE:            return "扫码完成";
    case TaskState::MOVING_TO_MATERIAL: return "前往物料区";
    case TaskState::PICKING_MATERIAL:   return "取料中";
    case TaskState::MATERIAL_DONE:      return "取料完成";
    case TaskState::MOVING_TO_PROCESS:  return "前往粗加工区";
    case TaskState::PLACING_MATERIAL:   return "放料中";
    case TaskState::PROCESS_DONE:       return "放料完成";
    case TaskState::MOVING_TO_BUFFER:   return "前往暂存区";
    case TaskState::BUFFER_DONE:        return "暂存完成";
    case TaskState::CYCLE_REPEAT:       return "准备循环";
    case TaskState::RETURNING:          return "返回启停区";
    case TaskState::COMPLETED:          return "任务完成";
    case TaskState::ERROR:              return "异常";
    default: return "未知";
    }
}

} // namespace gonxun