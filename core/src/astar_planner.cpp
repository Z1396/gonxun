/**
 * @file astar_planner.cpp
 * @brief A* 路径规划器实现。
 *
 * 包含日志系统初始化、固定障碍物定义、A* 搜索主循环、
 * 碰撞检测（边界/固定障碍物/用户障碍物/启停区）、路径重建与简化。
 */

#include "astar_planner.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <queue>

namespace gonxun {

namespace {

/// 全局日志文件输出流
std::ofstream g_log_file;

/**
 * @brief 初始化日志文件，首次调用时打开 gonxun.log。
 *
 * 日志目录为 /home/pldx/Desktop/gonxun/logs/，自动创建。
 * 后续调用若文件已打开则跳过。
 */
void init_log_file() {
    if (g_log_file.is_open()) return;

    std::string log_dir = "/home/pldx/Desktop/gonxun/logs";
    std::string log_file_name = log_dir + "/gonxun.log";
    system(("mkdir -p " + log_dir).c_str());

    g_log_file.open(log_file_name, std::ios::out | std::ios::trunc);
    if (!g_log_file.is_open()) return;

    time_t now = time(nullptr);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    std::cerr << "\n==== 程序启动 ====\n"
              << "日志文件: " << log_file_name << "\n"
              << "启动时间: " << time_str << "\n"
              << "==================\n\n";
    g_log_file << "\n==== 程序启动 ====\n"
               << "时间: " << time_str << "\n"
               << "==================\n\n";
    g_log_file.flush();
}

/**
 * @brief 日志流包装，自动添加时间戳并输出到 stderr 和日志文件。
 *
 * 每行首个输出前添加 [YYYY-MM-DD HH:MM:SS] 时间戳，
 * 遇到 std::endl 时刷新并重置时间戳标志。
 */
class LogStream {
    bool need_timestamp_{true};

public:
    template<typename T>
    LogStream& operator<<(const T& val) {
        if (need_timestamp_ && g_log_file.is_open()) {
            time_t now = time(nullptr);
            char time_str[32];
            strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S] ", localtime(&now));
            g_log_file << time_str;
            need_timestamp_ = false;
        }
        std::cerr << val;
        if (g_log_file.is_open()) {
            g_log_file << val;
        }
        return *this;
    }

    LogStream& operator<<(std::ostream& (*f)(std::ostream&)) {
        std::cerr << f;
        if (g_log_file.is_open()) {
            g_log_file << f;
            g_log_file.flush();
        }
        need_timestamp_ = true;
        return *this;
    }
};

/// 全局日志流实例
LogStream g_log;

/**
 * @brief A* 搜索节点，包含位置、g 值和 f = g + h 值。
 */
struct AStarNode {
    Point pos;  ///< 栅格位置
    int g;      ///< 从起点到当前点的实际代价
    int f;      ///< 估计总代价 f = g + h

    /// 优先队列比较：f 值小的优先
    bool operator>(const AStarNode& other) const noexcept {
        return f > other.f;
    }
};

/**
 * @brief 添加赛场固定障碍物到列表。
 *
 * 4 个黄色块 (550,550)-(1000,1000) 等、暂存区 (0,975)-(150,1425)、
 * 粗加工区 (910,2250)-(1490,2400)。
 * @param fixed 输出的固定障碍物向量
 */
void add_fixed_obstacle(std::vector<ObstacleRect>& fixed) {
    fixed.push_back({550, 550, 450, 450});
    fixed.push_back({1400, 550, 450, 450});
    fixed.push_back({550, 1400, 450, 450});
    fixed.push_back({1400, 1400, 450, 450});
    fixed.push_back({0, 975, 150, 450});
    fixed.push_back({910, 2250, 580, 150});
}

/**
 * @brief 打印固定障碍物列表到日志。
 * @param fixed 固定障碍物向量
 */
void log_fixed_obstacles(const std::vector<ObstacleRect>& fixed) {
    g_log << "\n==== 固定障碍物列表 ====" << std::endl;
    g_log << "黄色块1: X=550-1000, Y=550-1000" << std::endl;
    g_log << "黄色块2: X=1400-1850, Y=550-1000" << std::endl;
    g_log << "黄色块3: X=550-1000, Y=1400-1850" << std::endl;
    g_log << "黄色块4: X=1400-1850, Y=1400-1850" << std::endl;
    g_log << "暂存区: X=0-150, Y=975-1425" << std::endl;
    g_log << "粗加工区: X=910-1490, Y=2250-2400" << std::endl;
    g_log << "\n暂存区和粗加工区是障碍物" << std::endl;
    g_log << "总共 " << fixed.size() << " 个固定障碍物" << std::endl;
    g_log << "=====\n" << std::endl;
}

/**
 * @brief 判断两个矩形是否相交（AABB 碰撞检测）。
 * @param r1_min_x, r1_max_x, r1_min_y, r1_max_y 矩形 1 的边界
 * @param r2 矩形 2（ObstacleRect 格式）
 * @return true 表示两矩形有重叠区域
 */
bool rects_intersect(int r1_min_x, int r1_max_x,
                     int r1_min_y, int r1_max_y,
                     const ObstacleRect& r2) noexcept {
    if (r1_max_x <= r2.x) return false;
    if (r1_min_x >= r2.x + r2.w) return false;
    if (r1_max_y <= r2.y) return false;
    if (r1_min_y >= r2.y + r2.h) return false;
    return true;
}

/**
 * @brief 判断点是否在粗加工区内（X∈[900,1500], Y≥2200）。
 *
 * 粗加工区允许机器人 Y 坐标超出场地边界（向下延伸）。
 * @param center_x 中心 X（mm）
 * @param center_y 中心 Y（mm）
 * @return true 表示在粗加工区
 */
bool is_in_process_area(int center_x, int center_y) noexcept {
    return center_x >= 900 && center_x <= 1500 && center_y >= 2200;
}

} // anonymous namespace

// ==== AStarPlanner ====

AStarPlanner::AStarPlanner()
    : grid_map_(GRID_SIZE, std::vector<bool>(GRID_SIZE, false))
{
    init_log_file();
    // 构造时仅标记 4 个黄色块为栅格障碍（暂存区和粗加工区在 set_obstacles 时加入）
    mark_obstacle_on_grid({550, 550, 450, 450});
    mark_obstacle_on_grid({1400, 550, 450, 450});
    mark_obstacle_on_grid({550, 1400, 450, 450});
    mark_obstacle_on_grid({1400, 1400, 450, 450});
}

void AStarPlanner::set_obstacles(const std::vector<ObstacleRect>& obstacles) {
    obstacles_ = obstacles;

    // 清空栅格地图
    for (auto& row : grid_map_) {
        std::fill(row.begin(), row.end(), false);
    }

    // 重建固定障碍物（含暂存区和粗加工区），并标记到栅格地图
    fixed_obstacles_.clear();
    add_fixed_obstacle(fixed_obstacles_);
    log_fixed_obstacles(fixed_obstacles_);

    // 标记用户自定义障碍物到栅格地图
    for (const auto& obs : obstacles_) {
        mark_obstacle_on_grid(obs);
    }
}

void AStarPlanner::mark_obstacle_on_grid(const ObstacleRect& obs) noexcept {
    int gx_start = to_grid(obs.x);
    int gy_start = to_grid(obs.y);
    int gx_end = to_grid(obs.x + obs.w);
    int gy_end = to_grid(obs.y + obs.h);

    for (int gx = gx_start; gx < gx_end && gx < GRID_SIZE; ++gx) {
        for (int gy = gy_start; gy < gy_end && gy < GRID_SIZE; ++gy) {
            if (gx >= 0 && gy >= 0) {
                grid_map_[gx][gy] = true;
            }
        }
    }
}

Path AStarPlanner::plan(const Point& start, const Point& goal, bool allow_start_zone) {
    Point start_grid{to_grid(start.x), to_grid(start.y)};
    Point goal_grid{to_grid(goal.x), to_grid(goal.y)};

    // 边界检查
    if (start_grid.x < 0 || start_grid.x >= GRID_SIZE ||
        start_grid.y < 0 || start_grid.y >= GRID_SIZE ||
        goal_grid.x < 0 || goal_grid.x >= GRID_SIZE) {
        g_log << "[AStar] 坐标越界" << std::endl;
        return {};
    }

    // 目标点不可达
    if (check_collision(goal_grid.x, goal_grid.y, allow_start_zone)) {
        g_log << "[AStar] 目标点在障碍物内" << std::endl;
        return {};
    }

    // 起点在障碍物内，搜索半径 4 格内最近可通行点
    if (check_collision(start_grid.x, start_grid.y, allow_start_zone)) {
        bool found = false;
        for (int r = 1; r < 5 && !found; ++r) {
            for (int dx = -r; dx <= r && !found; ++dx) {
                for (int dy = -r; dy <= r && !found; ++dy) {
                    int nx = start_grid.x + dx;
                    int ny = start_grid.y + dy;
                    if (!check_collision(nx, ny, allow_start_zone)) {
                        start_grid.x = nx;
                        start_grid.y = ny;
                        found = true;
                    }
                }
            }
        }
        if (!found) {
            g_log << "[AStar] 起点周围无可通行点" << std::endl;
            return {};
        }
    }

    return run_astar_search(start_grid, goal_grid, allow_start_zone);
}

/**
 * @brief A* 搜索主循环。
 *
 * 使用最小堆（按 f 值排序）维护开放集，unordered_set 维护关闭集，
 * 四邻域扩展，g 值步长为 10，启发为曼哈顿距离×10。
 * @param start_grid 起点栅格坐标
 * @param goal_grid 终点栅格坐标
 * @param allow_start_zone 是否允许经过启停区
 * @return 简化后的路径（mm 坐标），无解返回空
 */
Path AStarPlanner::run_astar_search(const Point& start_grid, const Point& goal_grid,
                                     bool allow_start_zone) {
    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> open_set;
    std::unordered_set<Point, PointHash> closed_set;
    std::unordered_map<Point, Point, PointHash> came_from;
    std::unordered_map<Point, int, PointHash> g_score;

    g_score[start_grid] = 0;
    open_set.push({start_grid, 0, heuristic(start_grid, goal_grid)});

    while (!open_set.empty()) {
        AStarNode current = open_set.top();
        open_set.pop();

        // 到达终点，回溯并简化路径
        if (current.pos == goal_grid) {
            Path grid_path = reconstruct_path(came_from, current.pos);
            return simplify_path(grid_path);
        }

        if (closed_set.count(current.pos)) continue;
        closed_set.insert(current.pos);

        // 四邻域扩展
        for (const auto& neighbor : get_neighbors(current.pos)) {
            if (closed_set.count(neighbor)) continue;
            if (check_collision(neighbor.x, neighbor.y, allow_start_zone)) continue;

            int tentative_g = g_score[current.pos] + 10;
            if (!g_score.count(neighbor) || tentative_g < g_score[neighbor]) {
                came_from[neighbor] = current.pos;
                g_score[neighbor] = tentative_g;
                open_set.push({neighbor, tentative_g,
                               tentative_g + heuristic(neighbor, goal_grid)});
            }
        }
    }

    g_log << "[AStar] 未找到路径" << std::endl;
    return {};
}

int AStarPlanner::to_grid(int mm) const noexcept {
    return mm / GRID_RESOLUTION_MM;
}

int AStarPlanner::to_mm(int grid) const noexcept {
    // 取栅格中心坐标
    return grid * GRID_RESOLUTION_MM + GRID_RESOLUTION_MM / 2;
}

int AStarPlanner::heuristic(const Point& a, const Point& b) const noexcept {
    // 曼哈顿距离 ×10，与 g_score 步长对齐，保证可接纳性
    return 10 * (std::abs(a.x - b.x) + std::abs(a.y - b.y));
}

std::vector<Point> AStarPlanner::get_neighbors(const Point& p) const {
    // 四邻域：上、下、右、左
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

Path AStarPlanner::reconstruct_path(
    const std::unordered_map<Point, Point, PointHash>& came_from,
    const Point& current) const
{
    // 从终点沿 came_from 回溯到起点，再反转
    Path path;
    Point curr = current;
    path.push_back(curr);
    while (came_from.count(curr)) {
        curr = came_from.at(curr);
        path.push_back(curr);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

bool AStarPlanner::check_obstacle_collision(int robot_min_x, int robot_max_x,
                                             int robot_min_y, int robot_max_y,
                                             const ObstacleRect& obs) const noexcept
{
    return rects_intersect(robot_min_x, robot_max_x, robot_min_y, robot_max_y, obs);
}

bool AStarPlanner::check_collision(int grid_x, int grid_y, bool allow_start_zone) const {
    if (grid_x < 0 || grid_x >= GRID_SIZE || grid_y < 0 || grid_y >= GRID_SIZE) {
        return true;
    }

    int center_x = to_mm(grid_x);
    int center_y = to_mm(grid_y);

    // 计算机器人包围盒（半尺寸 150mm）
    static constexpr int ROBOT_HALF_SIZE = 150;
    int robot_min_x = center_x - ROBOT_HALF_SIZE;
    int robot_max_x = center_x + ROBOT_HALF_SIZE;
    int robot_min_y = center_y - ROBOT_HALF_SIZE;
    int robot_max_y = center_y + ROBOT_HALF_SIZE;

    // 依次检查：场地边界 → 固定障碍物 → 用户障碍物 → 启停区
    if (check_boundary_collision(center_x, center_y,
                                  robot_min_x, robot_max_x,
                                  robot_min_y, robot_max_y)) {
        return true;
    }

    for (const auto& obs : fixed_obstacles_) {
        if (check_obstacle_collision(robot_min_x, robot_max_x,
                                      robot_min_y, robot_max_y, obs)) {
            return true;
        }
    }

    if (check_user_obstacle_collision(robot_min_x, robot_max_x,
                                       robot_min_y, robot_max_y)) {
        return true;
    }

    if (!allow_start_zone && is_in_start_zone(center_x, center_y)) {
        return true;
    }

    return false;
}

bool AStarPlanner::check_boundary_collision(int center_x, int center_y,
                                             int robot_min_x, int robot_max_x,
                                             int robot_min_y, int robot_max_y) const {
    // X 方向和 Y 上方不允许超出场地边界
    if (robot_min_x < 0 || robot_max_x > FIELD_SIZE_MM) return true;
    if (robot_min_y < 0) return true;
    // Y 下方仅在粗加工区内允许超出
    if (robot_max_y > FIELD_SIZE_MM && !is_in_process_area(center_x, center_y)) {
        return true;
    }
    return false;
}

bool AStarPlanner::check_user_obstacle_collision(int robot_min_x, int robot_max_x,
                                                   int robot_min_y, int robot_max_y) const {
    // 将机器人包围盒映射到栅格范围，检查是否有格子被占用
    int gx_start = robot_min_x / GRID_RESOLUTION_MM;
    int gy_start = robot_min_y / GRID_RESOLUTION_MM;
    int gx_end = (robot_max_x - 1) / GRID_RESOLUTION_MM;
    int gy_end = (robot_max_y - 1) / GRID_RESOLUTION_MM;

    for (int gx = gx_start; gx <= gx_end; ++gx) {
        for (int gy = gy_start; gy <= gy_end; ++gy) {
            if (gx >= 0 && gx < GRID_SIZE && gy >= 0 && gy < GRID_SIZE) {
                if (grid_map_[gx][gy]) return true;
            }
        }
    }
    return false;
}

bool AStarPlanner::is_in_start_zone(int center_x, int center_y) const noexcept {
    // 启停区位于场地右侧两个角落：右上方 (X≥1850, Y∈[0,1000])
    // 和右下方 (X≥1850, Y∈[1850,2400])
    if (center_x >= 1850) {
        if (center_y >= 0 && center_y <= 1000) return true;
        if (center_y >= 1850 && center_y <= 2400) return true;
    }
    return false;
}

Path AStarPlanner::simplify_path(const Path& grid_path) const {
    // 路径不超过 2 个点，直接转换坐标
    if (grid_path.size() <= 2) {
        Path result;
        for (const auto& p : grid_path) {
            result.push_back({to_mm(p.x), to_mm(p.y)});
        }
        return result;
    }

    // 去除同方向连续途经点，仅保留拐点（方向改变的点）
    Path simplified;
    simplified.push_back({to_mm(grid_path[0].x), to_mm(grid_path[0].y)});

    for (size_t i = 1; i < grid_path.size() - 1; ++i) {
        int dx1 = grid_path[i].x - grid_path[i - 1].x;
        int dy1 = grid_path[i].y - grid_path[i - 1].y;
        int dx2 = grid_path[i + 1].x - grid_path[i].x;
        int dy2 = grid_path[i + 1].y - grid_path[i].y;

        // 方向改变时保留此点
        if (dx1 != dx2 || dy1 != dy2) {
            simplified.push_back({to_mm(grid_path[i].x), to_mm(grid_path[i].y)});
        }
    }

    simplified.push_back({to_mm(grid_path.back().x), to_mm(grid_path.back().y)});
    return simplified;
}

} // namespace gonxun
