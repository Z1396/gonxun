#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

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
    resize(480, 520);
    setMinimumSize(320, 350);

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

    // 4. 底部状态提示文本标签
    m_statusLabel = new QLabel(central);
    m_statusLabel->setFont(QFont("Microsoft YaHei", 10));

    toolbar->addWidget(m_startBtn);
    toolbar->addWidget(m_markBtn);
    toolbar->addWidget(m_selectStartBtn);
    toolbar->addWidget(m_statusLabel);
    toolbar->addStretch();

    m_courtMap = new CourtMapWidget(central);

    layout->addLayout(toolbar);
    layout->addWidget(m_courtMap, 1);

    setCentralWidget(central);

    // ==================== 信号槽绑定 ====================
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartButtonClicked);
    connect(m_markBtn, &QPushButton::clicked, this, &MainWindow::onMarkButtonClicked);
    connect(m_selectStartBtn, &QPushButton::clicked, this, &MainWindow::onSelectStartZoneClicked);
    connect(m_courtMap, &CourtMapWidget::obstacleToggled, this, &MainWindow::onObstacleToggled);
    connect(m_courtMap, &CourtMapWidget::startZoneSelected, this, &MainWindow::onStartZoneSelected);

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

    if (m_visionRunning) {
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