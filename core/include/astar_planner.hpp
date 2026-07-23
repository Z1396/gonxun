/**
 * @file astar_planner.hpp
 * @brief A* 路径规划算法，对 2400×2400mm 场地进行栅格化搜索。
 *
 * 将场地按 50mm 分辨率离散为 48×48 栅格，在考虑机器人尺寸（300mm 直径）、
 * 固定障碍物（4 个黄色块 + 暂存区 + 粗加工区）和用户自定义障碍物的条件下，
 * 使用 A* 算法搜索无碰撞路径，并通过方向简化去除冗余途经点。
 */

#pragma once

#include "field_constants.hpp"

#include <cmath>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gonxun {

/// 栅格分辨率，每格代表 50mm
constexpr int GRID_RESOLUTION_MM = 50;
/// 栅格边长 = FIELD_SIZE_MM / GRID_RESOLUTION_MM = 48
constexpr int GRID_SIZE = FIELD_SIZE_MM / GRID_RESOLUTION_MM;

// ==== 数据结构 ====

/**
 * @brief 二维点坐标，单位取决于上下文（mm 或栅格索引）。
 */
struct Point {
    int x{0};  ///< X 坐标
    int y{0};  ///< Y 坐标

    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Point& o) const { return x != o.x || y != o.y; }
};

/**
 * @brief Point 的哈希函数，用于 unordered_map/unordered_set。
 */
struct PointHash {
    [[nodiscard]] size_t operator()(const Point& p) const noexcept {
        return static_cast<size_t>(p.x) * 1000 + p.y;
    }
};

/// 路径类型，由一系列 Point 组成
using Path = std::vector<Point>;

/**
 * @brief 轴对齐矩形障碍物，坐标和尺寸单位为 mm。
 */
struct ObstacleRect {
    int x;  ///< 左上角 X
    int y;  ///< 左上角 Y
    int w;  ///< 宽度
    int h;  ///< 高度
};

// ==== A* 路径规划器 ====

/**
 * @brief 基于 A* 算法的栅格路径规划器。
 *
 * 在 48×48 栅格地图上执行四邻域 A* 搜索，考虑机器人半尺寸 150mm 的碰撞膨胀，
 * 支持固定障碍物与用户自定义障碍物，启停区可按需放行。
 */
class AStarPlanner {
public:
    AStarPlanner();

    /**
     * @brief 设置用户自定义障碍物列表。
     * @param obstacles 矩形障碍物向量，坐标单位 mm
     * @note 调用后会重建栅格地图并重新标记固定障碍物
     */
    void set_obstacles(const std::vector<ObstacleRect>& obstacles);

    /**
     * @brief 执行 A* 路径规划。
     * @param start 起点坐标（mm）
     * @param goal 终点坐标（mm）
     * @param allow_start_zone 是否允许路径经过启停区，默认 false
     * @return 简化后的路径（mm 坐标），规划失败返回空路径
     * @note 若起点在障碍物内，会自动在半径 4 格内搜索最近可通行点
     */
    [[nodiscard]] Path plan(const Point& start, const Point& goal,
                            bool allow_start_zone = false);

private:
    /// 将 mm 坐标转换为栅格索引
    [[nodiscard]] int to_grid(int mm) const noexcept;
    /// 将栅格索引转换为 mm 坐标（取格中心）
    [[nodiscard]] int to_mm(int grid) const noexcept;
    /// 将矩形障碍物标记到栅格地图上
    void mark_obstacle_on_grid(const ObstacleRect& obs) noexcept;
    /// 曼哈顿距离启发函数，乘以 10 以与 g_score 对齐
    [[nodiscard]] int heuristic(const Point& a, const Point& b) const noexcept;
    /// 检查栅格位置是否存在碰撞（含边界、固定/用户障碍物、启停区）
    [[nodiscard]] bool check_collision(int grid_x, int grid_y,
                                       bool allow_start_zone) const;
    /// 检查机器人包围盒与单个障碍物矩形是否相交
    [[nodiscard]] bool check_obstacle_collision(int robot_min_x, int robot_max_x,
                                                int robot_min_y, int robot_max_y,
                                                const ObstacleRect& obs) const noexcept;
    /// 获取四邻域相邻栅格点（上下左右）
    [[nodiscard]] std::vector<Point> get_neighbors(const Point& p) const;
    /// 从 came_from 表回溯重建路径
    [[nodiscard]] Path reconstruct_path(
        const std::unordered_map<Point, Point, PointHash>& came_from,
        const Point& current) const;
    /// 去除同方向连续途经点，仅保留拐点
    [[nodiscard]] Path simplify_path(const Path& grid_path) const;

    /// A* 搜索主循环
    [[nodiscard]] Path run_astar_search(const Point& start_grid, const Point& goal_grid,
                                        bool allow_start_zone);
    /// 检查机器人包围盒是否越出场地边界
    [[nodiscard]] bool check_boundary_collision(int center_x, int center_y,
                                                int robot_min_x, int robot_max_x,
                                                int robot_min_y, int robot_max_y) const;
    /// 检查机器人包围盒是否与用户障碍物碰撞
    [[nodiscard]] bool check_user_obstacle_collision(int robot_min_x, int robot_max_x,
                                                     int robot_min_y, int robot_max_y) const;
    /// 判断点是否在启停区内（右下角和右上角两个区域）
    [[nodiscard]] bool is_in_start_zone(int center_x, int center_y) const noexcept;

    std::vector<std::vector<bool>> grid_map_;       ///< 用户障碍物栅格地图，true 表示占用
    std::vector<ObstacleRect> obstacles_;            ///< 用户自定义障碍物
    std::vector<ObstacleRect> fixed_obstacles_;      ///< 固定障碍物（黄色块+暂存区+粗加工区）
};

} // namespace gonxun
