/**
 * 任务仿真系统实现
 */

#include "task_simulator.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <thread>
#include <chrono>
#include <ctime>

namespace gonxun {

TaskSimulator::TaskSimulator()
    : m_taskCode("123")
    , m_startPoint({2250, 150})
    , m_totalCycles(2)
    , m_speedMultiplier(1.0)
    , m_currentStep(0)
    , m_running(false)
{
    m_stateMachine.setTotalCycles(m_totalCycles);
    m_stateMachine.setTotalMaterials(3);

    // 状态机回调 → 转发到仿真器外部
    m_stateMachine.onStateChange([this](TaskState oldState, TaskState newState) {
        notifyStateChange(oldState, newState);
    });
}

// ===== 1. 任务码输入接口 =====

bool TaskSimulator::setTaskCode(const std::string& code)
{
    if (!validateTaskCode(code)) {
        std::cerr << "[Simulator] 任务码非法: " << code << std::endl;
        return false;
    }
    m_taskCode = code;
    m_stateMachine.setTaskCode(code);
    std::cout << "[Simulator] 任务码已设置: " << code << std::endl;
    return true;
}

bool TaskSimulator::validateTaskCode(const std::string& code)
{
    // 至少3位，最多6位，每位为1-3的数字
    if (code.length() < 3 || code.length() > 6) return false;
    for (char c : code) {
        if (c < '1' || c > '3') return false;
    }
    // 前3位不能有重复（物料放置顺序）
    if (code.length() >= 3) {
        if (code[0] == code[1] || code[0] == code[2] || code[1] == code[2]) {
            return false;
        }
    }
    return true;
}

std::vector<int> TaskSimulator::parseTaskCode() const
{
    // 前3位代表物料放置顺序
    // 例如 "312" → [3, 1, 2]
    //   物料A(第1个取的) → 放到3号槽
    //   物料B(第2个取的) → 放到1号槽
    //   物料C(第3个取的) → 放到2号槽
    std::vector<int> order;
    for (int i = 0; i < 3 && i < static_cast<int>(m_taskCode.length()); ++i) {
        order.push_back(m_taskCode[i] - '0');
    }
    return order;
}

// ===== 2. 机器人程序生成 =====

void TaskSimulator::setObstacles(const std::vector<ObstacleRect>& obstacles)
{
    m_obstacles = obstacles;
    m_planner.setObstacles(obstacles);
}

RobotProgram TaskSimulator::generateProgram()
{
    RobotProgram program;
    auto placementOrder = parseTaskCode();

    int cycleCount = m_totalCycles;
    Point currentPos = m_startPoint;

    for (int cycle = 0; cycle < cycleCount; ++cycle) {
        // ---- 阶段1: 前往扫码区 ----
        program.push_back({
            InstructionType::MOVE_TO,
            "第" + std::to_string(cycle + 1) + "轮: 前往扫码区",
            m_field.qrZone, 0, 0, 0, 0, 150
        });
        currentPos = m_field.qrZone;

        // 扫码
        program.push_back({
            InstructionType::SCAN_QR,
            "第" + std::to_string(cycle + 1) + "轮: 扫描二维码",
            {}, 0, 1000, 0, 0, 0
        });

        // ---- 阶段2: 前往物料区取料 ----
        // 每轮取3个物料，依次送到粗加工区
        for (int mat = 0; mat < 3; ++mat) {
            // 前往物料区
            program.push_back({
                InstructionType::MOVE_TO,
                "第" + std::to_string(cycle + 1) + "轮: 前往物料区取第" +
                std::to_string(mat + 1) + "个物料",
                m_field.materialZone, 0, 0, mat + 1, 0, 150
            });
            currentPos = m_field.materialZone;

            // 取料
            program.push_back({
                InstructionType::PICK,
                "第" + std::to_string(cycle + 1) + "轮: 取第" +
                std::to_string(mat + 1) + "个物料",
                {}, 0, 500, mat + 1, 0, 0
            });

            // 前往粗加工区对应槽位
            int slot = placementOrder[mat] - 1;  // 0-indexed
            program.push_back({
                InstructionType::MOVE_TO,
                "第" + std::to_string(cycle + 1) + "轮: 送物料" +
                std::to_string(mat + 1) + "到粗加工区" + std::to_string(slot + 1) + "号位",
                m_field.processSlots[slot], 0, 0, mat + 1, slot + 1, 150
            });
            currentPos = m_field.processSlots[slot];

            // 放料
            program.push_back({
                InstructionType::PLACE,
                "第" + std::to_string(cycle + 1) + "轮: 放物料" +
                std::to_string(mat + 1) + "到" + std::to_string(slot + 1) + "号位",
                {}, 0, 500, mat + 1, slot + 1, 0
            });
        }

        // ---- 阶段3: 前往暂存区 ----
        int bufferSlot = cycle % 3;
        program.push_back({
            InstructionType::MOVE_TO,
            "第" + std::to_string(cycle + 1) + "轮: 前往暂存区" +
            std::to_string(bufferSlot + 1) + "号位",
            m_field.bufferSlots[bufferSlot], 0, 0, 0, bufferSlot + 1, 150
        });
        currentPos = m_field.bufferSlots[bufferSlot];

        program.push_back({
            InstructionType::WAIT,
            "第" + std::to_string(cycle + 1) + "轮: 暂存区等待",
            {}, 0, 1000, 0, 0, 0
        });
    }

    // ---- 阶段4: 返回启停区 ----
    program.push_back({
        InstructionType::MOVE_TO,
        "返回启停区",
        m_startPoint, 0, 0, 0, 0, 150
    });

    program.push_back({
        InstructionType::STOP,
        "任务完成，停止",
        {}, 0, 0, 0, 0, 0
    });

    m_program = program;

    std::cout << "[Simulator] 程序已生成，共 " << program.size() << " 条指令" << std::endl;
    for (size_t i = 0; i < program.size(); ++i) {
        std::cout << "  [" << (i + 1) << "] " << program[i].description << std::endl;
    }

    return program;
}

// ===== 3. 任务执行引擎 =====

bool TaskSimulator::run()
{
    if (m_taskCode.empty()) {
        std::cerr << "[Simulator] 未设置任务码" << std::endl;
        return false;
    }

    // 生成程序
    generateProgram();

    // 初始化报告
    m_report = {};
    m_report.taskCode = m_taskCode;
    m_report.startTime = getTimestamp();
    m_report.totalSteps = static_cast<int>(m_program.size());
    m_startTime = std::chrono::steady_clock::now();
    m_running = true;
    m_currentStep = 0;

    // 启动状态机
    m_stateMachine.reset();
    m_stateMachine.handleEvent(TaskEvent::START_MARKING);
    m_stateMachine.handleEvent(TaskEvent::MARKING_DONE);
    m_stateMachine.handleEvent(TaskEvent::START_MISSION);

    std::cout << "\n========================================" << std::endl;
    std::cout << "  仿真开始" << std::endl;
    std::cout << "  任务码: " << m_taskCode << std::endl;
    std::cout << "  循环次数: " << m_totalCycles << std::endl;
    std::cout << "  总步骤数: " << m_program.size() << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 逐条执行指令
    for (const auto& instr : m_program) {
        if (!m_running) break;
        executeInstruction(instr);
        m_currentStep++;
    }

    // 完成
    m_report.endTime = getTimestamp();
    auto elapsed = std::chrono::steady_clock::now() - m_startTime;
    m_report.totalDuration = std::chrono::duration<double>(elapsed).count();
    m_report.finalStatus = m_running ? "完成" : "中断";
    m_report.cyclesCompleted = m_totalCycles;
    m_report.successSteps = m_report.totalSteps - m_report.failedSteps;

    // 生成总结
    std::ostringstream ss;
    ss << "任务码 " << m_taskCode << " 仿真完成。"
       << "共 " << m_report.totalSteps << " 步，"
       << "成功 " << m_report.successSteps << " 步，"
       << "失败 " << m_report.failedSteps << " 步，"
       << "耗时 " << std::fixed << std::setprecision(2) << m_report.totalDuration << " 秒。"
       << "路径总长 " << static_cast<int>(m_report.pathTotalLength) << " mm。";
    m_report.summary = ss.str();

    m_running = false;

    std::cout << "\n========================================" << std::endl;
    std::cout << "  仿真结束" << std::endl;
    std::cout << "  " << m_report.summary << std::endl;
    std::cout << "========================================" << std::endl;

    // 输出文本报告
    std::cout << generateTextReport() << std::endl;

    return m_report.failedSteps == 0;
}

bool TaskSimulator::step()
{
    if (m_currentStep >= static_cast<int>(m_program.size())) {
        return false;
    }
    if (!m_running && m_currentStep == 0) {
        m_running = true;
        m_startTime = std::chrono::steady_clock::now();
        m_report.startTime = getTimestamp();
        m_report.taskCode = m_taskCode;
        m_report.totalSteps = static_cast<int>(m_program.size());
    }

    executeInstruction(m_program[m_currentStep]);
    m_currentStep++;

    if (m_currentStep >= static_cast<int>(m_program.size())) {
        m_report.endTime = getTimestamp();
        auto elapsed = std::chrono::steady_clock::now() - m_startTime;
        m_report.totalDuration = std::chrono::duration<double>(elapsed).count();
        m_report.finalStatus = "完成";
        m_running = false;
        return false;
    }
    return true;
}

void TaskSimulator::reset()
{
    m_running = false;
    m_currentStep = 0;
    m_report = {};
    m_program.clear();
    m_stateMachine.reset();
    std::cout << "[Simulator] 已重置" << std::endl;
}

// ===== 内部方法 =====

Path TaskSimulator::planPath(const Point& from, const Point& to)
{
    Path path = m_planner.plan(from, to);
    if (path.empty()) {
        std::cerr << "[Simulator] 路径规划失败: (" << from.x << "," << from.y
                  << ") -> (" << to.x << "," << to.y << ")" << std::endl;
    } else {
        // 累计路径长度
        for (size_t i = 1; i < path.size(); ++i) {
            double dx = path[i].x - path[i-1].x;
            double dy = path[i].y - path[i-1].y;
            m_report.pathTotalLength += std::sqrt(dx*dx + dy*dy);
        }
        notifyPath(path);
    }
    return path;
}

void TaskSimulator::executeInstruction(const RobotInstruction& instr)
{
    auto startTime = std::chrono::steady_clock::now();

    std::cout << "[Simulator] 执行步骤 " << (m_currentStep + 1) << "/"
              << m_program.size() << ": " << instr.description << std::endl;

    bool success = true;
    std::string result;

    switch (instr.type) {
    case InstructionType::MOVE_TO: {
        // 规划路径
        Point from = m_startPoint;
        if (m_currentStep > 0) {
            // 从上一步的目标点出发
            for (int i = m_currentStep - 1; i >= 0; --i) {
                if (m_program[i].type == InstructionType::MOVE_TO) {
                    from = m_program[i].target;
                    break;
                }
            }
        }

        Path path = planPath(from, instr.target);
        if (path.empty()) {
            success = false;
            result = "路径规划失败";
        } else {
            result = "路径规划成功，共 " + std::to_string(path.size()) + " 个路径点";

            // 模拟移动延时（按速度倍率缩放）
            int delayMs = 200 / m_speedMultiplier;
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

            // 触发对应状态机事件
            if (instr.description.find("扫码区") != std::string::npos) {
                m_stateMachine.handleEvent(TaskEvent::REACHED_QR);
            } else if (instr.description.find("物料区") != std::string::npos) {
                m_stateMachine.handleEvent(TaskEvent::REACHED_MATERIAL);
            } else if (instr.description.find("粗加工区") != std::string::npos) {
                m_stateMachine.handleEvent(TaskEvent::REACHED_PROCESS);
            } else if (instr.description.find("暂存区") != std::string::npos) {
                m_stateMachine.handleEvent(TaskEvent::REACHED_BUFFER);
            } else if (instr.description.find("启停区") != std::string::npos) {
                m_stateMachine.handleEvent(TaskEvent::REACHED_START);
            }
        }
        break;
    }

    case InstructionType::SCAN_QR: {
        int delayMs = instr.waitMs / m_speedMultiplier;
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        result = "扫码完成，任务码: " + m_taskCode;
        m_stateMachine.handleEvent(TaskEvent::QR_SCANNED);
        break;
    }

    case InstructionType::PICK: {
        int delayMs = instr.waitMs / m_speedMultiplier;
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        result = "取料完成，物料编号: " + std::to_string(instr.materialId);
        m_report.materialsPicked++;
        m_stateMachine.handleEvent(TaskEvent::MATERIAL_PICKED);
        break;
    }

    case InstructionType::PLACE: {
        int delayMs = instr.waitMs / m_speedMultiplier;
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        result = "放料完成，物料" + std::to_string(instr.materialId)
               + "放到" + std::to_string(instr.slotId) + "号位";
        m_report.materialsPlaced++;
        m_stateMachine.handleEvent(TaskEvent::MATERIAL_PLACED);
        break;
    }

    case InstructionType::WAIT: {
        int delayMs = instr.waitMs / m_speedMultiplier;
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        result = "等待 " + std::to_string(instr.waitMs) + "ms";
        break;
    }

    case InstructionType::STOP: {
        result = "机器人停止";
        m_stateMachine.handleEvent(TaskEvent::ALL_DONE);
        break;
    }

    case InstructionType::TURN_TO: {
        int delayMs = 300 / m_speedMultiplier;
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        result = "转向 " + std::to_string(instr.angle) + " 度";
        break;
    }

    case InstructionType::LOG: {
        result = instr.description;
        break;
    }
    }

    auto endTime = std::chrono::steady_clock::now();
    double duration = std::chrono::duration<double>(endTime - startTime).count();

    recordStep(instr, success, result, duration);

    if (!success) {
        m_report.failedSteps++;
        std::cerr << "[Simulator] 步骤失败: " << instr.description << std::endl;
    } else {
        m_report.successSteps++;
    }
}

void TaskSimulator::recordStep(const RobotInstruction& instr, bool success,
                                const std::string& result, double duration)
{
    StepRecord record;
    record.stepIndex = m_currentStep + 1;
    record.timestamp = getTimestamp();
    record.type = instr.type;
    record.description = instr.description;
    record.result = result;
    record.duration = duration;
    record.success = success;

    m_report.records.push_back(record);

    if (m_stepCb) {
        m_stepCb(record);
    }
}

std::string TaskSimulator::getTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time), "%H:%M:%S")
       << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string TaskSimulator::instructionTypeToString(InstructionType type) const
{
    switch (type) {
    case InstructionType::MOVE_TO:  return "移动";
    case InstructionType::TURN_TO:  return "转向";
    case InstructionType::WAIT:     return "等待";
    case InstructionType::PICK:     return "取料";
    case InstructionType::PLACE:    return "放料";
    case InstructionType::SCAN_QR:  return "扫码";
    case InstructionType::STOP:     return "停止";
    case InstructionType::LOG:      return "日志";
    default: return "未知";
    }
}

std::string TaskSimulator::instructionToString(const RobotInstruction& instr) const
{
    std::ostringstream ss;
    ss << "[" << instructionTypeToString(instr.type) << "] " << instr.description;
    if (instr.type == InstructionType::MOVE_TO) {
        ss << " -> (" << instr.target.x << "," << instr.target.y << ")";
    }
    return ss.str();
}

void TaskSimulator::notifyStateChange(TaskState oldState, TaskState newState)
{
    if (m_stateCb) {
        m_stateCb(oldState, newState);
    }
}

void TaskSimulator::notifyPath(const Path& path)
{
    if (m_pathCb) {
        m_pathCb(path);
    }
}

// ===== 4. 结果反馈 =====

std::string TaskSimulator::generateTextReport() const
{
    std::ostringstream ss;

    ss << "\n";
    ss << "/==========================================================\\\n";
    ss << "|               任务仿真执行报告                            |\n";
    ss << "|==========================================================|\n";
    ss << "| 任务码:     " << std::left << std::setw(44) << m_report.taskCode << " |\n";
    ss << "| 开始时间:   " << std::setw(44) << m_report.startTime << " |\n";
    ss << "| 结束时间:   " << std::setw(44) << m_report.endTime << " |\n";
    ss << "| 总耗时:     " << std::setw(44) << (std::to_string(m_report.totalDuration) + " 秒") << " |\n";
    ss << "| 总步骤:     " << std::setw(44) << (std::to_string(m_report.totalSteps) + " 步") << " |\n";
    ss << "| 成功步骤:   " << std::setw(44) << (std::to_string(m_report.successSteps) + " 步") << " |\n";
    ss << "| 失败步骤:   " << std::setw(44) << (std::to_string(m_report.failedSteps) + " 步") << " |\n";
    ss << "| 循环次数:   " << std::setw(44) << (std::to_string(m_report.cyclesCompleted) + " 次") << " |\n";
    ss << "| 取料数量:   " << std::setw(44) << (std::to_string(m_report.materialsPicked) + " 个") << " |\n";
    ss << "| 放料数量:   " << std::setw(44) << (std::to_string(m_report.materialsPlaced) + " 个") << " |\n";
    ss << "| 路径总长:   " << std::setw(44) << (std::to_string(static_cast<int>(m_report.pathTotalLength)) + " mm") << " |\n";
    ss << "| 最终状态:   " << std::setw(44) << m_report.finalStatus << " |\n";
    ss << "|==========================================================|\n";
    ss << "| 详细步骤记录:                                            |\n";
    ss << "|----------------------------------------------------------|\n";

    for (const auto& r : m_report.records) {
        ss << "| " << std::setw(3) << r.stepIndex
           << " | " << r.timestamp
           << " | " << std::setw(4) << instructionTypeToString(r.type)
           << " | " << (r.success ? "OK " : "ERR")
           << " | " << r.description;
        if (r.description.length() < 20) {
            ss << std::string(20 - r.description.length(), ' ');
        }
        ss << " |\n";
        ss << "|      -> " << r.result;
        if (r.result.length() < 48) {
            ss << std::string(48 - r.result.length(), ' ');
        }
        ss << " (" << std::fixed << std::setprecision(3) << r.duration << "s) |\n";
    }

    ss << "|==========================================================|\n";
    ss << "| 总结: " << m_report.summary;
    if (m_report.summary.length() < 52) {
        ss << std::string(52 - m_report.summary.length(), ' ');
    }
    ss << " |\n";
    ss << "\\==========================================================/\n";

    return ss.str();
}

} // namespace gonxun