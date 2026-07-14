#include "simulation_controller.h"
#include <QDateTime>
#include <cmath>
#include <iostream>

SimulationController::SimulationController(CourtMapWidget* mapWidget,
                                           DataPanelWidget* dataPanel,
                                           QObject* parent)
    : QObject(parent)
    , m_mapWidget(mapWidget)
    , m_dataPanel(dataPanel)
{
    m_animTimer = new QTimer(this);
    m_animTimer->setTimerType(Qt::PreciseTimer);
    connect(m_animTimer, &QTimer::timeout, this, &SimulationController::onAnimationTick);

    // 状态机回调
    m_stateMachine.onStateChange([this](gonxun::TaskState oldState, gonxun::TaskState newState) {
        QString msg = QString("[状态] %1 -> %2")
            .arg(QString::fromStdString(gonxun::TaskStateMachine::stateToString(oldState)))
            .arg(QString::fromStdString(gonxun::TaskStateMachine::stateToString(newState)));
        emitLog(msg);

        if (m_dataPanel) {
            m_dataPanel->updateTaskState(
                QString::fromStdString(gonxun::TaskStateMachine::stateToString(newState)));
        }
    });

    m_stateMachine.onProgressUpdate([this](const gonxun::TaskProgress& p) {
        if (m_dataPanel) {
            m_dataPanel->updateTaskProgress(p.currentCycle, p.totalCycles,
                                           p.materialsPicked, p.materialsPlaced,
                                           p.totalMaterials);
            m_dataPanel->updateTaskCode(QString::fromStdString(p.taskCode));
        }
    });
}

bool SimulationController::start(const QString& taskCode)
{
    if (m_running) {
        emitLog("[仿真] 已在运行中，请先停止");
        return false;
    }

    // 检查是否已选择启停区
    if (m_mapWidget->selectedStartZone() < 0) {
        emitLog("[仿真] 错误：请先选择启停区");
        m_phase = SimPhase::ERROR;
        emit phaseChanged(m_phase, "错误：未选择启停区");
        return false;
    }

    // 检查机器人是否可见
    if (!m_mapWidget->isRobotVisible()) {
        emitLog("[仿真] 错误：机器人未放置");
        return false;
    }

    m_taskCode = taskCode;
    m_running = true;
    m_currentSegment = 0;
    m_currentCycle = 0;
    m_totalDistance = 0.0;
    m_totalSteps = 0;
    m_totalTimer.start();

    // 从地图获取障碍物
    updateObstaclesFromMap();

    // 构建导航序列
    m_navSequence = buildNavSequence();

    // 设置任务码
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
        m_dataPanel->updateSerialStatus(false, true, "仿真模式");
        m_dataPanel->updateRobotMotion(true, 150, "前进");
    }

    emitLog(QString("[仿真] 开始 | 任务码: %1 | 循环: %2 | 导航段: %3")
            .arg(taskCode).arg(m_totalCycles).arg(m_navSequence.size()));
    emit simulationStarted();

    // 开始第一段导航
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

    if (m_dataPanel) {
        m_dataPanel->updateRobotMotion(false, 0, "停止");
    }

    emitLog("[仿真] 已停止");
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
    emitLog(QString("[导航] 第 %1/%2 段: %3")
            .arg(m_currentSegment + 1)
            .arg(m_navSequence.size())
            .arg(target.name));

    m_phase = SimPhase::PLANNING;
    emit phaseChanged(m_phase, QString("规划路径: %1").arg(target.name));

    if (m_dataPanel) {
        m_dataPanel->updateTaskState(target.name);
    }

    // 规划路径
    planCurrentSegment();
}

void SimulationController::planCurrentSegment()
{
    const NavTarget& target = m_navSequence[m_currentSegment];

    // 当前机器人位置
    QPointF currentPos = m_mapWidget->robotPos();
    gonxun::Point start{static_cast<int>(currentPos.x()), static_cast<int>(currentPos.y())};
    gonxun::Point goal{target.pos.x, target.pos.y};

    m_segmentTimer.start();

    // 3×3 网格高层路径规划
    auto gridStart = gonxun::GridPlanner::fieldToGrid(start.x, start.y);
    auto gridGoal = gonxun::GridPlanner::fieldToGrid(goal.x, goal.y);
    auto gridPath = m_gridPlanner.plan(gridStart, gridGoal);

    if (!gridPath.empty()) {
        emitLog(QString("[网格] %1 (共%2步)")
                .arg(QString::fromStdString(gonxun::GridPlanner::pathToString(gridPath)))
                .arg(gridPath.size()));
        emitLog(QString::fromStdString(m_gridPlanner.dumpGrid(gridPath)));
    }

    // A* 精细路径规划
    m_currentPath = m_planner.plan(start, goal);

    double planTimeMs = m_segmentTimer.elapsed();
    double pathLen = 0;
    for (size_t i = 1; i < m_currentPath.size(); ++i) {
        double dx = m_currentPath[i].x - m_currentPath[i-1].x;
        double dy = m_currentPath[i].y - m_currentPath[i-1].y;
        pathLen += std::sqrt(dx*dx + dy*dy);
    }

    if (m_currentPath.empty()) {
        // 路径规划失败
        emitLog(QString("[错误] 路径规划失败: (%1,%2) -> (%3,%4)")
                .arg(start.x).arg(start.y).arg(goal.x).arg(goal.y));
        m_phase = SimPhase::ERROR;
        m_stateMachine.handleEvent(gonxun::TaskEvent::ERROR_OCCURRED);
        emit phaseChanged(m_phase, "路径规划失败");
        stop();
        return;
    }

    // 显示规划路径
    QVector<QPointF> pathPts;
    for (const auto& p : m_currentPath) {
        pathPts.append(QPointF(p.x, p.y));
    }
    m_mapWidget->setPath(pathPts);

    // 更新数据面板
    if (m_dataPanel) {
        m_dataPanel->updatePathInfo(static_cast<int>(m_currentPath.size()), pathLen, planTimeMs);
    }

    emitLog(QString("[规划] 路径: %1 个点, 长度: %2 mm, 耗时: %3 ms")
            .arg(m_currentPath.size())
            .arg(static_cast<int>(pathLen))
            .arg(planTimeMs, 0, 'f', 1));

    // 开始移动
    startMoving();
}

void SimulationController::startMoving()
{
    m_phase = SimPhase::MOVING;
    m_currentPathIdx = 0;
    emit phaseChanged(m_phase, "机器人移动中");

    if (m_dataPanel) {
        m_dataPanel->updateRobotMotion(true, 150, "前进");
    }

    m_animTimer->start(m_animInterval);
}

void SimulationController::startDwelling()
{
    m_phase = SimPhase::DWELLING;
    m_animTimer->stop();
    emit phaseChanged(m_phase, "到达目标点，停留中");

    const NavTarget& target = m_navSequence[m_currentSegment];
    emitLog(QString("[到达] %1, 停留 %2 ms").arg(target.name).arg(m_dwellTime));

    if (m_dataPanel) {
        m_dataPanel->updateRobotMotion(false, 0, "停留");
    }

    // 延迟后继续下一段
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

    // 移动到下一个路径点
    const auto& pt = m_currentPath[m_currentPathIdx];

    // 计算朝向角度
    QPointF currentPos = m_mapWidget->robotPos();
    double dx = pt.x - currentPos.x();
    double dy = pt.y - currentPos.y();
    double angle = std::atan2(dy, dx) * 180.0 / M_PI;
    if (angle < 0) angle += 360.0;

    // 累计行驶距离
    m_totalDistance += std::sqrt(dx*dx + dy*dy);
    m_totalSteps++;

    // 更新机器人位置
    m_mapWidget->setRobotPos(QPointF(pt.x, pt.y), angle);

    // 更新数据面板
    if (m_dataPanel) {
        m_dataPanel->updateRobotPose(pt.x, pt.y, angle);
        m_dataPanel->updateUptime(m_totalTimer.elapsed() / 1000.0);
        m_dataPanel->updateSerialStats(m_totalSteps, m_totalSteps);
    }

    m_currentPathIdx++;
}

void SimulationController::onSegmentComplete()
{
    const NavTarget& target = m_navSequence[m_currentSegment];

    // 触发状态机事件
    if (target.name.contains("扫码区")) {
        m_stateMachine.handleEvent(gonxun::TaskEvent::REACHED_QR);
        m_stateMachine.handleEvent(gonxun::TaskEvent::QR_SCANNED);
        if (m_dataPanel) {
            m_dataPanel->updateQrResult(m_taskCode);
            m_dataPanel->updateVisionResult("扫码完成", 30);
        }
    } else if (target.name.contains("原料区")) {
        m_stateMachine.handleEvent(gonxun::TaskEvent::REACHED_MATERIAL);
        m_stateMachine.handleEvent(gonxun::TaskEvent::MATERIAL_PICKED);
        if (m_dataPanel) {
            m_dataPanel->updateVisionResult("取料完成", 30);
        }
    } else if (target.name.contains("粗加工")) {
        m_stateMachine.handleEvent(gonxun::TaskEvent::REACHED_PROCESS);
        m_stateMachine.handleEvent(gonxun::TaskEvent::MATERIAL_PLACED);
        if (m_dataPanel) {
            m_dataPanel->updateVisionResult("放料完成", 30);
        }
    } else if (target.name.contains("暂存区")) {
        m_stateMachine.handleEvent(gonxun::TaskEvent::REACHED_BUFFER);
    } else if (target.name.contains("启停区")) {
        m_stateMachine.handleEvent(gonxun::TaskEvent::REACHED_START);
        m_stateMachine.handleEvent(gonxun::TaskEvent::ALL_DONE);
    }

    // 清除路径显示
    m_mapWidget->clearPath();

    m_currentSegment++;

    // 检查是否完成所有导航段
    if (m_currentSegment >= m_navSequence.size()) {
        onAllComplete();
        return;
    }

    // 开始下一段
    startNextSegment();
}

void SimulationController::onAllComplete()
{
    m_running = false;
    m_phase = SimPhase::COMPLETED;
    m_animTimer->stop();

    double totalTime = m_totalTimer.elapsed() / 1000.0;

    if (m_dataPanel) {
        m_dataPanel->updateRobotMotion(false, 0, "完成");
        m_dataPanel->updateTaskState("任务完成");
        m_dataPanel->updateUptime(totalTime);
    }

    // 输出统计报告
    emitLog(QString("\n========== 仿真完成 =========="));
    emitLog(QString("任务码:     %1").arg(m_taskCode));
    emitLog(QString("循环次数:   %1").arg(m_totalCycles));
    emitLog(QString("导航段数:   %1").arg(m_navSequence.size()));
    emitLog(QString("总步数:     %1").arg(m_totalSteps));
    emitLog(QString("总距离:     %1 mm").arg(static_cast<int>(m_totalDistance)));
    emitLog(QString("总耗时:     %1 s").arg(totalTime, 0, 'f', 2));
    emitLog(QString("==============================\n"));

    emit phaseChanged(m_phase, "全部完成");
    emit simulationFinished(true);
}

void SimulationController::updateObstaclesFromMap()
{
    // 从地图控件获取已标记的障碍物
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

    emitLog(QString("[地图] 障碍物: %1 个").arg(obstacles.size()));
}

void SimulationController::emitLog(const QString& msg)
{
    std::cout << msg.toStdString() << std::endl;
    emit logMessage(msg);
}

QString SimulationController::phaseToString(SimPhase p) const
{
    switch (p) {
    case SimPhase::IDLE:      return "空闲";
    case SimPhase::PLANNING:  return "规划中";
    case SimPhase::MOVING:    return "移动中";
    case SimPhase::DWELLING:  return "停留中";
    case SimPhase::COMPLETED: return "已完成";
    case SimPhase::ERROR:     return "错误";
    default: return "未知";
    }
}