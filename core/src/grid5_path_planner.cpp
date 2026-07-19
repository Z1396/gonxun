/**
 * @file grid5_path_planner.cpp
 * @brief 5×5网格路径决策系统实现文件
 * 
 * @details 本文件实现了基于向量计算的横平竖直路径规划功能。
 *          核心算法：
 *          1. 向量计算：目标坐标 - 当前坐标 = (dx, dy)
 *          2. 路径分解：优先X方向或Y方向移动
 *          3. 角度映射：向量方向 → 陀螺仪角度
 *          4. 转向计算：target_angle - current_angle（归一化）
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 */

#include "grid5_path_planner.hpp"
#include <iostream>
#include <cmath>
#include <sstream>

namespace gonxun {

Grid5PathPlanner::Grid5PathPlanner() {
}

Path5Decision Grid5PathPlanner::plan(Grid5Coord current, 
                                     Grid5Coord goal, 
                                     int current_angle,
                                     bool prioritize_x) {
    Path5Decision result;
    result.start = current;
    result.goal = goal;
    result.current_angle = current_angle;
    result.success = false;
    
    // 坐标有效性检查
    if (!isValidCoord(current)) {
        result.error_message = "无效的起点坐标";
        return result;
    }
    
    if (!isValidCoord(goal)) {
        result.error_message = "无效的终点坐标";
        return result;
    }
    
    // 陀螺仪角度有效性检查
    if (current_angle != 0 && current_angle != 90 && 
        current_angle != 180 && current_angle != 270) {
        result.error_message = "无效的陀螺仪角度（必须为0/90/180/270）";
        return result;
    }
    
    // 计算移动向量
    result.vector = calculateVector(current, goal);
    
    // 起点和终点相同
    if (current == goal) {
        result.success = true;
        result.error_message = "起点和终点相同，无需移动";
        return result;
    }
    
    std::vector<Move5Command> commands;
    int current_angle_state = current_angle;
    
    // 路径分解：优先X方向移动
    if (prioritize_x) {
        // 处理X方向移动
        if (result.vector.dx > 0) {
            // 向左移动（dx > 0）
            int target_angle = 0;
            int turn = normalizeAngle(target_angle - current_angle_state);
            
            Move5Command cmd;
            cmd.turn_angle = turn;
            cmd.target_angle = target_angle;
            cmd.dx = result.vector.dx;
            cmd.dy = 0;
            cmd.steps = result.vector.dx;
            cmd.direction_name = angleToDirectionName(target_angle);
            commands.push_back(cmd);
            current_angle_state = target_angle;
        } else if (result.vector.dx < 0) {
            // 向右移动（dx < 0）
            int target_angle = 180;
            int turn = normalizeAngle(target_angle - current_angle_state);
            
            Move5Command cmd;
            cmd.turn_angle = turn;
            cmd.target_angle = target_angle;
            cmd.dx = result.vector.dx;
            cmd.dy = 0;
            cmd.steps = -result.vector.dx;
            cmd.direction_name = angleToDirectionName(target_angle);
            commands.push_back(cmd);
            current_angle_state = target_angle;
        }
        
        // 处理Y方向移动
        if (result.vector.dy > 0) {
            // 向下移动（dy > 0）
            int target_angle = 90;
            int turn = normalizeAngle(target_angle - current_angle_state);
            
            Move5Command cmd;
            cmd.turn_angle = turn;
            cmd.target_angle = target_angle;
            cmd.dx = 0;
            cmd.dy = result.vector.dy;
            cmd.steps = result.vector.dy;
            cmd.direction_name = angleToDirectionName(target_angle);
            commands.push_back(cmd);
            current_angle_state = target_angle;
        } else if (result.vector.dy < 0) {
            // 向上移动（dy < 0）
            int target_angle = 270;
            int turn = normalizeAngle(target_angle - current_angle_state);
            
            Move5Command cmd;
            cmd.turn_angle = turn;
            cmd.target_angle = target_angle;
            cmd.dx = 0;
            cmd.dy = result.vector.dy;
            cmd.steps = -result.vector.dy;
            cmd.direction_name = angleToDirectionName(target_angle);
            commands.push_back(cmd);
            current_angle_state = target_angle;
        }
    } else {
        // 优先Y方向移动
        if (result.vector.dy > 0) {
            int target_angle = 90;
            int turn = normalizeAngle(target_angle - current_angle_state);
            
            Move5Command cmd;
            cmd.turn_angle = turn;
            cmd.target_angle = target_angle;
            cmd.dx = 0;
            cmd.dy = result.vector.dy;
            cmd.steps = result.vector.dy;
            cmd.direction_name = angleToDirectionName(target_angle);
            commands.push_back(cmd);
            current_angle_state = target_angle;
        } else if (result.vector.dy < 0) {
            int target_angle = 270;
            int turn = normalizeAngle(target_angle - current_angle_state);
            
            Move5Command cmd;
            cmd.turn_angle = turn;
            cmd.target_angle = target_angle;
            cmd.dx = 0;
            cmd.dy = result.vector.dy;
            cmd.steps = -result.vector.dy;
            cmd.direction_name = angleToDirectionName(target_angle);
            commands.push_back(cmd);
            current_angle_state = target_angle;
        }
        
        if (result.vector.dx > 0) {
            int target_angle = 0;
            int turn = normalizeAngle(target_angle - current_angle_state);
            
            Move5Command cmd;
            cmd.turn_angle = turn;
            cmd.target_angle = target_angle;
            cmd.dx = result.vector.dx;
            cmd.dy = 0;
            cmd.steps = result.vector.dx;
            cmd.direction_name = angleToDirectionName(target_angle);
            commands.push_back(cmd);
            current_angle_state = target_angle;
        } else if (result.vector.dx < 0) {
            int target_angle = 180;
            int turn = normalizeAngle(target_angle - current_angle_state);
            
            Move5Command cmd;
            cmd.turn_angle = turn;
            cmd.target_angle = target_angle;
            cmd.dx = result.vector.dx;
            cmd.dy = 0;
            cmd.steps = -result.vector.dx;
            cmd.direction_name = angleToDirectionName(target_angle);
            commands.push_back(cmd);
            current_angle_state = target_angle;
        }
    }
    
    result.commands = commands;
    result.success = true;
    return result;
}

Move5Vector Grid5PathPlanner::calculateVector(Grid5Coord from, Grid5Coord to) {
    Move5Vector vec;
    vec.dx = to.x - from.x;
    vec.dy = to.y - from.y;
    return vec;
}

int Grid5PathPlanner::vectorToAngle(int dx, int dy) {
    if (dx > 0 && dy == 0) return 0;    // 向左
    if (dx == 0 && dy > 0) return 90;   // 向下
    if (dx < 0 && dy == 0) return 180;  // 向右
    if (dx == 0 && dy < 0) return 270;  // 向上
    return -1;  // 无效向量
}

int Grid5PathPlanner::normalizeAngle(int angle) {
    // 归一化到[-180, 180]范围
    while (angle > 180) {
        angle -= 360;
    }
    while (angle < -180) {
        angle += 360;
    }
    return angle;
}

std::string Grid5PathPlanner::angleToDirectionName(int angle) {
    switch (angle) {
        case 0:   return "左";
        case 90:  return "下";
        case 180: return "右";
        case 270: return "上";
        default:  return "未知";
    }
}

bool Grid5PathPlanner::isValidCoord(Grid5Coord coord) {
    return coord.x >= 0 && coord.x <= 4 && coord.y >= 0 && coord.y <= 4;
}

void Grid5PathPlanner::printDecision(const Path5Decision& decision) {
    std::cout << "====== 路径决策结果 ======" << std::endl;
    std::cout << "起点: (" << decision.start.x << ", " << decision.start.y << ")" << std::endl;
    std::cout << "终点: (" << decision.goal.x << ", " << decision.goal.y << ")" << std::endl;
    std::cout << "向量: (" << decision.vector.dx << ", " << decision.vector.dy << ")" << std::endl;
    std::cout << "初始角度: " << decision.current_angle << "°" << std::endl;
    
    if (!decision.success) {
        std::cout << "状态: 规划失败" << std::endl;
        std::cout << "错误: " << decision.error_message << std::endl;
        return;
    }
    
    std::cout << "状态: 规划成功" << std::endl;
    std::cout << "移动指令序列:" << std::endl;
    
    for (size_t i = 0; i < decision.commands.size(); ++i) {
        const auto& cmd = decision.commands[i];
        std::cout << "  步骤" << (i + 1) << ": ";
        if (cmd.turn_angle != 0) {
            std::cout << "转向" << cmd.turn_angle << "° → ";
        }
        std::cout << "向" << cmd.direction_name << "移动" << cmd.steps << "格" << std::endl;
    }
    std::cout << "==========================" << std::endl;
}

} // namespace gonxun