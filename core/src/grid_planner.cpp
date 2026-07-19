/**
 * @file grid_planner.cpp
 * @brief 3×3 网格路径规划器实现文件
 * 
 * @details 本文件实现了基于 BFS 的 3×3 网格路径规划功能。
 *          核心特性：
 *          - 网格抽象：将场地抽象为 3×3 网格（9个格子）
 *          - BFS 搜索：广度优先搜索，保证最短路径
 *          - 场地映射：网格坐标 ↔ 场地坐标的转换
 *          - 横平竖直：四方向移动（上下左右）
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，实现基础 BFS 算法
 *       - 2025-02-15: 增加场地坐标映射
 *       
 * @note 网格布局：
 *       - 3×3 网格，共9个格子
 *       - 4块黄色障碍区域在格子之间（不在格子内）
 *       - 所有格子默认可通行
 *       
 * @note 坐标系统：
 *       - 网格坐标：x ∈ [0,2], y ∈ [0,2]
 *       - 场地坐标：x ∈ [0,2400mm], y ∈ [0,2400mm]
 *       - 映射关系：
 *         - x=0 → 右侧通道（1850-2400mm）
 *         - x=1 → 中间通道（1000-1400mm）
 *         - x=2 → 左侧通道（0-550mm）
 *         - y=0 → 顶部通道（0-550mm）
 *         - y=1 → 中间通道（1000-1400mm）
 *         - y=2 → 底部通道（1850-2400mm）
 *       
 * @note 算法选择：
 *       - 3×3 网格很小，BFS 足够高效
 *       - BFS 保证找到最短路径
 *       - 时间复杂度：O(N) = O(9) = 常数时间
 *       
 * @see grid_planner.hpp
 */
#include "grid_planner.hpp"
#include <queue>
#include <unordered_map>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace gonxun {

/**
 * @brief 网格坐标哈希结构体（用于 unordered_map）
 * 
 * @details 将 GridCoord 映射为 size_t 类型，用于哈希表索引。
 *          哈希函数：x * 10 + y（简单且无冲突）
 */
struct GridCoordHash {
    /**
     * @brief 哈希函数
     * 
     * @param c 网格坐标
     * @return size_t 哈希值
     */
    size_t operator()(const GridCoord& c) const {
        return static_cast<size_t>(c.x) * 10 + c.y;  // 10 进制编码
    }
};

/**
 * @brief 构造函数，初始化网格地图
 * 
 * @details 将所有9个格子设置为可通行状态。
 *          4块黄色障碍区域在格子之间，不阻挡任何格子。
 */
GridPlanner::GridPlanner()
{
    // 全部9格可通行
    // 4块黄色障碍区域在格子之间（不在格子内），不阻挡任何格子
    for (auto& row : m_walkable) {
        row.fill(true);  // 所有格子初始化为可通行
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

/**
 * @brief 规划从起点到终点的网格路径
 * 
 * @details 使用 BFS 算法寻找最短路径。
 *          
 * @param start 起点网格坐标
 * @param goal 终点网格坐标
 * 
 * @return GridPath 网格路径列表，如果失败则返回空列表
 * 
 * @note 算法流程：
 *       1. 边界检查：确保起点和终点可通行
 *       2. BFS 搜索：使用队列扩展节点
 *       3. 路径重建：从终点回溯到起点
 *       
 * @note BFS 数据结构：
 *       - queue: 待探索队列（先进先出）
 *       - visited: 已访问标记
 *       - cameFrom: 路径回溯映射
 *       
 * @note 性能：
 *       - 时间复杂度：O(N) = O(9) = 常数时间
 *       - 空间复杂度：O(N) = O(9) = 常数空间
 */
GridPath GridPlanner::plan(const GridCoord& start, const GridCoord& goal)
{
    // 边界检查
    if (!isWalkable(start) || !isWalkable(goal)) {
        std::cerr << "[GridPlanner] 起点或终点不可通行" << std::endl;
        return {};
    }
    if (start == goal) {
        return {start};  // 起点等于终点，直接返回
    }

    // BFS 广度优先搜索
    std::queue<GridCoord> q;  // 待探索队列
    std::unordered_map<GridCoord, GridCoord, GridCoordHash> cameFrom;  // 路径回溯映射
    std::unordered_map<GridCoord, bool, GridCoordHash> visited;  // 已访问标记

    // 初始化起点
    q.push(start);
    visited[start] = true;
    cameFrom[start] = start;

    bool found = false;
    
    // 主循环：扩展节点直到找到终点或队列为空
    while (!q.empty()) {
        GridCoord current = q.front();  // 取出队首节点
        q.pop();

        // 到达目标
        if (current == goal) {
            found = true;
            break;
        }

        // 遍历邻居（四方向：上下左右）
        for (const auto& next : getNeighbors(current)) {
            if (visited[next]) continue;  // 已访问则跳过
            if (!isWalkable(next)) continue;  // 不可通行则跳过

            visited[next] = true;  // 标记为已访问
            cameFrom[next] = current;  // 记录回溯关系
            q.push(next);  // 加入队列
        }
    }

    // 检查是否找到路径
    if (!found) {
        std::cerr << "[GridPlanner] 未找到路径: "
                  << toString(start) << " -> " << toString(goal) << std::endl;
        return {};
    }

    // 重建路径：从终点回溯到起点
    GridPath path;
    GridCoord current = goal;
    while (current != start) {
        path.push_back(current);
        current = cameFrom[current];
    }
    path.push_back(start);
    std::reverse(path.begin(), path.end());  // 反转路径（起点→终点）

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

/**
 * @brief 网格 X 坐标转换为场地 X 坐标
 * 
 * @details 将网格 x 坐标映射到场地纵向通道的中心 x 坐标。
 *          
 * @param gx 网格 X 坐标（0, 1, 2）
 * 
 * @return int 场地 X 坐标（毫米）
 *         - gx=0 → 2125mm（右侧通道中心）
 *         - gx=1 → 1200mm（中间通道中心）
 *         - gx=2 → 275mm（左侧通道中心）
 *         
 * @note 通道范围：
 *       - 右侧通道：1850-2400mm，中心 2125mm
 *       - 中间通道：1000-1400mm，中心 1200mm
 *       - 左侧通道：0-550mm，中心 275mm
 */
int GridPlanner::gridToFieldX(int gx)
{
    // 3条纵向通道的中心 x 坐标
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

/**
 * @brief 场地坐标转换为网格坐标
 * 
 * @details 根据场地坐标判断机器人所在的通道，并转换为网格坐标。
 *          
 * @param xMm 场地 X 坐标（毫米）
 * @param yMm 场地 Y 坐标（毫米）
 * 
 * @return GridCoord 网格坐标
 *         - gx: 纵向通道索引（0=右, 1=中, 2=左）
 *         - gy: 横向通道索引（0=上, 1=中, 2=下）
 *         
 * @note 判断逻辑：
 *       - X轴：
 *         - xMm >= 1850 → gx=0（右侧通道）
 *         - xMm >= 1000 → gx=1（中间通道）
 *         - xMm < 1000 → gx=2（左侧通道）
 *       - Y轴：
 *         - yMm >= 1850 → gy=2（底部通道）
 *         - yMm >= 1000 → gy=1（中间通道）
 *         - yMm < 1000 → gy=0（顶部通道）
 */
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