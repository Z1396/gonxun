/**
 * @file simulation_controller.cpp
 * @brief 仿真控制器实现文件
 * 
 * @details 本文件实现了任务仿真控制器，用于离线测试任务执行流程。
 *          核心功能：
 *          - 导航序列生成：自动生成从启停区到各区域的路径
 *          - 动画播放控制：基于定时器的帧动画，支持加速/减速
 *          - 状态机集成：与 TaskStateMachine 同步状态
 *          - 数据面板更新：实时显示机器人坐标、任务进度
 *          
 *          仿真流程：
 *          1. 用户点击“仿真”按钮，启动仿真
 *          2. 调用 buildNavSequence() 生成导航序列
 *          3. 使用定时器逐帧更新机器人位置
 *          4. 每到达一个目标点，触发状态机事件
 *          5. 循环执行直到完成所有轮次
 *          
 *          导航序列结构：
 *          - 每轮循环：启停区 → 扫码区 → 原料区 → 粗加工区 → 暂存区 → 启停区
 *          - 坐标基于实际地图尺寸（2400x2400 mm）
 *          - 使用 A* 算法生成路径（若启用路径规划）
 *          
 *          性能优化：
 *          - 使用 Qt::PreciseTimer 提高定时精度
 *          - 帧间隔可配置（默认 50ms，20fps）
 *          - 停留时间可配置（默认 800ms）
 *          
 * @see simulation_controller.h 头文件定义
 * @see courtmapwidget.h 场地地图控件
 * @see data_panel_widget.h 数据面板控件
 * @see task_state_machine.h 任务状态机
 * 
 * @author 工创赛2025智能物流搬运系统团队
 * @date 2024-01-15
 * @version 1.0.0
 * @history 2024-01-15 初始版本
 * @history 2024-02-20 新增状态机集成
 * 
 * @copyright 工创赛2025智能物流搬运系统
 */
#include "simulation_controller.h"
#include <QDateTime>
#include <cmath>
#include <iostream>

/**
 * @brief SimulationController 构造函数
 * 
 * @details 初始化仿真控制器，完成以下工作：
 *          1. 创建动画定时器（高精度）
 *          2. 绑定状态机回调函数
 *          3. 初始化内部状态变量
 *          
 * @param mapWidget 场地地图控件指针
 * @param dataPanel 数据面板控件指针
 * @param parent 父对象（默认 nullptr）
 */
SimulationController::SimulationController(CourtMapWidget* mapWidget,
                                           DataPanelWidget* dataPanel,
                                           QObject* parent)
    : QObject(parent)
    , m_mapWidget(mapWidget)
    , m_dataPanel(dataPanel)
{
    // ==================== 动画定时器初始化 ====================
    m_animTimer = new QTimer(this);
    m_animTimer->setTimerType(Qt::PreciseTimer);  // 高精度定时器
    connect(m_animTimer, &QTimer::timeout, this, &SimulationController::onAnimationTick);

    // ==================== 状态机回调绑定 ====================
    // 状态变更回调：更新数据面板的状态显示
    m_stateMachine.onStateChange([this](gonxun::TaskState oldState, gonxun::TaskState newState) {
        Q_UNUSED(oldState)
        if (m_dataPanel) {
            m_dataPanel->updateTaskState(
                QString::fromStdString(gonxun::TaskStateMachine::stateToString(newState)));
        }
    });

    // 进度更新回调：更新数据面板的进度显示
    m_stateMachine.onProgressUpdate([this](const gonxun::TaskProgress& p) {
        if (m_dataPanel) {
            m_dataPanel->updateTaskProgress(p.currentCycle, p.totalCycles,
                                           p.materialsPicked, p.materialsPlaced,
                                           p.totalMaterials);
            m_dataPanel->updateTaskCode(QString::fromStdString(p.taskCode));
        }
    });
}

/**
 * @brief 启动仿真
 * 
 * @details 执行完整的仿真流程：
 *          1. 检查启停区是否已选择
 *          2. 检查机器人是否可见
 *          3. 更新障碍物列表
 *          4. 生成导航序列
 *          5. 初始化状态机
 *          6. 开始动画播放
 *          
 * @param taskCode 任务码字符串（如 "123"）
 * @return true：启动成功；false：启动失败（未选择启停区或机器人不可见）
 */
bool SimulationController::start(const QString& taskCode)
{
    // 防止重复启动
    if (m_running) {
        return false;
    }

    // 检查启停区是否已选择
    if (m_mapWidget->selectedStartZone() < 0) {
        m_phase = SimPhase::ERROR;
        emit phaseChanged(m_phase, "错误：未选择启停区");
        return false;
    }

    // 检查机器人是否可见
    if (!m_mapWidget->isRobotVisible()) {
        return false;
    }

    // ==================== 初始化仿真状态 ====================
    m_taskCode = taskCode;
    m_running = true;
    m_currentSegment = 0;
    m_currentCycle = 0;
    m_totalDistance = 0.0;
    m_totalSteps = 0;
    m_totalTimer.start();

    // 更新障碍物列表（从地图控件同步）
    updateObstaclesFromMap();
    
    // 生成导航序列
    m_navSequence = buildNavSequence();

    // ==================== 初始化状态机 ====================
    m_stateMachine.setTaskCode(taskCode.toStdString());
    m_stateMachine.setTotalCycles(m_totalCycles);
    m_stateMachine.setTotalMaterials(3);
    m_stateMachine.reset();
    m_stateMachine.handleEvent(gonxun::TaskEvent::START_MARKING);
    m_stateMachine.handleEvent(gonxun::TaskEvent::MARKING_DONE);
    m_stateMachine.handleEvent(gonxun::TaskEvent::START_MISSION);

    // 更新数据面板
    if (m_dataPanel) {
        m_dataPanel->updateTaskCode(taskCode);
        m_dataPanel->updateTaskState("仿真开始");
    }

    // 发送仿真启动信号
    emit simulationStarted();
    
    // 开始执行第一个导航段
    startNextSegment();
    return true;
}

void SimulationController::stop()
{
    m_running = false;
    m_animTimer->stop();
    m_phase = SimPhase::IDLE;
    m_currentSegment = 0;
    m_currentPathIdx = 0;
    emit simulationFinished(false);
}

QVector<SimulationController::NavTarget> SimulationController::buildNavSequence() const
{
    QVector<NavTarget> seq;

    // 起点位置（已选启停区中心）
    QPointF startPos = m_mapWidget->robotPos();

    // 导航序列：起停区 → 扫码区 → 原料区 → 粗加工区 → 暂存区 → 起停区
    // 循环时重复：扫码区 → 原料区 → 粗加工区 → 暂存区
    // 坐标基于实际地图：二维码区(2360,1200) 原料区(1200,50) 粗加工区(1050/1200/1350,2325) 暂存区(75,1200)
    for (int cycle = 0; cycle < m_totalCycles; ++cycle) {
        // 前往二维码区（右侧）
        seq.append({"前往扫码区", {2360, 1200}, "前往扫码区"});

        // 前往原料区（顶部中心）
        seq.append({"前往原料区", {1200, 50}, "前往原料区"});

        // 粗加工区有3个槽位，根据任务码决定顺序
        auto order = [this]() -> QVector<int> {
            if (m_taskCode.length() >= 3) {
                return {m_taskCode[0].digitValue(), m_taskCode[1].digitValue(), m_taskCode[2].digitValue()};
            }
            return {1, 2, 3};
        }();

        for (int i = 0; i < 3; ++i) {
            int slot = order[i] - 1;
            // 粗加工区3个槽位：x=1050, 1200, 1350, y=2325
            seq.append({QString("送物料%1到粗加工%2").arg(i+1).arg(slot+1),
                       {1050 + slot * 150, 2325}, "前往粗加工区"});
            // 每次放完需要回原料区取下一个
            if (i < 2) {
                seq.append({"前往原料区", {1200, 50}, "前往原料区"});
            }
        }

        // 前往暂存区（左侧）
        seq.append({"前往暂存区", {75, 1200}, "前往暂存区"});
    }

    // 最后返回起停区
    seq.append({"返回启停区", {static_cast<int>(startPos.x()), static_cast<int>(startPos.y())}, "返回启停区"});

    return seq;
}

void SimulationController::startNextSegment()
{
    if (m_currentSegment >= m_navSequence.size()) {
        onAllComplete();
        return;
    }

    const NavTarget& target = m_navSequence[m_currentSegment];

    m_phase = SimPhase::PLANNING;
    emit phaseChanged(m_phase, target.name);

    if (m_dataPanel) {
        m_dataPanel->updateTaskState(target.name);
    }

    planCurrentSegment();
}

void SimulationController::planCurrentSegment()
{
    const NavTarget& target = m_navSequence[m_currentSegment];

    QPointF currentPos = m_mapWidget->robotPos();
    gonxun::Point start{static_cast<int>(currentPos.x()), static_cast<int>(currentPos.y())};
    gonxun::Point goal{target.pos.x, target.pos.y};

    m_segmentTimer.start();

    m_currentPath = m_planner.plan(start, goal);

    if (m_currentPath.empty()) {
        m_phase = SimPhase::ERROR;
        m_stateMachine.handleEvent(gonxun::TaskEvent::ERROR_OCCURRED);
        emit phaseChanged(m_phase, "路径规划失败");
        stop();
        return;
    }

    QVector<QPointF> pathPts;
    for (const auto& p : m_currentPath) {
        pathPts.append(QPointF(p.x, p.y));
    }
    m_mapWidget->setPath(pathPts);

    startMoving();
}

void SimulationController::startMoving()
{
    m_phase = SimPhase::MOVING;
    m_currentPathIdx = 0;
    emit phaseChanged(m_phase, "移动中");
    m_animTimer->start(m_animInterval);
}

void SimulationController::startDwelling()
{
    m_phase = SimPhase::DWELLING;
    m_animTimer->stop();
    emit phaseChanged(m_phase, "到达目标");

    const NavTarget& target = m_navSequence[m_currentSegment];
    if (m_dataPanel) {
        m_dataPanel->updateTaskState("到达" + target.name);
    }

    QTimer::singleShot(m_dwellTime, this, [this]() {
        onSegmentComplete();
    });
}

void SimulationController::onAnimationTick()
{
    if (m_currentPathIdx >= static_cast<int>(m_currentPath.size())) {
        startDwelling();
        return;
    }

    const auto& pt = m_currentPath[m_currentPathIdx];

    QPointF currentPos = m_mapWidget->robotPos();
    double dx = pt.x - currentPos.x();
    double dy = pt.y - currentPos.y();
    double angle = std::atan2(dy, dx) * 180.0 / M_PI;
    if (angle < 0) angle += 360.0;

    m_totalDistance += std::sqrt(dx*dx + dy*dy);
    m_totalSteps++;

    m_mapWidget->setRobotPos(QPointF(pt.x, pt.y), angle);

    if (m_dataPanel) {
        m_dataPanel->updateRobotPose(pt.x, pt.y, angle);
    }

    m_currentPathIdx++;
}

void SimulationController::onSegmentComplete()
{
    const NavTarget& target = m_navSequence[m_currentSegment];

    if (target.name.contains("扫码区")) {
        m_stateMachine.handleEvent(gonxun::TaskEvent::REACHED_QR);
        m_stateMachine.handleEvent(gonxun::TaskEvent::QR_SCANNED);
    } else if (target.name.contains("原料区")) {
        m_stateMachine.handleEvent(gonxun::TaskEvent::REACHED_MATERIAL);
        m_stateMachine.handleEvent(gonxun::TaskEvent::MATERIAL_PICKED);
    } else if (target.name.contains("粗加工")) {
        m_stateMachine.handleEvent(gonxun::TaskEvent::REACHED_PROCESS);
        m_stateMachine.handleEvent(gonxun::TaskEvent::MATERIAL_PLACED);
    } else if (target.name.contains("暂存区")) {
        m_stateMachine.handleEvent(gonxun::TaskEvent::REACHED_BUFFER);
    } else if (target.name.contains("启停区")) {
        m_stateMachine.handleEvent(gonxun::TaskEvent::REACHED_START);
        m_stateMachine.handleEvent(gonxun::TaskEvent::ALL_DONE);
    }

    m_mapWidget->clearPath();
    m_currentSegment++;

    if (m_currentSegment >= m_navSequence.size()) {
        onAllComplete();
        return;
    }

    startNextSegment();
}

void SimulationController::onAllComplete()
{
    m_running = false;
    m_phase = SimPhase::COMPLETED;
    m_animTimer->stop();

    if (m_dataPanel) {
        m_dataPanel->updateTaskState("任务完成");
    }

    emit phaseChanged(m_phase, "全部完成");
    emit simulationFinished(true);
}

void SimulationController::updateObstaclesFromMap()
{
    auto markedObs = m_mapWidget->getMarkedObstacles();
    std::vector<gonxun::ObstacleRect> obstacles;
    for (const auto& obs : markedObs) {
        obstacles.push_back({
            static_cast<int>(obs.rect.x()),
            static_cast<int>(obs.rect.y()),
            static_cast<int>(obs.rect.width()),
            static_cast<int>(obs.rect.height())
        });
    }
    m_planner.setObstacles(obstacles);
}

void SimulationController::emitLog(const QString& msg)
{
    std::cout << msg.toStdString() << std::endl;
    emit logMessage(msg);
}