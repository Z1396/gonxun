/**
 * @file common_types.hpp
 * @brief 项目通用数据类型定义。
 *
 * 定义点坐标、路径、矩形障碍物等通用数据结构，
 * 供路径规划、任务状态机、视觉系统等模块统一引用。
 */

#pragma once

#include "field_constants.hpp"

#include <cstddef>
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

} // namespace gonxun
