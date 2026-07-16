/**
 * @file data_panel_widget.cpp
 * @brief 数据面板控件实现文件
 * 
 * @details 本文件实现了主窗口右侧的数据面板控件。
 *          核心功能：
 *          - 显示机器人坐标（X, Y, 朝向角度）
 *          - 显示任务信息（任务码、状态、循环进度、物料进度）
 *          - 定时刷新机制（200ms 间隔）
 *          - 动态样式管理（正常/高亮/闪烁）
 *          
 *          UI组件结构：
 *          - 机器人坐标组：X轴、Y轴、朝向角度
 *          - 任务信息组：任务码、当前状态、任务循环、物料进度
 *          - 使用 QGridLayout 实现标签-值对齐布局
 *          
 *          数据更新机制：
 *          - 定时器触发 refreshAll() 刷新所有数据
 *          - 外部调用 updateRobotPose() 更新机器人坐标
 *          - 外部调用 updateTaskState() 更新任务状态
 *          - 外部调用 updateTaskProgress() 更新任务进度
 *          
 * @see data_panel_widget.h 头文件定义
 * @see mainwindow.cpp 使用此控件的主窗口
 * 
 * @author 工创赛2025智能物流搬运系统团队
 * @date 2024-01-15
 * @version 1.0.0
 * @history 2024-01-15 初始版本
 * 
 * @copyright 工创赛2025智能物流搬运系统
 */
#include "data_panel_widget.h"
#include <QDateTime>

/**
 * @brief DataPanelWidget 构造函数
 * 
 * @details 初始化数据面板控件，完成以下工作：
 *          1. 设置样式表（标签、值、分组框）
 *          2. 搭建 UI 界面（机器人坐标组 + 任务信息组）
 *          3. 创建定时刷新定时器（200ms 间隔）
 *          
 * @param parent 父窗口对象（默认 nullptr）
 */
DataPanelWidget::DataPanelWidget(QWidget *parent)
    : QWidget(parent)
{
    // 初始化样式表
    setupStyles();
    // 搭建 UI 界面
    setupUI();

    // ==================== 定时刷新定时器 ====================
    // 刷新间隔：200ms（实时性较高）
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(200);
    connect(m_refreshTimer, &QTimer::timeout, this, &DataPanelWidget::refreshAll);
    m_refreshTimer->start();
}

/**
 * @brief 初始化样式表
 * 
 * @details 定义各类标签、分组框的样式字符串，用于统一界面外观。
 *          样式包括：
 *          - 标签样式：灰色字体、微软雅黑
 *          - 数值样式：深色字体、Consolas 等宽字体、粗体
 *          - 分组框样式：边框、圆角、标题样式
 *          - 高亮样式：红色字体（用于异常状态）
 *          - 状态样式：蓝色正常、白色闪烁背景
 */
void DataPanelWidget::setupStyles()
{
    // 标签样式（说明文字）
    m_labelStyle = "QLabel { color: #7f8c8d; font-size: 10pt; font-family: 'Microsoft YaHei'; }";
    
    // 数值样式（数据显示）
    m_valueStyle = "QLabel { color: #2c3e50; font-size: 14pt; font-family: 'Consolas','Microsoft YaHei'; font-weight: bold; }";
    
    // 分组框样式
    m_groupStyle = "QGroupBox { "
                   "  font-family: 'Microsoft YaHei'; font-size: 10pt; font-weight: bold; "
                   "  color: #34495e; "
                   "  border: 2px solid #bdc3c7; border-radius: 6px; "
                   "  margin-top: 12px; padding-top: 12px; "
                   "} "
                   "QGroupBox::title { "
                   "  subcontrol-origin: margin; left: 10px; padding: 0 6px; "
                   "}";
    
    // 高亮样式（异常状态）
    m_highlightStyle = "QLabel { color: #e74c3c; font-size: 14pt; font-family: 'Consolas','Microsoft YaHei'; font-weight: bold; }";
    
    // 状态正常样式
    m_stateNormalStyle = "QLabel { color: #2980b9; font-size: 14pt; font-family: 'Microsoft YaHei'; font-weight: bold; }";
    
    // 状态闪烁样式
    m_stateFlashStyle = "QLabel { color: #ffffff; background-color: #2980b9; font-size: 14pt; font-family: 'Microsoft YaHei'; font-weight: bold; padding: 4px 8px; border-radius: 4px; }";
}

void DataPanelWidget::setupUI()
{
    setStyleSheet(m_groupStyle);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(12);

    m_robotGroup = new QGroupBox("机器人坐标", this);
    QGridLayout* robotLayout = new QGridLayout(m_robotGroup);
    robotLayout->setSpacing(8);
    robotLayout->setVerticalSpacing(10);

    robotLayout->addWidget(createLabel("X 轴坐标"), 0, 0, Qt::AlignLeft);
    m_robotX = createValueLabel("0 mm");
    m_robotX->setStyleSheet("QLabel { color: #27ae60; font-size: 16pt; font-family: 'Consolas'; font-weight: bold; }");
    robotLayout->addWidget(m_robotX, 0, 1, Qt::AlignRight);

    robotLayout->addWidget(createLabel("Y 轴坐标"), 1, 0, Qt::AlignLeft);
    m_robotY = createValueLabel("0 mm");
    m_robotY->setStyleSheet("QLabel { color: #27ae60; font-size: 16pt; font-family: 'Consolas'; font-weight: bold; }");
    robotLayout->addWidget(m_robotY, 1, 1, Qt::AlignRight);

    robotLayout->addWidget(createLabel("朝向角度"), 2, 0, Qt::AlignLeft);
    m_robotTheta = createValueLabel("0.0°");
    m_robotTheta->setStyleSheet("QLabel { color: #8e44ad; font-size: 12pt; font-family: 'Consolas'; font-weight: bold; }");
    robotLayout->addWidget(m_robotTheta, 2, 1, Qt::AlignRight);

    mainLayout->addWidget(m_robotGroup);

    m_taskGroup = new QGroupBox("任务信息", this);
    QGridLayout* taskLayout = new QGridLayout(m_taskGroup);
    taskLayout->setSpacing(8);
    taskLayout->setVerticalSpacing(10);

    taskLayout->addWidget(createLabel("任务编码"), 0, 0, Qt::AlignLeft);
    m_taskCode = createValueLabel("--");
    m_taskCode->setStyleSheet("QLabel { color: #e67e22; font-size: 18pt; font-family: 'Consolas'; font-weight: bold; }");
    taskLayout->addWidget(m_taskCode, 0, 1, Qt::AlignRight);

    taskLayout->addWidget(createLabel("当前状态"), 1, 0, Qt::AlignLeft);
    m_taskState = createValueLabel("空闲");
    m_taskState->setStyleSheet(m_stateNormalStyle);
    taskLayout->addWidget(m_taskState, 1, 1, Qt::AlignRight);

    taskLayout->addWidget(createLabel("任务循环"), 2, 0, Qt::AlignLeft);
    m_taskCycle = createValueLabel("0/0");
    m_taskCycle->setStyleSheet("QLabel { color: #34495e; font-size: 11pt; font-family: 'Consolas'; font-weight: bold; }");
    taskLayout->addWidget(m_taskCycle, 2, 1, Qt::AlignRight);

    taskLayout->addWidget(createLabel("物料进度"), 3, 0, Qt::AlignLeft);
    m_taskMaterials = createValueLabel("0/0");
    m_taskMaterials->setStyleSheet("QLabel { color: #34495e; font-size: 11pt; font-family: 'Consolas'; font-weight: bold; }");
    taskLayout->addWidget(m_taskMaterials, 3, 1, Qt::AlignRight);

    mainLayout->addWidget(m_taskGroup);
    mainLayout->addStretch();
}

QLabel* DataPanelWidget::createLabel(const QString& text, const QString& style)
{
    QLabel* label = new QLabel(text, this);
    label->setStyleSheet(style.isEmpty() ? m_labelStyle : style);
    return label;
}

QLabel* DataPanelWidget::createValueLabel(const QString& text, const QString& color)
{
    QLabel* label = new QLabel(text, this);
    QString style = m_valueStyle;
    style.replace("#2c3e50", color);
    label->setStyleSheet(style);
    return label;
}

void DataPanelWidget::flashStateLabel()
{
    if (!m_taskState) return;

    m_taskState->setStyleSheet(m_stateFlashStyle);
    QTimer::singleShot(400, this, [this]() {
        if (m_taskState) {
            m_taskState->setStyleSheet(m_stateNormalStyle);
        }
    });
}

void DataPanelWidget::updateRobotPose(int xMm, int yMm, double thetaDeg)
{
    m_robotX->setText(QString::number(xMm) + " mm");
    m_robotY->setText(QString::number(yMm) + " mm");
    m_robotTheta->setText(QString::number(thetaDeg, 'f', 1) + QString::fromUtf8("°"));
}

void DataPanelWidget::updateTaskState(const QString& state)
{
    m_taskState->setText(state);
    flashStateLabel();
}

void DataPanelWidget::updateTaskProgress(int cycle, int totalCycles, int picked, int placed, int totalMaterials)
{
    m_taskCycle->setText(QString::number(cycle) + "/" + QString::number(totalCycles));
    m_taskMaterials->setText(QString::number(picked) + "/" + QString::number(totalMaterials) +
                             " | " + QString::number(placed) + "/" + QString::number(totalMaterials));
}

void DataPanelWidget::updateTaskCode(const QString& code)
{
    m_taskCode->setText(code);
}

void DataPanelWidget::refreshAll()
{
    emit dataRefreshRequested();
}
