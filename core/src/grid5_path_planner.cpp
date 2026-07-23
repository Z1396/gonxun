/**
 * @file grid5_path_planner.cpp
 * @brief 5×5 网格路径决策系统实现。
 *
 * 根据起终点网格坐标计算移动向量，按优先轴顺序生成
 * 横平竖直的移动指令序列，每条指令含转向角度和步数。
 */

#include "grid5_path_planner.hpp"

#include <cmath>
#include <iostream>
#include <sstream>

namespace gonxun {

namespace {

/**
 * @brief 构造单步移动指令。
 * @param target_angle 目标朝向（0/90/180/270）
 * @param current_angle_state 当前朝向
 * @param dx X 方向位移（格）
 * @param dy Y 方向位移（格）
 * @param steps 移动步数
 * @return 包含转向角度和移动参数的 Move5Command
 */
Move5Command make_move_command(int target_angle, int current_angle_state,
                               int dx, int dy, int steps) {
    // 角度差归一化到 [-180, 180]
    int diff = target_angle - current_angle_state;
    while (diff > 180) diff -= 360;
    while (diff < -180) diff += 360;

    // 方向名称映射
    static const char* names[] = {"左", "下", "右", "上"};
    const char* dir_name = "";
    switch (target_angle) {
        case 0: dir_name = names[0]; break;
        case 90: dir_name = names[1]; break;
        case 180: dir_name = names[2]; break;
        case 270: dir_name = names[3]; break;
        default: dir_name = "未知"; break;
    }

    return Move5Command{diff, target_angle, dx, dy, steps, std::string(dir_name)};
}

} // anonymous namespace

Grid5PathPlanner::Grid5PathPlanner() = default;

Path5Decision Grid5PathPlanner::plan(Grid5Coord current, Grid5Coord goal,
                                      int current_angle, bool prioritize_x) {
    Path5Decision result;
    result.start = current;
    result.goal = goal;
    result.current_angle = current_angle;
    result.success = false;

    // 输入校验：坐标范围和角度值
    if (!is_valid_coord(current)) {
        result.error_message = "无效的起点坐标";
        return result;
    }
    if (!is_valid_coord(goal)) {
        result.error_message = "无效的终点坐标";
        return result;
    }
    if (current_angle != 0 && current_angle != 90 &&
        current_angle != 180 && current_angle != 270) {
        result.error_message = "无效的陀螺仪角度（必须为0/90/180/270）";
        return result;
    }

    result.vector = calculate_vector(current, goal);
    if (current == goal) {
        result.success = true;
        result.error_message = "起点和终点相同，无需移动";
        return result;
    }

    // 按优先轴顺序生成移动指令
    std::vector<Move5Command> commands;
    int angle_state = current_angle;

    if (prioritize_x) {
        // 先走 X 方向，再走 Y 方向
        angle_state = build_x_commands(result.vector.dx, angle_state, commands);
        build_y_commands(result.vector.dy, angle_state, commands);
    } else {
        // 先走 Y 方向，再走 X 方向
        angle_state = build_y_commands(result.vector.dy, angle_state, commands);
        build_x_commands(result.vector.dx, angle_state, commands);
    }

    result.commands = commands;
    result.success = true;
    return result;
}

int Grid5PathPlanner::build_x_commands(int dx, int angle_state,
                                        std::vector<Move5Command>& commands) {
    // dx > 0 向左（0°），dx < 0 向右（180°）
    if (dx > 0) {
        commands.push_back(make_move_command(0, angle_state, dx, 0, dx));
        return 0;
    }
    if (dx < 0) {
        commands.push_back(make_move_command(180, angle_state, dx, 0, -dx));
        return 180;
    }
    return angle_state;
}

int Grid5PathPlanner::build_y_commands(int dy, int angle_state,
                                        std::vector<Move5Command>& commands) {
    // dy > 0 向下（90°），dy < 0 向上（270°）
    if (dy > 0) {
        commands.push_back(make_move_command(90, angle_state, 0, dy, dy));
        return 90;
    }
    if (dy < 0) {
        commands.push_back(make_move_command(270, angle_state, 0, dy, -dy));
        return 270;
    }
    return angle_state;
}

Move5Vector Grid5PathPlanner::calculate_vector(Grid5Coord from, Grid5Coord to) noexcept {
    return {to.x - from.x, to.y - from.y};
}

int Grid5PathPlanner::vector_to_angle(int dx, int dy) noexcept {
    // 仅支持横平竖直四个方向
    if (dx > 0 && dy == 0) return 0;
    if (dx == 0 && dy > 0) return 90;
    if (dx < 0 && dy == 0) return 180;
    if (dx == 0 && dy < 0) return 270;
    return -1;
}

int Grid5PathPlanner::normalize_angle(int angle) noexcept {
    while (angle > 180) angle -= 360;
    while (angle < -180) angle += 360;
    return angle;
}

std::string Grid5PathPlanner::angle_to_direction_name(int angle) {
    switch (angle) {
        case 0:   return "左";
        case 90:  return "下";
        case 180: return "右";
        case 270: return "上";
        default:  return "未知";
    }
}

bool Grid5PathPlanner::is_valid_coord(Grid5Coord coord) noexcept {
    // 5×5 网格坐标范围 [0, 4]
    return coord.x >= 0 && coord.x <= 4 && coord.y >= 0 && coord.y <= 4;
}

void Grid5PathPlanner::print_decision(const Path5Decision& decision) {
    std::cout << "==== 路径决策结果 ====" << std::endl;
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
    std::cout << "====================" << std::endl;
}

} // namespace gonxun
