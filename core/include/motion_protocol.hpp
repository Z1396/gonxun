/**
 * @file motion_protocol.hpp
 * @brief 上位机与下位机串口通信协议定义。
 *
 * 协议采用严格 stop-and-wait 交互：上位机发一帧指令后阻塞等待下位机
 * 返回的 done 信号（move_done/grab_done），收到后才发下一条。
 *
 * 发送帧（13 字节）:
 *   [0] header 0x66
 *   [1] mode (1=路径规划, 2=定位/视觉坐标上传)
 *   [2-3] angle (uint16 大端, 真实角度 0/90/180/270)
 *   [4-5] steps (int16 大端，带符号，正=前进，负=后退)
 *   [6-7] x (uint16 大端, mm)
 *   [8-9] y (uint16 大端, mm)
 *   [10] grab (0/1，1=触发抓取)
 *   [11] checksum = bytes[1..10] 累加和 & 0xFF
 *   [12] tail 0x77
 *
 * 接收帧（6 字节）:
 *   [0] header 0x66
 *   [1] match_start (0/1，比赛开始后锁存为 1)
 *   [2] move_done (0/1，每完成一段路径触发一次)
 *   [3] grab_done (0/1，每次抓取完成触发)
 *   [4] checksum = bytes[1..3] 累加和 & 0xFF
 *   [5] tail 0x77
 */
#pragma once

#include <QPair>
#include <QVector>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

namespace gonxun {

// ==== 帧结构常量 ====

constexpr uint8_t FRAME_HEADER   = 0x66;  ///< 帧头标识字节
constexpr uint8_t FRAME_TAIL     = 0x77;  ///< 帧尾标识字节
constexpr std::size_t CMD_FRAME_LEN = 13; ///< 发送帧长度
constexpr std::size_t FB_FRAME_LEN  = 6;  ///< 接收帧长度

// ==== 角度真实值（直接作为协议 angle 字段值传输） ====

constexpr uint16_t ANGLE_0   = 0;   ///< 0°（向左，X 减小方向）
constexpr uint16_t ANGLE_90  = 90;  ///< 90°（向下，Y 增大方向）
constexpr uint16_t ANGLE_180 = 180; ///< 180°（向右，X 增大方向）
constexpr uint16_t ANGLE_270 = 270; ///< 270°（向上，Y 减小方向）

// ==== 模式枚举 ====

/// @brief 发送帧 mode 字段取值
enum class FrameMode : uint8_t {
    Path   = 1,  ///< 路径规划（步进移动）
    Locate = 2   ///< 定位/视觉坐标上传
};

/// @brief 抓取指令取值
enum class GrabAction : uint8_t {
    None   = 0,  ///< 无动作
    Trigger = 1  ///< 触发一次抓取
};

// ==== 帧结构定义 ====

#pragma pack(push, 1)

/**
 * @brief 发送帧（13 字节）。
 *
 * 上位机→下位机的统一指令帧，可同时表达"移动+定位+抓取"组合。
 */
struct CommandFrame {
    uint8_t header;       ///< [0] 帧头 0x66
    uint8_t mode;         ///< [1] 模式 (FrameMode)
    uint8_t angle_hi;     ///< [2] 角度高字节（大端，真实值 0/90/180/270）
    uint8_t angle_lo;     ///< [3] 角度低字节
    uint8_t steps_hi;     ///< [4] 步数高字节（大端，带符号）
    uint8_t steps_lo;     ///< [5] 步数低字节
    uint8_t x_hi;         ///< [6] X 高字节（大端）
    uint8_t x_lo;         ///< [7] X 低字节
    uint8_t y_hi;         ///< [8] Y 高字节（大端）
    uint8_t y_lo;         ///< [9] Y 低字节
    uint8_t grab;         ///< [10] 抓取指令 (GrabAction)
    uint8_t checksum;     ///< [11] 校验和
    uint8_t tail;         ///< [12] 帧尾 0x77

    /// @brief 设置角度（uint16 大端编码）
    void set_angle(uint16_t v) noexcept {
        angle_hi = static_cast<uint8_t>((v >> 8) & 0xFF);
        angle_lo = static_cast<uint8_t>(v & 0xFF);
    }

    /// @brief 读取角度
    [[nodiscard]] uint16_t get_angle() const noexcept {
        return (static_cast<uint16_t>(angle_hi) << 8) | angle_lo;
    }

    /// @brief 设置步数（int16 大端编码）
    void set_steps(int16_t s) noexcept {
        uint16_t v = static_cast<uint16_t>(s);
        steps_hi = static_cast<uint8_t>((v >> 8) & 0xFF);
        steps_lo = static_cast<uint8_t>(v & 0xFF);
    }

    /// @brief 读取步数（int16 大端解码）
    [[nodiscard]] int16_t get_steps() const noexcept {
        return static_cast<int16_t>(
            (static_cast<uint16_t>(steps_hi) << 8) | steps_lo);
    }

    /// @brief 设置 X 坐标（uint16 大端编码）
    void set_x(uint16_t v) noexcept {
        x_hi = static_cast<uint8_t>((v >> 8) & 0xFF);
        x_lo = static_cast<uint8_t>(v & 0xFF);
    }

    /// @brief 读取 X 坐标
    [[nodiscard]] uint16_t get_x() const noexcept {
        return (static_cast<uint16_t>(x_hi) << 8) | x_lo;
    }

    /// @brief 设置 Y 坐标（uint16 大端编码）
    void set_y(uint16_t v) noexcept {
        y_hi = static_cast<uint8_t>((v >> 8) & 0xFF);
        y_lo = static_cast<uint8_t>(v & 0xFF);
    }

    /// @brief 读取 Y 坐标
    [[nodiscard]] uint16_t get_y() const noexcept {
        return (static_cast<uint16_t>(y_hi) << 8) | y_lo;
    }

    /// @brief 重新计算并填入校验和（bytes[1..10] 累加和 & 0xFF）
    void update_checksum() noexcept {
        uint8_t sum = 0;
        sum += mode;
        sum += angle_hi;
        sum += angle_lo;
        sum += steps_hi;
        sum += steps_lo;
        sum += x_hi;
        sum += x_lo;
        sum += y_hi;
        sum += y_lo;
        sum += grab;
        checksum = sum;
    }

    /// @brief 校验当前帧的校验和与帧尾
    [[nodiscard]] bool verify() const noexcept {
        if (header != FRAME_HEADER || tail != FRAME_TAIL) return false;
        uint8_t sum = 0;
        sum += mode;
        sum += angle_hi;
        sum += angle_lo;
        sum += steps_hi;
        sum += steps_lo;
        sum += x_hi;
        sum += x_lo;
        sum += y_hi;
        sum += y_lo;
        sum += grab;
        return sum == checksum;
    }

    /// @brief 序列化为字节向量
    [[nodiscard]] std::vector<uint8_t> to_bytes() const {
        return {header, mode, angle_hi, angle_lo, steps_hi, steps_lo,
                x_hi, x_lo, y_hi, y_lo, grab, checksum, tail};
    }
};

static_assert(sizeof(CommandFrame) == CMD_FRAME_LEN, "CommandFrame size mismatch");

/**
 * @brief 接收帧（6 字节）。
 *
 * 下位机→上位机的反馈帧，携带三个事件标志。
 */
struct FeedbackFrame {
    uint8_t header;       ///< [0] 帧头 0x66
    uint8_t match_start;  ///< [1] 比赛开始标志（锁存）
    uint8_t move_done;    ///< [2] 走路完成事件（边沿）
    uint8_t grab_done;    ///< [3] 抓取完成事件（边沿）
    uint8_t checksum;     ///< [4] 校验和
    uint8_t tail;         ///< [5] 帧尾 0x77

    /// @brief 校验帧头、校验和、帧尾
    [[nodiscard]] bool verify() const noexcept {
        if (header != FRAME_HEADER || tail != FRAME_TAIL) return false;
        uint8_t sum = 0;
        sum += match_start;
        sum += move_done;
        sum += grab_done;
        return sum == checksum;
    }
};

static_assert(sizeof(FeedbackFrame) == FB_FRAME_LEN, "FeedbackFrame size mismatch");

#pragma pack(pop)

// ==== 路径段定义 ====

/// @brief 单段移动指令（一个方向上的连续步数）
struct MoveSegment {
    uint16_t angle;    ///< 移动角度 0/90/180/270（真实值）
    int16_t steps;     ///< 步数（恒为正，方向由 angle 决定）
};

// ==== 帧构建函数 ====

/// @brief 构建路径规划移动帧（mode=Path, grab=0, x=y=0）
/// @param angle 移动角度 0/90/180/270
/// @param steps 步数（带符号，正=前进，负=后退）
/// @return 完整的 CommandFrame
[[nodiscard]] inline CommandFrame build_move_frame(uint16_t angle, int16_t steps) noexcept {
    CommandFrame f{};
    f.header = FRAME_HEADER;
    f.mode = static_cast<uint8_t>(FrameMode::Path);
    f.set_angle(angle);
    f.set_steps(steps);
    f.set_x(0);
    f.set_y(0);
    f.grab = static_cast<uint8_t>(GrabAction::None);
    f.update_checksum();
    f.tail = FRAME_TAIL;
    return f;
}

/// @brief 构建视觉定位帧（mode=Locate）
/// @param x 物料 X 坐标 (mm)
/// @param y 物料 Y 坐标 (mm)
/// @param grab 抓取指令 0/1
/// @return 完整的 CommandFrame
[[nodiscard]] inline CommandFrame build_locate_frame(uint16_t x, uint16_t y, uint8_t grab) noexcept {
    CommandFrame f{};
    f.header = FRAME_HEADER;
    f.mode = static_cast<uint8_t>(FrameMode::Locate);
    f.set_angle(0);
    f.set_steps(0);
    f.set_x(x);
    f.set_y(y);
    f.grab = grab;
    f.update_checksum();
    f.tail = FRAME_TAIL;
    return f;
}

/// @brief 构建纯抓取帧（mode=Path, steps=0, grab=1）
/// @return 完整的 CommandFrame
[[nodiscard]] inline CommandFrame build_grab_frame() noexcept {
    CommandFrame f{};
    f.header = FRAME_HEADER;
    f.mode = static_cast<uint8_t>(FrameMode::Path);
    f.set_angle(0);
    f.set_steps(0);
    f.set_x(0);
    f.set_y(0);
    f.grab = static_cast<uint8_t>(GrabAction::Trigger);
    f.update_checksum();
    f.tail = FRAME_TAIL;
    return f;
}

// ==== 帧解析函数 ====

/// @brief 从字节流解析接收帧。
///        要求 data 至少 6 字节；不进行 resync，仅校验单帧。
/// @param data 数据指针
/// @param n 数据长度
/// @return 校验通过返回 FeedbackFrame，否则 std::nullopt
[[nodiscard]] inline std::optional<FeedbackFrame> parse_feedback(
    const uint8_t* data, std::size_t n) noexcept
{
    if (data == nullptr || n < FB_FRAME_LEN) return std::nullopt;
    FeedbackFrame f{};
    std::memcpy(&f, data, FB_FRAME_LEN);
    if (!f.verify()) return std::nullopt;
    return f;
}

// ==== 路径分段函数 ====

/// @brief 将格子增量映射为 (angle, steps)。
///        约定沿用赛场坐标系：dx>0→ANGLE_0(左), dx<0→ANGLE_180(右),
///        dy>0→ANGLE_90(下), dy<0→ANGLE_270(上)。steps 恒为正。
/// @param dx X 增量
/// @param dy Y 增量
/// @return (角度, 步数)；dx==0 && dy==0 时返回 (0, 0)
[[nodiscard]] inline std::pair<uint16_t, int16_t> grid_delta_to_move(int dx, int dy) noexcept {
    if (dx > 0) return {ANGLE_0,   static_cast<int16_t>(dx)};
    if (dx < 0) return {ANGLE_180, static_cast<int16_t>(-dx)};
    if (dy > 0) return {ANGLE_90,  static_cast<int16_t>(dy)};
    if (dy < 0) return {ANGLE_270, static_cast<int16_t>(-dy)};
    return {ANGLE_0, 0};
}

/// @brief 判断两组格子增量是否同方向（可合并为一段）
/// @param dx1 第一组 dx
/// @param dy1 第一组 dy
/// @param dx2 第二组 dx
/// @param dy2 第二组 dy
/// @return true 表示方向相同
[[nodiscard]] inline bool same_direction(int dx1, int dy1, int dx2, int dy2) noexcept {
    // 同方向 ⇔ (dx 同号且都非零) 或 (dy 同号且都非零)
    if (dx1 != 0 && dx2 != 0) {
        return (dx1 > 0) == (dx2 > 0);
    }
    if (dy1 != 0 && dy2 != 0) {
        return (dy1 > 0) == (dy2 > 0);
    }
    return false;
}

/// @brief 将格子路径按方向分段合并。
///        连续同方向的格子合并为一段，方向变化时闭合当前段并开启新段。
///        例如 [(0,0),(1,0),(2,0),(2,1)] → [(0°, 2步), (90°, 1步)]
/// @param grid_path 格子坐标路径 [(x0,y0), (x1,y1), ...]
/// @return 分段后的 MoveSegment 列表
[[nodiscard]] inline QVector<MoveSegment> segment_grid_path(
    const QVector<QPair<int, int>>& grid_path)
{
    QVector<MoveSegment> segments;
    if (grid_path.size() < 2) return segments;

    auto [cur_angle, cur_steps] = grid_delta_to_move(
        grid_path[1].first - grid_path[0].first,
        grid_path[1].second - grid_path[0].second);

    for (int i = 2; i < grid_path.size(); ++i) {
        int dx = grid_path[i].first - grid_path[i - 1].first;
        int dy = grid_path[i].second - grid_path[i - 1].second;
        auto [angle, steps] = grid_delta_to_move(dx, dy);

        // 沿用上一段的增量判断是否同方向
        int prev_dx = grid_path[i - 1].first - grid_path[i - 2].first;
        int prev_dy = grid_path[i - 1].second - grid_path[i - 2].second;

        if (steps > 0 && same_direction(prev_dx, prev_dy, dx, dy)) {
            cur_steps = static_cast<int16_t>(cur_steps + steps);
        } else {
            if (cur_steps > 0) {
                segments.append({cur_angle, cur_steps});
            }
            cur_angle = angle;
            cur_steps = steps;
        }
    }

    // 收尾段
    if (cur_steps > 0) {
        segments.append({cur_angle, cur_steps});
    }
    return segments;
}

} // namespace gonxun
