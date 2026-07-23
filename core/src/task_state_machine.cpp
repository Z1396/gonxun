/**
 * @file task_state_machine.cpp
 * @brief 任务状态机实现。
 *
 * 实现 16 种状态的事件分发与转移逻辑，每状态对应一个 handle_xxx_event
 * 方法，通过 switch-case 映射事件到目标状态。转移时触发回调通知。
 */

#include "task_state_machine.hpp"

#include <iostream>

namespace gonxun {

TaskStateMachine::TaskStateMachine() {
    current_state_ = TaskState::IDLE;
    progress_ = {};
    progress_.current_state = current_state_;
    progress_.current_cycle = 0;
    progress_.total_cycles = 2;        // 默认 2 轮循环
    progress_.materials_picked = 0;
    progress_.materials_placed = 0;
    progress_.total_materials = 3;     // 默认每轮 3 个物料
    progress_.state_description = state_to_string(current_state_);
}

/**
 * @brief 事件分发入口，根据当前状态调用对应的 handler。
 * @param event 输入事件
 * @return 处理后的当前状态
 */
TaskState TaskStateMachine::handle_event(TaskEvent event) {
    switch (current_state_) {
    case TaskState::IDLE:
        handle_idle_event(event);
        break;
    case TaskState::MARKING:
        handle_marking_event(event);
        break;
    case TaskState::READY:
        handle_ready_event(event);
        break;
    case TaskState::MOVING_TO_QR:
        handle_moving_to_qr_event(event);
        break;
    case TaskState::SCANNING_QR:
        handle_scanning_qr_event(event);
        break;
    case TaskState::QR_DONE:
        // QR 扫码完成后自动转入前往物料区
        transition_to(TaskState::MOVING_TO_MATERIAL);
        break;
    case TaskState::MOVING_TO_MATERIAL:
        handle_moving_to_material_event(event);
        break;
    case TaskState::PICKING_MATERIAL:
        handle_picking_material_event(event);
        break;
    case TaskState::MATERIAL_DONE:
        // 取料完成后自动转入前往粗加工区
        transition_to(TaskState::MOVING_TO_PROCESS);
        break;
    case TaskState::MOVING_TO_PROCESS:
        handle_moving_to_process_event(event);
        break;
    case TaskState::PLACING_MATERIAL:
        handle_placing_material_event(event);
        break;
    case TaskState::PROCESS_DONE:
        // 放料完成后自动转入前往暂存区
        transition_to(TaskState::MOVING_TO_BUFFER);
        break;
    case TaskState::MOVING_TO_BUFFER:
        handle_moving_to_buffer_event(event);
        break;
    case TaskState::BUFFER_DONE:
        handle_buffer_done_event(event);
        break;
    case TaskState::CYCLE_REPEAT:
        handle_cycle_repeat_event(event);
        break;
    case TaskState::RETURNING:
        handle_returning_event(event);
        break;
    case TaskState::COMPLETED:
        if (event == TaskEvent::RESET) reset();
        break;
    case TaskState::ERROR:
        handle_error_event(event);
        break;
    }
    return current_state_;
}

void TaskStateMachine::handle_idle_event(TaskEvent event) {
    switch (event) {
    case TaskEvent::START_MARKING:
        transition_to(TaskState::MARKING);
        break;
    case TaskEvent::RESET:
        reset();
        break;
    default: break;
    }
}

void TaskStateMachine::handle_marking_event(TaskEvent event) {
    switch (event) {
    case TaskEvent::MARKING_DONE:
        transition_to(TaskState::READY);
        break;
    case TaskEvent::RESET:
        reset();
        break;
    default: break;
    }
}

void TaskStateMachine::handle_ready_event(TaskEvent event) {
    switch (event) {
    case TaskEvent::START_MISSION:
        progress_.current_cycle = 1;  // 首轮循环
        transition_to(TaskState::MOVING_TO_QR);
        break;
    case TaskEvent::RESET:
        reset();
        break;
    default: break;
    }
}

void TaskStateMachine::handle_moving_to_qr_event(TaskEvent event) {
    switch (event) {
    case TaskEvent::REACHED_QR:
        transition_to(TaskState::SCANNING_QR);
        break;
    case TaskEvent::ERROR_OCCURRED:
    case TaskEvent::EMERGENCY_STOP:
        transition_to(TaskState::ERROR);
        break;
    default: break;
    }
}

void TaskStateMachine::handle_scanning_qr_event(TaskEvent event) {
    switch (event) {
    case TaskEvent::QR_SCANNED:
        transition_to(TaskState::QR_DONE);
        break;
    case TaskEvent::ERROR_OCCURRED:
        transition_to(TaskState::ERROR);
        break;
    default: break;
    }
}

void TaskStateMachine::handle_moving_to_material_event(TaskEvent event) {
    switch (event) {
    case TaskEvent::REACHED_MATERIAL:
        transition_to(TaskState::PICKING_MATERIAL);
        break;
    case TaskEvent::ERROR_OCCURRED:
        transition_to(TaskState::ERROR);
        break;
    default: break;
    }
}

void TaskStateMachine::handle_picking_material_event(TaskEvent event) {
    switch (event) {
    case TaskEvent::MATERIAL_PICKED:
        progress_.materials_picked++;
        // 单轮物料全部取完才转入 MATERIAL_DONE
        if (progress_.materials_picked >= progress_.total_materials) {
            transition_to(TaskState::MATERIAL_DONE);
        }
        break;
    case TaskEvent::ERROR_OCCURRED:
        transition_to(TaskState::ERROR);
        break;
    default: break;
    }
}

void TaskStateMachine::handle_moving_to_process_event(TaskEvent event) {
    switch (event) {
    case TaskEvent::REACHED_PROCESS:
        transition_to(TaskState::PLACING_MATERIAL);
        break;
    case TaskEvent::ERROR_OCCURRED:
        transition_to(TaskState::ERROR);
        break;
    default: break;
    }
}

void TaskStateMachine::handle_placing_material_event(TaskEvent event) {
    switch (event) {
    case TaskEvent::MATERIAL_PLACED:
        progress_.materials_placed++;
        // 单轮物料全部放完才转入 PROCESS_DONE
        if (progress_.materials_placed >= progress_.total_materials) {
            transition_to(TaskState::PROCESS_DONE);
        }
        break;
    case TaskEvent::ERROR_OCCURRED:
        transition_to(TaskState::ERROR);
        break;
    default: break;
    }
}

void TaskStateMachine::handle_moving_to_buffer_event(TaskEvent event) {
    switch (event) {
    case TaskEvent::REACHED_BUFFER:
        transition_to(TaskState::BUFFER_DONE);
        break;
    case TaskEvent::ERROR_OCCURRED:
        transition_to(TaskState::ERROR);
        break;
    default: break;
    }
}

void TaskStateMachine::handle_buffer_done_event(TaskEvent event) {
    // 暂存完成后判断是否需要下一轮循环
    if (progress_.current_cycle < progress_.total_cycles) {
        transition_to(TaskState::CYCLE_REPEAT);
    } else {
        transition_to(TaskState::RETURNING);
    }
}

void TaskStateMachine::handle_cycle_repeat_event(TaskEvent event) {
    if (event == TaskEvent::CYCLE_START) {
        progress_.current_cycle++;
        progress_.materials_picked = 0;    // 重置本轮计数
        progress_.materials_placed = 0;
        transition_to(TaskState::MOVING_TO_QR);
    }
}

void TaskStateMachine::handle_returning_event(TaskEvent event) {
    switch (event) {
    case TaskEvent::REACHED_START:
        transition_to(TaskState::COMPLETED);
        break;
    case TaskEvent::ERROR_OCCURRED:
        transition_to(TaskState::ERROR);
        break;
    default: break;
    }
}

void TaskStateMachine::handle_error_event(TaskEvent event) {
    switch (event) {
    case TaskEvent::RESET:
        reset();
        break;
    case TaskEvent::START_MISSION:
        // 从异常恢复到就绪状态
        transition_to(TaskState::READY);
        break;
    default: break;
    }
}

/**
 * @brief 转移到新状态，更新进度并触发回调。
 * @param new_state 目标状态
 * @note 若 new_state 与当前状态相同则跳过
 */
void TaskStateMachine::transition_to(TaskState new_state) {
    if (new_state == current_state_) return;

    TaskState old_state = current_state_;
    current_state_ = new_state;
    progress_.current_state = new_state;
    progress_.state_description = state_to_string(new_state);

    std::cout << "[StateMachine] " << state_to_string(old_state)
              << " -> " << state_to_string(new_state) << std::endl;

    if (state_change_cb_) {
        state_change_cb_(old_state, new_state);
    }
    notify_progress();
}

void TaskStateMachine::notify_progress() {
    if (progress_cb_) {
        progress_cb_(progress_);
    }
}

bool TaskStateMachine::is_active() const {
    return current_state_ != TaskState::IDLE &&
           current_state_ != TaskState::COMPLETED &&
           current_state_ != TaskState::ERROR;
}

/**
 * @brief 重置状态机到 IDLE，恢复默认进度值。
 */
void TaskStateMachine::reset() {
    current_state_ = TaskState::IDLE;
    progress_ = {};
    progress_.current_state = TaskState::IDLE;
    progress_.total_cycles = 2;
    progress_.total_materials = 3;
    progress_.state_description = state_to_string(TaskState::IDLE);
    std::cout << "[StateMachine] 已重置" << std::endl;
    notify_progress();
}

std::string TaskStateMachine::state_to_string(TaskState state) {
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
    }
    return "未知";
}

Point TaskStateMachine::get_current_target() const {
    // 占位实现，返回原点
    return {0, 0};
}

} // namespace gonxun
