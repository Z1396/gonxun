/**
 * 机器人控制器实现
 */

#include "robot_controller.hpp"
#include "serial_comm.hpp"
#include <cmath>
#include <iostream>
#include <thread>
#include <chrono>

namespace gonxun {

// 运动指令字节码（与下位机协议对应）
constexpr uint8_t CMD_STOP          = 0x00;
constexpr uint8_t CMD_FORWARD       = 0x01;
constexpr uint8_t CMD_BACKWARD      = 0x02;
constexpr uint8_t CMD_TURN_LEFT     = 0x03;
constexpr uint8_t CMD_TURN_RIGHT    = 0x04;
constexpr uint8_t CMD_LATERAL_LEFT  = 0x05;
constexpr uint8_t CMD_LATERAL_RIGHT = 0x06;
constexpr uint8_t CMD_MOVE_TO_POINT = 0x0A;

RobotController::RobotController(SerialComm* serial)
    : m_serial(serial)
{
    m_status = {};
    m_status.currentMode = MotionMode::STOP;
    m_status.isMoving = false;
    m_status.isAligned = true;
    m_status.batteryLevel = 100;
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

bool RobotController::followPath(const Path& path, int speed)
{
    if (path.empty()) {
        std::cerr << "[Robot] 路径为空" << std::endl;
        return false;
    }

    std::cout << "[Robot] 开始沿路径移动，共 " << path.size() << " 个点" << std::endl;

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

std::vector<uint8_t> RobotController::encodeCommand(const MotionCommand& cmd)
{
    // 命令编码（与下位机协议对应）
    // 帧格式: [帧头 0x66] [命令] [速度] [数据1] [数据2] [数据3] [数据4] [校验] [帧尾 0x77]
    std::vector<uint8_t> frame;

    uint8_t cmdByte = static_cast<uint8_t>(cmd.mode);
    uint8_t speed = static_cast<uint8_t>(cmd.speed & 0xFF);

    // 数据段：根据模式填充不同数据
    int data1, data2;
    if (cmd.mode == MotionMode::MOVE_TO_POINT) {
        data1 = cmd.target.x;
        data2 = cmd.target.y;
    } else {
        data1 = cmd.distance;
        data2 = cmd.angle;
    }

    // 构建帧（使用 SerialComm 的 sendCoordinates 接口，这里仅返回编码用于日志）
    frame.push_back(FRAME_HEADER);
    frame.push_back(cmdByte);
    frame.push_back(speed);
    frame.push_back(static_cast<uint8_t>(data1 & 0xFF));
    frame.push_back(static_cast<uint8_t>((data1 >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(data2 & 0xFF));
    frame.push_back(static_cast<uint8_t>((data2 >> 8) & 0xFF));

    // 校验和
    uint8_t checksum = 0;
    for (size_t i = 1; i < frame.size(); ++i) {
        checksum ^= frame[i];
    }
    frame.push_back(checksum);
    frame.push_back(FRAME_TAIL);

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