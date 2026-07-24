/// @file mainwindow.hpp
/// @brief 主窗口类声明，集成赛场地图、仿真控制器与运动控制器。
///        负责顶层UI布局、工具栏按钮管理以及各子模块间的信号槽协调。
///        用户通过此窗口完成障碍物标记、启停区选择与仿真启停等操作。

#pragma once

#include "courtmapwidget.hpp"
#include "serial_comm.hpp"
#include "simulation_controller.hpp"

#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QImage>

class MotionController;
class VisionSystem;

/// @brief 应用程序主窗口，组合地图控件、仿真与运动控制器。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /// @brief 构造主窗口，绑定外部串口实例与视觉系统。
    /// @param serial_comm 外部注入的串口实例（与 VisionSystem 共享）
    /// @param vision_system 外部注入的视觉系统（用于取物料坐标、设置视觉模式）
    /// @param parent 父控件
    explicit MainWindow(gonxun::SerialComm& serial_comm, VisionSystem& vision_system,
                         QWidget* parent = nullptr) noexcept;
    ~MainWindow() override;

signals:
    /// @brief 二维码扫描完成信号
    void qr_code_scanned(const QString& task_code);

    /// @brief 用户点击"开始"按钮时发射，请求启动视觉系统。
    void vision_start_requested();

    /// @brief 用户点击"停止"按钮时发射，请求停止视觉系统。
    void vision_stop_requested();

    /// @brief 模式切换信号（手动覆写视觉系统工作模式）
    /// @param mode 视觉模式常量 (VISION_COLOR/RING/QR)
    /// @param manual true=手动覆写开启, false=回到自动模式
    void mode_switch_requested(int mode, bool manual);

    /// @brief 比赛开始（收到下位机 match_start=1）信号
    void match_started();

private slots:
    void on_mark_button_clicked();
    void on_obstacle_toggled(int id, bool marked);
    void on_select_start_zone_clicked();
    void on_start_zone_selected(int zone_index, const QString &zone_name);
    void on_start_button_clicked();
    void on_sim_button_clicked();
    void on_mode_button_clicked();
    void on_qr_code_scanned(const QString& task_code);

    /// @brief 收到下位机比赛开始信号（由 SerialComm 回调跨线程 invoke）
    void on_match_started();

public slots:
    void on_frame_ready(const QImage& frame);
    void on_qr_frame_ready(const QImage& frame);

private:
    /// @brief 检查自动启动条件（启停区+任务码+比赛开始），满足则启动仿真。
    void try_auto_start_mission();

    CourtMapWidget *court_map_ = nullptr;
    SimulationController *sim_controller_ = nullptr;
    gonxun::SerialComm *serial_comm_ = nullptr;          ///< 外部注入，不拥有
    MotionController *motion_controller_ = nullptr;
    VisionSystem *vision_system_ = nullptr;      ///< 外部注入，不拥有

    QPushButton *mark_btn_ = nullptr;
    QPushButton *select_start_btn_ = nullptr;
    QPushButton *start_btn_ = nullptr;
    QPushButton *sim_btn_ = nullptr;
    QPushButton *color_btn_ = nullptr;
    QPushButton *ring_btn_ = nullptr;
    QPushButton *qr_btn_ = nullptr;

    QLabel *main_camera_label_ = nullptr;
    QLabel *qr_camera_label_ = nullptr;

    bool vision_running_ = false;
    bool has_start_zone_ = false;     ///< 是否已选择启停区
    bool has_task_code_ = false;      ///< 是否已扫码得到任务码
    bool match_started_ = false;      ///< 是否已收到比赛开始信号
    QString task_code_;              ///< 缓存任务码
};
