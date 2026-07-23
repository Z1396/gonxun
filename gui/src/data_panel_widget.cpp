/// @file data_panel_widget.cpp
/// @brief 数据面板控件实现，包含任务码显示、机器人坐标与任务信息。
///        任务码字体高度≥12mm（约45px），白底黑字，简洁清晰。

#include "data_panel_widget.hpp"

#include <QDateTime>

/// @brief 构造数据面板。
DataPanelWidget::DataPanelWidget(QWidget *parent) noexcept
    : QWidget(parent)
{
    setup_styles();
    setup_ui();
}

/// @brief 初始化样式表。
void DataPanelWidget::setup_styles()
{
    label_style_ = "QLabel { color: #666666; font-size: 10pt; }";
    value_style_ = "QLabel { color: #333333; font-size: 13pt; font-weight: bold; }";
    group_style_ = "QGroupBox { "
                   "  font-size: 10pt; font-weight: bold; "
                   "  color: #333333; "
                   "  border: 1px solid #cccccc; border-radius: 4px; "
                   "  margin-top: 10px; padding-top: 10px; "
                   "} "
                   "QGroupBox::title { "
                   "  subcontrol-origin: margin; left: 8px; padding: 0 4px; "
                   "}";
    highlight_style_ = "QLabel { color: #d32f2f; font-size: 13pt; font-weight: bold; }";
    state_normal_style_ = "QLabel { color: #1976d2; font-size: 13pt; font-weight: bold; }";
    state_flash_style_ = "QLabel { color: #ffffff; background-color: #1976d2; font-size: 13pt; font-weight: bold; padding: 2px 6px; border-radius: 3px; }";

    // 任务码显示样式：白底黑字，字体≥12mm（约45px）
    display_group_style_ = "QGroupBox { "
                           "  background-color: #ffffff; "
                           "  border: 2px solid #333333; border-radius: 4px; "
                           "  margin: 0px; padding: 8px; "
                           "}";
}

/// @brief 构建UI布局。
void DataPanelWidget::setup_ui()
{
    setStyleSheet(group_style_);

    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(4, 4, 4, 4);
    main_layout->setSpacing(8);

    // ---- 任务码显示（白底黑字，无标题）----
    display_group_ = new QGroupBox(this);
    display_group_->setStyleSheet(display_group_style_);

    QVBoxLayout* display_layout = new QVBoxLayout(display_group_);
    display_layout->setContentsMargins(4, 4, 4, 4);

    full_code_label_ = new QLabel(QString::fromUtf8("等待扫码..."), this);
    full_code_label_->setStyleSheet("QLabel { color: #000000; font-size: 32pt; font-weight: bold; background-color: #ffffff; }");
    full_code_label_->setAlignment(Qt::AlignCenter);
    full_code_label_->setMinimumHeight(50);
    display_layout->addWidget(full_code_label_);

    main_layout->addWidget(display_group_);

    // ---- 机器人坐标组 ----
    robot_group_ = new QGroupBox(QString::fromUtf8("机器人坐标"), this);
    QGridLayout* robot_layout = new QGridLayout(robot_group_);
    robot_layout->setSpacing(6);
    robot_layout->setContentsMargins(8, 8, 8, 8);

    robot_layout->addWidget(create_label(QString::fromUtf8("X:")), 0, 0, Qt::AlignLeft);
    robot_x_ = create_value_label("0 mm");
    robot_x_->setStyleSheet("QLabel { color: #2e7d32; font-size: 14pt; font-weight: bold; }");
    robot_layout->addWidget(robot_x_, 0, 1, Qt::AlignRight);

    robot_layout->addWidget(create_label(QString::fromUtf8("Y:")), 0, 2, Qt::AlignLeft);
    robot_y_ = create_value_label("0 mm");
    robot_y_->setStyleSheet("QLabel { color: #2e7d32; font-size: 14pt; font-weight: bold; }");
    robot_layout->addWidget(robot_y_, 0, 3, Qt::AlignRight);

    robot_layout->addWidget(create_label(QString::fromUtf8("角度:")), 1, 0, Qt::AlignLeft);
    robot_theta_ = create_value_label("0.0°");
    robot_theta_->setStyleSheet("QLabel { color: #7b1fa2; font-size: 12pt; font-weight: bold; }");
    robot_layout->addWidget(robot_theta_, 1, 1, 1, 3, Qt::AlignRight);

    main_layout->addWidget(robot_group_);

    // ---- 任务信息组 ----
    task_group_ = new QGroupBox(QString::fromUtf8("任务信息"), this);
    QGridLayout* task_layout = new QGridLayout(task_group_);
    task_layout->setSpacing(6);
    task_layout->setContentsMargins(8, 8, 8, 8);

    task_layout->addWidget(create_label(QString::fromUtf8("状态:")), 0, 0, Qt::AlignLeft);
    task_state_ = create_value_label(QString::fromUtf8("空闲"));
    task_state_->setStyleSheet(state_normal_style_);
    task_layout->addWidget(task_state_, 0, 1, Qt::AlignRight);

    task_layout->addWidget(create_label(QString::fromUtf8("循环:")), 1, 0, Qt::AlignLeft);
    task_cycle_ = create_value_label("0/0");
    task_layout->addWidget(task_cycle_, 1, 1, Qt::AlignRight);

    task_layout->addWidget(create_label(QString::fromUtf8("物料:")), 2, 0, Qt::AlignLeft);
    task_materials_ = create_value_label("0/0 | 0/0");
    task_layout->addWidget(task_materials_, 2, 1, Qt::AlignRight);

    main_layout->addWidget(task_group_);
    main_layout->addStretch();
}

/// @brief 创建描述标签。
QLabel* DataPanelWidget::create_label(const QString& text, const QString& style)
{
    QLabel* label = new QLabel(text, this);
    label->setStyleSheet(style.isEmpty() ? label_style_ : style);
    return label;
}

/// @brief 创建数值标签。
QLabel* DataPanelWidget::create_value_label(const QString& text, const QString& color)
{
    QLabel* label = new QLabel(text, this);
    QString style = value_style_;
    style.replace("#333333", color);
    label->setStyleSheet(style);
    return label;
}

/// @brief 创建大字体标签。
QLabel* DataPanelWidget::create_large_label(const QString& text, const QString& color)
{
    QLabel* label = new QLabel(text, this);
    QString style = large_font_style_;
    style.replace("#000000", color);
    label->setStyleSheet(style);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

/// @brief 触发状态标签闪烁。
void DataPanelWidget::flash_state_label()
{
    if (!task_state_) return;

    task_state_->setStyleSheet(state_flash_style_);
    QTimer::singleShot(400, this, [this]() {
        if (task_state_) {
            task_state_->setStyleSheet(state_normal_style_);
        }
    });
}

/// @brief 颜色编号到名称的映射。
QString DataPanelWidget::color_code_to_name(int code) const
{
    switch (code) {
        case 1: return QString::fromUtf8("红");
        case 2: return QString::fromUtf8("黄");
        case 3: return QString::fromUtf8("蓝");
        case 4: return QString::fromUtf8("绿");
        case 5: return QString::fromUtf8("黑");
        case 6: return QString::fromUtf8("浅蓝");
        default: return "?";
    }
}

/// @brief 颜色编号到显示颜色的映射。
QString DataPanelWidget::color_code_to_display_color(int code) const
{
    switch (code) {
        case 1: return "#ff0000";
        case 2: return "#ffff00";
        case 3: return "#0080ff";
        case 4: return "#00ff00";
        case 5: return "#ffffff";
        case 6: return "#00ffff";
        default: return "#888888";
    }
}

/// @brief 更新机器人位姿显示。
void DataPanelWidget::update_robot_pose(int x_mm, int y_mm, double theta_deg) noexcept
{
    robot_x_->setText(QString::number(x_mm) + " mm");
    robot_y_->setText(QString::number(y_mm) + " mm");
    robot_theta_->setText(QString::number(theta_deg, 'f', 1) + QString::fromUtf8("°"));
}

/// @brief 更新任务状态。
void DataPanelWidget::update_task_state(const QString& state)
{
    task_state_->setText(state);
    flash_state_label();
}

/// @brief 更新任务进度。
void DataPanelWidget::update_task_progress(int cycle, int total_cycles, int picked, int placed, int total_materials)
{
    task_cycle_->setText(QString::number(cycle) + "/" + QString::number(total_cycles));
    task_materials_->setText(QString::number(picked) + "/" + QString::number(total_materials) +
                             " | " + QString::number(placed) + "/" + QString::number(total_materials));
}

/// @brief 更新任务编码（显示在任务码区域）。
void DataPanelWidget::update_task_code(const QString& code)
{
    full_code_label_->setText(code);
}

/// @brief 设置完整任务码显示。
void DataPanelWidget::set_full_task_code(const QString& full_code,
                                          const QVector<int>& batch1_colors,
                                          const QVector<int>& batch1_positions,
                                          const QVector<int>& batch2_colors,
                                          const QVector<int>& batch2_positions)
{
    // 只显示完整任务码，扫码后一直显示
    full_code_label_->setText(full_code);

    // 重置状态
    batch1_completed_ = {false, false, false};
    batch2_completed_ = {false, false, false};
}

/// @brief 更新批次完成状态。
void DataPanelWidget::update_batch_status(int batch, int step, bool completed)
{
    QVector<bool>* status = (batch == 1) ? &batch1_completed_ : &batch2_completed_;

    if (step >= 1 && step <= 3) {
        status->replace(step - 1, completed);
    }
}