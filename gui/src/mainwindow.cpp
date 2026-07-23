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

// Qt布局组件
#include <QHBoxLayout>
#include <QSplitter>
#include <QVBoxLayout>

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
MainWindow::MainWindow(QWidget *parent) noexcept
    : QMainWindow(parent)
{
    // ===================== 1. 基础窗口配置 =====================
    setWindowTitle("赛场地图");          // 窗口标题
    resize(780, 560);                   // 初始窗口大小
    setMinimumSize(600, 400);           // 窗口最小尺寸，防止拉伸变形

    // ===================== 2. 顶层容器与主布局 =====================
    // QMainWindow必须设置centralWidget作为所有控件载体
    QWidget *central = new QWidget(this);
    // 垂直主布局：顶部工具栏 + 底部主体分栏
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(10, 10, 10, 10); // 整体边距
    layout->setSpacing(8);                      // 控件间距

    // ===================== 3. 顶部工具栏布局 =====================
    QHBoxLayout *toolbar = new QHBoxLayout();
    toolbar->setSpacing(10);

    // -------- 开始/停止视觉任务按钮 --------
    start_btn_ = new QPushButton("开始", central);
    start_btn_->setMinimumHeight(36);
    start_btn_->setCheckable(true); // 可选中切换状态
    // 绿常态、深绿悬浮、红选中（停止警示色）
    start_btn_->setStyleSheet(button_style("#2ecc71", "#27ae60", "#c0392b"));

    // -------- 标记障碍物按钮 --------
    mark_btn_ = new QPushButton("标记障碍物", central);
    mark_btn_->setMinimumHeight(36);
    mark_btn_->setCheckable(true);
    // 蓝常态、深蓝悬浮、红选中
    mark_btn_->setStyleSheet(button_style("#4a90e2", "#357abd", "#e74c3c"));

    // -------- 选择启停区按钮 --------
    select_start_btn_ = new QPushButton("选择启停区", central);
    select_start_btn_->setMinimumHeight(36);
    select_start_btn_->setCheckable(true);
    // 绿常态、深绿悬浮、青绿选中
    select_start_btn_->setStyleSheet(button_style("#27ae60", "#229954", "#16a085"));

    // -------- 仿真运行按钮 --------
    sim_btn_ = new QPushButton("仿真", central);
    sim_btn_->setMinimumHeight(36);
    sim_btn_->setCheckable(true);
    // 紫常态、深紫悬浮、红选中
    sim_btn_->setStyleSheet(button_style("#8e44ad", "#7d3c98", "#c0392b"));

    // -------- 路径预览按钮（瞬时点击，无选中状态） --------
    path_preview_btn_ = new QPushButton("路径预览", central);
    path_preview_btn_->setMinimumHeight(36);
    path_preview_btn_->setCheckable(false);
    // 橙色调，固定样式
    path_preview_btn_->setStyleSheet(button_style("#e67e22", "#d35400", "#d35400"));

    // -------- 全局状态栏文本标签 --------
    status_label_ = new QLabel(central);
    status_label_->setFont(QFont("Microsoft YaHei", 10));

    // 将所有控件加入工具栏，末尾拉伸留白对齐
    toolbar->addWidget(start_btn_);
    toolbar->addWidget(mark_btn_);
    toolbar->addWidget(select_start_btn_);
    toolbar->addWidget(sim_btn_);
    toolbar->addWidget(path_preview_btn_);
    toolbar->addWidget(status_label_);
    toolbar->addStretch();

    // ===================== 4. 主体分栏布局：地图 + 数据面板 =====================
    // 赛场地图画布（核心可视化区域）
    court_map_ = new CourtMapWidget(central);
    // 右侧数据面板：展示位姿、任务状态、统计数据
    data_panel_ = new DataPanelWidget(central);

    // 水平分割器，支持鼠标拖拽调整比例
    QSplitter *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->addWidget(court_map_);
    splitter->addWidget(data_panel_);

    // 设置拉伸权重：地图3 ： 面板1
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    // 初始固定尺寸分配
    splitter->setSizes({500, 260});

    // 组装整体页面
    layout->addLayout(toolbar);
    layout->addWidget(splitter, 1);

    setCentralWidget(central);

    // ===================== 5. 底层硬件初始化：串口通信 =====================
    // 参数：模拟关闭、指定串口、115200波特率、无校验、非调试
    serial_comm_ = new SerialComm(true, "/dev/ttyCH341USB0", 115200, 0, false);
    serial_comm_->start(); // 启动串口接收发送线程

    // ===================== 6. 业务控制器初始化 =====================
    // 运动控制器：绑定串口、地图画布，负责真实移动逻辑
    motion_controller_ = new MotionController(*serial_comm_, *court_map_, this);

    // 仿真控制器：绑定画布、数据面板、运动控制器，负责离线仿真推演
    sim_controller_ = new SimulationController(*court_map_, data_panel_, motion_controller_, this);
    sim_controller_->set_animation_interval(50);    // 仿真动画刷新间隔50ms
    sim_controller_->set_dwell_time(800);           // 节点停留等待800ms
    sim_controller_->set_total_cycles(1);            // 默认单次任务循环

    // ===================== 7. 全局信号槽绑定 =====================
    // 按钮点击事件绑定
    connect(start_btn_, &QPushButton::clicked, this, &MainWindow::on_start_button_clicked);
    connect(mark_btn_, &QPushButton::clicked, this, &MainWindow::on_mark_button_clicked);
    connect(select_start_btn_, &QPushButton::clicked, this, &MainWindow::on_select_start_zone_clicked);
    connect(sim_btn_, &QPushButton::clicked, this, &MainWindow::on_sim_button_clicked);
    connect(path_preview_btn_, &QPushButton::clicked, this, &MainWindow::on_path_preview_clicked);

    // 地图画布交互回调
    connect(court_map_, &CourtMapWidget::obstacle_toggled, this, &MainWindow::on_obstacle_toggled);
    connect(court_map_, &CourtMapWidget::start_zone_selected, this, &MainWindow::on_start_zone_selected);

    // 仿真结束自动复位UI状态
    connect(sim_controller_, &SimulationController::simulation_finished, [this](bool success) {
        Q_UNUSED(success)
        sim_btn_->setText("仿真");
        sim_btn_->setChecked(false);
        update_status();
    });

    // 初始化默认状态栏
    update_status();
}

/**
 * @brief 主窗口析构函数
 * @note 手动释放动态申请的串口资源，防止内存泄漏
 */
MainWindow::~MainWindow()
{
    delete serial_comm_;
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
    if (vision_running_) {
        // 关闭视觉系统
        vision_running_ = false;
        start_btn_->setText("开始");
        start_btn_->setChecked(false);
        emit vision_stop_requested();
    } else {
        // 开启视觉系统
        vision_running_ = true;
        start_btn_->setText("停止");
        start_btn_->setChecked(true);
        emit vision_start_requested();
    }
    update_status();
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
    bool enabled = mark_btn_->isChecked();
    // 通知地图画布切换标记模式
    court_map_->set_mark_mode(enabled);

    if (enabled) {
        mark_btn_->setText("结束标记");
        // ========== 模式互斥：标记开启则关闭选区 ==========
        if (select_start_btn_->isChecked())
        {
            select_start_btn_->setChecked(false);
            court_map_->set_start_zone_selectable(false);
            select_start_btn_->setText("选择启停区");
        }
    } else {
        mark_btn_->setText("标记障碍物");
    }
    update_status();
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
    update_status();
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
        select_start_btn_->setText("取消选择");
        // ========== 模式互斥：选区开启则关闭标记 ==========
        if (mark_btn_->isChecked()) {
            mark_btn_->setChecked(false);
            court_map_->set_mark_mode(false);
            mark_btn_->setText("标记障碍物");
        }
    } else {
        select_start_btn_->setText("选择启停区");
    }
    update_status();
}

/**
 * @brief 启停区选中完成回调
 * @param zone_index 选区编号
 * @param zone_name 选区名称
 * @details 选择完成后自动退出选择模式，固化选中启停区
 * 更新状态栏绿色成功提示
 */
void MainWindow::on_start_zone_selected(int zone_index, const QString &zone_name)
{
    QString pos = (zone_index == 0) ? "右上角" : "右下角";
    status_label_->setText(QString("已选择: %1 (%2) | 障碍物: %3个")
                           .arg(zone_name, pos).arg(court_map_->marked_count()));
    status_label_->setStyleSheet("color: #27ae60; font-weight: bold;");

    // 自动复位按钮与模式
    select_start_btn_->setChecked(false);
    select_start_btn_->setText("选择启停区");
    court_map_->set_start_zone_selectable(false);
}

/**
 * @brief 全局状态栏状态刷新函数
 * @details 【状态优先级机制（从高到低）】
 *  1. 仿真运行中（最高优先级，紫色）
 *  2. 视觉算法运行中（绿色）
 *  3. 障碍物标记模式（红色）
 *  4. 已选定启停区（绿色）
 *  5. 默认空闲状态（灰色）
 *
 * 根据当前系统最高优先级状态动态刷新文本与字体颜色
 */
void MainWindow::update_status()
{
    int count = court_map_->marked_count();
    QString zone = court_map_->selected_start_zone_name();

    QString status_text;
    QString color;

    if (sim_controller_ && sim_controller_->is_running()) {
        status_text = QString("仿真运行中 | 障碍物: %1个").arg(count);
        color = "color: #8e44ad; font-weight: bold;";
    } else if (vision_running_) {
        status_text = QString("视觉系统运行中 | 障碍物: %1个").arg(count);
        color = "color: #27ae60; font-weight: bold;";
    } else if (mark_btn_->isChecked()) {
        status_text = QString("点击红色矩形标记障碍物 | 已标记: %1个").arg(count);
        color = "color: #e74c3c; font-weight: bold;";
    } else if (!zone.isEmpty()) {
        QString pos = (court_map_->selected_start_zone() == 0) ? "右上角" : "右下角";
        status_text = QString("启动区: %1 (%2) | 障碍物: %3个").arg(zone, pos).arg(count);
        color = "color: #27ae60; font-weight: bold;";
    } else {
        status_text = QString("障碍物: %1个").arg(count);
        color = "color: #555;";
    }

    status_label_->setText(status_text);
    status_label_->setStyleSheet(color);
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
    if (sim_controller_->is_running()) {
        // 停止仿真
        sim_controller_->stop();
        sim_btn_->setText("仿真");
        sim_btn_->setChecked(false);
    } else {
        // 启动仿真，固定任务码312
        bool ok = sim_controller_->start("312");
        if (ok) {
            sim_btn_->setText("停止仿真");
            sim_btn_->setChecked(true);
        } else {
            sim_btn_->setChecked(false);
        }
    }
    update_status();
}

/**
 * @brief 路径预览按钮回调
 * @details 自动生成赛场标准闭环任务路径
 * 路径固定业务流程：
 *  自定义启停区 → 扫码区 → 原料区 → 粗加工区 → 暂存区 → 返回启停区
 *
 * 前置校验：必须先选择启停区，否则红字提示用户
 * 功能：
 *  1. 自动拼接全路径坐标点
 *  2. 在地图上绘制预览轨迹
 *  3. 刷新状态栏路径信息
 *  4. 刷新右侧面板机器人初始位姿与任务状态
 */
void MainWindow::on_path_preview_clicked()
{
    // 获取用户选中的启停区索引
    int start_zone = court_map_->selected_start_zone();
    // 前置校验：未选择启停区直接报错返回
    if (start_zone < 0) {
        status_label_->setText("请先选择启停区（右上角或右下角）");
        status_label_->setStyleSheet("color: #e74c3c; font-weight: bold;");
        return;
    }

    QVector<QPointF> full_path;
    // 根据选区自动适配起点坐标
    int start_x = 0;
    int start_y = (start_zone == 0) ? 0 : 4;

    // 标准赛场任务点位序列（固定比赛流程）
    struct GridStep { int x, y; const char* name; };
    GridStep steps[] = {
        {start_x, start_y, "启停区"},
        {0, 2, "扫码区"},
        {2, 0, "原料区"},
        {2, 4, "粗加工区"},
        {4, 2, "暂存区"},
        {start_x, start_y, "启停区"}
    };

    // 遍历生成每一个格子中心点坐标，拼接完整路径
    for (const auto& step : steps) {
        full_path.append(court_map_->get_cell_center(step.x, step.y));
    }

    // 推送路径至地图画布渲染
    court_map_->set_path(full_path);

    // 更新状态栏路径预览信息
    QString zone_name = court_map_->selected_start_zone_name();
    QString pos = (start_zone == 0) ? "右上角" : "右下角";
    status_label_->setText(QString("路径预览: %1 (%2) → 扫码 → 原料 → 粗加工 → 暂存 → 返回 | 总路径点: %3个")
                          .arg(zone_name, pos).arg(full_path.size()));
    status_label_->setStyleSheet("color: #e67e22; font-weight: bold;");

    // 更新右侧数据面板：机器人初始位姿 + 任务状态
    if (data_panel_) {
        QPointF robot_pos = full_path.first();
        data_panel_->update_robot_pose(static_cast<int>(robot_pos.x()),
                                       static_cast<int>(robot_pos.y()),
                                       0.0);
        data_panel_->update_task_state("路径预览");
    }
}
