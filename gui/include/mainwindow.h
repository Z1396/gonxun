// 头文件保护宏：防止该头文件被多次重复包含（C++标准写法，避免重复定义编译报错）
#ifndef MAINWINDOW_H
// 如果未定义 MAINWINDOW_H 这个标识，则执行下方代码
#define MAINWINDOW_H

// 引入Qt主窗口基础类，所有窗口程序顶层窗口都继承自QMainWindow
#include <QMainWindow>
// Qt按钮控件，用于界面点击交互
#include <QPushButton>
// Qt文本标签，用来展示文字、状态提示
#include <QLabel>
// 引入自定义赛场地图绘图控件（自己写的可视化画布，用于机器人赛场渲染、点位标记）
#include "courtmapwidget.h"
// 引入实时数据面板控件
#include "data_panel_widget.h"
// 引入仿真控制器
#include "simulation_controller.h"

// 主窗口类，继承Qt标准主窗口QMainWindow
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;

    // 获取视觉系统运行状态
    bool isVisionRunning() const { return m_visionRunning; }

    // 获取数据面板指针（供外部连接信号槽）
    DataPanelWidget* dataPanel() { return m_dataPanel; }

signals:
    // 启动视觉系统请求信号
    void visionStartRequested();
    // 停止视觉系统请求信号
    void visionStopRequested();

private slots:
    void onMarkButtonClicked();
    void onObstacleToggled(int id, bool marked);
    void onSelectStartZoneClicked();
    void onStartZoneSelected(int zoneIndex, const QString &zoneName);
    // 开始按钮点击槽函数
    void onStartButtonClicked();
    // 仿真按钮点击槽函数
    void onSimButtonClicked();
    // 仿真日志
    void onSimLog(const QString& msg);
    // 路径预览按钮点击槽函数
    void onPathPreviewClicked();

private:
    void updateStatus();

private:
    CourtMapWidget *m_courtMap = nullptr;
    DataPanelWidget *m_dataPanel = nullptr;  // 实时数据面板
    SimulationController *m_simController = nullptr;  // 仿真控制器
    QPushButton *m_markBtn = nullptr;
    QPushButton *m_selectStartBtn = nullptr;
    QPushButton *m_startBtn = nullptr;      // 开始/停止按钮
    QPushButton *m_simBtn = nullptr;        // 仿真按钮
    QPushButton *m_pathPreviewBtn = nullptr; // 路径预览按钮
    QLabel *m_statusLabel = nullptr;
    bool m_visionRunning = false;            // 视觉系统运行状态
};

#endif // MAINWINDOW_H