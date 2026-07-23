/// @file mainwindow.hpp
/// @brief 主窗口类声明，集成赛场地图、数据面板、仿真控制器与运动控制器。
///        负责顶层UI布局、工具栏按钮管理以及各子模块间的信号槽协调。
///        用户通过此窗口完成障碍物标记、启停区选择、路径预览与仿真启停等操作。

#pragma once

#include "courtmapwidget.hpp"
#include "data_panel_widget.hpp"
#include "serial_comm.hpp"
#include "simulation_controller.hpp"

#include <QLabel>
#include <QMainWindow>
#include <QPushButton>

class MotionController;

/// @brief 应用程序主窗口，组合地图控件、数据面板、仿真与运动控制器。
///
/// 顶层布局为：顶部工具栏（标记/启停区/开始/仿真/路径预览按钮 + 状态标签）
/// + 中央 QSplitter（左侧地图 3:1 右侧数据面板）。
/// 按钮互斥逻辑：标记模式与启停区选择不可同时激活。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /// @brief 构造主窗口，初始化所有子控件、串口通信与信号槽连接。
    /// @param parent 父控件指针，默认 nullptr
    explicit MainWindow(QWidget *parent = nullptr) noexcept;

    /// @brief 析构主窗口，释放 serial_comm_ 资源。
    ~MainWindow() override;

signals:
    /// @brief 用户点击"开始"按钮时发射，请求启动视觉系统。
    void vision_start_requested();

    /// @brief 用户点击"停止"按钮时发射，请求停止视觉系统。
    void vision_stop_requested();

private slots:
    /// @brief 标记障碍物按钮点击槽，切换地图标记模式并互斥取消启停区选择。
    void on_mark_button_clicked();

    /// @brief 障碍物标记状态变更槽，刷新状态栏显示。
    /// @param id 障碍物编号
    /// @param marked 是否已标记
    void on_obstacle_toggled(int id, bool marked);

    /// @brief 选择启停区按钮点击槽，切换启停区可选模式并互斥取消标记模式。
    void on_select_start_zone_clicked();

    /// @brief 启停区选择完成槽，显示选中区域信息并退出选择模式。
    /// @param zone_index 启停区索引（0=右上角，1=右下角）
    /// @param zone_name 启停区名称
    void on_start_zone_selected(int zone_index, const QString &zone_name);

    /// @brief 开始/停止按钮点击槽，切换视觉系统启停状态。
    void on_start_button_clicked();

    /// @brief 仿真按钮点击槽，启动或停止仿真控制器。
    void on_sim_button_clicked();

    /// @brief 路径预览按钮点击槽，生成并显示完整任务路径预览。
    void on_path_preview_clicked();

private:
    /// @brief 根据当前系统状态更新状态栏文本与颜色。
    ///        优先级：仿真运行 > 视觉运行 > 标记模式 > 启停区已选 > 默认。
    void update_status();

private:
    CourtMapWidget *court_map_ = nullptr;       ///< 赛场地图绘制控件
    DataPanelWidget *data_panel_ = nullptr;     ///< 右侧数据面板控件
    SimulationController *sim_controller_ = nullptr; ///< 仿真控制器
    SerialComm *serial_comm_ = nullptr;         ///< 串口通信实例
    MotionController *motion_controller_ = nullptr;  ///< 运动控制器

    QPushButton *mark_btn_ = nullptr;           ///< 标记障碍物切换按钮
    QPushButton *select_start_btn_ = nullptr;   ///< 选择启停区切换按钮
    QPushButton *start_btn_ = nullptr;          ///< 开始/停止视觉系统按钮
    QPushButton *sim_btn_ = nullptr;            ///< 仿真启停按钮
    QPushButton *path_preview_btn_ = nullptr;   ///< 路径预览按钮
    QLabel *status_label_ = nullptr;            ///< 状态提示标签

    bool vision_running_ = false;               ///< 视觉系统是否正在运行
};
