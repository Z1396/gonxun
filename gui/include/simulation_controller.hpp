/// @file simulation_controller.hpp
/// @brief GUI可视化仿真控制器，驱动5×5格子BFS路径规划与段级 pace 动画。
///        按任务编码顺序依次导航至扫码区→原料区→粗加工区→暂存区，
///        每段路径使用BFS避开已标记障碍物，到达后驻留等待再规划下一段。
///        动画与下位机段进度严格同步：收到 segment_completed 才推进机器人位置。

#pragma once

#include "courtmapwidget.hpp"
#include "motion_controller.hpp"
#include "task_state_machine.hpp"

#include <QElapsedTimer>
#include <QPointF>
#include <QTimer>
#include <QVector>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

/// @brief 仿真阶段枚举，描述仿真控制器的当前运行阶段。
enum class SimPhase {
    IDLE,                ///< 空闲，未启动仿真
    PLANNING,            ///< 正在为当前段规划BFS路径
    WAITING_MOVE_DONE,   ///< 已发送路径段，等待下位机 move_done
    WAITING_GRAB_DONE,   ///< 已发送抓取指令，等待下位机 grab_done
    DWELLING,            ///< 到达目标后驻留等待
    COMPLETED,           ///< 全部导航段已完成
    ERROR                ///< 路径规划失败或其他错误
};

/// @brief 仿真控制器，管理多段导航序列的路径规划、动画播放与状态机推进。
///
/// 工作流程：start() → build_nav_sequence() → 循环(start_next_segment →
/// plan_current_segment[BFS] → start_moving → on_animation_tick →
/// start_dwelling → on_segment_complete) → on_all_complete。
/// 每段BFS在5×5格子图上搜索，避开用户标记的障碍物和固定障碍物。
class SimulationController : public QObject
{
    Q_OBJECT

public:
    /// @brief 构造仿真控制器，初始化动画定时器与状态机回调。
    /// @param map_widget 赛场地图控件（必须存在）
    /// @param motion_controller 运动控制器（可为 nullptr）
    /// @param parent 父对象
    explicit SimulationController(CourtMapWidget& map_widget,
                                  MotionController* motion_controller = nullptr,
                                  QObject* parent = nullptr) noexcept;
    ~SimulationController() override = default;

    /// @brief 启动仿真，初始化导航序列与状态机，开始第一段路径规划。
    /// @param task_code 任务编码，如 "123"、"312"，决定物料抓取顺序
    /// @return true 启动成功；false 已在运行或未选择启停区或机器人不可见
    [[nodiscard]] bool start(const QString& task_code = "123");

    /// @brief 停止仿真，重置所有状态并发射 simulation_finished(false)。
    void stop() noexcept;

    /// @brief 查询仿真是否正在运行。
    /// @return true 正在运行
    [[nodiscard]] bool is_running() const noexcept { return running_; }

    /// @brief 设置到达目标后的驻留时间。
    /// @param ms 驻留毫秒数，默认1000ms
    void set_dwell_time(int ms) noexcept { dwell_time_ = ms; }

    /// @brief 设置总循环次数。
    /// @param cycles 循环次数，默认1
    void set_total_cycles(int cycles) noexcept { total_cycles_ = cycles; }

    // ==== 物料坐标提供者（到达抓取目标时由 MainWindow 注入） ====

    /// @brief 物料坐标提供者：返回下一个待抓取物料坐标，无则 nullopt
    using CoordProvider = std::function<std::optional<std::pair<uint16_t, uint16_t>>()>;

    /// @brief 设置物料坐标提供者（由 MainWindow 注入，读取 VisionSystem 缓存）
    void set_material_coord_provider(CoordProvider p) noexcept {
        material_coord_provider_ = std::move(p);
    }

signals:
    /// @brief 仿真成功启动时发射。
    void simulation_started();

    /// @brief 仿真结束时发射（正常完成或被停止）。
    /// @param success true 全部段正常完成；false 被手动停止或出错
    void simulation_finished(bool success);

    /// @brief 仿真阶段变更时发射，携带新阶段与描述文本。
    /// @param phase 新阶段
    /// @param description 阶段描述（如"前往扫码区"、"移动中"）
    void phase_changed(SimPhase phase, const QString& description);

private slots:
    /// @brief 收到一段移动完成（move_done）：把机器人跳到该段终点。
    /// @param seg_idx 段索引
    void on_segment_completed(int seg_idx);

    /// @brief 整段路径全部完成（path_completed）：判断是否需要抓取。
    void on_path_completed();

    /// @brief 抓取完成（grab_done）：进入驻留或推进下一段。
    void on_grab_completed();

    /// @brief 运动错误：停止仿真。
    void on_motion_error(const QString& err);

private:
    /// @brief 启动下一段导航：更新阶段为PLANNING，调用plan_current_segment()。
    void start_next_segment();

    /// @brief 为当前段执行BFS路径规划，找到路径后启动移动。
    ///        若起点与终点重合则直接完成；若路径被阻断则报错停止。
    void plan_current_segment();

    /// @brief 切换至DWELLING阶段：停止动画，延迟dwell_time_后推进到下一段。
    void start_dwelling();

    /// @brief 当前段完成处理：推进状态机事件、清除路径、递增段索引。
    void on_segment_complete();

    /// @brief 全部导航段完成：停止定时器，设阶段COMPLETED，发射simulation_finished(true)。
    void on_all_complete();

    /// @brief 判断当前导航目标是否需要抓取（原料区/粗加工区）。
    /// @return true 表示到达后需要发送抓取指令
    [[nodiscard]] bool current_target_needs_grab() const;

    /// @brief 导航目标结构，描述一个要到达的格子位置。
    struct NavTarget {
        QString name;   ///< 目标名称（如"前往扫码区"）
        int grid_x;     ///< 目标格子X坐标（0-4）
        int grid_y;     ///< 目标格子Y坐标（0-4）
    };

    /// @brief 根据循环次数与启停区选择构建完整导航序列。
    /// @return 导航目标列表，每循环包含4个区域，末尾返回启停区
    [[nodiscard]] QVector<NavTarget> build_nav_sequence() const;

private:
    CourtMapWidget& map_widget_;            ///< 赛场地图控件
    MotionController* motion_controller_;   ///< 运动控制器（可为空）

    gonxun::TaskStateMachine state_machine_; ///< 任务状态机

    bool running_ = false;                   ///< 仿真是否正在运行
    SimPhase phase_ = SimPhase::IDLE;        ///< 当前仿真阶段
    int current_segment_ = 0;                ///< 当前导航段索引
    int current_cycle_ = 0;                  ///< 当前循环计数
    int total_cycles_ = 1;                   ///< 总循环次数

    QVector<NavTarget> nav_sequence_;        ///< 导航目标序列
    QVector<QPair<int, int>> segment_endpoints_; ///< 当前段路径每段的终点格子坐标

    QTimer* dwell_timer_ = nullptr;          ///< 驻留定时器
    int dwell_time_ = 1000;                   ///< 驻留时间(ms)

    QElapsedTimer total_timer_;              ///< 仿真总耗时计时器
    double total_distance_ = 0.0;            ///< 累计移动距离(mm)
    int total_steps_ = 0;                    ///< 累计动画步数

    QString task_code_;                      ///< 当前任务编码

    CoordProvider material_coord_provider_;  ///< 物料坐标提供者（可为空）
};
