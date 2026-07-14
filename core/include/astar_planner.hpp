/**
 * A* 路径规划模块
 * 在 2400x2400mm 场地上进行栅格化路径规划
 * 支持障碍物规避、路径优化
 */
#pragma once

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <cmath>

namespace gonxun {

// 场地参数
constexpr int FIELD_SIZE_MM = 2400;       // 场地尺寸 2400x2400mm
constexpr int GRID_RESOLUTION_MM = 50;    // 栅格分辨率 50mm/格
constexpr int GRID_SIZE = FIELD_SIZE_MM / GRID_RESOLUTION_MM; // 48x48 格

// 二维点坐标（毫米）
struct Point {
    int x{0};
    int y{0};

    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Point& o) const { return x != o.x || y != o.y; }
};

// Point 哈希函数（用于 unordered_set/map）
struct PointHash {
    size_t operator()(const Point& p) const {
        return static_cast<size_t>(p.x) * 1000 + p.y;
    }
};

// 路径类型
using Path = std::vector<Point>;

// 障碍物矩形（毫米坐标）
struct ObstacleRect {
    int x, y, w, h;  // 左上角坐标 + 宽高
};

/**
 * A* 路径规划器
 */
class AStarPlanner {
public:
    AStarPlanner();

    /**
     * 设置障碍物列表
     * @param obstacles 障碍物矩形数组（毫米坐标）
     */
    void setObstacles(const std::vector<ObstacleRect>& obstacles);

    /**
     * 添加单个障碍物
     */
    void addObstacle(const ObstacleRect& obs);

    /**
     * 清空障碍物
     */
    void clearObstacles();

    /**
     * 规划路径
     * @param start 起点（毫米）
     * @param goal  终点（毫米）
     * @return 路径点数组（毫米），空数组表示无路径
     */
    Path plan(const Point& start, const Point& goal);

    /**
     * 获取栅格地图（调试用）
     * @return 48x48 的二维数组，true=障碍物
     */
    const std::vector<std::vector<bool>>& getGridMap() const { return m_gridMap; }

    /**
     * 检查点是否在障碍物内
     */
    bool isInObstacle(int xMm, int yMm) const;

    /**
     * 检查栅格是否可通行
     */
    bool isWalkable(int gridX, int gridY) const;

private:
    // 毫米坐标转栅格坐标
    int toGrid(int mm) const;
    // 栅格坐标转毫米坐标
    int toMm(int grid) const;
    // 将障碍物标记到栅格地图上
    void markObstacleOnGrid(const ObstacleRect& obs);
    // 曼哈顿距离启发函数
    int heuristic(const Point& a, const Point& b) const;
    // 获取邻居节点（4方向，横平竖直）
    std::vector<Point> getNeighbors(const Point& p) const;
    // 重建路径
    Path reconstructPath(const std::unordered_map<Point, Point, PointHash>& cameFrom,
                         const Point& current) const;
    // 将栅格路径转换为毫米路径并简化
    Path simplifyPath(const Path& gridPath) const;

    std::vector<std::vector<bool>> m_gridMap; // 栅格地图
    std::vector<ObstacleRect> m_obstacles;    // 障碍物列表
};

} // namespace gonxun