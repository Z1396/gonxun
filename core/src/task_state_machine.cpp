/**
 * 任务状态机实现
 */

#include "task_state_machine.hpp"
#include <iostream>

namespace gonxun {

TaskStateMachine::TaskStateMachine()
{
    m_currentState = TaskState::IDLE;
    m_progress = {};
    m_progress.currentState = m_currentState;
    m_progress.currentCycle = 0;
    m_progress.totalCycles = 2;     // 默认2轮循环
    m_progress.materialsPicked = 0;
    m_progress.materialsPlaced = 0;
    m_progress.totalMaterials = 3;  // 默认3个物料
    m_progress.stateDescription = stateToString(m_currentState);
}

TaskState TaskStateMachine::handleEvent(TaskEvent event)
{
    TaskState prevState = m_currentState;

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