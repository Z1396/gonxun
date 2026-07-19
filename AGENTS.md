# gonxun 代码风格规范

> 代码定位：gonxun 是工创赛智能物流搬运系统，面向 Qt GUI + 串口通信 + 路径规划。
>
> 核心原则：用 snake_case 命名函数和变量，用 PascalCase 命名类型；用 std::expected 传播错误，
> 用 std::variant 表达互斥状态；构造函数 noexcept，可能失败的初始化用 static create()；
> 返回非 void 的函数必须 [[nodiscard]]；辅助函数放入匿名命名空间；单函数不超过 30 行。

---

## 命名约定

| 元素       | 风格             | 示例                              |
|------------|-----------------|-----------------------------------|
| 类名       | PascalCase      | `AStarPlanner`, `MotionController` |
| 函数名     | snake_case      | `plan_path()`, `check_collision()` |
| 成员变量   | snake_case_     | `grid_map_`, `current_cmd_`       |
| 局部变量   | snake_case      | `camera_matrix`, `start_angle`    |
| 常量       | UPPER_SNAKE     | `GRID_SIZE`, `FIELD_SIZE_MM`      |
| 枚举值     | PascalCase      | `Small`, `Large`, `Invalid`       |
| 命名空间   | snake_case      | `gonxun::planning`, `gonxun::motion` |
| 类型别名   | PascalCase      | `Path`, `BuildResult`             |
| 文件名     | snake_case.hpp  | `astar_planner.hpp`               |

## 文件组织

1. `#pragma once`（统一，禁止 `#ifndef`）
2. 文件头 Doxygen（`@file` + `@brief`，3-5 行）
3. Include 排序：项目内部 → 第三方 → 标准库
4. 扩展名统一 `.hpp` / `.cpp`

## 注释规范

- 文件头：`@file` + `@brief`（3-5 行），删除 `@author/@version/@date/@history/@copyright`
- 函数注释：`@brief` + `@param` + `@return`（5-10 行），删除 `@details/@note/@par` 大段说明
- 行内注释：仅关键逻辑处注释，自明代码不注释
- 分隔线：`// ==== Section Name ====` 或 `// ---- subsection ----`

## 函数设计

1. 单函数 ≤ 30 行，超过必须拆分
2. 辅助函数放入匿名命名空间
3. 构造函数 `noexcept`，可能失败的初始化用 `static create() noexcept -> std::expected<T, std::string>`
4. 返回非 void 的函数必须 `[[nodiscard]]`
5. 内部函数必须 `noexcept`
6. 单参数构造函数必须 `explicit`
7. 禁止 `default` 分支掩盖新增枚举值

## DO / DON'T

| DO                              | DON'T                       |
|---------------------------------|-----------------------------|
| snake_case 函数/变量            | camelCase 函数/变量          |
| 尾下划线成员变量                | m_ 前缀成员变量             |
| std::expected 表达错误          | 返回空容器/bool 表达错误    |
| std::variant 表达互斥状态       | enum + bool 拼状态          |
| `#pragma once`                  | `#ifndef` 保护              |
| `.hpp` 扩展名                   | `.h` 扩展名                 |
| 匿名命名空间辅助函数            | 类私有大函数               |
| 精简注释（5-10行/函数）         | 过度注释（40+行/函数）      |

## 依赖库版本

| 依赖       | 版本   |
|------------|--------|
| C++ 标准   | C++17  |
| Qt         | 5.x    |
| OpenCV     | 4.x    |
| Eigen3     | 3.4+   |
