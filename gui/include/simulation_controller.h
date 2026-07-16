#ifndef SIMULATION_CONTROLLER_H
#define SIMULATION_CONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QPointF>
#include <QVector>
#include <QElapsedTimer>
#include "courtmapwidget.h"
#include "data_panel_widget.h"
#include "astar_planner.hpp"
#include "grid_planner.hpp"
#include "task_state_machine.hpp"

/**
 * @brief 仿真阶段枚举
 */
enum class SimPhase {
    IDLE,               // 空闲
    PLANNING,           // 正在规划路径
    MOVING,             // 机器人移动中
    DWELLING,           // 到达目标点停留中
    COMPLETED,          // 全部完成
    ERROR               // 错误（路径不可达等）
};

/**
 * @brief GUI 可视化仿真控制器
 *
 * 负责：
 * 1. 从 CourtMapWidget 获取障碍物和起点
 * 2. 使用 AStarPlanner 规划各段路径
 * 3. 用 QTimer 驱动机器人沿路径点动画移动
 * 4. 实时更新 CourtMapWidget（机器人位置+路径轨迹）
 * 5. 实时更新 DataPanelWidget（状态/进度/路径信息）
 * 6. 联动 TaskStateMachine 推进流程
 */
class SimulationController : public QObject
{
    Q_OBJECT

public:
    explicit SimulationController(CourtMapWidget* mapWidget,
                                  DataPanelWidget* dataPanel,
                                  QObject* parent = nullptr);
    ~SimulationController() = default;

    /**
     * @brief 启动完整仿真流程
     * @param taskCode 任务码（如 "312"）
     * @return 是否成功启动
     */
    bool start(const QString& taskCode = "123");

    /**
     * @brief 停止仿真
     */
    void stop();

    /**
     * @brief 是否正在运行
     */
    bool isRunning() const { return m_running; }

    /**
     * @brief 设置动画速度（毫秒/步）
     *        默认 50ms/步 = 20fps
     */
    void setAnimationInterval(int ms) { m_animInterval = ms; }

    /**
     * @brief 设置停留时间（毫秒）
     *        到达每个目标点后的等待时间
     */
    void setDwellTime(int ms) { m_dwellTime = ms; }

    /**
     * @brief 设置循环次数
     */
    void setTotalCycles(int cycles) { m_totalCycles = cycles; }

signals:
    void simulationStarted();
    void simulationFinished(bool success);
    void phaseChanged(SimPhase phase, const QString& description);
    void logMessage(const QString& msg);

private slots:
    void onAnimationTick();

private:
    // 仿真流程控制
    void startNextSegment();
    void planCurrentSegment();
    void startMoving();
    void startDwelling();
    void onSegmentComplete();
    void onAllComplete();

    // 工具方法
    void updateObstaclesFromMap();
    void emitLog(const QString& msg);

    // 目标点定义（场地坐标，毫米）
    struct NavTarget {
        QString name;
        gonxun::Point pos;
        QString taskEventDesc;
    };

    // 完整导航序列
    QVector<NavTarget> buildNavSequence() const;

private:
    CourtMapWidget* m_mapWidget;
    DataPanelWidget* m_dataPanel;

    // 核心模块
    gonxun::AStarPlanner m_planner;
    gonxun::GridPlanner m_gridPlanner;
    gonxun::TaskStateMachine m_stateMachine;

    // 仿真状态
    bool m_running = false;
    SimPhase m_phase = SimPhase::IDLE;
    int m_currentSegment = 0;       // 当前导航段索引
    int m_currentPathIdx = 0;       // 当前路径点索引
    int m_currentCycle = 0;         // 当前循环次数
    int m_totalCycles = 1;          // 总循环次数

    // 路径数据
    QVector<NavTarget> m_navSequence;
    gonxun::Path m_currentPath;

    // 动画定时器
    QTimer* m_animTimer = nullptr;
    int m_animInterval = 50;        // 动画间隔（ms）
    int m_dwellTime = 1000;         // 停留时间（ms）

    // 统计数据
    QElapsedTimer m_segmentTimer;   // 当前段计时
    QElapsedTimer m_totalTimer;     // 总计时
    double m_totalDistance = 0.0;   // 总行驶距离
    int m_totalSteps = 0;           // 总步数

    // 任务码
    QString m_taskCode;
};

#endif // SIMULATION_CONTROLLER_H