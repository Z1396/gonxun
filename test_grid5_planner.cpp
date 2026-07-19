/**
 * @file test_grid5_planner.cpp
 * @brief 5×5网格路径规划器测试程序
 */

#include "grid5_path_planner.hpp"
#include <iostream>

using namespace gonxun;

int main() {
    Grid5PathPlanner planner;
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "  5×5网格路径规划器测试" << std::endl;
    std::cout << "======================================\n" << std::endl;
    
    // 测试用例1：从(0,0)到(2,3)，初始角度0°
    std::cout << "【测试1】从(0,0)到(2,3)，当前角度0°（向左）" << std::endl;
    auto decision1 = planner.plan({0, 0}, {2, 3}, 0);
    planner.printDecision(decision1);
    
    // 测试用例2：从(0,0)到(4,4)，初始角度90°
    std::cout << "\n【测试2】从(0,0)到(4,4)，当前角度90°（向下）" << std::endl;
    auto decision2 = planner.plan({0, 0}, {4, 4}, 90);
    planner.printDecision(decision2);
    
    // 测试用例3：从(2,2)到(4,3)，初始角度180°
    std::cout << "\n【测试3】从(2,2)到(4,3)，当前角度180°（向右）" << std::endl;
    auto decision3 = planner.plan({2, 2}, {4, 3}, 180);
    planner.printDecision(decision3);
    
    // 测试用例4：从(3,1)到(1,4)，初始角度270°
    std::cout << "\n【测试4】从(3,1)到(1,4)，当前角度270°（向上）" << std::endl;
    auto decision4 = planner.plan({3, 1}, {1, 4}, 270);
    planner.printDecision(decision4);
    
    // 测试用例5：无效坐标测试
    std::cout << "\n【测试5】无效坐标测试：(5,5)超出范围" << std::endl;
    auto decision5 = planner.plan({5, 5}, {2, 2}, 0);
    planner.printDecision(decision5);
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "  所有测试完成！" << std::endl;
    std::cout << "======================================\n" << std::endl;
    
    return 0;
}