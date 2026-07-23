/**
 * @file grid5_path_planner.hpp
 * @brief 5×5 网格路径决策系统，支持陀螺仪角度融合。
 *
 * 将场地划分为 5×5 的粗粒度网格，根据起终点坐标计算移动向量，
 * 生成横平竖直的分步移动指令序列（含转向角度），
 * 适用于下位底盘的全向移动控制。
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace gonxun {

/**
 * @brief 5×5 网格坐标，取值范围 x∈[0,4], y∈[0,4]。
 */
struct Grid5Coord {
    int x;  ///< 网格列索引
    int y;  ///< 网格行索引

    bool operator==(const Grid5Coord& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Grid5Coord& other) const {
        return !(*this == other);
    }
};

/**
 * @brief 5×5 网格上的移动向量（Δx, Δy）。
 */
struct Move5Vector {
    int dx;  ///< X 方向位移（格）
    int dy;  ///< Y 方向位移（格）
};

/**
 * @brief 单步移动指令，包含转向信息和移动参数。
 */
struct Move5Command {
    int turn_angle;          ///< 需要转向的角度（归一化到 [-180,180]）
    int target_angle;        ///< 转向后的目标朝向（0/90/180/270）
    int dx;                  ///< X 方向移动格数
    int dy;                  ///< Y 方向移动格数
    int steps;               ///< 移动步数（绝对值）
    std::string direction_name;  ///< 方向名称（中文：左/下/右/上）
};

/**
 * @brief 路径规划决策结果，包含起终点、移动向量及指令序列。
 */
struct Path5Decision {
    Grid5Coord start;                    ///< 起点
    Grid5Coord goal;                     ///< 终点
    Move5Vector vector;                  ///< 移动向量
    int current_angle;                   ///< 初始陀螺仪角度
    std::vector<Move5Command> commands;  ///< 移动指令序列
    bool success;                        ///< 规划是否成功
    std::string error_message;           ///< 错误信息（成功时为空）
};

/**
 * @brief 5×5 网格路径规划器，横平竖直移动 + 陀螺仪角度融合。
 *
 * 先计算起终点的移动向量，再按 prioritize_x 决定先走 X 还是 Y 方向，
 * 每个方向生成一条 Move5Command（含转向 + 直行）。
 */
class Grid5PathPlanner {
public:
    Grid5PathPlanner();

    /**
     * @brief 执行路径规划。
     * @param current 当前网格坐标
     * @param goal 目标网格坐标
     * @param current_angle 当前陀螺仪角度，仅支持 0/90/180/270
     * @param prioritize_x 是否优先走 X 方向，默认 true
     * @return 路径决策结果
     */
    [[nodiscard]] Path5Decision plan(Grid5Coord current, Grid5Coord goal,
                                     int current_angle, bool prioritize_x = true);

    /**
     * @brief 打印路径决策结果到标准输出。
     * @param decision 待打印的决策结果
     */
    void print_decision(const Path5Decision& decision);

private:
    /// 计算两点间的移动向量
    [[nodiscard]] Move5Vector calculate_vector(Grid5Coord from, Grid5Coord to) noexcept;
    /// 将移动向量转换为朝向角度
    [[nodiscard]] int vector_to_angle(int dx, int dy) noexcept;
    /// 将角度归一化到 [-180, 180] 区间
    [[nodiscard]] int normalize_angle(int angle) noexcept;
    /// 将角度转换为中文方向名称
    [[nodiscard]] std::string angle_to_direction_name(int angle);
    /// 验证坐标是否在 5×5 网格范围内
    [[nodiscard]] bool is_valid_coord(Grid5Coord coord) noexcept;

    /// 生成 X 方向的移动指令，返回更新后的朝向角度
    int build_x_commands(int dx, int angle_state,
                         std::vector<Move5Command>& commands);
    /// 生成 Y 方向的移动指令，返回更新后的朝向角度
    int build_y_commands(int dy, int angle_state,
                         std::vector<Move5Command>& commands);
};

} // namespace gonxun
