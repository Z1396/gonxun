/**
 * @file grid5_path_planner.hpp
 * @brief 5×5网格路径决策系统头文件
 * 
 * @details 本文件定义了基于5×5网格的路径规划数据结构和接口。
 *          核心特性：
 *          - 网格坐标：5×5范围(0,0)到(4,4)
 *          - 横平竖直移动：禁止斜线移动
 *          - 陀螺仪融合：角度与移动方向映射
 *          - 向量计算：目标坐标 - 当前坐标
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 陀螺仪角度映射：
 *       - 0°   → 向左（X轴正方向）
 *       - 90°  → 向下（Y轴正方向）
 *       - 180° → 向右（X轴负方向）
 *       - 270° → 向上（Y轴负方向）
 *       
 * @note 坐标系定义：
 *       - 起点/停止区：(0,0)
 *       - X轴：向左递增（0→4）
 *       - Y轴：向下递增（0→4）
 */

#ifndef GRID5_PATH_PLANNER_HPP
#define GRID5_PATH_PLANNER_HPP

#include <vector>
#include <string>
#include <optional>

namespace gonxun {

/**
 * @brief 5×5网格坐标结构体
 */
struct Grid5Coord {
    int x;  ///< X轴坐标（0-4，向左递增）
    int y;  ///< Y轴坐标（0-4，向下递增）
    
    bool operator==(const Grid5Coord& other) const {
        return x == other.x && y == other.y;
    }
    
    bool operator!=(const Grid5Coord& other) const {
        return !(*this == other);
    }
};

/**
 * @brief 移动向量结构体
 */
struct Move5Vector {
    int dx;  ///< X方向变化量（正=向左，负=向右）
    int dy;  ///< Y方向变化量（正=向下，负=向上）
};

/**
 * @brief 单步移动指令结构体
 */
struct Move5Command {
    int turn_angle;         ///< 转向角度（相对于当前朝向，[-180,180]）
    int target_angle;       ///< 目标陀螺仪角度（绝对值：0/90/180/270）
    int dx;                 ///< X方向移动格数（正=向左）
    int dy;                 ///< Y方向移动格数（正=向下）
    int steps;              ///< 本次移动格数
    std::string direction_name;  ///< 方向描述（"左"/"下"/"右"/"上"）
};

/**
 * @brief 完整路径决策结果结构体
 */
struct Path5Decision {
    Grid5Coord start;                   ///< 起点
    Grid5Coord goal;                    ///< 终点
    Move5Vector vector;                 ///< 总向量
    int current_angle;                  ///< 初始陀螺仪角度
    std::vector<Move5Command> commands; ///< 移动指令序列
    
    bool success;                       ///< 是否成功规划
    std::string error_message;          ///< 错误信息（失败时）
};

/**
 * @brief 5×5网格路径规划器类
 * 
 * @details 实现基于向量计算的横平竖直路径规划，
 *          支持陀螺仪角度融合和转向角度计算。
 */
class Grid5PathPlanner {
public:
    /**
     * @brief 构造函数
     */
    Grid5PathPlanner();
    
    /**
     * @brief 规划从当前点到目标点的路径
     * 
     * @param current 当前位置坐标
     * @param goal 目标位置坐标
     * @param current_angle 当前陀螺仪角度（0/90/180/270）
     * @param prioritize_x 是否优先移动X方向（默认true）
     * 
     * @return Path5Decision 路径决策结果
     * 
     * @note 算法流程：
     *       1. 坐标有效性检查
     *       2. 向量计算：goal - current
     *       3. 路径分解：横平竖直移动序列
     *       4. 转向角度计算：target_angle - current_angle
     *       5. 归一化转向角度：[-180, 180]
     */
    Path5Decision plan(Grid5Coord current, 
                       Grid5Coord goal, 
                       int current_angle,
                       bool prioritize_x = true);
    
    /**
     * @brief 计算两点之间的移动向量
     * 
     * @param from 起点坐标
     * @param to 终点坐标
     * 
     * @return Move5Vector 移动向量(dx, dy)
     */
    Move5Vector calculateVector(Grid5Coord from, Grid5Coord to);
    
    /**
     * @brief 将移动向量转换为陀螺仪角度
     * 
     * @param dx X方向移动量（正=向左）
     * @param dy Y方向移动量（正=向下）
     * 
     * @return int 陀螺仪角度（0/90/180/270），失败返回-1
     */
    int vectorToAngle(int dx, int dy);
    
    /**
     * @brief 将角度归一化到[-180, 180]范围
     * 
     * @param angle 原始角度
     * 
     * @return int 归一化后的角度
     */
    int normalizeAngle(int angle);
    
    /**
     * @brief 将陀螺仪角度转换为方向名称
     * 
     * @param angle 陀螺仪角度（0/90/180/270）
     * 
     * @return std::string 方向名称（"左"/"下"/"右"/"上"/"未知"）
     */
    std::string angleToDirectionName(int angle);
    
    /**
     * @brief 检查坐标是否有效（在0-4范围内）
     * 
     * @param coord 网格坐标
     * 
     * @return bool 是否有效
     */
    bool isValidCoord(Grid5Coord coord);
    
    /**
     * @brief 打印路径决策结果（用于调试）
     * 
     * @param decision 路径决策结果
     */
    void printDecision(const Path5Decision& decision);
};

} // namespace gonxun

#endif // GRID5_PATH_PLANNER_HPP