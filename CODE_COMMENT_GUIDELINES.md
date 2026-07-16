# 代码注释规范与模板

> **适用范围：** 工创赛2025智能物流搬运系统（gonxun）
> **更新日期：** 2026-07-15

---

## 一、注释原则

### 1.1 注释的目的

- **提升可读性**：让其他开发者快速理解代码功能
- **便于维护**：记录设计决策、限制条件和优化点
- **辅助调试**：标注关键算法和潜在问题
- **规范协作**：统一团队代码风格

### 1.2 注释要求

| 要求 | 说明 |
|------|------|
| **准确性** | 注释必须与代码逻辑一致，禁止误导 |
| **简洁性** | 避免冗余，用最少的文字表达清楚 |
| **及时性** | 修改代码时同步更新注释 |
| **规范性** | 遵循 Doxygen 格式，支持自动生成文档 |

---

## 二、Doxygen 注释格式

### 2.1 文件头部注释

```cpp
/**
 * @file 文件名.cpp
 * @brief 一句话概述文件功能
 * 
 * @details 详细描述文件的主要功能、实现思路、关键算法等。
 *          可以多行，详细说明模块的设计原理。
 * 
 * @author 作者姓名
 * @version 版本号
 * @date 创建/修改日期
 * 
 * @note 关键注意事项：
 *       - 性能指标
 *       - 已知限制
 *       - 依赖条件
 * 
 * @see 相关类/函数
 * @copyright 版权信息
 */
```

**示例：**

```cpp
/**
 * @file astar_planner.hpp
 * @brief A* 路径规划算法实现模块
 * 
 * @details 本模块实现了基于 A* 算法的机器人路径规划功能。
 *          主要用于在 2400×2400mm 的比赛场地上规划从起点到终点的最优路径，
 *          同时避开场地中的静态障碍物（黄色区域）和动态标记的障碍物。
 * 
 * @author gonxun 开发团队
 * @version 1.0
 * @date 2026-07-15
 * 
 * @note 算法性能：
 *       - 平均规划时间：< 5ms（单个路径）
 *       - 内存占用：约 2.3KB（48×48 栅格地图）
 * 
 * @copyright 工创赛2025智能物流搬运系统
 */
```

---

### 2.2 类注释

```cpp
/**
 * @class 类名
 * @brief 一句话概述类的功能
 * 
 * @details 详细说明类的设计目的、使用场景、核心算法。
 *          包含使用示例代码。
 * 
 * @see 相关类
 * 
 * @par 使用示例：
 * @code
 * ClassName obj;
 * obj.method();
 * @endcode
 */
class ClassName {
    // ...
};
```

**示例：**

```cpp
/**
 * @class AStarPlanner
 * @brief A* 路径规划算法实现类
 * 
 * @details 本类实现了经典的 A* 搜索算法，用于在栅格化地图上寻找最短路径。
 *          算法核心思想：
 *          1. 维护两个集合：开放集和封闭集
 *          2. 优先探索 f(n) = g(n) + h(n) 最小的节点
 *          3. 使用曼哈顿距离作为启发函数
 * 
 * @see GridPlanner 高层网格规划器
 */
class AStarPlanner {
    // ...
};
```

---

### 2.3 函数注释

```cpp
/**
 * @brief 一句话概述函数功能
 * 
 * @details 详细说明函数的实现思路、算法流程（如果复杂）。
 * 
 * @param 参数1 参数1的说明（类型、用途、默认值）
 * @param 参数2 参数2的说明
 * 
 * @return 返回值说明（类型、含义、可能的取值）
 * 
 * @note 关键注意事项
 * 
 * @par 性能说明：
 *      时间复杂度、空间复杂度、平均耗时
 * 
 * @par 异常处理：
 *      可能的异常情况及处理方式
 */
ReturnType functionName(Param1 param1, Param2 param2);
```

**示例：**

```cpp
/**
 * @brief 规划从起点到终点的最优路径
 * 
 * @details 使用 A* 算法在栅格地图上搜索最短路径，避开所有障碍物。
 *          算法流程：
 *          1. 坐标转换和边界检查
 *          2. 处理起点在障碍物内的情况
 *          3. 执行 A* 搜索
 *          4. 回溯重建路径
 *          5. 路径简化
 * 
 * @param start 起点（毫米坐标）
 * @param goal 终点（毫米坐标）
 * 
 * @return 路径点数组（毫米坐标）
 *         - 非空数组：规划成功
 *         - 空数组：规划失败
 * 
 * @par 性能说明：
 *      - 平均规划时间：2-5ms
 *      - 最坏情况：约 2300 次节点扩展
 */
Path plan(const Point& start, const Point& goal);
```

---

### 2.4 成员变量注释

```cpp
/**
 * @brief 成员变量的简要说明
 * 
 * 详细说明变量的用途、取值范围、单位等。
 */
int m_variable;  ///< 行尾注释（适用于简单变量）
```

**示例：**

```cpp
/**
 * @brief 栅格地图
 * 
 * 48×48 的二维数组，表示整个场地的栅格化模型。
 * - m_gridMap[x][y] = true：障碍物
 * - m_gridMap[x][y] = false：可通行
 * 
 * @note 索引顺序为 [x][y]，对应栅格坐标。
 */
std::vector<std::vector<bool>> m_gridMap;

int FIELD_SIZE_MM = 2400;  ///< 场地尺寸（毫米）
```

---

### 2.5 结构体注释

```cpp
/**
 * @struct 结构体名
 * @brief 一句话概述结构体功能
 * 
 * 详细说明结构体的用途、成员变量含义、使用场景。
 * 
 * @note 关键注意事项（如内存布局、对齐等）
 */
struct StructName {
    int x;  ///< X 坐标（毫米）
    int y;  ///< Y 坐标（毫米）
};
```

**示例：**

```cpp
/**
 * @struct Point
 * @brief 二维点坐标结构体
 * 
 * 用于表示场地中的位置，包含 X 和 Y 坐标（单位：毫米）。
 * 支持相等性比较，可用于 STL 容器。
 * 
 * @note 坐标原点位于场地左上角，X 轴向右，Y 轴向下。
 *       有效范围：0 ≤ x, y < 2400
 */
struct Point {
    int x{0};  ///< X 轴坐标（毫米），范围 [0, 2400)
    int y{0};  ///< Y 轴坐标（毫米），范围 [0, 2400)
};
```

---

### 2.6 枚举注释

```cpp
/**
 * @enum 枚举名
 * @brief 一句话概述枚举功能
 * 
 * 详细说明枚举的用途、各值的含义。
 */
enum class EnumName {
    VALUE1,  ///< 值1的含义
    VALUE2,  ///< 值2的含义
};
```

**示例：**

```cpp
/**
 * @enum TaskState
 * @brief 任务状态枚举
 * 
 * 表示任务状态机的当前状态，用于控制机器人任务流程。
 */
enum class TaskState {
    IDLE,              ///< 空闲状态，等待开始
    MARKING_OBSTACLES, ///< 标记障碍物中
    SELECTING_ZONE,    ///< 选择启停区中
    MOVING_TO_QR,      ///< 前往二维码区
    SCANNING_QR,       ///< 扫描二维码中
    MOVING_TO_MATERIAL,///< 前往原料区
    PICKING_MATERIAL,  ///< 取料中
    MOVING_TO_PROCESS, ///< 前往粗加工区
    PLACING_MATERIAL,  ///< 放料中
    MOVING_TO_BUFFER,  ///< 前往暂存区
    RETURNING_START,   ///< 返回启停区
    COMPLETED,         ///< 任务完成
    ERROR              ///< 错误状态
};
```

---

## 三、代码块注释

### 3.1 算法实现注释

```cpp
// ============================================================================
// 算法名称
// ============================================================================

/**
 * @brief 算法步骤1：初始化
 * 
 * 详细说明初始化过程。
 */
void step1() {
    // 创建初始数据结构
    // ...
}

/**
 * @brief 算法步骤2：核心逻辑
 * 
 * 详细说明核心处理流程。
 */
void step2() {
    // 循环处理每个数据项
    for (auto& item : items) {
        // 检查条件
        if (condition) {
            // 执行操作
            // ...
        }
    }
}
```

---

### 3.2 复杂逻辑注释

```cpp
// 处理特殊情况：起点在障碍物内
if (!isWalkable(startGrid.x, startGrid.y)) {
    bool found = false;
    // 在周围半径 200mm 范围内搜索最近可行点
    for (int r = 1; r < 5 && !found; ++r) {  // r=1~4，对应 50mm~200mm
        for (int dx = -r; dx <= r && !found; ++dx) {
            for (int dy = -r; dy <= r && !found; ++dy) {
                int nx = startGrid.x + dx;
                int ny = startGrid.y + dy;
                // 检查新位置是否在有效范围内且可通行
                if (nx >= 0 && nx < GRID_SIZE && 
                    ny >= 0 && ny < GRID_SIZE && 
                    isWalkable(nx, ny)) {
                    startGrid.x = nx;
                    startGrid.y = ny;
                    found = true;
                }
            }
        }
    }
}
```

---

### 3.3 性能优化注释

```cpp
/**
 * @par 性能优化：
 *      1. 使用 unordered_map 代替 map，查找复杂度从 O(log n) 降到 O(1)
 *      2. 预先分配 vector 容量，避免动态扩容
 *      3. 使用移动语义减少拷贝开销
 */
std::unordered_map<Point, int, PointHash> gScore;
gScore.reserve(1000);  // 预分配容量
```

---

## 四、注释模板快速参考

### 4.1 文件模板

```cpp
/**
 * @file 文件名
 * @brief 功能概述
 * @details 详细描述
 * @author 作者
 * @version 版本
 * @date 日期
 * @note 注意事项
 * @copyright 版权
 */
```

### 4.2 类模板

```cpp
/**
 * @class 类名
 * @brief 功能概述
 * @details 详细描述
 * @see 相关类
 */
```

### 4.3 函数模板

```cpp
/**
 * @brief 功能概述
 * @param 参数 参数说明
 * @return 返回值说明
 * @note 注意事项
 * @par 性能说明
 */
```

### 4.4 变量模板

```cpp
int m_var;  ///< 简要说明

/**
 * @brief 详细说明
 */
int m_complexVar;
```

---

## 五、已添加注释的核心文件

以下文件已按照本规范添加完整注释：

| 文件 | 路径 | 说明 |
|------|------|------|
| `astar_planner.hpp` | [core/include/astar_planner.hpp](file:///home/pldx/Desktop/gonxun/core/include/astar_planner.hpp) | A* 路径规划算法（完整注释示范） |

---

## 六、注释检查清单

在提交代码前，请检查以下项目：

- [ ] 文件头部包含完整的 Doxygen 注释
- [ ] 类定义包含 `@brief` 和 `@details`
- [ ] 公有方法包含完整的参数和返回值说明
- [ ] 复杂算法有步骤说明
- [ ] 性能关键代码有性能注释
- [ ] 成员变量有用途说明
- [ ] 注释与代码逻辑一致
- [ ] 无拼写错误和语法错误

---

## 七、Doxygen 生成文档

### 7.1 安装 Doxygen

```bash
sudo apt install doxygen
```

### 7.2 生成文档

```bash
cd /home/pldx/Desktop/gonxun
doxygen -g Doxyfile
doxygen Doxyfile
```

生成的 HTML 文档位于 `html/` 目录。

---

**最后更新：** 2026-07-15
**维护者：** gonxun 开发团队