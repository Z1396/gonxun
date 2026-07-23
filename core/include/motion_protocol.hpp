/**
 * @file motion_protocol.hpp
 * @brief 四轮全向底盘运动控制通信协议定义。
 *
 * 定义上位机与下位机之间的串口帧结构、命令码、设备地址、
 * 运动模式与方向等常量，以及帧构建辅助函数。
 * 协议格式：Header(0x66) + Addr + Cmd + Data[12] + Checksum + Tail(0x77)
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace gonxun {

// ==== 帧结构常量 ====

/// 帧头标识字节
constexpr uint8_t FRAME_HEADER    = 0x66;
/// 帧尾标识字节
constexpr uint8_t FRAME_TAIL      = 0x77;
/// 数据域最大长度（字节）
constexpr std::size_t FRAME_MAX_DATA = 12;

// ==== 设备地址 ====

/// 上位机（主机）地址
constexpr uint8_t ADDR_HOST     = 0x01;
/// 下位机（底盘控制器）地址
constexpr uint8_t ADDR_CHASSIS  = 0x02;

// ==== 上位机→下位机 命令码 ====

/// 前进（相对方向）
constexpr uint8_t CMD_MOVE_FORWARD  = 0x10;
/// 后退
constexpr uint8_t CMD_MOVE_BACKWARD = 0x11;
/// 左移
constexpr uint8_t CMD_MOVE_LEFT     = 0x12;
/// 右移
constexpr uint8_t CMD_MOVE_RIGHT    = 0x13;
/// 停止运动
constexpr uint8_t CMD_STOP          = 0x14;
/// 设置运动速度
constexpr uint8_t CMD_SET_SPEED     = 0x15;

/// 移动到指定坐标（mm）
constexpr uint8_t CMD_MOVE_TO_POS   = 0x20;
/// 移动到指定网格位置
constexpr uint8_t CMD_MOVE_TO_GRID  = 0x21;
/// 步进移动（按步数+方向，带速度参数）
constexpr uint8_t CMD_MOVE_STEP     = 0x22;
/// 简化步进移动（仅方向+步数，使用下位机默认参数）
constexpr uint8_t CMD_MOVE_STEP_SIMPLE = 0x23;

/// 设置运动模式
constexpr uint8_t CMD_SET_MODE      = 0x30;
/// 紧急停止
constexpr uint8_t CMD_EMERGENCY     = 0x31;

/// 查询底盘状态
constexpr uint8_t CMD_QUERY_STATUS  = 0x40;
/// 查询当前位置
constexpr uint8_t CMD_QUERY_POS     = 0x41;

// ==== 下位机→上位机 命令码 ====

/// 状态反馈
constexpr uint8_t CMD_STATUS_REPORT = 0x80;
/// 位置反馈
constexpr uint8_t CMD_POS_REPORT    = 0x81;
/// 应答确认
constexpr uint8_t CMD_ACK           = 0x90;
/// 应答否认
constexpr uint8_t CMD_NACK          = 0x91;
/// 错误报告
constexpr uint8_t CMD_ERROR         = 0x92;

// ==== 运动模式 ====

/// 水平运动模式（左右优先）
constexpr uint8_t MODE_HORIZON  = 0x01;
/// 垂直运动模式（上下优先）
constexpr uint8_t MODE_VERTICAL = 0x02;
/// 空闲模式
constexpr uint8_t MODE_IDLE     = 0x00;

// ==== 运动方向（与陀螺仪角度对应） ====

/// 向左，对应角度 0°
constexpr uint8_t DIR_LEFT  = 0x00;
/// 向下，对应角度 90°
constexpr uint8_t DIR_DOWN  = 0x01;
/// 向右，对应角度 180°
constexpr uint8_t DIR_RIGHT = 0x02;
/// 向上，对应角度 270°
constexpr uint8_t DIR_UP    = 0x03;

// ==== 应答码 ====

/// 成功
constexpr uint8_t ACK_OK       = 0x00;
/// 校验和错误
constexpr uint8_t ACK_CHECKSUM = 0x01;
/// 底盘忙碌
constexpr uint8_t ACK_BUSY     = 0x02;
/// 非法命令
constexpr uint8_t ACK_INVALID  = 0x03;
/// 底盘内部错误
constexpr uint8_t ACK_ERROR    = 0x04;

// ==== 帧结构定义 ====
#pragma pack(push, 1)

/**
 * @brief 通用帧结构（兼容 SerialComm）。
 *
 * 18 字节定长帧，采用 1 字节对齐确保与串口收发一致。
 * 校验和 = (addr + cmd + data[0..11]) & 0xFF
 */
struct MotionFrame {
    uint8_t header;       ///< 帧头，固定 0x66
    uint8_t addr;         ///< 设备地址
    uint8_t cmd;          ///< 命令码
    uint8_t data[12];     ///< 数据域，12 字节
    uint8_t checksum;     ///< 校验和
    uint8_t tail;         ///< 帧尾，固定 0x77

    /**
     * @brief 重新计算并填入校验和。
     * @note 调用后须确保 data[] 已填充完毕。
     */
    void update_checksum() noexcept {
        checksum = 0;
        checksum += addr;
        checksum += cmd;
        for (int i = 0; i < 12; ++i) {
            checksum += data[i];
        }
        checksum &= 0xFF;
    }

    /**
     * @brief 校验当前帧的校验和是否正确。
     * @return true 表示校验通过
     */
    [[nodiscard]] bool verify_checksum() const noexcept {
        uint8_t sum = 0;
        sum += addr;
        sum += cmd;
        for (int i = 0; i < 12; ++i) {
            sum += data[i];
        }
        return (sum & 0xFF) == checksum;
    }

    /**
     * @brief 将帧序列化为字节向量，用于串口发送。
     * @return 18 字节的完整帧数据
     */
    [[nodiscard]] std::vector<uint8_t> to_bytes() const {
        return {header, addr, cmd,
                data[0], data[1], data[2], data[3],
                data[4], data[5], data[6], data[7],
                data[8], data[9], data[10], data[11],
                checksum, tail};
    }
};

/**
 * @brief 步进移动指令数据域（CMD_MOVE_STEP）。
 *
 * 描述按方向步进的移动参数，速度和加速度采用大端双字节编码。
 */
struct StepMoveData {
    uint8_t direction;     ///< 运动方向，取值 DIR_LEFT/DIR_DOWN/DIR_RIGHT/DIR_UP
    uint8_t steps;         ///< 步进数
    uint8_t speed_h;       ///< 速度高字节
    uint8_t speed_l;       ///< 速度低字节
    uint8_t accel_h;       ///< 加速度高字节
    uint8_t accel_l;       ///< 加速度低字节
    uint8_t reserved[6];   ///< 保留字节

    /// 设置速度（大端编码），范围 0~65535
    void set_speed(uint16_t speed) noexcept {
        speed_h = static_cast<uint8_t>((speed >> 8) & 0xFF);
        speed_l = static_cast<uint8_t>(speed & 0xFF);
    }

    /// 设置加速度（大端编码），范围 0~65535
    void set_accel(uint16_t accel) noexcept {
        accel_h = static_cast<uint8_t>((accel >> 8) & 0xFF);
        accel_l = static_cast<uint8_t>(accel & 0xFF);
    }

    /// 读取速度值
    [[nodiscard]] uint16_t get_speed() const noexcept {
        return (static_cast<uint16_t>(speed_h) << 8) | speed_l;
    }

    /// 读取加速度值
    [[nodiscard]] uint16_t get_accel() const noexcept {
        return (static_cast<uint16_t>(accel_h) << 8) | accel_l;
    }
};

/**
 * @brief 位置控制指令数据域（CMD_MOVE_TO_POS）。
 *
 * 描述移动到目标坐标的参数，包含 mm 级坐标、速度、加速度及网格坐标。
 */
struct PositionMoveData {
    uint8_t target_x_h;    ///< 目标 X 高字节（mm）
    uint8_t target_x_l;    ///< 目标 X 低字节（mm）
    uint8_t target_y_h;    ///< 目标 Y 高字节（mm）
    uint8_t target_y_l;    ///< 目标 Y 低字节（mm）
    uint8_t speed_h;       ///< 速度高字节
    uint8_t speed_l;       ///< 速度低字节
    uint8_t accel_h;       ///< 加速度高字节
    uint8_t accel_l;       ///< 加速度低字节
    uint8_t grid_x;        ///< 网格 X 坐标
    uint8_t grid_y;        ///< 网格 Y 坐标
    uint8_t reserved[2];   ///< 保留字节

    /// 设置目标 X 坐标（mm），大端编码
    void set_target_x(uint16_t x) noexcept {
        target_x_h = static_cast<uint8_t>((x >> 8) & 0xFF);
        target_x_l = static_cast<uint8_t>(x & 0xFF);
    }

    /// 设置目标 Y 坐标（mm），大端编码
    void set_target_y(uint16_t y) noexcept {
        target_y_h = static_cast<uint8_t>((y >> 8) & 0xFF);
        target_y_l = static_cast<uint8_t>(y & 0xFF);
    }

    /// 设置速度（大端编码）
    void set_speed(uint16_t speed) noexcept {
        speed_h = static_cast<uint8_t>((speed >> 8) & 0xFF);
        speed_l = static_cast<uint8_t>(speed & 0xFF);
    }

    /// 设置加速度（大端编码）
    void set_accel(uint16_t accel) noexcept {
        accel_h = static_cast<uint8_t>((accel >> 8) & 0xFF);
        accel_l = static_cast<uint8_t>(accel & 0xFF);
    }

    /// 读取目标 X 坐标（mm）
    [[nodiscard]] uint16_t get_target_x() const noexcept {
        return (static_cast<uint16_t>(target_x_h) << 8) | target_x_l;
    }

    /// 读取目标 Y 坐标（mm）
    [[nodiscard]] uint16_t get_target_y() const noexcept {
        return (static_cast<uint16_t>(target_y_h) << 8) | target_y_l;
    }
};

/**
 * @brief 状态反馈数据域（CMD_STATUS_REPORT）。
 *
 * 下位机返回的当前位置、速度、方向、模式及错误码等信息。
 */
struct StatusReportData {
    uint8_t pos_x_h;       ///< 当前 X 高字节（mm）
    uint8_t pos_x_l;       ///< 当前 X 低字节（mm）
    uint8_t pos_y_h;       ///< 当前 Y 高字节（mm）
    uint8_t pos_y_l;       ///< 当前 Y 低字节（mm）
    uint8_t speed_h;       ///< 速度高字节
    uint8_t speed_l;       ///< 速度低字节
    uint8_t direction;     ///< 当前运动方向
    uint8_t mode;          ///< 当前运动模式
    uint8_t motor_status;  ///< 电机状态位图
    uint8_t seq_num;       ///< 指令序列号
    uint8_t error_code;    ///< 错误码
    uint8_t reserved;      ///< 保留字节

    /// 读取当前 X 坐标（mm）
    [[nodiscard]] uint16_t get_pos_x() const noexcept {
        return (static_cast<uint16_t>(pos_x_h) << 8) | pos_x_l;
    }

    /// 读取当前 Y 坐标（mm）
    [[nodiscard]] uint16_t get_pos_y() const noexcept {
        return (static_cast<uint16_t>(pos_y_h) << 8) | pos_y_l;
    }

    /// 读取当前速度
    [[nodiscard]] uint16_t get_speed() const noexcept {
        return (static_cast<uint16_t>(speed_h) << 8) | speed_l;
    }
};

#pragma pack(pop)

// ==== 辅助函数：方向与角度转换 ====

/**
 * @brief 将陀螺仪角度转换为运动方向码。
 * @param angle 角度值，仅支持 0/90/180/270
 * @return 方向码 DIR_LEFT/DIR_DOWN/DIR_RIGHT/DIR_UP，非法角度返回 DIR_LEFT
 */
[[nodiscard]] inline uint8_t angle_to_direction(int angle) noexcept {
    switch (angle) {
        case 0:   return DIR_LEFT;
        case 90:  return DIR_DOWN;
        case 180: return DIR_RIGHT;
        case 270: return DIR_UP;
        default:  return DIR_LEFT;
    }
}

/**
 * @brief 将运动方向码转换为陀螺仪角度。
 * @param dir 方向码
 * @return 角度值 0/90/180/270，非法方向返回 0
 */
[[nodiscard]] inline int direction_to_angle(uint8_t dir) noexcept {
    switch (dir) {
        case DIR_LEFT:  return 0;
        case DIR_DOWN:  return 90;
        case DIR_RIGHT: return 180;
        case DIR_UP:    return 270;
        default:        return 0;
    }
}

/**
 * @brief 构建步进移动帧（CMD_MOVE_STEP）。
 * @param direction 运动方向，取值 DIR_LEFT/DIR_DOWN/DIR_RIGHT/DIR_UP
 * @param steps 步进数
 * @param speed 运动速度，默认 300
 * @param accel 加速度，默认 500
 * @return 完整的 MotionFrame，可直接通过串口发送
 */
[[nodiscard]] inline MotionFrame build_step_move_frame(
    uint8_t direction, uint8_t steps,
    uint16_t speed = 300, uint16_t accel = 500) noexcept
{
    MotionFrame frame{};
    frame.header = FRAME_HEADER;
    frame.addr = ADDR_CHASSIS;
    frame.cmd = CMD_MOVE_STEP;

    StepMoveData data{};
    data.direction = direction;
    data.steps = steps;
    data.set_speed(speed);
    data.set_accel(accel);

    std::memcpy(frame.data, &data, sizeof(data));
    frame.update_checksum();
    frame.tail = FRAME_TAIL;
    return frame;
}

/**
 * @brief 构建简化步进移动帧（CMD_MOVE_STEP_SIMPLE）。
 *        仅传递方向和步数，速度和加速度使用下位机内置默认值。
 *        适用于下位机已配置固定运动参数的场景，减少传输数据量。
 * @param direction 运动方向，取值 DIR_LEFT/DIR_DOWN/DIR_RIGHT/DIR_UP
 * @param steps 步进数
 * @return 完整的 MotionFrame，数据域仅填充方向和步数
 */
[[nodiscard]] inline MotionFrame build_step_move_simple_frame(
    uint8_t direction, uint8_t steps) noexcept
{
    MotionFrame frame{};
    frame.header = FRAME_HEADER;
    frame.addr = ADDR_CHASSIS;
    frame.cmd = CMD_MOVE_STEP_SIMPLE;

    std::memset(frame.data, 0, sizeof(frame.data));
    frame.data[0] = direction;
    frame.data[1] = steps;

    frame.update_checksum();
    frame.tail = FRAME_TAIL;
    return frame;
}

/**
 * @brief 构建位置控制帧（CMD_MOVE_TO_POS）。
 * @param target_x 目标 X 坐标（mm）
 * @param target_y 目标 Y 坐标（mm）
 * @param speed 运动速度，默认 300
 * @param grid_x 网格 X 坐标，默认 0
 * @param grid_y 网格 Y 坐标，默认 0
 * @return 完整的 MotionFrame
 */
[[nodiscard]] inline MotionFrame build_position_move_frame(
    uint16_t target_x, uint16_t target_y,
    uint16_t speed = 300,
    uint8_t grid_x = 0, uint8_t grid_y = 0) noexcept
{
    MotionFrame frame{};
    frame.header = FRAME_HEADER;
    frame.addr = ADDR_CHASSIS;
    frame.cmd = CMD_MOVE_TO_POS;

    PositionMoveData data{};
    data.set_target_x(target_x);
    data.set_target_y(target_y);
    data.set_speed(speed);
    data.set_accel(500);
    data.grid_x = grid_x;
    data.grid_y = grid_y;

    std::memcpy(frame.data, &data, sizeof(data));
    frame.update_checksum();
    frame.tail = FRAME_TAIL;
    return frame;
}

/**
 * @brief 构建停止帧（CMD_STOP）。
 * @return 数据域全零的停止帧
 */
[[nodiscard]] inline MotionFrame build_stop_frame() noexcept {
    MotionFrame frame{};
    frame.header = FRAME_HEADER;
    frame.addr = ADDR_CHASSIS;
    frame.cmd = CMD_STOP;
    std::memset(frame.data, 0, sizeof(frame.data));
    frame.update_checksum();
    frame.tail = FRAME_TAIL;
    return frame;
}

/**
 * @brief 构建紧急停止帧（CMD_EMERGENCY）。
 * @return 数据域全零的紧急停止帧
 */
[[nodiscard]] inline MotionFrame build_emergency_frame() noexcept {
    MotionFrame frame{};
    frame.header = FRAME_HEADER;
    frame.addr = ADDR_CHASSIS;
    frame.cmd = CMD_EMERGENCY;
    std::memset(frame.data, 0, sizeof(frame.data));
    frame.update_checksum();
    frame.tail = FRAME_TAIL;
    return frame;
}

/**
 * @brief 构建速度设置帧（CMD_SET_SPEED）。
 * @param speed 目标速度，双字节大端编码存入 data[0..1]
 * @return 速度设置帧
 */
[[nodiscard]] inline MotionFrame build_set_speed_frame(uint16_t speed) noexcept {
    MotionFrame frame{};
    frame.header = FRAME_HEADER;
    frame.addr = ADDR_CHASSIS;
    frame.cmd = CMD_SET_SPEED;
    std::memset(frame.data, 0, sizeof(frame.data));
    frame.data[0] = static_cast<uint8_t>((speed >> 8) & 0xFF);
    frame.data[1] = static_cast<uint8_t>(speed & 0xFF);
    frame.update_checksum();
    frame.tail = FRAME_TAIL;
    return frame;
}

/**
 * @brief 构建状态查询帧（CMD_QUERY_STATUS）。
 * @return 数据域全零的查询帧
 */
[[nodiscard]] inline MotionFrame build_query_status_frame() noexcept {
    MotionFrame frame{};
    frame.header = FRAME_HEADER;
    frame.addr = ADDR_CHASSIS;
    frame.cmd = CMD_QUERY_STATUS;
    std::memset(frame.data, 0, sizeof(frame.data));
    frame.update_checksum();
    frame.tail = FRAME_TAIL;
    return frame;
}

} // namespace gonxun
