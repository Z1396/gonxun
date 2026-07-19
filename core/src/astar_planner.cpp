/**
 * @file astar_planner.cpp
 * @brief A* 路径规划器实现文件
 * 
 * @details 本文件实现了基于 A* 算法的路径规划功能。
 *          核心特性：
 *          - A* 搜索算法：启发式搜索，保证最短路径
 *          - 栅格化地图：连续坐标转换为离散栅格
 *          - 障碍物管理：固定障碍（黄色块）+ 动态障碍（可标记块）
 *          - 路径简化：去除冗余路径点，仅保留拐点
 *          - 四方向移动：横平竖直，不允许斜向移动
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，实现基础 A* 算法
 *       - 2025-02-15: 增加路径简化功能
 *       - 2025-03-30: 优化障碍物管理，支持动态添加
 *       
 * @note A* 算法原理：
 *       1. 维护两个集合：openSet（待探索）和 closedSet（已探索）
 *       2. 优先队列按 f 值排序：f = g + h
 *          - g: 从起点到当前点的实际代价
 *          - h: 从当前点到终点的启发式估计（曼哈顿距离）
 *       3. 每次从 openSet 中取出 f 值最小的节点进行扩展
 *       4. 重复直到找到终点或 openSet 为空
 *       
 * @note 性能优化：
 *       - 使用优先队列（堆）实现 openSet，O(log n) 插入/删除
 *       - 使用 unordered_set 实现 closedSet，O(1) 查找
 *       - 启发式函数采用曼哈顿距离，适合四方向移动
 *       
 * @note 场地布局：
 *       - 4块固定黄色障碍（450×450mm）：左上、右上、左下、右下
 *       - 15个可标记障碍块（200×200mm）：动态添加
 *       - 栅格分辨率：50mm/格（可配置）
 *       
 * @see astar_planner.hpp
 */
#include "astar_planner.hpp"
#include <algorithm>
#include <queue>
#include <cmath>
#include <iostream>

namespace gonxun {

/**
 * @brief A* 节点结构体（用于优先队列）
 * 
 * @details 表示搜索过程中的一个节点，包含位置和代价信息。
 */
struct AStarNode {
    Point pos;  ///< 节点位置（栅格坐标）
    int g;      ///< 已走代价：从起点到当前点的实际代价
    int f;      ///< 总代价：g + h（用于优先队列排序）

    /**
     * @brief 比较运算符（用于优先队列）
     * 
     * @details 实现“大于”比较，使优先队列按 f 值升序排列。
     *          std::priority_queue 默认是大顶堆，使用 std::greater 会变成小顶堆。
     *          
     * @param other 另一个节点
     * @return true 当前节点的 f 值大于 other 的 f 值
     */
    bool operator>(const AStarNode& other) const {
        return f > other.f;
    }
};

/**
 * @brief 构造函数，初始化栅格地图和默认障碍物
 * 
 * @details 创建空的栅格地图，并添加4块固定的黄色障碍区域。
 *          这些黄色块将场地分割成3横3纵的通道结构。
 *          
 * @note 黄色障碍布局：
 *       - 左上 (550, 550, 450×450)
 *       - 右上 (1400, 550, 450×450)
 *       - 左下 (550, 1400, 450×450)
 *       - 右下 (1400, 1400, 450×450)
 *       
 * @note 栅格地图初始化：
 *       - GRID_SIZE × GRID_SIZE 的二维数组
 *       - 初始值：false（无障碍）
 *       - 添加障碍物后标记为 true
 */
AStarPlanner::AStarPlanner()
    : m_gridMap(GRID_SIZE, std::vector<bool>(GRID_SIZE, false))
{
    // 添加4块固定的黄色障碍区域（450×450mm）
    // 这些区域将场地分割成3横3纵的通道
    addObstacle({550, 550, 450, 450});     // 左上黄色块
    addObstacle({1400, 550, 450, 450});    // 右上黄色块
    addObstacle({550, 1400, 450, 450});    // 左下黄色块
    addObstacle({1400, 1400, 450, 450});   // 右下黄色块
}

/**
 * @brief 设置障碍物列表
 * 
 * @details 替换当前障碍物列表，并重新标记栅格地图。
 *          固定的黄色障碍区域始终存在，不会被清除。
 *          
 * @param obstacles 新的障碍物列表（包含15个可标记障碍块）
 * 
 * @note 处理流程：
 *       1. 保存新的障碍物列表
 *       2. 重置栅格地图（清空所有标记）
 *       3. 重新标记固定的黄色障碍区域
 *       4. 标记用户自定义障碍物
 *       
 * @see markObstacleOnGrid()
 */
void AStarPlanner::setObstacles(const std::vector<ObstacleRect>& obstacles)
{
    m_obstacles = obstacles;

    // 重置栅格地图（清空所有标记）
    for (auto& row : m_gridMap) {
        std::fill(row.begin(), row.end(), false);
    }

    // ========== 固定障碍物（不膨胀，使用实时碰撞检测） ==========

    std::cerr << "\n========== 固定障碍物列表 ==========" << std::endl;

    // 左侧障碍区域（根据GUI显示的15个障碍物布局）
    // 第二行第一列：X=0-550, Y=550-910
    m_fixedObstacles.push_back({0, 550, 550, 360});
    std::cerr << "左侧障碍1: X=0-550, Y=550-910" << std::endl;

    // 第四行第一列：X=0-550, Y=1490-1850
    m_fixedObstacles.push_back({0, 1490, 550, 360});
    std::cerr << "左侧障碍2: X=0-550, Y=1490-1850" << std::endl;

    // 中心黄色格子（4块，X=550-1850范围）
    // 左上中心：X=550-1000, Y=550-1000
    m_fixedObstacles.push_back({550, 550, 450, 450});
    std::cerr << "中心黄色1: X=550-1000, Y=550-1000" << std::endl;

    // 右上中心：X=1400-1850, Y=550-1000
    m_fixedObstacles.push_back({1400, 550, 450, 450});
    std::cerr << "中心黄色2: X=1400-1850, Y=550-1000" << std::endl;

    // 左下中心：X=550-1000, Y=1400-1850
    m_fixedObstacles.push_back({550, 1400, 450, 450});
    std::cerr << "中心黄色3: X=550-1000, Y=1400-1850" << std::endl;

    // 右下中心：X=1400-1850, Y=1400-1850
    m_fixedObstacles.push_back({1400, 1400, 450, 450});
    std::cerr << "中心黄色4: X=1400-1850, Y=1400-1850" << std::endl;

    // 右侧障碍区域
    // 第二行最后一列：X=1850-2400, Y=550-1000
    m_fixedObstacles.push_back({1850, 550, 550, 450});
    std::cerr << "右侧障碍1: X=1850-2400, Y=550-1000" << std::endl;

    // 第四行最后一列：X=1850-2400, Y=1400-1850
    m_fixedObstacles.push_back({1850, 1400, 550, 450});
    std::cerr << "右侧障碍2: X=1850-2400, Y=1400-1850" << std::endl;

    // 暂存区（左侧，3个圆形区域）
    // 将圆形区域近似为正方形障碍物
    std::cerr << "\n暂存区障碍物：" << std::endl;
    for (int i = 0; i < 3; ++i) {
        ObstacleRect bufferZone = {0, 1050 + i * 150 - 75, 150, 150};
        m_fixedObstacles.push_back(bufferZone);
        std::cerr << "暂存区" << (i+1) << ": X=0-150, Y=" << (1050 + i * 150 - 75)
                  << "-" << (1050 + i * 150 - 75 + 150) << std::endl;
    }

    // 粗加工区（底部，3个圆形区域）
    // 将圆形区域近似为正方形障碍物
    std::cerr << "\n粗加工区障碍物：" << std::endl;
    for (int i = 0; i < 3; ++i) {
        ObstacleRect processZone = {1050 + i * 150 - 75, 2250, 150, 150};
        m_fixedObstacles.push_back(processZone);
        std::cerr << "粗加工区" << (i+1) << ": X=" << (1050 + i * 150 - 75)
                  << "-" << (1050 + i * 150 - 75 + 150)
                  << ", Y=2250-2400" << std::endl;
    }

    std::cerr << "\n总共 " << m_fixedObstacles.size() << " 个固定障碍物" << std::endl;
    std::cerr << "======================================\n" << std::endl;

    // 用户自定义障碍物（添加到障碍物列表）
    // 不膨胀，在碰撞检测时考虑机器人尺寸
    for (const auto& obs : m_obstacles) {
        markObstacleOnGrid(obs);
    }
}

/**
 * @brief 在栅格地图上标记障碍物
 * 
 * @details 将障碍物区域标记为不可通行（true）。
 *          
 * @param obs 障碍物矩形（x, y, width, height，单位：mm）
 * 
 * @note 标记流程：
 *       1. 将障碍物的毫米坐标转换为栅格坐标
 *       2. 遍历障碍物覆盖的所有栅格
 *       3. 将每个栅格标记为 true（障碍）
 *       
 * @note 坐标转换：
 *       - toGrid(x) = x / GRID_RESOLUTION_MM
 *       - 示例：toGrid(550) = 550 / 50 = 11
 */
void AStarPlanner::markObstacleOnGrid(const ObstacleRect& obs)
{
    // 将障碍物边界转换为栅格坐标
    int gxStart = toGrid(obs.x);
    int gyStart = toGrid(obs.y);
    int gxEnd = toGrid(obs.x + obs.w);
    int gyEnd = toGrid(obs.y + obs.h);
    
    // 标记障碍物覆盖的所有栅格
    for (int gx = gxStart; gx < gxEnd && gx < GRID_SIZE; ++gx) {
        for (int gy = gyStart; gy < gyEnd && gy < GRID_SIZE; ++gy) {
            if (gx >= 0 && gy >= 0) {
                m_gridMap[gx][gy] = true;  // 标记为障碍
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

/**
 * @brief 规划从起点到终点的路径
 * 
 * @details 使用 A* 算法寻找最短路径，并进行路径简化。
 *          
 * @param start 起点（毫米坐标）
 * @param goal 终点（毫米坐标）
 * 
 * @return Path 路径点列表（毫米坐标），如果失败则返回空列表
 * 
 * @note 算法流程：
 *       1. 坐标转换：毫米坐标 → 栅格坐标
 *       2. 边界检查：确保起点和终点在栅格地图内
 *       3. 可行性检查：确保目标点可通行
 *       4. 起点调整：如果起点不可通行，寻找最近的可行点
 *       5. A* 搜索：使用优先队列扩展节点
 *       6. 路径重建：从终点回溯到起点
 *       7. 路径简化：去除冗余路径点
 *       
 * @note A* 核心数据结构：
 *       - openSet: 优先队列，按 f 值排序
 *       - closedSet: 哈希集合，记录已探索节点
 *       - cameFrom: 哈希映射，记录路径回溯关系
 *       - gScore: 哈希映射，记录每个节点的 g 值
 *       
 * @note 代价计算：
 *       - g 值：已走代价，每步移动代价为 10
 *       - h 值：启发式估计，曼哈顿距离 × 10
 *       - f 值：g + h
 *       
 * @see reconstructPath(), simplifyPath()
 */
Path AStarPlanner::plan(const Point& start, const Point& goal, bool allowStartZone)
{
    // 步骤 1: 转换为栅格坐标
    Point startGrid{toGrid(start.x), toGrid(start.y)};
    Point goalGrid{toGrid(goal.x), toGrid(goal.y)};

    // 步骤 2: 边界检查
    if (startGrid.x < 0 || startGrid.x >= GRID_SIZE ||
        startGrid.y < 0 || startGrid.y >= GRID_SIZE ||
        goalGrid.x < 0 || goalGrid.x >= GRID_SIZE) {
        std::cerr << "[AStar] 坐标越界" << std::endl;
        return {};
    }

    // 步骤 3: 检查目标是否在障碍物内（使用实时碰撞检测）
    if (checkCollision(goalGrid.x, goalGrid.y, allowStartZone)) {
        std::cerr << "[AStar] 目标点在障碍物内" << std::endl;
        return {};
    }

    // 步骤 4: 如果起点不可通行，找最近的可行点（使用实时碰撞检测）
    if (checkCollision(startGrid.x, startGrid.y, allowStartZone)) {
        bool found = false;
        // 螺旋搜索周围4格范围
        for (int r = 1; r < 5 && !found; ++r) {
            for (int dx = -r; dx <= r && !found; ++dx) {
                for (int dy = -r; dy <= r && !found; ++dy) {
                    int nx = startGrid.x + dx;
                    int ny = startGrid.y + dy;
                    if (!checkCollision(nx, ny, allowStartZone)) {
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

    // 步骤 5: A* 核心算法
    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;  // 待探索集合
    std::unordered_set<Point, PointHash> closedSet;  // 已探索集合
    std::unordered_map<Point, Point, PointHash> cameFrom;  // 路径回溯映射
    std::unordered_map<Point, int, PointHash> gScore;  // g值映射

    // 初始化起点
    gScore[startGrid] = 0;
    openSet.push({startGrid, 0, heuristic(startGrid, goalGrid)});

    // 主循环：扩展节点直到找到终点或集合为空
    while (!openSet.empty()) {
        AStarNode current = openSet.top();  // 取出 f 值最小的节点
        openSet.pop();

        // 到达目标
        if (current.pos == goalGrid) {
            Path gridPath = reconstructPath(cameFrom, current.pos);  // 重建路径
            return simplifyPath(gridPath);  // 简化路径
        }

        // 已处理过则跳过
        if (closedSet.count(current.pos)) {
            continue;
        }
        closedSet.insert(current.pos);  // 标记为已探索

        // 遍历邻居（四方向：上下左右）
        for (const auto& neighbor : getNeighbors(current.pos)) {
            if (closedSet.count(neighbor)) continue;  // 已探索则跳过

            // ========== 实时碰撞检测（替代栅格地图查询） ==========
            // 检查机器人边界是否与障碍物碰撞
            if (checkCollision(neighbor.x, neighbor.y, allowStartZone)) {
                continue;  // 发生碰撞，跳过该邻居节点
            }

            // 计算移动代价（横平竖直统一代价=10）
            int moveCost = 10;
            int tentativeG = gScore[current.pos] + moveCost;  // 计算新 g 值

            // 如果找到更短路径或首次到达该节点
            if (!gScore.count(neighbor) || tentativeG < gScore[neighbor]) {
                cameFrom[neighbor] = current.pos;  // 记录回溯关系
                gScore[neighbor] = tentativeG;     // 更新 g 值
                int f = tentativeG + heuristic(neighbor, goalGrid);  // 计算 f 值
                openSet.push({neighbor, tentativeG, f});  // 加入待探索集合
            }
        }
    }

    // 步骤 6: 无路径（openSet 为空）
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

/**
 * @brief 实时检查机器人边界是否与单个障碍物碰撞
 *
 * @details 使用矩形相交算法判断机器人边界与障碍物是否有重叠。
 *          不使用障碍物膨胀，直接计算边界交集。
 *
 * @param robotMinX 机器人最小X坐标（毫米）
 * @param robotMaxX 机器人最大X坐标（毫米）
 * @param robotMinY 机器人最小Y坐标（毫米）
 * @param robotMaxY 机器人最大Y坐标（毫米）
 * @param obs 障碍物矩形（毫米坐标）
 *
 * @return true=有碰撞，false=无碰撞
 *
 * @note 机器人尺寸：300×300mm（边界为中心±150mm）
 * @note 碰撞判断：两个矩形有交集即为碰撞
 */
bool AStarPlanner::checkObstacleCollision(int robotMinX, int robotMaxX,
                                           int robotMinY, int robotMaxY,
                                           const ObstacleRect& obs) const
{
    // 矩形相交判断：
    // 如果两个矩形不相交，则满足以下任一条件：
    // - 机器人在障碍物左侧：robotMaxX <= obs.x
    // - 机器人在障碍物右侧：robotMinX >= obs.x + obs.w
    // - 机器人在障碍物上方：robotMaxY <= obs.y
    // - 机器人在障碍物下方：robotMinY >= obs.y + obs.h
    //
    // 如果以上条件都不满足，说明两个矩形有交集（碰撞）

    if (robotMaxX <= obs.x) {
        // 机器人在障碍物左侧，无碰撞
        return false;
    }
    if (robotMinX >= obs.x + obs.w) {
        // 机器人在障碍物右侧，无碰撞
        return false;
    }
    if (robotMaxY <= obs.y) {
        // 机器人在障碍物上方，无碰撞
        return false;
    }
    if (robotMinY >= obs.y + obs.h) {
        // 机器人在障碍物下方，无碰撞
        return false;
    }

    // 两个矩形有交集，发生碰撞
    return true;
}

/**
 * @brief 实时检查机器人边界是否与障碍物碰撞
 *
 * @details 实时碰撞检测核心方法：
 *          1. 将栅格坐标转换为机器人中心坐标（毫米）
 *          2. 计算机器人边界（中心±150mm）
 *          3. 检查是否与固定障碍物碰撞（黄色格子、暂存区、粗加工区）
 *          4. 检查是否与用户标记障碍物碰撞
 *          5. 检查启停区约束
 *
 * @param gridX 机器人中心栅格坐标X
 * @param gridY 机器人中心栅格坐标Y
 * @param allowStartZone 是否允许进入启停区
 *
 * @return true=有碰撞，false=无碰撞
 *
 * @note 机器人尺寸：300×300mm（边界为中心±150mm）
 * @note 固定障碍物：不膨胀，直接判断边界交集
 * @note 用户标记障碍物：检查栅格地图标记
 */
bool AStarPlanner::checkCollision(int gridX, int gridY, bool allowStartZone) const
{
    // ========== 步骤1：边界检查 ==========
    if (gridX < 0 || gridX >= GRID_SIZE || gridY < 0 || gridY >= GRID_SIZE) {
        // 超出地图边界，视为碰撞
        return true;
    }

    // ========== 步骤2：坐标转换 ==========
    // 栅格坐标 → 毫米坐标（栅格中心）
    int centerX = toMm(gridX);
    int centerY = toMm(gridY);

    // ========== 步骤3：计算机器人边界 ==========
    // 机器人尺寸：300×300mm，边界为中心±150mm
    static constexpr int ROBOT_HALF_SIZE = 150;  // 机器人半宽：150mm
    int robotMinX = centerX - ROBOT_HALF_SIZE;
    int robotMaxX = centerX + ROBOT_HALF_SIZE;
    int robotMinY = centerY - ROBOT_HALF_SIZE;
    int robotMaxY = centerY + ROBOT_HALF_SIZE;

    // ========== 步骤4：检查固定障碍物碰撞 ==========
    // 固定障碍物：黄色格子、暂存区、粗加工区（不膨胀）
    for (const auto& obs : m_fixedObstacles) {
        if (checkObstacleCollision(robotMinX, robotMaxX, robotMinY, robotMaxY, obs)) {
            // 与固定障碍物碰撞，输出调试信息
            std::cerr << "[碰撞检测] 栅格(" << gridX << "," << gridY << ") "
                      << "中心(" << centerX << "," << centerY << ") "
                      << "机器人边界[" << robotMinX << "-" << robotMaxX << ", "
                      << robotMinY << "-" << robotMaxY << "] "
                      << "与固定障碍物[" << obs.x << "-" << obs.x+obs.w << ", "
                      << obs.y << "-" << obs.y+obs.h << "] 碰撞" << std::endl;
            return true;
        }
    }

    // ========== 步骤5：检查用户标记障碍物碰撞 ==========
    // 用户标记的障碍物已在栅格地图中标记，检查栅格即可
    // 注意：需要检查机器人覆盖的所有栅格（不只是一个栅格中心）
    int gxStart = robotMinX / GRID_RESOLUTION_MM;
    int gyStart = robotMinY / GRID_RESOLUTION_MM;
    int gxEnd = (robotMaxX - 1) / GRID_RESOLUTION_MM;  // -1避免边界问题
    int gyEnd = (robotMaxY - 1) / GRID_RESOLUTION_MM;

    for (int gx = gxStart; gx <= gxEnd; ++gx) {
        for (int gy = gyStart; gy <= gyEnd; ++gy) {
            if (gx >= 0 && gx < GRID_SIZE && gy >= 0 && gy < GRID_SIZE) {
                if (m_gridMap[gx][gy]) {
                    // 机器人覆盖了被标记的栅格，发生碰撞
                    std::cerr << "[碰撞检测] 栅格(" << gridX << "," << gridY << ") "
                              << "与用户标记障碍物（栅格" << gx << "," << gy << "）碰撞" << std::endl;
                    return true;
                }
            }
        }
    }

    // ========== 步骤6：检查启停区约束 ==========
    if (!allowStartZone) {
        // 启停区区域（X >= 1850，即右侧两列格子）
        // 包括启停区1和启停区2所在的整个格子区域
        if (centerX >= 1850) {
            // 启停区1所在的格子：(0,0) 和 (0,1)
            // 坐标范围：X: 1850-2400, Y: 0-1000
            if (centerY >= 0 && centerY <= 1000) {
                // 机器人进入启停区1区域
                return true;
            }
            // 启停区2所在的格子：(0,3) 和 (0,4)
            // 坐标范围：X: 1850-2400, Y: 1850-2400
            if (centerY >= 1850 && centerY <= 2400) {
                // 机器人进入启停区2区域
                return true;
            }
        }
    }

    // 所有检查通过，无碰撞
    return false;
}

/**
 * @brief 简化路径，去除冗余路径点
 *
 * @details 使用 Line-of-Sight 简化算法，仅保留方向改变的拐点。
 *          大幅减少路径点数量，提高执行效率。
 *
 * @param gridPath 栅格坐标路径
 *
 * @return Path 简化后的路径（毫米坐标）
 *
 * @note 简化原理：
 *       - 如果连续三点的方向相同，中间点可以删除
 *       - 只保留方向改变的拐点
 *       - 示例：(0,0)→(1,0)→(2,0)→(2,1)→(2,2)
 *         简化后：(0,0)→(2,0)→(2,2)
 *
 * @note 转换为毫米坐标：
 *       - toMm(grid) = grid × GRID_RESOLUTION_MM + GRID_RESOLUTION_MM / 2
 *       - 示例：toMm(11) = 11 × 50 + 25 = 575mm
 */
Path AStarPlanner::simplifyPath(const Path& gridPath) const
{
    // 特殊情况：路径点数 ≤ 2，直接转换坐标
    if (gridPath.size() <= 2) {
        Path result;
        for (const auto& p : gridPath) {
            result.push_back({toMm(p.x), toMm(p.y)});
        }
        return result;
    }

    // 路径简化：只保留方向改变的拐点
    Path simplified;
    simplified.push_back({toMm(gridPath[0].x), toMm(gridPath[0].y)});  // 起点

    // 遍历中间点，检查方向是否改变
    for (size_t i = 1; i < gridPath.size() - 1; ++i) {
        // 计算前后两段的方向向量
        int dx1 = gridPath[i].x - gridPath[i - 1].x;
        int dy1 = gridPath[i].y - gridPath[i - 1].y;
        int dx2 = gridPath[i + 1].x - gridPath[i].x;
        int dy2 = gridPath[i + 1].y - gridPath[i].y;

        // 方向改变时保留该点
        if (dx1 != dx2 || dy1 != dy2) {
            simplified.push_back({toMm(gridPath[i].x), toMm(gridPath[i].y)});
        }
    }

    simplified.push_back({toMm(gridPath.back().x), toMm(gridPath.back().y)});  // 终点
    return simplified;
}

} // namespace gonxun