/**
 * 3×3 网格路径规划器
 *
 * 场地 2400×2400mm，4块黄色障碍区域(450×450)在格子之间
 * 黄色块将场地分割成3条横向通道 + 3条纵向通道
 * 9个格子 = 通道交叉点，全部可通行
 *
 * 网格通行图（□=可通行，黄色块在格子之间）：
 *   ┌─────┬─────┬─────┐
 *   │(0,0)│(1,0)│(2,0)│   顶部通道 y=0-550
 *   │启停1│原料 │     │
 *   ├─────┼─────┼─────┤
 *   │(0,1)│(1,1)│(2,1)│   中间通道 y=1000-1400
 *   │二维码│中心 │暂存 │   ↑ 黄色块在 y=550-1000 和 y=1400-1850
 *   ├─────┼─────┼─────┤
 *   │(0,2)│(1,2)│(2,2)│   底部通道 y=1850-2400
 *   │启停2│粗加工│    │
 *   └─────┴─────┴─────┘
 *    右侧   中间   左侧
 *   x=1850+ x=1000-1400 x=0-550
 *    ↑ 黄色块在 x=550-1000 和 x=1400-1850
 *
 * 通道中心坐标（网格→场地mm）：
 *   (0,0)=(2125,275)  (1,0)=(1200,275)  (2,0)=(275,275)
 *   (0,1)=(2125,1200) (1,1)=(1200,1200) (2,1)=(275,1200)
 *   (0,2)=(2125,2125) (1,2)=(1200,2125) (2,2)=(275,2125)
 *
 * 移动规则：仅允许上下左右相邻格之间移动（横平竖直）
 */
#pragma once

#include <vector>
#include <array>
#include <string>
#include <optional>

namespace gonxun {

// 网格坐标（整数）
struct GridCoord {
    int x;  // 0=右, 1=中, 2=左
    int y;  // 0=顶, 1=中, 2=底

    bool operator==(const GridCoord& o) const { return x == o.x && y == o.y; }
    bool operator!=(const GridCoord& o) const { return x != o.x || y != o.y; }
};

// 网格路径
using GridPath = std::vector<GridCoord>;

// 各区域对应的网格坐标
// 启停区1 = (0,0)  原料区 = (1,0)
// 二维码区 = (0,1)  暂存区 = (2,1)
// 启停区2 = (0,2)  粗加工区 = (1,2)

class GridPlanner {
public:
    GridPlanner();

    /**
     * 检查网格是否可通行
     */
    bool isWalkable(int x, int y) const;

    /**
     * 检查网格是否可通行
     */
    bool isWalkable(const GridCoord& c) const { return isWalkable(c.x, c.y); }

    /**
     * 设置障碍物格子（标记为不可通行）
     */
    void setBlocked(int x, int y, bool blocked = true);

    /**
     * 规划路径（BFS 最短路径，仅上下左右移动）
     * @param start 起点
     * @param goal  终点
     * @return 路径（网格坐标序列），空表示不可达
     */
    GridPath plan(const GridCoord& start, const GridCoord& goal);

    /**
     * 网格坐标 → 场地坐标（毫米，格子中心点）
     */
    static int gridToFieldX(int gx);
    static int gridToFieldY(int gy);

    /**
     * 场地坐标（毫米）→ 网格坐标
     */
    static GridCoord fieldToGrid(int xMm, int yMm);

    /**
     * 网格坐标 → 可读字符串
     */
    static std::string toString(const GridCoord& c);

    /**
     * 网格路径 → 可读字符串
     */
    static std::string pathToString(const GridPath& path);

    /**
     * 打印网格地图（调试用）
     */
    std::string dumpGrid(const GridPath& highlight = {}) const;

private:
    // 3×3 通行图：true=可通行, false=障碍
    std::array<std::array<bool, 3>, 3> m_walkable;

    // 获取相邻格子（仅上下左右4方向）
    std::vector<GridCoord> getNeighbors(const GridCoord& c) const;
};

} // namespace gonxun