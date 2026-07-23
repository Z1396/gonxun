/// @file mainwindow.cpp
/// @brief 主窗口实现，负责UI布局构建、按钮样式配置与各子模块信号槽连接。
///
/// 核心职责：
///  1. 搭建顶层UI结构：工具栏 + 地图视图 + 数据面板（3:1比例）
///  2. 统一封装全局按钮样式，美化交互界面
///  3. 初始化底层硬件与算法控制器：串口/运动/仿真控制器
///  4. 绑定所有用户操作信号槽，处理按钮点击业务逻辑
///  5. 实现【标记模式 / 启停区选择】互斥保护，防止模式冲突
///  6. 维护全局UI状态机，实时刷新状态栏信息
///
/// 交互规则：
///  - 标记障碍物模式 & 选择启停区模式 互斥，同一时间只能开启一个
///  - 仿真运行状态优先级最高，覆盖普通视觉运行提示
///  - 自动路径预览：根据选中启停区生成完整赛场任务闭环路径

#include "mainwindow.hpp"
#include "motion_controller.hpp"
#include "vision_system.hpp"

// Qt布局组件
#include <QHBoxLayout>
#include <QMetaObject>
#include <QSplitter>
#include <QVBoxLayout>
#include <iostream>
#include <QPixmap>
#include <cstdint>
#include <optional>
#include <utility>

namespace {

/**
 * @brief 全局按钮QSS样式模板
 * @note 固定三种状态：正常/悬浮/选中
 *       使用占位符 %1 %2 %3 动态填充颜色，实现多按钮差异化配色
 */
constexpr const char* BTN_STYLE =
    "QPushButton { background-color: %1; color: white; border: none; "
    "border-radius: 6px; padding: 8px 16px; font-family: 'Microsoft YaHei'; font-size: 10pt; font-weight: bold; }"
    "QPushButton:hover { background-color: %2; }"
    "QPushButton:checked { background-color: %3; }";

/**
 * @brief 生成定制化按钮样式表
 * @param normal 常态背景色
 * @param hover  鼠标悬浮色
 * @param checked 选中/激活色
 * @return 拼接完成的QSS样式字符串
 */
QString button_style(const char *normal, const char *hover, const char *checked) {
    return QString(BTN_STYLE).arg(normal, hover, checked);
}

} // namespace

/**
 * @brief MainWindow 主窗口构造函数
 * @param parent 父窗口指针
 * @ noexcept 保证构造不抛出异常
 *
 * 初始化流水线（严格顺序）：
 *  1. 基础窗口属性（大小、标题、最小尺寸）
 *  2. 顶层布局容器与页面结构
 *  3. 全部功能按钮创建 + 差异化配色样式
 *  4. 地图视图 + 数据面板分栏布局（3:1黄金比例）
 *  5. 底层硬件：串口通信初始化并启动
 *  6. 业务控制器：运动控制器、仿真控制器初始化
 *  7. 所有信号槽绑定（UI ↔ 业务逻辑 ↔ 仿真逻辑）
 *  8. 初始化全局状态栏状态
 */
MainWindow::MainWindow(SerialComm& serial_comm, VisionSystem& vision_system,
                         QWidget* parent) noexcept
    : QMainWindow(parent),
      serial_comm_(&serial_comm),
      vision_system_(&vision_system)
{
    // ===================== 1. 基础窗口配置 =====================
    setWindowFlags(Qt::FramelessWindowHint);  // 无边框窗口，去掉标题栏
    setFixedSize(570, 350);                   // 固定窗口大小，禁止调整

    // ===================== 2. 顶层容器与主布局 =====================
    // QMainWindow必须设置centralWidget作为所有控件载体
    QWidget *central = new QWidget(this);
    // 水平主布局：左侧工具栏 + 右侧地图
    QHBoxLayout *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 10, 0, 10); // 整体边距
    layout->setSpacing(0);                      // 控件间距

    // ===================== 3. 左侧工具栏布局（垂直排列） =====================
    QVBoxLayout *toolbar = new QVBoxLayout();
    toolbar->setSpacing(10);

    // -------- 开始/停止视觉任务按钮 --------
    start_btn_ = new QPushButton("开始", central);
    start_btn_->setFixedSize(60, 36);
    start_btn_->setCheckable(true); // 可选中切换状态
    // 绿常态、深绿悬浮、红选中（停止警示色）
    start_btn_->setStyleSheet(button_style("#2ecc71", "#27ae60", "#c0392b"));

    // -------- 障碍物按钮 --------
    mark_btn_ = new QPushButton("障碍物", central);
    mark_btn_->setFixedSize(60, 36);
    mark_btn_->setCheckable(true);
    // 蓝常态、深蓝悬浮、红选中
    mark_btn_->setStyleSheet(button_style("#4a90e2", "#357abd", "#e74c3c"));

    // -------- 启停区按钮 --------
    select_start_btn_ = new QPushButton("启停区", central);
    select_start_btn_->setFixedSize(60, 36);
    select_start_btn_->setCheckable(true);
    // 绿常态、深绿悬浮、青绿选中
    select_start_btn_->setStyleSheet(button_style("#27ae60", "#229954", "#16a085"));

    // -------- 仿真运行按钮 --------
    sim_btn_ = new QPushButton("仿真", central);
    sim_btn_->setFixedSize(60, 36);
    sim_btn_->setCheckable(true);
    // 紫常态、深紫悬浮、红选中
    sim_btn_->setStyleSheet(button_style("#8e44ad", "#7d3c98", "#c0392b"));

    // -------- 物料识别模式按钮 --------
    color_btn_ = new QPushButton("物料", central);
    color_btn_->setFixedSize(60, 36);
    color_btn_->setCheckable(true);
    // 橙常态、深橙悬浮、红选中
    color_btn_->setStyleSheet(button_style("#e67e22", "#d35400", "#e74c3c"));

    // -------- 圆环检测模式按钮 --------
    ring_btn_ = new QPushButton("圆环", central);
    ring_btn_->setFixedSize(60, 36);
    ring_btn_->setCheckable(true);
    // 蓝常态、深蓝悬浮、红选中
    ring_btn_->setStyleSheet(button_style("#2980b9", "#2471a3", "#e74c3c"));

    // -------- 二维码扫描模式按钮 --------
    qr_btn_ = new QPushButton("二维码", central);
    qr_btn_->setFixedSize(60, 36);
    qr_btn_->setCheckable(true);
    // 青常态、深青悬浮、红选中
    qr_btn_->setStyleSheet(button_style("#16a085", "#138d75", "#e74c3c"));

    // -------- 关闭按钮 --------
    QPushButton *close_btn = new QPushButton("关闭", central);
    close_btn->setFixedSize(60, 36);
    close_btn->setStyleSheet(button_style("#c0392b", "#e74c3c", "#c0392b"));
    connect(close_btn, &QPushButton::clicked, this, &QMainWindow::close);

    // 将所有控件加入工具栏，末尾拉伸留白对齐
    toolbar->addWidget(start_btn_, 0, Qt::AlignTop);
    toolbar->addWidget(color_btn_, 0, Qt::AlignTop);
    toolbar->addWidget(ring_btn_, 0, Qt::AlignTop);
    toolbar->addWidget(qr_btn_, 0, Qt::AlignTop);
    toolbar->addWidget(mark_btn_, 0, Qt::AlignTop);
    toolbar->addWidget(select_start_btn_, 0, Qt::AlignTop);
    toolbar->addWidget(sim_btn_, 0, Qt::AlignTop);
    toolbar->addStretch();
    toolbar->addWidget(close_btn, 0, Qt::AlignBottom);  // 关闭按钮在底部

    // ===================== 摄像头显示窗口 =====================
    // 主摄像头窗口
    main_camera_label_ = new QLabel(nullptr);
    main_camera_label_->setWindowTitle("主摄像头");
    main_camera_label_->setFixedSize(320, 240);
    main_camera_label_->setAlignment(Qt::AlignCenter);
    main_camera_label_->setStyleSheet("background-color: black; color: gray;");
    main_camera_label_->setText("等待图像...");
    main_camera_label_->hide();  // 初始隐藏，视觉启动后显示

    // 扫码摄像头窗口
    qr_camera_label_ = new QLabel(nullptr);
    qr_camera_label_->setWindowTitle("扫码摄像头");
    qr_camera_label_->setFixedSize(320, 240);
    qr_camera_label_->setAlignment(Qt::AlignCenter);
    qr_camera_label_->setStyleSheet("background-color: black; color: gray;");
    qr_camera_label_->setText("等待图像...");
    qr_camera_label_->hide();  // 初始隐藏，视觉启动后显示

    // ===================== 4. 主体布局：地图 =====================
    // 赛场地图画布（核心可视化区域）
    court_map_ = new CourtMapWidget(central);

    // 组装整体页面：左侧工具栏 + 右侧地图
    layout->addLayout(toolbar);
    layout->addWidget(court_map_, 1);

    setCentralWidget(central);

    // ===================== 5. 业务控制器初始化 =====================
    // 运动控制器：绑定外部注入的串口、地图画布，负责真实移动逻辑
    // 串口实例由 main 统一构造并启动，此处直接使用引用
    motion_controller_ = new MotionController(*serial_comm_, *court_map_, this);

    // 仿真控制器：绑定画布、运动控制器，负责离线仿真推演
    sim_controller_ = new SimulationController(*court_map_, motion_controller_, this);
    sim_controller_->set_dwell_time(800);           // 节点停留等待800ms
    sim_controller_->set_total_cycles(1);            // 默认单次任务循环

    // 注入物料坐标提供者：到达抓取目标时取首坐标发 mode=2 定位帧
    sim_controller_->set_material_coord_provider(
        [this]() -> std::optional<std::pair<uint16_t, uint16_t>> {
            const auto& coords = vision_system_->material_coords();
            if (coords.empty()) return std::nullopt;
            return coords.front();
        });

    // 注册串口回调：比赛开始信号跨线程 marshal 到本对象线程
    serial_comm_->set_match_start_callback([this](bool) {
        QMetaObject::invokeMethod(this, "on_match_started", Qt::QueuedConnection);
    });

    // ===================== 7. 全局信号槽绑定 =====================
    // 按钮点击事件绑定
    // 视觉启停按钮点击回调
    connect(start_btn_, &QPushButton::clicked, this, &MainWindow::on_start_button_clicked);
    // 障碍物按钮点击回调
    connect(mark_btn_, &QPushButton::clicked, this, &MainWindow::on_mark_button_clicked);
    // 启停区按钮点击回调
    connect(select_start_btn_, &QPushButton::clicked, this, &MainWindow::on_select_start_zone_clicked);
    // 仿真运行按钮点击回调
    connect(sim_btn_, &QPushButton::clicked, this, &MainWindow::on_sim_button_clicked);
    // 模式切换按钮点击回调（三个按钮共用一个槽函数）
    connect(color_btn_, &QPushButton::clicked, this, &MainWindow::on_mode_button_clicked);
    connect(ring_btn_, &QPushButton::clicked, this, &MainWindow::on_mode_button_clicked);
    connect(qr_btn_, &QPushButton::clicked, this, &MainWindow::on_mode_button_clicked);

    // 地图画布交互回调：处理障碍物与启停区选择
    // 障碍物切换回调
    connect(court_map_, &CourtMapWidget::obstacle_toggled, this, &MainWindow::on_obstacle_toggled);
    // 启停区选择回调   
    connect(court_map_, &CourtMapWidget::start_zone_selected, this, &MainWindow::on_start_zone_selected);

    // 仿真结束自动复位UI状态回调
    connect(sim_controller_, &SimulationController::simulation_finished, [this](bool success) {
        Q_UNUSED(success)
        sim_btn_->setText("仿真");
        sim_btn_->setChecked(false);
    });

    // 二维码扫描完成信号槽（线程安全跨线程通信）
    connect(this, &MainWindow::qr_code_scanned, this, &MainWindow::on_qr_code_scanned);
}

/**
 * @brief 主窗口析构函数
 * @note serial_comm_ 与 vision_system_ 由外部（main）管理，此处不释放
 */
MainWindow::~MainWindow()
{
}

/**
 * @brief 视觉启停按钮点击回调
 * @details 维护全局视觉运行状态 vision_running_
 *  开启：发射视觉启动信号，通知底层VisionController启动算法
 *  关闭：发射视觉停止信号，终止视觉采集与推理
 *  同步切换按钮文本与选中状态
 */
void MainWindow::on_start_button_clicked()
{
    if (vision_running_) 
    {
        // 关闭视觉系统
        vision_running_ = false;
        start_btn_->setText("开始");
        start_btn_->setChecked(false);
        emit vision_stop_requested();
        
        // 隐藏摄像头窗口
        main_camera_label_->hide();
        qr_camera_label_->hide();
    } else 
    {
        // 开启视觉系统
        vision_running_ = true;
        // 同步更新按钮文本与选中状态
        start_btn_->setText("停止");
        // 同步更新选中状态
        start_btn_->setChecked(true);
        emit vision_start_requested();
        
        // 显示摄像头窗口
        main_camera_label_->move(100, 100);
        main_camera_label_->show();
        qr_camera_label_->move(450, 100);
        qr_camera_label_->show();
        /*emit是Qt的信号槽机制，用于跨线程通信*/
    }
}

/**
 * @brief 标记障碍物按钮回调
 * @details 核心【模式互斥逻辑】
 *  1. 开启标记模式：关闭启停区选择模式，防止操作冲突
 *  2. 关闭标记模式：恢复默认按钮文本
 *  3. 同步刷新状态栏提示
 */
void MainWindow::on_mark_button_clicked()
{
    // 检查当前是否为标记模式
    bool enabled = mark_btn_->isChecked();
    // 通知地图画布切换标记模式
    court_map_->set_mark_mode(enabled);

    if (enabled) 
    {
        mark_btn_->setText("结束");
        // ========== 模式互斥：标记开启则关闭选区 ==========
        if (select_start_btn_->isChecked())
        {
            // 同步更新选中状态
            select_start_btn_->setChecked(false);
            court_map_->set_start_zone_selectable(false);
            select_start_btn_->setText("启停区");
        }
    } else
    {
        mark_btn_->setText("障碍物");
    }
}

/**
 * @brief 障碍物标记变更回调
 * @param id 障碍物格子ID
 * @param marked 是否标记
 * @note 无需处理参数，仅触发状态栏刷新计数
 */
void MainWindow::on_obstacle_toggled(int id, bool marked)
{
    Q_UNUSED(id)
    Q_UNUSED(marked)
}

/**
 * @brief 启停区选择按钮回调
 * @details 反向互斥逻辑：开启选区则关闭标记模式
 * 保证同一时间只能有一种编辑模式生效
 */
void MainWindow::on_select_start_zone_clicked()
{
    bool enabled = select_start_btn_->isChecked();
    court_map_->set_start_zone_selectable(enabled);

    if (enabled) {
        select_start_btn_->setText("结束");
        // ========== 模式互斥：选区开启则关闭标记 ==========
        if (mark_btn_->isChecked()) {
            mark_btn_->setChecked(false);
            court_map_->set_mark_mode(false);
            mark_btn_->setText("障碍物");
        }
    } else {
        select_start_btn_->setText("启停区");
    }
}

/**
 * @brief 启停区选中完成回调
 * @param zone_index 选区编号
 * @param zone_name 选区名称
 * @details 选择完成后自动退出选择模式，固化选中启停区
 */
void MainWindow::on_start_zone_selected(int zone_index, const QString &zone_name)
{
    Q_UNUSED(zone_index)
    Q_UNUSED(zone_name)

    // 自动复位按钮与模式
    select_start_btn_->setChecked(false);
    select_start_btn_->setText("启停区");
    court_map_->set_start_zone_selectable(false);

    has_start_zone_ = true;      // 标记启停区已选定
    try_auto_start_mission();    // 尝试自动启动任务
}

/**
 * @brief 仿真按钮启停回调
 * @details
 *  点击开启：启动固定312任务码仿真流程
 *  点击关闭：立刻终止仿真动画与流程
 *  自动切换按钮文本：仿真 / 停止仿真
 */
void MainWindow::on_sim_button_clicked()
{
    if (sim_controller_->is_running()) 
    {
        // 停止仿真
        sim_controller_->stop();
        sim_btn_->setText("仿真");
        sim_btn_->setChecked(false);
    } else 
    {
        // 启动仿真，固定任务码312
        // 使用预设的完整任务码格式：156+123+516+231
        bool ok = sim_controller_->start("156+123+516+231");
        if (ok) 
        {
            sim_btn_->setText("停止仿真");
            sim_btn_->setChecked(true);
        } else 
        {
            sim_btn_->setChecked(false);
        }
    }
}

/**
 * @brief 模式切换按钮点击回调
 * @details 三按钮共用一个槽函数，通过 sender() 判断点击源
 *  互斥逻辑：点击任一按钮，取消其他两个按钮的选中状态
 *  再次点击已选中的按钮，取消选中（回到串口自动模式）
 *  根据选中状态发射 mode_switch_requested 信号
 */
void MainWindow::on_mode_button_clicked()
{
    auto *btn = qobject_cast<QPushButton*>(sender());

    // 判断点击的是哪个按钮及其选中状态
    int mode = VISION_IDLE;
    bool manual = false;

    if (btn == color_btn_)
    {
        if (color_btn_->isChecked())
        {
            // 物料模式选中，取消其他
            ring_btn_->setChecked(false);
            qr_btn_->setChecked(false);
            mode = VISION_COLOR;
            manual = true;
        }
        // 再次点击取消选中 → manual=false，回到自动模式
    }
    else if (btn == ring_btn_)
    {
        if (ring_btn_->isChecked())
        {
            color_btn_->setChecked(false);
            qr_btn_->setChecked(false);
            mode = VISION_RING;
            manual = true;
        }
    }
    else if (btn == qr_btn_)
    {
        if (qr_btn_->isChecked())
        {
            color_btn_->setChecked(false);
            ring_btn_->setChecked(false);
            mode = VISION_QR;
            manual = true;
        }
    }

    emit mode_switch_requested(mode, manual);
}

/**
 * @brief 处理二维码扫描完成信号（线程安全更新GUI）
 * @param task_code 任务码字符串（如 "156+123+516+231"）
 * @details 扫码成功后更新GUI任务码显示装置，一旦设置将一直显示直到程序结束。
 *          同时缓存任务码并尝试自动启动任务（启停区+任务码+比赛开始三条件）。
 *          此槽函数通过Qt信号槽机制从工作线程安全调用
 */
void MainWindow::on_qr_code_scanned(const QString& task_code)
{
    std::cout << "[MainWindow] 收到二维码扫描信号: " << task_code.toStdString() << std::endl;
    // 设置完整任务码（显示在地图上方）
    court_map_->set_task_code(task_code);

    has_task_code_ = true;     // 标记任务码已就绪
    task_code_ = task_code;    // 缓存供自动启动
    try_auto_start_mission();
}

/**
 * @brief 收到下位机比赛开始信号回调（由 SerialComm 跨线程 invoke）
 * @details match_start 为锁存信号：仅首次置位 match_started_ 并尝试自动启动任务。
 */
void MainWindow::on_match_started()
{
    if (match_started_) return;  // 锁存，仅触发一次
    match_started_ = true;
    std::cout << "[MainWindow] 收到比赛开始信号" << std::endl;
    emit match_started();
    try_auto_start_mission();
}

/**
 * @brief 检查自动启动条件，三条件齐备则启动仿真任务。
 * @details 条件：已选启停区 + 已扫码得任务码 + 已收比赛开始信号。
 *          任一缺失则静默返回，等待后续条件就绪时再次触发。
 */
void MainWindow::try_auto_start_mission()
{
    if (sim_controller_->is_running()) return;
    if (!has_start_zone_ || !has_task_code_ || !match_started_) return;

    std::cout << "[MainWindow] 三条件满足，自动启动任务: "
              << task_code_.toStdString() << std::endl;
    if (sim_controller_->start(task_code_)) {
        sim_btn_->setText("停止仿真");
        sim_btn_->setChecked(true);
    }
}

/**
 * @brief 更新主摄像头显示
 * @param frame 图像帧
 */
void MainWindow::on_frame_ready(const QImage& frame)
{
    if (main_camera_label_ && !frame.isNull())
    {
        main_camera_label_->setPixmap(QPixmap::fromImage(frame).scaled(
            main_camera_label_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

/**
 * @brief 更新扫码摄像头显示
 * @param frame 图像帧
 */
void MainWindow::on_qr_frame_ready(const QImage& frame)
{
    if (qr_camera_label_ && !frame.isNull())
    {
        qr_camera_label_->setPixmap(QPixmap::fromImage(frame).scaled(
            qr_camera_label_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}
