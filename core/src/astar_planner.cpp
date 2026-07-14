/**
 * A* 路径规划器实现
 */

#include "astar_planner.hpp"
#include <algorithm>
#include <queue>
#include <cmath>
#include <iostream>

namespace gonxun {

// A* 节点（用于优先队列）
struct AStarNode {
    Point pos;
    int g;  // 已走代价
    int f;  // 总代价 g + h

    bool operator>(const AStarNode& other) const {
        return f > other.f;
    }
};

AStarPlanner::AStarPlanner()
    : m_gridMap(GRID_SIZE, std::vector<bool>(GRID_SIZE, false))
{
    // 默认加入4块黄色障碍区域（450×450mm）
    // 这些区域将场地分割成3横3纵的通道
    addObstacle({550, 550, 450, 450});     // 左上黄色块
    addObstacle({1400, 550, 450, 450});    // 右上黄色块
    addObstacle({550, 1400, 450, 450});    // 左下黄色块
    addObstacle({1400, 1400, 450, 450});   // 右下黄色块
}

void AStarPlanner::setObstacles(const std::vector<ObstacleRect>& obstacles)
{
    m_obstacles = obstacles;
    // 重置栅格地图
    for (auto& row : m_gridMap) {
        std::fill(row.begin(), row.end(), false);
    }
    // 始终加入4块黄色障碍区域（场地固有障碍）
    static const ObstacleRect yellowBlocks[] = {
        {550, 550, 450, 450},   // 左上
        {1400, 550, 450, 450},  // 右上
        {550, 1400, 450, 450},  // 左下
        {1400, 1400, 450, 450}, // 右下
    };
    for (const auto& obs : yellowBlocks) {
        markObstacleOnGrid(obs);
    }
    // 加入用户自定义障碍物（含15个可标记障碍块）
    for (const auto& obs : m_obstacles) {
        markObstacleOnGrid(obs);
    }
}

void AStarPlanner::markObstacleOnGrid(const ObstacleRect& obs)
{
    int gxStart = toGrid(obs.x);
    int gyStart = toGrid(obs.y);
    int gxEnd = toGrid(obs.x + obs.w);
    int gyEnd = toGrid(obs.y + obs.h);
    for (int gx = gxStart; gx < gxEnd && gx < GRID_SIZE; ++gx) {
        for (int gy = gyStart; gy < gyEnd && gy < GRID_SIZE; ++gy) {
            if (gx >= 0 && gy >= 0) {
                m_gridMap[gx][gy] = true;
            }
        }
    }
}

void AStarPlanner::addObstacle(const ObstacleRect& obs)
{
    m_obstacles.push_back(obs);
    markObstacleOnGrid(obs);
}

void AStarPlanner::clearObstacles()
{
    m_obstacles.clear();
    // 重置栅格地图
    for (auto& row : m_gridMap) {
        std::fill(row.begin(), row.end(), false);
    }
    // 始终重新加入4块黄色障碍区域（场地固有障碍）
    static const ObstacleRect yellowBlocks[] = {
        {550, 550, 450, 450},   // 左上
        {1400, 550, 450, 450},  // 右上
        {550, 1400, 450, 450},  // 左下
        {1400, 1400, 450, 450}, // 右下
    };
    for (const auto& obs : yellowBlocks) {
        markObstacleOnGrid(obs);
    }
}

Path AStarPlanner::plan(const Point& start, const Point& goal)
{
    // 转换为栅格坐标
    Point startGrid{toGrid(start.x), toGrid(start.y)};
    Point goalGrid{toGrid(goal.x), toGrid(goal.y)};

    // 边界检查
    if (startGrid.x < 0 || startGrid.x >= GRID_SIZE ||
        startGrid.y < 0 || startGrid.y >= GRID_SIZE ||
        goalGrid.x < 0 || goalGrid.x >= GRID_SIZE) {
        std::cerr << "[AStar] 坐标越界" << std::endl;
        return {};
    }

    // 检查目标是否在障碍物内
    if (!isWalkable(goalGrid.x, goalGrid.y)) {
        std::cerr << "[AStar] 目标点在障碍物内" << std::endl;
        return {};
    }

    // 如果起点不可通行，找最近的可行点
    if (!isWalkable(startGrid.x, startGrid.y)) {
        bool found = false;
        for (int r = 1; r < 5 && !found; ++r) {
            for (int dx = -r; dx <= r && !found; ++dx) {
                for (int dy = -r; dy <= r && !found; ++dy) {
                    int nx = startGrid.x + dx;
                    int ny = startGrid.y + dy;
                    if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE && isWalkable(nx, ny)) {
                        startGrid.x = nx;
                        startGrid.y = ny;
                        found = true;
                    }
                }
            }
        }
        if (!found) {
            std::cerr << "[AStar] 起点周围无可通行点" << std::endl;
            return {};
        }
    }

    // A* 核心算法
    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;
    std::unordered_set<Point, PointHash> closedSet;
    std::unordered_map<Point, Point, PointHash> cameFrom;
    std::unordered_map<Point, int, PointHash> gScore;

    gScore[startGrid] = 0;
    openSet.push({startGrid, 0, heuristic(startGrid, goalGrid)});

    while (!openSet.empty()) {
        AStarNode current = openSet.top();
        openSet.pop();

        // 到达目标
        if (current.pos == goalGrid) {
            Path gridPath = reconstructPath(cameFrom, current.pos);
            return simplifyPath(gridPath);
        }

        // 已处理过则跳过
        if (closedSet.count(current.pos)) {
            continue;
        }
        closedSet.insert(current.pos);

        // 遍历邻居
        for (const auto& neighbor : getNeighbors(current.pos)) {
            if (closedSet.count(neighbor)) {
                continue;
            }
            if (!isWalkable(neighbor.x, neighbor.y)) {
                continue;
            }

            // 计算移动代价（横平竖直统一代价=10）
            int moveCost = 10;
            int tentativeG = gScore[current.pos] + moveCost;

            if (!gScore.count(neighbor) || tentativeG < gScore[neighbor]) {
                cameFrom[neighbor] = current.pos;
                gScore[neighbor] = tentativeG;
                int f = tentativeG + heuristic(neighbor, goalGrid);
                openSet.push({neighbor, tentativeG, f});
            }
        }
    }

    // 无路径
    std::cerr << "[AStar] 未找到路径" << std::endl;
    return {};
}

bool AStarPlanner::isInObstacle(int xMm, int yMm) const
{
    int gx = toGrid(xMm);
    int gy = toGrid(yMm);
    if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE) {
        return true;  // 越界视为障碍物
    }
    return m_gridMap[gx][gy];
}

bool AStarPlanner::isWalkable(int gridX, int gridY) const
{
    if (gridX < 0 || gridX >= GRID_SIZE || gridY < 0 || gridY >= GRID_SIZE) {
        return false;
    }
    return !m_gridMap[gridX][gridY];
}

int AStarPlanner::toGrid(int mm) const
{
    return mm / GRID_RESOLUTION_MM;
}

int AStarPlanner::toMm(int grid) const
{
    return grid * GRID_RESOLUTION_MM + GRID_RESOLUTION_MM / 2;
}

int AStarPlanner::heuristic(const Point& a, const Point& b) const
{
    // 曼哈顿距离（四方向启发函数，横平竖直）
    return 10 * (std::abs(a.x - b.x) + std::abs(a.y - b.y));
}

std::vector<Point> AStarPlanner::getNeighbors(const Point& p) const
{
    // 4方向移动：上下左右（横平竖直，不允许斜向）
    static const int dx[] = {0, 0, 1, -1};
    static const int dy[] = {1, -1, 0, 0};

    std::vector<Point> neighbors;
    for (int i = 0; i < 4; ++i) {
        Point n{p.x + dx[i], p.y + dy[i]};
        if (n.x >= 0 && n.x < GRID_SIZE && n.y >= 0 && n.y < GRID_SIZE) {
            neighbors.push_back(n);
        }
    }
    return neighbors;
}

Path AStarPlanner::reconstructPath(const std::unordered_map<Point, Point, PointHash>& cameFrom,
                                    const Point& current) const
{
    Path path;
    Point curr = current;
    path.push_back(curr);
    while (cameFrom.count(curr)) {
        curr = cameFrom.at(curr);
        path.push_back(curr);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

Path AStarPlanner::simplifyPath(const Path& gridPath) const
{
    if (gridPath.size() <= 2) {
        // 直接转换为毫米坐标
        Path result;
        for (const auto& p : gridPath) {
            result.push_back({toMm(p.x), toMm(p.y)});
        }
        return result;
    }

    // 路径简化：只保留方向改变的拐点（Line-of-Sight 简化）
    Path simplified;
    simplified.push_back({toMm(gridPath[0].x), toMm(gridPath[0].y)});

    for (size_t i = 1; i < gridPath.size() - 1; ++i) {
        int dx1 = gridPath[i].x - gridPath[i - 1].x;
        int dy1 = gridPath[i].y - gridPath[i - 1].y;
        int dx2 = gridPath[i + 1].x - gridPath[i].x;
        int dy2 = gridPath[i + 1].y - gridPath[i].y;

        // 方向改变时保留该点
        if (dx1 != dx2 || dy1 != dy2) {
            simplified.push_back({toMm(gridPath[i].x), toMm(gridPath[i].y)});
        }
    }

    simplified.push_back({toMm(gridPath.back().x), toMm(gridPath.back().y)});
    return simplified;
}

} // namespace gonxun