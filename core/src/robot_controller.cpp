/**
 * @file robot_controller.cpp
 * @brief 机器人控制器实现文件
 * 
 * @details 本文件实现了机器人的运动控制和状态管理功能。
 *          核心特性：
 *          - 运动控制：前进、后退、转向、横移等
 *          - 路径跟踪：沿路径点序列移动
 *          - 位姿管理：位置和姿态的跟踪与更新
 *          - 线程安全：使用互斥锁保护状态数据
 *          - 下位机通信：通过串口发送运动指令
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，实现基础运动控制
 *       - 2025-02-15: 增加路径跟踪功能
 *       - 2025-03-30: 优化线程安全设计
 *       
 * @note 运动模式：
 *       - STOP (0x00): 停止
 *       - FORWARD (0x01): 前进
 *       - BACKWARD (0x02): 后退
 *       - TURN_LEFT (0x03): 左转
 *       - TURN_RIGHT (0x04): 右转
 *       - LATERAL_LEFT (0x05): 左横移
 *       - LATERAL_RIGHT (0x06): 右横移
 *       - MOVE_TO_POINT (0x0A): 移动到指定点
 *       
 * @note 下位机协议：
 *       帧格式: [帧头 0x66] [命令] [速度] [数据1] [数据2] [数据3] [数据4] [校验] [帧尾 0x77]
 *       - 帧头：0x66
 *       - 命令：运动指令字节码
 *       - 速度：运动速度（0-255）
 *       - 数据1-4：参数数据（距离、角度、坐标等）
 *       - 校验：XOR 校验和
 *       - 帧尾：0x77
 *       
 * @see robot_controller.hpp
 */
#include "robot_controller.hpp"
#include "serial_comm.hpp"
#include <cmath>
#include <iostream>
#include <thread>
#include <chrono>

namespace gonxun {

/**
 * @brief 机器人运动指令字节码定义
 * @details 定义了所有支持的机器人运动指令。
 *          这些字节码必须与下位机固件保持一致。
 * 
 * @note 暂时未使用的指令保留用于未来扩展
 */
// constexpr uint8_t CMD_STOP          = 0x00;  ///< 停止
// constexpr uint8_t CMD_FORWARD       = 0x01;  ///< 前进
// constexpr uint8_t CMD_BACKWARD      = 0x02;  ///< 后退
// constexpr uint8_t CMD_TURN_LEFT     = 0x03;  ///< 左转
// constexpr uint8_t CMD_TURN_RIGHT    = 0x04;  ///< 右转
// constexpr uint8_t CMD_LATERAL_LEFT  = 0x05;  ///< 左横移
// constexpr uint8_t CMD_LATERAL_RIGHT = 0x06;  ///< 右横移
// constexpr uint8_t CMD_MOVE_TO_POINT = 0x0A;  ///< 移动到指定点

/**
 * @brief 构造函数，初始化控制器状态
 * 
 * @details 初始化机器人状态为停止状态，电池电量默认为 100%。
 *          
 * @param serial 串口通信对象指针（用于发送运动指令）
 */
RobotController::RobotController(SerialComm* serial)
    : m_serial(serial)
{
    m_status = {};
    m_status.currentMode = MotionMode::STOP;  // 初始模式：停止
    m_status.isMoving = false;                 // 初始状态：未移动
    m_status.isAligned = true;                 // 初始状态：已对齐
    m_status.batteryLevel = 100;               // 初始电量：100%
}

RobotController::~RobotController()
{
    stop();
}

bool RobotController::executeCommand(const MotionCommand& cmd)
{
    if (!m_serial) {
        std::cerr << "[Robot] 串口未连接" << std::endl;
        return false;
    }

    auto frame = encodeCommand(cmd);
    if (frame.empty()) {
        std::cerr << "[Robot] 指令编码失败" << std::endl;
        return false;
    }

    // 发送坐标到下位机（复用 SerialComm 的发送接口）
    std::vector<std::pair<int, int>> coords;
    if (cmd.mode == MotionMode::MOVE_TO_POINT) {
        coords.push_back({cmd.target.x, cmd.target.y});
    } else {
        coords.push_back({cmd.distance, cmd.angle});
    }

    // 使用对应命令字节发送
    uint8_t cmdByte = static_cast<uint8_t>(cmd.mode);
    m_serial->sendCoordinates(cmdByte, coords);

    // 更新状态
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status.currentMode = cmd.mode;
        m_status.currentSpeed = cmd.speed;
        m_status.isMoving = (cmd.mode != MotionMode::STOP);
    }

    std::cout << "[Robot] 指令已发送: mode=" << static_cast<int>(cmdByte)
              << " speed=" << cmd.speed << std::endl;

    notifyStatus();
    return true;
}

bool RobotController::stop()
{
    MotionCommand cmd;
    cmd.mode = MotionMode::STOP;
    cmd.speed = 0;
    return executeCommand(cmd);
}

bool RobotController::moveForward(int distanceMm, int speed)
{
    MotionCommand cmd;
    cmd.mode = MotionMode::FORWARD;
    cmd.speed = speed;
    cmd.distance = distanceMm;
    return executeCommand(cmd);
}

bool RobotController::moveBackward(int distanceMm, int speed)
{
    MotionCommand cmd;
    cmd.mode = MotionMode::BACKWARD;
    cmd.speed = speed;
    cmd.distance = distanceMm;
    return executeCommand(cmd);
}

bool RobotController::turnLeft(int angleDeg, int speed)
{
    MotionCommand cmd;
    cmd.mode = MotionMode::TURN_LEFT;
    cmd.speed = speed;
    cmd.angle = angleDeg;
    return executeCommand(cmd);
}

bool RobotController::turnRight(int angleDeg, int speed)
{
    MotionCommand cmd;
    cmd.mode = MotionMode::TURN_RIGHT;
    cmd.speed = speed;
    cmd.angle = angleDeg;
    return executeCommand(cmd);
}

bool RobotController::moveLateralLeft(int distanceMm, int speed)
{
    MotionCommand cmd;
    cmd.mode = MotionMode::LATERAL_LEFT;
    cmd.speed = speed;
    cmd.distance = distanceMm;
    return executeCommand(cmd);
}

bool RobotController::moveLateralRight(int distanceMm, int speed)
{
    MotionCommand cmd;
    cmd.mode = MotionMode::LATERAL_RIGHT;
    cmd.speed = speed;
    cmd.distance = distanceMm;
    return executeCommand(cmd);
}

/**
 * @brief 沿路径移动
 * 
 * @details 遍历路径点序列，依次转向并移动到每个目标点。
 *          
 * @param path 路径点列表（毫米坐标）
 * @param speed 移动速度（0-255）
 * 
 * @return true 路径执行成功
 * @return false 路径执行失败（路径为空）
 * 
 * @note 执行流程：
 *       1. 遍历路径点序列
 *       2. 计算当前位置到目标点的角度
 *       3. 转向目标方向（角度差 > 10° 时转向）
 *       4. 前进到目标点（距离 > 50mm 时移动）
 *       5. 更新位姿状态
 *       6. 继续下一个路径点
 *       
 * @note 简化处理：
 *       - 使用固定延时等待动作完成（实际应使用传感器反馈）
 *       - 位姿更新基于路径规划结果（实际应由视觉系统反馈）
 *       
 * @warning 此函数会阻塞当前线程，直到路径执行完成。
 *          实际应用中应考虑使用异步执行或状态机管理。
 *          
 * @see moveForward(), turnLeft(), turnRight()
 */
bool RobotController::followPath(const Path& path, int speed)
{
    // 检查路径是否为空
    if (path.empty()) {
        std::cerr << "[Robot] 路径为空" << std::endl;
        return false;
    }

    std::cout << "[Robot] 开始沿路径移动，共 " << path.size() << " 个点" << std::endl;

    // 遍历路径点序列
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& target = path[i];
        std::cout << "[Robot] 移动到点 " << i + 1 << "/" << path.size()
                  << " (" << target.x << ", " << target.y << ")" << std::endl;

        // 计算转向角度
        RobotPose pose = getPose();
        Point currentPos{pose.x, pose.y};
        double targetAngle = calculateAngle(currentPos, target);
        double angleDiff = targetAngle - pose.theta;

        // 归一化角度差到 [-180, 180]
        while (angleDiff > 180.0) angleDiff -= 360.0;
        while (angleDiff < -180.0) angleDiff += 360.0;

        // 先转向目标方向（角度差 > 10度时转向）
        if (std::abs(angleDiff) > 10.0) {
            if (angleDiff > 0) {
                turnLeft(static_cast<int>(std::abs(angleDiff)), speed);
            } else {
                turnRight(static_cast<int>(std::abs(angleDiff)), speed);
            }
            // 等待转向完成（实际应用中应根据反馈判断）
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // 计算距离并前进
        double dist = distance(currentPos, target);
        if (dist > 50.0) {  // 大于 50mm 才移动
            moveForward(static_cast<int>(dist), speed);
            // 等待移动完成
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<int>(dist / speed * 1000)));
        }

        // 更新位姿（简化处理，实际应由视觉系统反馈）
        updatePose(target.x, target.y, targetAngle);
    }

    stop();
    std::cout << "[Robot] 路径执行完成" << std::endl;
    return true;
}

void RobotController::updatePose(int xMm, int yMm, double thetaDeg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_status.pose.x = xMm;
    m_status.pose.y = yMm;
    m_status.pose.theta = thetaDeg;
}

RobotPose RobotController::getPose() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status.pose;
}

RobotStatus RobotController::getStatus() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

bool RobotController::isAtPoint(const Point& target, int toleranceMm) const
{
    RobotPose pose = getPose();
    double dist = std::sqrt(std::pow(target.x - pose.x, 2) +
                            std::pow(target.y - pose.y, 2));
    return dist <= toleranceMm;
}

bool RobotController::isAlignedTo(double targetAngle, double toleranceDeg) const
{
    RobotPose pose = getPose();
    double diff = std::abs(targetAngle - pose.theta);
    while (diff > 180.0) diff = 360.0 - diff;
    return diff <= toleranceDeg;
}

/**
 * @brief 编码运动指令为字节帧
 * 
 * @details 将运动指令转换为符合下位机协议的字节序列。
 *          
 * @param cmd 运动指令结构体
 * 
 * @return std::vector<uint8_t> 编码后的字节帧
 *         - 帧头：0x66
 *         - 命令：运动指令字节码
 *         - 速度：速度值（0-255）
 *         - 数据1-4：参数数据（小端序）
 *         - 校验：XOR 校验和
 *         - 帧尾：0x77
 *         
 * @note 编码流程：
 *       1. 提取命令字节和速度值
 *       2. 根据模式填充数据段（坐标/距离/角度）
 *       3. 计算 XOR 校验和
 *       4. 添加帧头和帧尾
 *       
 * @note 数据编码：
 *       - MOVE_TO_POINT 模式：data1 = x, data2 = y
 *       - 其他模式：data1 = distance, data2 = angle
 *       - 小端序：低字节在前，高字节在后
 *       
 * @warning 此函数仅用于日志记录，实际发送使用 SerialComm::sendCoordinates()。
 *          返回的字节帧仅用于调试和监控，不会直接发送到下位机。
 */
std::vector<uint8_t> RobotController::encodeCommand(const MotionCommand& cmd)
{
    std::vector<uint8_t> frame;

    // 提取命令字节和速度值
    uint8_t cmdByte = static_cast<uint8_t>(cmd.mode);
    uint8_t speed = static_cast<uint8_t>(cmd.speed & 0xFF);

    // 数据段：根据模式填充不同数据
    int data1, data2;
    if (cmd.mode == MotionMode::MOVE_TO_POINT) {
        // 移动到点模式：data1 = x, data2 = y
        data1 = cmd.target.x;
        data2 = cmd.target.y;
    } else {
        // 其他模式：data1 = distance, data2 = angle
        data1 = cmd.distance;
        data2 = cmd.angle;
    }

    // 构建帧
    frame.push_back(FRAME_HEADER);  // 帧头
    frame.push_back(cmdByte);       // 命令
    frame.push_back(speed);         // 速度
    
    // 数据段（小端序）
    frame.push_back(static_cast<uint8_t>(data1 & 0xFF));         // data1 低字节
    frame.push_back(static_cast<uint8_t>((data1 >> 8) & 0xFF));  // data1 高字节
    frame.push_back(static_cast<uint8_t>(data2 & 0xFF));         // data2 低字节
    frame.push_back(static_cast<uint8_t>((data2 >> 8) & 0xFF));  // data2 高字节

    // 计算 XOR 校验和（从命令字节开始）
    uint8_t checksum = 0;
    for (size_t i = 1; i < frame.size(); ++i) {
        checksum ^= frame[i];  // 异或校验
    }
    frame.push_back(checksum);  // 校验和
    frame.push_back(FRAME_TAIL);  // 帧尾

    return frame;
}

void RobotController::notifyStatus()
{
    if (m_statusCb) {
        m_statusCb(getStatus());
    }
}

double RobotController::distance(const Point& a, const Point& b) const
{
    return std::sqrt(std::pow(b.x - a.x, 2) + std::pow(b.y - a.y, 2));
}

double RobotController::calculateAngle(const Point& from, const Point& to) const
{
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    // atan2 返回 [-pi, pi]，转换为 [0, 360)
    double angle = std::atan2(dy, dx) * 180.0 / M_PI;
    if (angle < 0) angle += 360.0;
    return angle;
}

} // namespace gonxun