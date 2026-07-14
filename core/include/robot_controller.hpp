/**
 * 机器人控制模块
 * 与下位机 STM32 通信，控制麦克纳姆轮四驱机器人运动
 * 支持前进、后退、转向、停止、定位等运动模式
 */
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <functional>
#include "astar_planner.hpp"

// 前向声明
class SerialComm;

namespace gonxun {

// 运动模式
enum class MotionMode {
    STOP,           // 停止
    FORWARD,        // 前进
    BACKWARD,       // 后退
    TURN_LEFT,      // 左转
    TURN_RIGHT,     // 右转
    LATERAL_LEFT,   // 左平移（麦轮特性）
    LATERAL_RIGHT,  // 右平移（麦轮特性）
    DIAGONAL_FL,    // 左前对角线
    DIAGONAL_FR,    // 右前对角线
    DIAGONAL_BL,    // 左后对角线
    DIAGONAL_BR,    // 右后对角线
    MOVE_TO_POINT,  // 移动到指定点
    ALIGN_TO_ANGLE   // 对齐到指定角度
};

// 机器人位姿（位置+朝向）
struct RobotPose {
    int x{0};           // X 坐标（毫米）
    int y{0};           // Y 坐标（毫米）
    double theta{0.0};  // 朝向角度（度，0=东，逆时针正）
};

// 运动指令
struct MotionCommand {
    MotionMode mode;    // 运动模式
    int speed{0};       // 速度 (0-255)
    int distance{0};    // 距离（毫米），用于前进/后退
    int angle{0};       // 角度（度），用于转向
    Point target;       // 目标点（毫米），用于 MOVE_TO_POINT
};

// 机器人状态反馈
struct RobotStatus {
    RobotPose pose;             // 当前位姿
    MotionMode currentMode;     // 当前运动模式
    int currentSpeed;           // 当前速度
    bool isMoving;              // 是否正在运动
    bool isAligned;             // 是否已对准目标
    int batteryLevel;           // 电池电量百分比
    std::string lastError;      // 最近错误信息
};

// 状态反馈回调
using StatusUpdateCallback = std::function<void(const RobotStatus&)>;

/**
 * 机器人控制器
 */
class RobotController {
public:
    /**
     * 构造函数
     * @param serial 串口通信实例（外部管理生命周期）
     */
    explicit RobotController(SerialComm* serial);
    ~RobotController();

    // 禁止拷贝
    RobotController(const RobotController&) = delete;
    RobotController& operator=(const RobotController&) = delete;

    /**
     * 执行运动指令
     * @param cmd 运动指令
     * @return 是否发送成功
     */
    bool executeCommand(const MotionCommand& cmd);

    /**
     * 停止运动
     */
    bool stop();

    /**
     * 前进指定距离
     * @param distanceMm 距离（毫米）
     * @param speed 速度 (0-255)
     */
    bool moveForward(int distanceMm, int speed = 150);

    /**
     * 后退指定距离
     */
    bool moveBackward(int distanceMm, int speed = 150);

    /**
     * 左转指定角度
     * @param angleDeg 角度（度）
     */
    bool turnLeft(int angleDeg, int speed = 150);

    /**
     * 右转指定角度
     */
    bool turnRight(int angleDeg, int speed = 150);

    /**
     * 左平移（麦轮全向移动）
     */
    bool moveLateralLeft(int distanceMm, int speed = 150);

    /**
     * 右平移
     */
    bool moveLateralRight(int distanceMm, int speed = 150);

    /**
     * 沿路径移动（依次执行路径上的每个点）
     * @param path 路径点数组
     * @param speed 移动速度
     * @return 是否全部完成
     */
    bool followPath(const Path& path, int speed = 150);

    /**
     * 更新机器人位姿（由视觉系统或里程计反馈）
     */
    void updatePose(int xMm, int yMm, double thetaDeg);

    /**
     * 获取当前位姿
     */
    RobotPose getPose() const;

    /**
     * 获取机器人状态
     */
    RobotStatus getStatus() const;

    /**
     * 注册状态更新回调
     */
    void onStatusUpdate(StatusUpdateCallback cb) { m_statusCb = std::move(cb); }

    /**
     * 检查是否正在运动
     */
    bool isMoving() const { return m_status.isMoving; }

    /**
     * 检查是否已到达目标点
     * @param target 目标点
     * @param toleranceMm 容差（毫米）
     */
    bool isAtPoint(const Point& target, int toleranceMm = 50) const;

    /**
     * 检查是否已对准目标角度
     * @param targetAngle 目标角度
     * @param toleranceDeg 角度容差（度）
     */
    bool isAlignedTo(double targetAngle, double toleranceDeg = 5.0) const;

private:
    // 编码运动指令为串口数据帧
    std::vector<uint8_t> encodeCommand(const MotionCommand& cmd);
    // 通知状态更新
    void notifyStatus();
    // 计算两点间距离
    double distance(const Point& a, const Point& b) const;
    // 计算目标角度
    double calculateAngle(const Point& from, const Point& to) const;

    SerialComm* m_serial;
    mutable std::mutex m_mutex;
    RobotStatus m_status;
    StatusUpdateCallback m_statusCb;
};

} // namespace gonxun