#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>

namespace {
const char* BTN_STYLE =
    "QPushButton { background-color: %1; color: white; border: none; "
    "border-radius: 6px; padding: 8px 16px; font-family: 'Microsoft YaHei'; font-size: 10pt; font-weight: bold; }"
    "QPushButton:hover { background-color: %2; }"
    "QPushButton:checked { background-color: %3; }";

QString buttonStyle(const char *normal, const char *hover, const char *checked) {
    return QString(BTN_STYLE).arg(normal, hover, checked);
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("赛场地图");
    resize(780, 560);
    setMinimumSize(600, 400);

    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // 顶部工具栏水平布局
    QHBoxLayout *toolbar = new QHBoxLayout();
    toolbar->setSpacing(10);

    // 1. 开始按钮
    m_startBtn = new QPushButton("开始", central);
    m_startBtn->setMinimumHeight(36);
    m_startBtn->setCheckable(true);
    m_startBtn->setStyleSheet(buttonStyle("#2ecc71", "#27ae60", "#c0392b"));

    // 2. 标记障碍物按钮
    m_markBtn = new QPushButton("标记障碍物", central);
    m_markBtn->setMinimumHeight(36);
    m_markBtn->setCheckable(true);
    m_markBtn->setStyleSheet(buttonStyle("#4a90e2", "#357abd", "#e74c3c"));

    // 3. 选择启停区按钮
    m_selectStartBtn = new QPushButton("选择启停区", central);
    m_selectStartBtn->setMinimumHeight(36);
    m_selectStartBtn->setCheckable(true);
    m_selectStartBtn->setStyleSheet(buttonStyle("#27ae60", "#229954", "#16a085"));

    // 4. 仿真按钮
    m_simBtn = new QPushButton("仿真", central);
    m_simBtn->setMinimumHeight(36);
    m_simBtn->setCheckable(true);
    m_simBtn->setStyleSheet(buttonStyle("#8e44ad", "#7d3c98", "#c0392b"));

    // 5. 底部状态提示文本标签
    m_statusLabel = new QLabel(central);
    m_statusLabel->setFont(QFont("Microsoft YaHei", 10));

    toolbar->addWidget(m_startBtn);
    toolbar->addWidget(m_markBtn);
    toolbar->addWidget(m_selectStartBtn);
    toolbar->addWidget(m_simBtn);
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