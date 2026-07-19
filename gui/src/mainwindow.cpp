/**
 * @file mainwindow.cpp
 * @brief 主窗口实现文件
 * 
 * @details 本文件实现了智能物流搬运系统的主窗口界面。
 *          核心功能：
 *          - UI布局搭建：顶部工具栏、中央地图、右侧数据面板
 *          - 按钮功能实现：开始、标记障碍物、选择启停区、仿真
 *          - 信号槽连接：处理用户交互、状态更新
 *          - 子模块集成：地图控件、数据面板、仿真控制器
 *          
 *          界面结构：
 *          - 顶部工具栏：4个功能按钮 + 状态提示标签
 *          - 中央区域：左侧地图（占3/4） + 右侧数据面板（占1/4）
 *          - 使用 QSplitter 实现可拖拽调整的左右分栏
 *          
 *          按钮状态管理：
 *          - 开始按钮：启动/停止任务执行
 *          - 标记障碍物：切换障碍物标记模式
 *          - 选择启停区：切换启停区选择模式
 *          - 仿真按钮：启动/停止任务仿真
 *          
 *          状态同步机制：
 *          - 使用 Qt 信号槽机制连接各模块
 *          - 地图控件的障碍物变化、启停区选择通过信号传递
 *          - 仿真控制器的日志、完成状态通过信号反馈
 *          
 * @see mainwindow.h 头文件定义
 * @see courtmapwidget.h 场地地图控件
 * @see data_panel_widget.h 数据面板控件
 * @see simulation_controller.h 仿真控制器
 * 
 * @author 工创赛2025智能物流搬运系统团队
 * @date 2024-01-15
 * @version 1.0.0
 * @history 2024-01-15 初始版本
 * @history 2024-02-20 新增仿真控制器集成
 * 
 * @copyright 工创赛2025智能物流搬运系统
 */
#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>

/**
 * @brief 按钮样式模板
 * 
 * @details 定义统一的按钮样式格式，包括正常、悬停、选中三种状态的颜色。
 *          使用 QString::arg() 填充颜色值。
 */
namespace {
const char* BTN_STYLE =
    "QPushButton { background-color: %1; color: white; border: none; "
    "border-radius: 6px; padding: 8px 16px; font-family: 'Microsoft YaHei'; font-size: 10pt; font-weight: bold; }"
    "QPushButton:hover { background-color: %2; }"
    "QPushButton:checked { background-color: %3; }";

/**
 * @brief 生成按钮样式字符串
 * 
 * @param normal 正常状态颜色（十六进制）
 * @param hover 悬停状态颜色（十六进制）
 * @param checked 选中状态颜色（十六进制）
 * @return 完整的按钮样式字符串
 */
QString buttonStyle(const char *normal, const char *hover, const char *checked) {
    return QString(BTN_STYLE).arg(normal, hover, checked);
}
}

/**
 * @brief MainWindow 构造函数
 * 
 * @details 初始化主窗口，完成以下工作：
 *          1. 设置窗口标题、尺寸、最小尺寸
 *          2. 搭建UI布局（顶部工具栏 + 中央分栏）
 *          3. 创建子控件（地图、数据面板、仿真控制器）
 *          4. 绑定信号槽连接
 *          
 * @param parent 父窗口对象（默认 nullptr）
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // ==================== 窗口基本设置 ====================
    setWindowTitle("赛场地图");
    resize(780, 560);  // 默认窗口尺寸
    setMinimumSize(600, 400);  // 最小窗口尺寸

    // ==================== 中央容器与主布局 ====================
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(10, 10, 10, 10);  // 布局边距
    layout->setSpacing(8);  // 控件间距

    // ==================== 顶部工具栏布局 ====================
    QHBoxLayout *toolbar = new QHBoxLayout();
    toolbar->setSpacing(10);  // 按钮间距

    // ==================== 1. 开始按钮 ====================
    // 功能：启动/停止任务执行
    // 样式：绿色正常，深绿悬停，红色选中
    m_startBtn = new QPushButton("开始", central);
    m_startBtn->setMinimumHeight(36);  // 最小高度
    m_startBtn->setCheckable(true);  // 可切换状态
    m_startBtn->setStyleSheet(buttonStyle("#2ecc71", "#27ae60", "#c0392b"));

    // ==================== 2. 标记障碍物按钮 ====================
    // 功能：切换障碍物标记模式
    // 样式：蓝色正常，深蓝悬停，红色选中
    m_markBtn = new QPushButton("标记障碍物", central);
    m_markBtn->setMinimumHeight(36);
    m_markBtn->setCheckable(true);
    m_markBtn->setStyleSheet(buttonStyle("#4a90e2", "#357abd", "#e74c3c"));

    // ==================== 3. 选择启停区按钮 ====================
    // 功能：切换启停区选择模式
    // 样式：绿色正常，深绿悬停，青色选中
    m_selectStartBtn = new QPushButton("选择启停区", central);
    m_selectStartBtn->setMinimumHeight(36);
    m_selectStartBtn->setCheckable(true);
    m_selectStartBtn->setStyleSheet(buttonStyle("#27ae60", "#229954", "#16a085"));

    // ==================== 4. 仿真按钮 ====================
    // 功能：启动/停止任务仿真
    // 样式：紫色正常，深紫悬停，红色选中
    m_simBtn = new QPushButton("仿真", central);
    m_simBtn->setMinimumHeight(36);
    m_simBtn->setCheckable(true);
    m_simBtn->setStyleSheet(buttonStyle("#8e44ad", "#7d3c98", "#c0392b"));

    // ==================== 5. 路径预览按钮 ====================
    // 功能：生成并显示完整任务流程路径
    // 样式：橙色正常，深橙悬停，无选中状态
    m_pathPreviewBtn = new QPushButton("路径预览", central);
    m_pathPreviewBtn->setMinimumHeight(36);
    m_pathPreviewBtn->setCheckable(false);
    m_pathPreviewBtn->setStyleSheet(buttonStyle("#e67e22", "#d35400", "#d35400"));

    // ==================== 6. 状态提示标签 ====================
    m_statusLabel = new QLabel(central);
    m_statusLabel->setFont(QFont("Microsoft YaHei", 10));

    toolbar->addWidget(m_startBtn);
    toolbar->addWidget(m_markBtn);
    toolbar->addWidget(m_selectStartBtn);
    toolbar->addWidget(m_simBtn);
    toolbar->addWidget(m_pathPreviewBtn);
    toolbar->addWidget(m_statusLabel);
    toolbar->addStretch();

    m_courtMap = new CourtMapWidget(central);
    m_dataPanel = new DataPanelWidget(central);

    // 左右分栏：地图 + 数据面板
    QSplitter *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->addWidget(m_courtMap);
    splitter->addWidget(m_dataPanel);
    splitter->setStretchFactor(0, 3);  // 地图占 3/4
    splitter->setStretchFactor(1, 1);  // 数据面板占 1/4
    splitter->setSizes({500, 260});

    layout->addLayout(toolbar);
    layout->addWidget(splitter, 1);

    setCentralWidget(central);

    // 初始化仿真控制器
    m_simController = new SimulationController(m_courtMap, m_dataPanel, this);
    m_simController->setAnimationInterval(50);   // 20fps
    m_simController->setDwellTime(800);           // 800ms 停留
    m_simController->setTotalCycles(1);           // 1 轮循环

    // ==================== 信号槽绑定 ====================
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartButtonClicked);
    connect(m_markBtn, &QPushButton::clicked, this, &MainWindow::onMarkButtonClicked);
    connect(m_selectStartBtn, &QPushButton::clicked, this, &MainWindow::onSelectStartZoneClicked);
    connect(m_simBtn, &QPushButton::clicked, this, &MainWindow::onSimButtonClicked);
    connect(m_pathPreviewBtn, &QPushButton::clicked, this, &MainWindow::onPathPreviewClicked);
    connect(m_courtMap, &CourtMapWidget::obstacleToggled, this, &MainWindow::onObstacleToggled);
    connect(m_courtMap, &CourtMapWidget::startZoneSelected, this, &MainWindow::onStartZoneSelected);
    connect(m_simController, &SimulationController::logMessage, this, &MainWindow::onSimLog);
    connect(m_simController, &SimulationController::simulationFinished, [this](bool success) {
        m_simBtn->setText("仿真");
        m_simBtn->setChecked(false);
        updateStatus();
    });

    updateStatus();
}

void MainWindow::onStartButtonClicked()
{
    if (m_visionRunning) {
        // 当前运行中，点击停止
        m_visionRunning = false;
        m_startBtn->setText("开始");
        m_startBtn->setChecked(false);
        emit visionStopRequested();
    } else {
        // 当前未运行，点击开始
        m_visionRunning = true;
        m_startBtn->setText("停止");
        m_startBtn->setChecked(true);
        emit visionStartRequested();
    }
    updateStatus();
}

void MainWindow::onMarkButtonClicked()
{
    bool enabled = m_markBtn->isChecked();
    m_courtMap->setMarkMode(enabled);

    if (enabled) {
        m_markBtn->setText("结束标记");
        if (m_selectStartBtn->isChecked())
        {
            m_selectStartBtn->setChecked(false);
            m_courtMap->setStartZoneSelectable(false);
            m_selectStartBtn->setText("选择启停区");
        }
    } else {
        m_markBtn->setText("标记障碍物");
    }
    updateStatus();
}

void MainWindow::onObstacleToggled(int id, bool marked)
{
    Q_UNUSED(id)
    Q_UNUSED(marked)
    updateStatus();
}

void MainWindow::onSelectStartZoneClicked()
{
    bool enabled = m_selectStartBtn->isChecked();
    m_courtMap->setStartZoneSelectable(enabled);

    if (enabled) {
        m_selectStartBtn->setText("取消选择");
        if (m_markBtn->isChecked()) {
            m_markBtn->setChecked(false);
            m_courtMap->setMarkMode(false);
            m_markBtn->setText("标记障碍物");
        }
    } else {
        m_selectStartBtn->setText("选择启停区");
    }
    updateStatus();
}

void MainWindow::onStartZoneSelected(int zoneIndex, const QString &zoneName)
{
    QString pos = (zoneIndex == 0) ? "右上角" : "右下角";
    m_statusLabel->setText(QString("已选择: %1 (%2) | 障碍物: %3个")
                           .arg(zoneName, pos).arg(m_courtMap->markedCount()));
    m_statusLabel->setStyleSheet("color: #27ae60; font-weight: bold;");

    m_selectStartBtn->setChecked(false);
    m_selectStartBtn->setText("选择启停区");
    m_courtMap->setStartZoneSelectable(false);
}

void MainWindow::updateStatus()
{
    int count = m_courtMap->markedCount();
    QString zone = m_courtMap->selectedStartZoneName();

    QString status;
    QString color;

    if (m_simController && m_simController->isRunning()) {
        status = QString("仿真运行中 | 障碍物: %1个").arg(count);
        color = "color: #8e44ad; font-weight: bold;";
    } else if (m_visionRunning) {
        status = QString("视觉系统运行中 | 障碍物: %1个").arg(count);
        color = "color: #27ae60; font-weight: bold;";
    } else if (m_markBtn->isChecked()) {
        status = QString("点击红色矩形标记障碍物 | 已标记: %1个").arg(count);
        color = "color: #e74c3c; font-weight: bold;";
    } else if (!zone.isEmpty()) {
        QString pos = (m_courtMap->selectedStartZone() == 0) ? "右上角" : "右下角";
        status = QString("启动区: %1 (%2) | 障碍物: %3个").arg(zone, pos).arg(count);
        color = "color: #27ae60; font-weight: bold;";
    } else {
        status = QString("障碍物: %1个").arg(count);
        color = "color: #555;";
    }

    m_statusLabel->setText(status);
    m_statusLabel->setStyleSheet(color);
}

void MainWindow::onSimButtonClicked()
{
    if (m_simController->isRunning()) {
        // 停止仿真
        m_simController->stop();
        m_simBtn->setText("仿真");
        m_simBtn->setChecked(false);
    } else {
        // 启动仿真（SimulationController 内部会检查启停区）
        bool ok = m_simController->start("312");
        if (ok) {
            m_simBtn->setText("停止仿真");
            m_simBtn->setChecked(true);
        } else {
            m_simBtn->setChecked(false);
        }
    }
    updateStatus();
}

void MainWindow::onSimLog(const QString& msg)
{
    // 将日志显示在状态栏
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet("color: #8e44ad; font-weight: bold;");
}

void MainWindow::onPathPreviewClicked()
{
    // ===== 检查是否选择了启停区 =====
    int startZone = m_courtMap->selectedStartZone();
    if (startZone < 0) {
        m_statusLabel->setText("请先选择启停区（右上角或右下角）");
        m_statusLabel->setStyleSheet("color: #e74c3c; font-weight: bold;");
        return;
    }
    
    // ===== 生成完整任务路径 =====
    QString taskCode = "312";  // 默认任务码
    QVector<QPointF> fullPath = m_courtMap->generateFullMissionPath(startZone, taskCode);
    
    // ===== 显示路径在地图上 =====
    m_courtMap->setPath(fullPath);
    
    // ===== 更新状态提示 =====
    QString zoneName = m_courtMap->selectedStartZoneName();
    QString pos = (startZone == 0) ? "右上角" : "右下角";
    m_statusLabel->setText(QString("路径预览: %1 (%2) → 二维码 → 原料 → 粗加工 → 暂存 → 返回 | 总路径点: %3个")
                          .arg(zoneName, pos).arg(fullPath.size()));
    m_statusLabel->setStyleSheet("color: #e67e22; font-weight: bold;");
    
    // ===== 更新数据面板 =====
    if (m_dataPanel) {
        QPointF robotPos = fullPath.first();
        m_dataPanel->updateRobotPose(static_cast<int>(robotPos.x()), 
                                     static_cast<int>(robotPos.y()), 
                                     0.0);
        m_dataPanel->updateTaskState("路径预览");
    }
}