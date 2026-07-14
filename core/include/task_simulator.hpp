/**
 * 任务仿真系统
 * 1) 任务码输入接口
 * 2) 机器人程序生成模块（根据任务码生成指令序列）
 * 3) 任务流程执行引擎（模拟运行指令序列）
 * 4) 执行结果反馈（步骤记录 + 结果报告）
 *
 * 任务码格式：6位字符串，例如 "321132"
 *   第1位：物料A放置顺序位（粗加工区位号 1-3）
 *   第2位：物料B放置顺序位
 *   第3位：物料C放置顺序位
 *   第4-6位：循环次数+暂存顺序（可扩展）
 *
 * 示例：
 *   "123"   → 物料按 ABC 顺序放置到粗加工区 1/2/3 号位
 *   "312"   → 物料按 CAB 顺序放置
 *   "213"   → 物料按 BAC 顺序放置
 */
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <unordered_map>
#include "astar_planner.hpp"
#include "task_state_machine.hpp"
#include "robot_controller.hpp"

namespace gonxun {

// ========== 机器人指令（生成的可执行程序） ==========

// 指令类型
enum class InstructionType {
    MOVE_TO,        // 移动到指定点
    TURN_TO,        // 转向指定角度
    WAIT,           // 等待（毫秒）
    PICK,           // 取料
    PLACE,          // 放料
    SCAN_QR,        // 扫码
    STOP,           // 停止
    LOG             // 日志记录
};

// 机器人指令
struct RobotInstruction {
    InstructionType type;
    std::string description;    // 指令描述
    Point target;               // 目标点（MOVE_TO 用）
    double angle{0.0};          // 目标角度（TURN_TO 用）
    int waitMs{0};              // 等待时间（WAIT 用）
    int materialId{0};          // 物料编号（PICK/PLACE 用）
    int slotId{0};              // 放置槽位（PLACE 用）
    int speed{150};             // 运动速度
};

// 机器人程序（指令序列）
using RobotProgram = std::vector<RobotInstruction>;

// ========== 执行记录 ==========

// 单步执行记录
struct StepRecord {
    int stepIndex;                  // 步骤序号
    std::string timestamp;          // 时间戳
    InstructionType type;           // 指令类型
    std::string description;        // 指令描述
    std::string result;             // 执行结果
    double duration;                // 耗时（秒）
    bool success;                   // 是否成功
};

// 最终结果报告
struct SimulationReport {
    std::string taskCode;                   // 任务码
    std::string startTime;                  // 开始时间
    std::string endTime;                    // 结束时间
    double totalDuration;                   // 总耗时（秒）
    int totalSteps;                         // 总步骤数
    int successSteps;                       // 成功步骤数
    int failedSteps;                        // 失败步骤数
    std::vector<StepRecord> records;        // 步骤记录
    int cyclesCompleted;                    // 完成循环次数
    int materialsPicked;                    // 取料数
    int materialsPlaced;                    // 放料数
    double pathTotalLength;                 // 路径总长度（毫米）
    std::string finalStatus;                // 最终状态
    std::string summary;                    // 总结
};

// ========== 场地区域坐标 ==========

// 场地关键点（毫米坐标，基于实际地图）
struct FieldLocations {
    Point startZone1{2250, 150};            // 启停区1中心
    Point startZone2{2250, 2250};           // 启停区2中心
    Point qrZone{2360, 1200};               // 二维码区（右侧）
    Point materialZone{1200, 50};           // 原料区（顶部中心）
    Point processSlots[3] = {               // 粗加工区3个槽位
        {1050, 2325}, {1200, 2325}, {1350, 2325}
    };
    Point bufferSlots[3] = {                // 暂存区3个槽位
        {75, 1050}, {75, 1200}, {75, 1350}
    };
};

// ========== 仿真回调 ==========

// 步骤执行回调（每执行一步触发）
using StepCallback = std::function<void(const StepRecord&)>;
// 状态变更回调
using SimStateCallback = std::function<void(TaskState oldState, TaskState newState)>;
// 路径可视化回调（用于 GUI 显示路径）
using PathVisualCallback = std::function<void(const Path&)>;

// ========== 任务仿真系统 ==========

/**
 * 任务仿真系统
 * 接收任务码 → 生成机器人程序 → 模拟执行 → 输出报告
 */
class TaskSimulator {
public:
    TaskSimulator();
    ~TaskSimulator() = default;

    // ===== 1. 任务码输入接口 =====

    /**
     * 设置任务码
     * @param code 任务码（3-6位数字字符串）
     * @return 是否合法
     */
    bool setTaskCode(const std::string& code);

    /**
     * 获取当前任务码
     */
    const std::string& getTaskCode() const { return m_taskCode; }

    /**
     * 验证任务码合法性
     */
    static bool validateTaskCode(const std::string& code);

    /**
     * 解析任务码，获取放置顺序
     * @return 槽位顺序数组，例如 [3,1,2] 表示第1个物料放到3号槽
     */
    std::vector<int> parseTaskCode() const;

    // ===== 2. 机器人程序生成 =====

    /**
     * 根据任务码生成完整的机器人程序
     * @return 指令序列
     */
    RobotProgram generateProgram();

    /**
     * 获取上一次生成的程序
     */
    const RobotProgram& getProgram() const { return m_program; }

    /**
     * 设置起始位置
     */
    void setStartPoint(const Point& pt) { m_startPoint = pt; }

    /**
     * 设置障碍物列表
     */
    void setObstacles(const std::vector<ObstacleRect>& obstacles);

    /**
     * 设置循环次数
     */
    void setTotalCycles(int cycles) { m_totalCycles = cycles; }

    /**
     * 设置仿真速度倍率（1.0=实时，10.0=10倍速）
     */
    void setSpeedMultiplier(double mult) { m_speedMultiplier = mult; }

    // ===== 3. 任务执行引擎 =====

    /**
     * 运行仿真
     * @return 是否成功完成
     */
    bool run();

    /**
     * 单步执行（调试用）
     * @return 是否还有下一步
     */
    bool step();

    /**
     * 重置仿真器
     */
    void reset();

    /**
     * 获取当前执行步骤索引
     */
    int getCurrentStep() const { return m_currentStep; }

    /**
     * 是否正在运行
     */
    bool isRunning() const { return m_running; }

    // ===== 4. 结果反馈 =====

    /**
     * 获取仿真报告
     */
    const SimulationReport& getReport() const { return m_report; }

    /**
     * 获取步骤记录
     */
    const std::vector<StepRecord>& getRecords() const { return m_report.records; }

    /**
     * 生成文本格式报告
     */
    std::string generateTextReport() const;

    /**
     * 注册步骤执行回调
     */
    void onStep(StepCallback cb) { m_stepCb = std::move(cb); }

    /**
     * 注册状态变更回调
     */
    void onStateChange(SimStateCallback cb) { m_stateCb = std::move(cb); }

    /**
     * 注册路径可视化回调
     */
    void onPathVisual(PathVisualCallback cb) { m_pathCb = std::move(cb); }

    /**
     * 获取内部状态机引用
     */
    TaskStateMachine& getStateMachine() { return m_stateMachine; }

    /**
     * 获取场地位置信息
     */
    const FieldLocations& getFieldLocations() const { return m_field; }

private:
    // 内部方法
    Path planPath(const Point& from, const Point& to);
    void executeInstruction(const RobotInstruction& instr);
    void recordStep(const RobotInstruction& instr, bool success, const std::string& result, double duration);
    std::string getTimestamp() const;
    std::string instructionToString(const RobotInstruction& instr) const;
    std::string instructionTypeToString(InstructionType type) const;
    void notifyStateChange(TaskState oldState, TaskState newState);
    void notifyPath(const Path& path);

    // 任务码和配置
    std::string m_taskCode;
    Point m_startPoint;
    std::vector<ObstacleRect> m_obstacles;
    int m_totalCycles;
    double m_speedMultiplier;

    // 生成的程序
    RobotProgram m_program;

    // 执行状态
    int m_currentStep;
    bool m_running;

    // 报告
    SimulationReport m_report;

    // 内部模块
    AStarPlanner m_planner;
    TaskStateMachine m_stateMachine;
    FieldLocations m_field;

    // 回调
    StepCallback m_stepCb;
    SimStateCallback m_stateCb;
    PathVisualCallback m_pathCb;

    // 计时
    std::chrono::steady_clock::time_point m_startTime;
};

} // namespace gonxun