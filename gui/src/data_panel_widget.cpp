#include "data_panel_widget.h"
#include <QDateTime>

DataPanelWidget::DataPanelWidget(QWidget *parent)
    : QWidget(parent)
{
    setupStyles();
    setupUI();

    // 定时刷新：200ms 间隔，确保延迟 < 500ms
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(200);
    connect(m_refreshTimer, &QTimer::timeout, this, &DataPanelWidget::refreshAll);
    m_refreshTimer->start();
}

void DataPanelWidget::setupStyles()
{
    m_labelStyle = "QLabel { color: #7f8c8d; font-size: 9pt; font-family: 'Microsoft YaHei'; }";
    m_valueStyle = "QLabel { color: #2c3e50; font-size: 10pt; font-family: 'Consolas','Microsoft YaHei'; font-weight: bold; }";
    m_groupStyle = "QGroupBox { "
                   "  font-family: 'Microsoft YaHei'; font-size: 9pt; font-weight: bold; "
                   "  color: #34495e; "
                   "  border: 1px solid #bdc3c7; border-radius: 4px; "
                   "  margin-top: 8px; padding-top: 8px; "
                   "} "
                   "QGroupBox::title { "
                   "  subcontrol-origin: margin; left: 8px; padding: 0 4px; "
                   "}";
    m_highlightStyle = "QLabel { color: #e74c3c; font-size: 10pt; font-family: 'Consolas','Microsoft YaHei'; font-weight: bold; }";
}

void DataPanelWidget::setupUI()
{
    setStyleSheet(m_groupStyle);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // ========== 机器人状态组 ==========
    m_robotGroup = new QGroupBox("机器人状态", this);
    QGridLayout* robotLayout = new QGridLayout(m_robotGroup);
    robotLayout->setSpacing(4);

    robotLayout->addWidget(createLabel("坐标 X"), 0, 0);
    m_robotX = createValueLabel("0 mm");
    robotLayout->addWidget(m_robotX, 0, 1);

    robotLayout->addWidget(createLabel("坐标 Y"), 0, 2);
    m_robotY = createValueLabel("0 mm");
    robotLayout->addWidget(m_robotY, 0, 3);

    robotLayout->addWidget(createLabel("朝向"), 1, 0);
    m_robotTheta = createValueLabel("0.0");
    robotLayout->addWidget(m_robotTheta, 1, 1);

    robotLayout->addWidget(createLabel("速度"), 1, 2);
    m_robotSpeed = createValueLabel("0");
    robotLayout->addWidget(m_robotSpeed, 1, 3);

    robotLayout->addWidget(createLabel("模式"), 2, 0);
    m_robotMode = createValueLabel("停止");
    robotLayout->addWidget(m_robotMode, 2, 1);

    robotLayout->addWidget(createLabel("运动中"), 2, 2);
    m_robotMoving = createValueLabel("否");
    robotLayout->addWidget(m_robotMoving, 2, 3);

    robotLayout->addWidget(createLabel("电量"), 3, 0);
    m_batteryLevel = createValueLabel("100%");
    robotLayout->addWidget(m_batteryLevel, 3, 1);

    mainLayout->addWidget(m_robotGroup);

    // ========== 任务进度组 ==========
    m_taskGroup = new QGroupBox("任务进度", this);
    QGridLayout* taskLayout = new QGridLayout(m_taskGroup);
    taskLayout->setSpacing(4);

    taskLayout->addWidget(createLabel("当前状态"), 0, 0);
    m_taskState = createValueLabel("空闲");
    taskLayout->addWidget(m_taskState, 0, 1);

    taskLayout->addWidget(createLabel("任务码"), 0, 2);
    m_taskCode = createValueLabel("--");
    taskLayout->addWidget(m_taskCode, 0, 3);

    taskLayout->addWidget(createLabel("循环"), 1, 0);
    m_taskCycle = createValueLabel("0/0");
    taskLayout->addWidget(m_taskCycle, 1, 1);

    taskLayout->addWidget(createLabel("物料"), 1, 2);
    m_taskMaterials = createValueLabel("0/0");
    taskLayout->addWidget(m_taskMaterials, 1, 3);

    mainLayout->addWidget(m_taskGroup);

    // ========== 视觉检测组 ==========
    m_visionGroup = new QGroupBox("视觉检测", this);
    QGridLayout* visionLayout = new QGridLayout(m_visionGroup);
    visionLayout->setSpacing(4);

    visionLayout->addWidget(createLabel("检测结果"), 0, 0);
    m_visionResult = createValueLabel("等待中...");
    visionLayout->addWidget(m_visionResult, 0, 1);

    visionLayout->addWidget(createLabel("FPS"), 0, 2);
    m_visionFps = createValueLabel("0");
    visionLayout->addWidget(m_visionFps, 0, 3);

    visionLayout->addWidget(createLabel("二维码"), 1, 0);
    m_qrResult = createValueLabel("--");
    visionLayout->addWidget(m_qrResult, 1, 1, 1, 3);

    mainLayout->addWidget(m_visionGroup);

    // ========== 通信状态组 ==========
    m_commGroup = new QGroupBox("通信 & 路径", this);
    QGridLayout* commLayout = new QGridLayout(m_commGroup);
    commLayout->setSpacing(4);

    commLayout->addWidget(createLabel("串口"), 0, 0);
    m_serialStatus = createValueLabel("未连接");
    commLayout->addWidget(m_serialStatus, 0, 1);

    commLayout->addWidget(createLabel("端口"), 0, 2);
    m_serialPort = createValueLabel("--");
    commLayout->addWidget(m_serialPort, 0, 3);

    commLayout->addWidget(createLabel("收发"), 1, 0);
    m_serialStats = createValueLabel("0/0");
    commLayout->addWidget(m_serialStats, 1, 1);

    commLayout->addWidget(createLabel("路径"), 1, 2);
    m_pathInfo = createValueLabel("--");
    commLayout->addWidget(m_pathInfo, 1, 3);

    mainLayout->addWidget(m_commGroup);

    // ========== 系统信息组 ==========
    m_systemGroup = new QGroupBox("系统", this);
    QGridLayout* sysLayout = new QGridLayout(m_systemGroup);
    sysLayout->setSpacing(4);

    sysLayout->addWidget(createLabel("运行时间"), 0, 0);
    m_uptime = createValueLabel("0s");
    sysLayout->addWidget(m_uptime, 0, 1);

    sysLayout->addWidget(createLabel("刷新率"), 0, 2);
    m_refreshRate = createValueLabel("5 Hz");
    sysLayout->addWidget(m_refreshRate, 0, 3);

    mainLayout->addWidget(m_systemGroup);
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

// ===== 数据更新 Slots =====

void DataPanelWidget::updateRobotPose(int xMm, int yMm, double thetaDeg)
{
    m_robotX->setText(QString::number(xMm) + " mm");
    m_robotY->setText(QString::number(yMm) + " mm");
    m_robotTheta->setText(QString::number(thetaDeg, 'f', 1) + QString::fromUtf8("°"));
}

void DataPanelWidget::updateRobotMotion(bool moving, int speed, const QString& mode)
{
    m_robotMoving->setText(moving ? "是" : "否");
    m_robotMoving->setStyleSheet(moving ? m_highlightStyle : m_valueStyle);
    m_robotSpeed->setText(QString::number(speed));
    m_robotMode->setText(mode);
}

void DataPanelWidget::updateBattery(int percent)
{
    m_batteryLevel->setText(QString::number(percent) + "%");
    if (percent < 20) {
        m_batteryLevel->setStyleSheet(m_highlightStyle);
    } else {
        m_batteryLevel->setStyleSheet(m_valueStyle);
    }
}

void DataPanelWidget::updateTaskState(const QString& state)
{
    m_taskState->setText(state);
    if (state == "异常") {
        m_taskState->setStyleSheet(m_highlightStyle);
    } else {
        m_taskState->setStyleSheet(m_valueStyle);
    }
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
    if (code != "--" && !code.isEmpty()) {
        m_taskCode->setStyleSheet(m_highlightStyle);
    }
}

void DataPanelWidget::updateVisionResult(const QString& detection, int fps)
{
    m_visionResult->setText(detection);
    m_visionFps->setText(QString::number(fps));
}

void DataPanelWidget::updateQrResult(const QString& qrData)
{
    m_qrResult->setText(qrData.isEmpty() ? "--" : qrData);
    if (!qrData.isEmpty()) {
        m_qrResult->setStyleSheet(m_highlightStyle);
    }
}

void DataPanelWidget::updateSerialStatus(bool connected, bool mock, const QString& port)
{
    if (mock) {
        m_serialStatus->setText("模拟模式");
        m_serialStatus->setStyleSheet(m_highlightStyle);
    } else if (connected) {
        m_serialStatus->setText("已连接");
        m_serialStatus->setStyleSheet(m_valueStyle);
    } else {
        m_serialStatus->setText("未连接");
        m_serialStatus->setStyleSheet(m_highlightStyle);
    }
    m_serialPort->setText(port);
}

void DataPanelWidget::updateSerialStats(int sentFrames, int recvFrames)
{
    m_serialStats->setText(QString::number(sentFrames) + "/" + QString::number(recvFrames));
}

void DataPanelWidget::updatePathInfo(int pathPoints, double pathLength, double planTimeMs)
{
    m_pathInfo->setText(QString::number(pathPoints) + "pt / " +
                        QString::number(pathLength, 'f', 0) + "mm / " +
                        QString::number(planTimeMs, 'f', 1) + "ms");
}

void DataPanelWidget::updateUptime(double seconds)
{
    int h = static_cast<int>(seconds) / 3600;
    int m = (static_cast<int>(seconds) % 3600) / 60;
    int s = static_cast<int>(seconds) % 60;
    m_uptime->setText(QString::asprintf("%02d:%02d:%02d", h, m, s));
}

void DataPanelWidget::refreshAll()
{
    // 发射信号请求外部数据源提供最新数据
    emit dataRefreshRequested();
}