#ifndef DATA_PANEL_WIDGET_H
#define DATA_PANEL_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QGroupBox>
#include <QGridLayout>
#include <QString>

/**
 * 实时数据面板控件
 * 显示机器人状态、任务进度、视觉检测结果、串口状态等
 * 使用 QTimer 定时刷新（200ms），确保延迟 < 500ms
 * 直接 setText 更新 QLabel，无闪烁
 */
class DataPanelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DataPanelWidget(QWidget *parent = nullptr);
    ~DataPanelWidget() = default;

public slots:
    // ===== 数据更新接口（各模块调用这些 slot 推送数据） =====

    // 机器人位姿更新
    void updateRobotPose(int xMm, int yMm, double thetaDeg);
    // 机器人运动状态
    void updateRobotMotion(bool moving, int speed, const QString& mode);
    // 机器人电量
    void updateBattery(int percent);

    // 任务状态更新
    void updateTaskState(const QString& state);
    // 任务进度更新
    void updateTaskProgress(int cycle, int totalCycles, int picked, int placed, int totalMaterials);
    // 任务码更新
    void updateTaskCode(const QString& code);

    // 视觉检测结果
    void updateVisionResult(const QString& detection, int fps);
    // 二维码结果
    void updateQrResult(const QString& qrData);

    // 串口状态
    void updateSerialStatus(bool connected, bool mock, const QString& port);
    // 串口收发计数
    void updateSerialStats(int sentFrames, int recvFrames);

    // 路径规划信息
    void updatePathInfo(int pathPoints, double pathLength, double planTimeMs);

    // 系统运行时间
    void updateUptime(double seconds);

    // 一键更新所有数据（从外部数据源拉取）
    void refreshAll();

signals:
    // 请求外部数据源提供最新数据（定时器触发时发射）
    void dataRefreshRequested();

private:
    void setupUI();
    void setupStyles();
    QLabel* createLabel(const QString& text, const QString& style = "");
    QLabel* createValueLabel(const QString& text = "--", const QString& color = "#2c3e50");

private:
    // ===== 定时器 =====
    QTimer* m_refreshTimer = nullptr;

    // ===== 机器人状态组 =====
    QGroupBox* m_robotGroup = nullptr;
    QLabel* m_robotX = nullptr;
    QLabel* m_robotY = nullptr;
    QLabel* m_robotTheta = nullptr;
    QLabel* m_robotSpeed = nullptr;
    QLabel* m_robotMode = nullptr;
    QLabel* m_robotMoving = nullptr;
    QLabel* m_batteryLevel = nullptr;

    // ===== 任务进度组 =====
    QGroupBox* m_taskGroup = nullptr;
    QLabel* m_taskState = nullptr;
    QLabel* m_taskCycle = nullptr;
    QLabel* m_taskMaterials = nullptr;
    QLabel* m_taskCode = nullptr;

    // ===== 视觉检测组 =====
    QGroupBox* m_visionGroup = nullptr;
    QLabel* m_visionResult = nullptr;
    QLabel* m_visionFps = nullptr;
    QLabel* m_qrResult = nullptr;

    // ===== 通信状态组 =====
    QGroupBox* m_commGroup = nullptr;
    QLabel* m_serialStatus = nullptr;
    QLabel* m_serialPort = nullptr;
    QLabel* m_serialStats = nullptr;
    QLabel* m_pathInfo = nullptr;

    // ===== 系统信息组 =====
    QGroupBox* m_systemGroup = nullptr;
    QLabel* m_uptime = nullptr;
    QLabel* m_refreshRate = nullptr;

    // 样式
    QString m_labelStyle;
    QString m_valueStyle;
    QString m_groupStyle;
    QString m_highlightStyle;
};

#endif // DATA_PANEL_WIDGET_H