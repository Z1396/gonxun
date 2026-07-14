/**
 * 3×3 网格路径规划器实现
 */

#include "grid_planner.hpp"
#include <queue>
#include <unordered_map>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace gonxun {

// 网格坐标哈希（用于 unordered_map）
struct GridCoordHash {
    size_t operator()(const GridCoord& c) const {
        return static_cast<size_t>(c.x) * 10 + c.y;
    }
};

GridPlanner::GridPlanner()
{
    // 全部9格可通行
    // 4块黄色障碍区域在格子之间（不在格子内），不阻挡任何格子
    for (auto& row : m_walkable) {
        row.fill(true);
    }
}

bool GridPlanner::isWalkable(int x, int y) const
{
    if (x < 0 || x >= 3 || y < 0 || y >= 3) return false;
    return m_walkable[x][y];
}

void GridPlanner::setBlocked(int x, int y, bool blocked)
{
    if (x >= 0 && x < 3 && y >= 0 && y < 3) {
        m_walkable[x][y] = !blocked;
    }
}

GridPath GridPlanner::plan(const GridCoord& start, const GridCoord& goal)
{
    // 边界检查
    if (!isWalkable(start) || !isWalkable(goal)) {
        std::cerr << "[GridPlanner] 起点或终点不可通行" << std::endl;
        return {};
    }
    if (start == goal) {
        return {start};
    }

    // BFS 广度优先搜索（3×3 网格很小，BFS 足够）
    std::queue<GridCoord> q;
    std::unordered_map<GridCoord, GridCoord, GridCoordHash> cameFrom;
    std::unordered_map<GridCoord, bool, GridCoordHash> visited;

    q.push(start);
    visited[start] = true;
    cameFrom[start] = start;

    bool found = false;
    while (!q.empty()) {
        GridCoord current = q.front();
        q.pop();

        if (current == goal) {
            found = true;
            break;
        }

        for (const auto& next : getNeighbors(current)) {
            if (visited[next]) continue;
            if (!isWalkable(next)) continue;

            visited[next] = true;
            cameFrom[next] = current;
            q.push(next);
        }
    }

    if (!found) {
        std::cerr << "[GridPlanner] 未找到路径: "
                  << toString(start) << " -> " << toString(goal) << std::endl;
        return {};
    }

    // 重建路径
    GridPath path;
    GridCoord current = goal;
    while (current != start) {
        path.push_back(current);
        current = cameFrom[current];
    }
    path.push_back(start);
    std::reverse(path.begin(), path.end());

    return path;
}

std::vector<GridCoord> GridPlanner::getNeighbors(const GridCoord& c) const
{
    // 4方向：上下左右（横平竖直）
    static const int dx[] = {0, 0, 1, -1};
    static const int dy[] = {1, -1, 0, 0};

    std::vector<GridCoord> neighbors;
    for (int i = 0; i < 4; ++i) {
        GridCoord n{c.x + dx[i], c.y + dy[i]};
        if (n.x >= 0 && n.x < 3 && n.y >= 0 && n.y < 3) {
            neighbors.push_back(n);
        }
    }
    return neighbors;
}

int GridPlanner::gridToFieldX(int gx)
{
    // 3条纵向通道的中心 x 坐标
    // x=0 → 右侧通道中心 2125mm (x=1850-2400)
    // x=1 → 中间通道中心 1200mm (x=1000-1400)
    // x=2 → 左侧通道中心 275mm  (x=0-550)
    static const int fieldX[] = {2125, 1200, 275};
    return fieldX[gx];
}

int GridPlanner::gridToFieldY(int gy)
{
    // 3条横向通道的中心 y 坐标
    // y=0 → 顶部通道中心 275mm  (y=0-550)
    // y=1 → 中间通道中心 1200mm (y=1000-1400)
    // y=2 → 底部通道中心 2125mm (y=1850-2400)
    static const int fieldY[] = {275, 1200, 2125};
    return fieldY[gy];
}

GridCoord GridPlanner::fieldToGrid(int xMm, int yMm)
{
    // 根据场地坐标判断在哪条通道
    int gx, gy;

    // X轴：判断在哪条纵向通道
    if (xMm >= 1850)      gx = 0;  // 右侧通道
    else if (xMm >= 1000) gx = 1;  // 中间通道
    else                   gx = 2;  // 左侧通道

    // Y轴：判断在哪条横向通道
    if (yMm >= 1850)      gy = 2;  // 底部通道
    else if (yMm >= 1000) gy = 1;  // 中间通道
    else                   gy = 0;  // 顶部通道

    return {gx, gy};
}

std::string GridPlanner::toString(const GridCoord& c)
{
    std::ostringstream ss;
    ss << "(" << c.x << "," << c.y << ")";
    return ss.str();
}

std::string GridPlanner::pathToString(const GridPath& path)
{
    std::ostringstream ss;
    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) ss << " -> ";
        ss << toString(path[i]);
    }
    return ss.str();
}

std::string GridPlanner::dumpGrid(const GridPath& highlight) const
{
    std::ostringstream ss;
    ss << "\n  3×3 网格地图:\n";
    ss << "  ┌─────┬─────┬─────┐\n";

    for (int y = 0; y < 3; ++y) {
        ss << "  │";
        for (int x = 0; x < 3; ++x) {
            // 检查是否在路径中
            bool inPath = false;
            for (const auto& p : highlight) {
                if (p.x == x && p.y == y) {
                    inPath = true;
                    break;
                }
            }

            if (inPath) {
                ss << " ◉◉◉ ";
            } else if (isWalkable(x, y)) {
                ss << "     ";
            } else {
                ss << " ■■■ ";
            }
            ss << "│";
        }
        ss << "\n";
        if (y < 2) {
            ss << "  ├─────┼─────┼─────┤\n";
        }
    }
    ss << "  └─────┴─────┴─────┘\n";
    ss << "  (0," << 0 << ")  (1," << 0 << ")  (2," << 0 << ")\n";
    return ss.str();
}

} // namespace gonxun