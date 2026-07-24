/**
 * @file task_state_machine.cpp
 * @brief 任务状态机实现。
 *
 * 使用 std::visit 穷尽处理状态转移，符合 Talos 规范。
 */

#include "task_state_machine.hpp"

#include <iostream>
#include <utility>

namespace gonxun {

TaskStateMachine::TaskStateMachine() {
    current_state_ = Idle{};
    progress_ = {};
    progress_.current_cycle = 0;
    progress_.total_cycles = 2;
    progress_.materials_picked = 0;
    progress_.materials_placed = 0;
    progress_.total_materials = 3;
    progress_.state_description = state_to_string(current_state_);
}

TaskState TaskStateMachine::handle_event(TaskEvent event) {
    auto new_state = std::visit(overloaded{
        [&](const Idle&) -> TaskState {
            switch (event) {
                case TaskEvent::START_MARKING:
                    return Marking{};
                case TaskEvent::RESET:
                    return Idle{};
                default:
                    return current_state_;
            }
        },
        [&](const Marking&) -> TaskState {
            switch (event) {
                case TaskEvent::MARKING_DONE:
                    return Ready{};
                case TaskEvent::RESET:
                    return Idle{};
                default:
                    return current_state_;
            }
        },
        [&](const Ready&) -> TaskState {
            switch (event) {
                case TaskEvent::START_MISSION:
                    progress_.current_cycle = 1;
                    return MovingToQr{};
                case TaskEvent::RESET:
                    return Idle{};
                default:
                    return current_state_;
            }
        },
        [&](const MovingToQr&) -> TaskState {
            switch (event) {
                case TaskEvent::REACHED_QR:
                    return ScanningQr{};
                case TaskEvent::ERROR_OCCURRED:
                case TaskEvent::EMERGENCY_STOP:
                    return Error{"移动到扫码区失败"};
                default:
                    return current_state_;
            }
        },
        [&](const ScanningQr&) -> TaskState {
            switch (event) {
                case TaskEvent::QR_SCANNED:
                    return QrDone{};
                case TaskEvent::ERROR_OCCURRED:
                    return Error{"扫码失败"};
                default:
                    return current_state_;
            }
        },
        [&](const QrDone&) -> TaskState {
            return MovingToMaterial{};
        },
        [&](const MovingToMaterial&) -> TaskState {
            switch (event) {
                case TaskEvent::REACHED_MATERIAL:
                    return PickingMaterial{0};
                case TaskEvent::ERROR_OCCURRED:
                    return Error{"移动到物料区失败"};
                default:
                    return current_state_;
            }
        },
        [&](const PickingMaterial& state) -> TaskState {
            switch (event) {
                case TaskEvent::MATERIAL_PICKED:
                    progress_.materials_picked++;
                    if (progress_.materials_picked >= progress_.total_materials) {
                        return MaterialDone{};
                    }
                    return PickingMaterial{progress_.materials_picked};
                case TaskEvent::ERROR_OCCURRED:
                    return Error{"取料失败"};
                default:
                    return current_state_;
            }
        },
        [&](const MaterialDone&) -> TaskState {
            return MovingToProcess{};
        },
        [&](const MovingToProcess&) -> TaskState {
            switch (event) {
                case TaskEvent::REACHED_PROCESS:
                    return PlacingMaterial{0};
                case TaskEvent::ERROR_OCCURRED:
                    return Error{"移动到粗加工区失败"};
                default:
                    return current_state_;
            }
        },
        [&](const PlacingMaterial& state) -> TaskState {
            switch (event) {
                case TaskEvent::MATERIAL_PLACED:
                    progress_.materials_placed++;
                    if (progress_.materials_placed >= progress_.total_materials) {
                        return ProcessDone{};
                    }
                    return PlacingMaterial{progress_.materials_placed};
                case TaskEvent::ERROR_OCCURRED:
                    return Error{"放料失败"};
                default:
                    return current_state_;
            }
        },
        [&](const ProcessDone&) -> TaskState {
            return MovingToBuffer{};
        },
        [&](const MovingToBuffer&) -> TaskState {
            switch (event) {
                case TaskEvent::REACHED_BUFFER:
                    return BufferDone{};
                case TaskEvent::ERROR_OCCURRED:
                    return Error{"移动到暂存区失败"};
                default:
                    return current_state_;
            }
        },
        [&](const BufferDone&) -> TaskState {
            if (progress_.current_cycle < progress_.total_cycles) {
                return CycleRepeat{};
            }
            return Returning{};
        },
        [&](const CycleRepeat&) -> TaskState {
            if (event == TaskEvent::CYCLE_START) {
                progress_.current_cycle++;
                progress_.materials_picked = 0;
                progress_.materials_placed = 0;
                return MovingToQr{};
            }
            return current_state_;
        },
        [&](const Returning&) -> TaskState {
            switch (event) {
                case TaskEvent::REACHED_START:
                    return Completed{};
                case TaskEvent::ERROR_OCCURRED:
                    return Error{"返回启停区失败"};
                default:
                    return current_state_;
            }
        },
        [&](const Completed&) -> TaskState {
            if (event == TaskEvent::RESET) {
                return Idle{};
            }
            return current_state_;
        },
        [&](const Error& err) -> TaskState {
            switch (event) {
                case TaskEvent::RESET:
                    return Idle{};
                case TaskEvent::START_MISSION:
                    return Ready{};
                default:
                    return current_state_;
            }
        }
    }, current_state_);

    if (new_state.index() != current_state_.index()) {
        transition_to(std::move(new_state));
    }
    return current_state_;
}

void TaskStateMachine::transition_to(TaskState new_state) {
    if (new_state.index() == current_state_.index()) {
        return;
    }

    TaskState old_state = current_state_;
    current_state_ = std::move(new_state);
    progress_.state_description = state_to_string(current_state_);

    std::cout << "[StateMachine] " << state_to_string(old_state)
              << " -> " << state_to_string(current_state_) << std::endl;

    if (state_change_cb_) {
        state_change_cb_(old_state, current_state_);
    }
    notify_progress();
}

void TaskStateMachine::notify_progress() {
    if (progress_cb_) {
        progress_cb_(progress_);
    }
}

bool TaskStateMachine::is_active() const {
    return !std::holds_alternative<Idle>(current_state_) &&
           !std::holds_alternative<Completed>(current_state_) &&
           !std::holds_alternative<Error>(current_state_);
}

void TaskStateMachine::reset() {
    current_state_ = Idle{};
    progress_ = {};
    progress_.total_cycles = 2;
    progress_.total_materials = 3;
    progress_.state_description = state_to_string(Idle{});
    std::cout << "[StateMachine] 已重置" << std::endl;
    notify_progress();
}

std::string TaskStateMachine::state_to_string(const TaskState& state) {
    return std::visit(overloaded{
        [](const Idle&) { return "空闲"; },
        [](const Marking&) { return "标记中"; },
        [](const Ready&) { return "就绪"; },
        [](const MovingToQr&) { return "前往扫码区"; },
        [](const ScanningQr&) { return "扫码中"; },
        [](const QrDone&) { return "扫码完成"; },
        [](const MovingToMaterial&) { return "前往物料区"; },
        [](const PickingMaterial&) { return "取料中"; },
        [](const MaterialDone&) { return "取料完成"; },
        [](const MovingToProcess&) { return "前往粗加工区"; },
        [](const PlacingMaterial&) { return "放料中"; },
        [](const ProcessDone&) { return "放料完成"; },
        [](const MovingToBuffer&) { return "前往暂存区"; },
        [](const BufferDone&) { return "暂存完成"; },
        [](const CycleRepeat&) { return "准备循环"; },
        [](const Returning&) { return "返回启停区"; },
        [](const Completed&) { return "任务完成"; },
        [](const Error&) { return "异常"; }
    }, state);
}

} // namespace gonxun