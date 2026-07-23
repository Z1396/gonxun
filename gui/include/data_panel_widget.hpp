/// @file data_panel_widget.hpp
/// @brief 右侧数据面板控件，显示机器人实时坐标、朝向角度与任务执行信息。
///        面板分为"任务码显示装置"、"机器人坐标"与"任务信息"三个分组框，
///        支持任务状态变更时的闪烁动画提示。
///        任务码显示装置符合比赛规则：字体高度≥12mm，亮光显示，不被遮挡。

#pragma once

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPropertyAnimation>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <QVector>

/// @brief 右侧数据面板控件，实时展示机器人位姿与任务进度信息。
///
/// 布局为垂直排列的三个QGroupBox：
/// - 任务码显示装置组：完整任务码 + 两批次颜色/位置 + 完成状态（字体≥12mm）
/// - 机器人坐标组：X/Y坐标(mm) + 朝向角度(°)
/// - 任务信息组：任务编码 + 当前状态 + 循环进度 + 物料进度
/// 状态标签在更新时自动触发蓝色闪烁动画以吸引用户注意。
class DataPanelWidget : public QWidget
{
    Q_OBJECT

public:
    /// @brief 构造数据面板，初始化样式与UI布局。
    /// @param parent 父控件指针
    explicit DataPanelWidget(QWidget *parent = nullptr) noexcept;
    ~DataPanelWidget() override = default;

public slots:
    /// @brief 更新机器人位姿显示。
    /// @param x_mm X轴坐标(mm)
    /// @param y_mm Y轴坐标(mm)
    /// @param theta_deg 朝向角度(°)，0=右，90=下，逆时针为正
    void update_robot_pose(int x_mm, int y_mm, double theta_deg) noexcept;

    /// @brief 更新任务状态显示，同时触发状态标签闪烁动画。
    /// @param state 状态描述文本（如"仿真开始"、"前往扫码区"）
    void update_task_state(const QString& state);

    /// @brief 更新任务循环与物料进度显示。
    /// @param cycle 当前循环编号（从1开始）
    /// @param total_cycles 总循环数
    /// @param picked 已抓取物料数
    /// @param placed 已放置物料数
    /// @param total_materials 总物料数
    void update_task_progress(int cycle, int total_cycles, int picked, int placed, int total_materials);

    /// @brief 更新任务编码显示。
    /// @param code 任务编码字符串（如"312"）
    void update_task_code(const QString& code);

    /// @brief 设置完整任务码显示装置内容。
    /// @param full_code 完整任务码（如"156+123+516+231"）
    /// @param batch1_colors 第一批颜色编号 (如 {1, 5, 6} 表示红、黑、浅蓝)
    /// @param batch1_positions 第一批位置编号
    /// @param batch2_colors 第二批颜色编号
    /// @param batch2_positions 第二批位置编号
    void set_full_task_code(const QString& full_code,
                            const QVector<int>& batch1_colors,
                            const QVector<int>& batch1_positions,
                            const QVector<int>& batch2_colors,
                            const QVector<int>& batch2_positions);

    /// @brief 更新批次完成状态。
    /// @param batch 批次号 (1 或 2)
    /// @param step 步骤号 (1, 2, 3)
    /// @param completed 是否完成
    void update_batch_status(int batch, int step, bool completed);

private:
    /// @brief 构建完整UI布局，包含任务码显示装置、机器人坐标组与任务信息组。
    void setup_ui();

    /// @brief 初始化所有样式表字符串。
    void setup_styles();

    /// @brief 创建带样式的描述标签。
    /// @param text 标签文本
    /// @param style 自定义样式，为空时使用默认label_style_
    /// @return 新创建的QLabel指针
    QLabel* create_label(const QString& text, const QString& style = "");

    /// @brief 创建带颜色值样式标签，用于显示数值。
    /// @param text 初始文本，默认"--"
    /// @param color 文本颜色，默认"#2c3e50"（深灰蓝）
    /// @return 新创建的QLabel指针
    QLabel* create_value_label(const QString& text = "--", const QString& color = "#2c3e50");

    /// @brief 创建大字体标签（符合比赛规则≥12mm）。
    /// @param text 标签文本
    /// @param color 文本颜色
    /// @return 新创建的QLabel指针
    QLabel* create_large_label(const QString& text, const QString& color = "#00ff00");

    /// @brief 触发任务状态标签闪烁动画：先设为高亮样式，400ms后恢复常态。
    void flash_state_label();

    /// @brief 颜色编号到名称的映射。
    QString color_code_to_name(int code) const;

    /// @brief 颜色编号到显示颜色的映射。
    QString color_code_to_display_color(int code) const;

private:
    QPropertyAnimation* state_flash_anim_ = nullptr; ///< 状态闪烁动画（保留扩展用）

    // 任务码显示装置组件
    QGroupBox* display_group_ = nullptr;        ///< 任务码显示装置分组框
    QLabel* full_code_label_ = nullptr;         ///< 完整任务码标签
    QLabel* batch1_label_ = nullptr;            ///< 第一批次标题
    QLabel* batch1_colors_label_ = nullptr;     ///< 第一批颜色标签
    QLabel* batch1_status_label_ = nullptr;     ///< 第一批完成状态
    QLabel* batch2_label_ = nullptr;            ///< 第二批次标题
    QLabel* batch2_colors_label_ = nullptr;     ///< 第二批颜色标签
    QLabel* batch2_status_label_ = nullptr;     ///< 第二批完成状态

    QVector<bool> batch1_completed_{false, false, false}; ///< 第一批完成状态
    QVector<bool> batch2_completed_{false, false, false}; ///< 第二批完成状态

    QGroupBox* robot_group_ = nullptr;    ///< 机器人坐标分组框
    QLabel* robot_x_ = nullptr;           ///< X轴坐标值标签
    QLabel* robot_y_ = nullptr;           ///< Y轴坐标值标签
    QLabel* robot_theta_ = nullptr;       ///< 朝向角度值标签

    QGroupBox* task_group_ = nullptr;     ///< 任务信息分组框
    QLabel* task_state_ = nullptr;        ///< 当前状态值标签
    QLabel* task_code_ = nullptr;         ///< 任务编码值标签
    QLabel* task_cycle_ = nullptr;        ///< 循环进度值标签
    QLabel* task_materials_ = nullptr;    ///< 物料进度值标签

    QString label_style_;          ///< 描述标签样式表
    QString value_style_;          ///< 数值标签样式表
    QString group_style_;          ///< 分组框样式表
    QString highlight_style_;      ///< 高亮数值样式表
    QString state_normal_style_;   ///< 状态标签常态样式表
    QString state_flash_style_;    ///< 状态标签闪烁样式表
    QString display_group_style_;  ///< 任务码显示装置样式表（黑底亮字）
    QString large_font_style_;     ///< 大字体样式表（≥12mm）
};
