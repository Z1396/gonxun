/// @file courtmapwidget.hpp
/// @brief 赛场地图绘制控件，支持障碍物标记、启停区选择与路径可视化。
///        基于赛场规格(2400×2400mm)绘制场地分区：启停区、原料区、粗加工区、
///        暂存区、二维码板等；支持5×5格子网格、15个可标记障碍物、机器人位姿
///        与路径叠加显示。鼠标/触摸事件用于障碍物标记与启停区选择。

#pragma once

#include "field_constants.hpp"

#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QWidget>

/// @brief 赛场区域结构，描述一个命名矩形区域（如启停区）。
struct CourtZone {
    QString name;       ///< 区域名称（如"启停区1"）
    QRectF rect;        ///< 区域矩形（赛场坐标系，单位mm）
    QColor color;       ///< 区域填充颜色
    bool is_selected;   ///< 是否被选中
};

/// @brief 同心圆结构，用于暂存区和粗加工区圆形标记的绘制。
struct CourtCircle {
    QPointF center;         ///< 圆心坐标（赛场坐标系）
    qreal outer_radius;     ///< 外圆半径(mm)
    qreal inner_radius;     ///< 内圆半径(mm)
    QColor outer_color;     ///< 外圆填充颜色
    QColor inner_color;     ///< 内圆填充颜色
};

/// @brief 障碍物矩形结构，描述一个可标记的障碍物区域。
struct ObstacleRect {
    int id;             ///< 障碍物唯一编号（0-14）
    QRectF rect;        ///< 障碍物矩形区域（赛场坐标系）
    bool is_marked;     ///< 是否被用户标记为障碍
};

/// @brief 5×5格子单元格结构，建立赛场坐标与格子索引的映射。
struct Grid5Cell {
    int id;             ///< 单元格唯一编号（0-24）
    int grid_x;         ///< 格子X坐标（0=左侧，4=右侧）
    int grid_y;         ///< 格子Y坐标（0=顶部，4=底部）
    QRectF rect;        ///< 单元格矩形区域（赛场坐标系）
};

/// @brief 赛场地图绘制控件，渲染赛场布局并交互处理障碍物标记与启停区选择。
///
/// 坐标系：赛场左上角为原点(0,0)，X向右，Y向下，单位mm。
/// 绘制层次：背景 → 外框 → 5×5网格 → 中心方块 → 中心十字 → 原料区 →
/// 启停区 → 暂存区 → 粗加工区 → 二维码板 → 障碍物 → 路径 → 机器人 → 标注。
/// 交互模式：mark_mode_下点击标记障碍物，start_zone_selectable_下点击选择启停区。
class CourtMapWidget : public QWidget
{
    Q_OBJECT

public:
    /// @brief 构造地图控件，初始化场地数据、障碍物与5×5网格。
    /// @param parent 父控件指针
    explicit CourtMapWidget(QWidget *parent = nullptr) noexcept;
    ~CourtMapWidget() override = default;

    /// @brief 设置障碍物标记模式，开启后光标变为十字。
    /// @param enabled true 开启标记模式
    void set_mark_mode(bool enabled);

    /// @brief 查询是否处于标记模式。
    /// @return true 标记模式已开启
    [[nodiscard]] bool is_mark_mode() const noexcept { return mark_mode_; }

    /// @brief 设置启停区是否可选，开启后启停区显示虚线边框。
    /// @param selectable true 启停区可点击选择
    void set_start_zone_selectable(bool selectable);

    /// @brief 查询启停区是否可选。
    /// @return true 启停区可选模式已开启
    [[nodiscard]] bool is_start_zone_selectable() const noexcept { return start_zone_selectable_; }

    /// @brief 获取已选启停区索引。
    /// @return 启停区索引（0或1），-1表示未选择
    [[nodiscard]] int selected_start_zone() const noexcept { return selected_start_zone_; }

    /// @brief 获取已选启停区名称。
    /// @return 启停区名称字符串，未选择时返回空串
    [[nodiscard]] QString selected_start_zone_name() const;

    /// @brief 获取已标记障碍物数量。
    /// @return 已标记数量
    [[nodiscard]] int marked_count() const;

    /// @brief 清除所有障碍物标记。
    void clear_all_marks();

    /// @brief 设置机器人位置与朝向，同时设为可见并触发重绘。
    /// @param pos 机器人位置（赛场坐标mm）
    /// @param angle 朝向角度(°)，0=右，顺时针增加
    void set_robot_pos(const QPointF &pos, qreal angle = 0.0);

    /// @brief 设置机器人是否可见。
    /// @param visible true 显示机器人
    void set_robot_visible(bool visible) noexcept { robot_visible_ = visible; update(); }

    /// @brief 获取机器人当前位置。
    /// @return 赛场坐标位置(mm)
    [[nodiscard]] QPointF robot_pos() const noexcept { return robot_pos_; }

    /// @brief 获取机器人当前朝向角度。
    /// @return 角度(°)
    [[nodiscard]] qreal robot_angle() const noexcept { return robot_angle_; }

    /// @brief 查询机器人是否可见。
    /// @return true 可见
    [[nodiscard]] bool is_robot_visible() const noexcept { return robot_visible_; }

    /// @brief 设置任务码显示（在地图顶部显示）。
    /// @param task_code 任务码字符串，格式如 "156+123+516+231"
    void set_task_code(const QString& task_code);

    /// @brief 设置地图全局缩放因子。
    /// @param factor 缩放因子，1.0为原始大小，0.5为缩小一半，2.0为放大两倍
    void set_scale_factor(qreal factor) noexcept { scale_factor_ = factor; update(); }

    /// @brief 获取地图全局缩放因子。
    [[nodiscard]] qreal scale_factor() const noexcept { return scale_factor_; }

    /// @brief 设置路径点序列，触发重绘显示路径。
    /// @param points 路径点列表（赛场坐标mm）
    void set_path(const QVector<QPointF> &points);

    /// @brief 清除路径显示。
    void clear_path() noexcept { path_points_.clear(); update(); }

    /// @brief 查询指定格子是否存在障碍物（含固定障碍物与用户标记障碍物）。
    /// @param grid_x 格子X坐标（0-4）
    /// @param grid_y 格子Y坐标（0-4）
    /// @return true 存在障碍物
    [[nodiscard]] bool has_obstacle_in_cell(int grid_x, int grid_y) const;

    /// @brief 获取指定格子的中心坐标（赛场坐标系）。
    /// @param grid_x 格子X坐标（0-4）
    /// @param grid_y 格子Y坐标（0-4）
    /// @return 格子中心点(mm)，未找到时返回(1200,1200)
    [[nodiscard]] QPointF get_cell_center(int grid_x, int grid_y) const;

    /// @brief 将赛场坐标转换为5×5格子单元。
    /// @param x 赛场X坐标(mm)
    /// @param y 赛场Y坐标(mm)
    /// @return 包含该点的Grid5Cell，点不在任何格子内时返回最近格子
    [[nodiscard]] Grid5Cell field_to_grid5(int x, int y) const;

signals:
    /// @brief 障碍物标记状态变更时发射。
    /// @param id 障碍物编号
    /// @param marked true 已标记，false 已取消
    void obstacle_toggled(int id, bool marked);

    /// @brief 启停区被选择时发射。
    /// @param zone_index 启停区索引（0或1）
    /// @param zone_name 启停区名称
    void start_zone_selected(int zone_index, const QString &zone_name);

protected:
    /// @brief 绘制事件，按层次绘制赛场各元素。
    void paintEvent(QPaintEvent *event) override;

    /// @brief 鼠标释放事件，委托给handle_point_selection()。
    void mouseReleaseEvent(QMouseEvent *event) override;

    /// @brief 窗口大小变更事件，触发重绘以更新缩放。
    void resizeEvent(QResizeEvent *event) override;

    /// @brief 通用事件处理，拦截TouchEnd事件用于触摸屏交互。
    bool event(QEvent *event) override;

private:
    /// @brief 初始化赛场区域数据（启停区、暂存区/粗加工区圆圈）。
    void init_map_data();

    /// @brief 初始化15个可标记障碍物矩形。
    void init_obstacles();

    /// @brief 初始化5×5格子网格，建立格子坐标与赛场矩形的映射。
    void init_grid5();

    // ==== 绘制子函数（按层次调用） ====

    void draw_background(QPainter &p);         ///< 绘制白色背景与灰色赛场底色
    void draw_outer_frame(QPainter &p);        ///< 绘制赛场蓝色外框
    void draw_raw_material_area(QPainter &p);  ///< 绘制原料区（圆形托盘+三孔位）
    void draw_start_stop_zones(QPainter &p);   ///< 绘制启停区（含选中/可选/默认三种状态）
    void draw_buffer_area(QPainter &p);        ///< 绘制暂存区（圆圈+标签）
    void draw_rough_process_area(QPainter &p); ///< 绘制粗加工区（圆圈+标签）
    void draw_center_blocks(QPainter &p);      ///< 绘制4个中心方块
    void draw_center_cross(QPainter &p);       ///< 绘制中心十字虚线
    void draw_qr_board(QPainter &p);           ///< 绘制二维码板标记
    void draw_obstacles(QPainter &p);          ///< 绘制障碍物（已标记/标记模式边框）
    void draw_robot(QPainter &p);              ///< 绘制机器人（方体+四轮+方向箭头）
    void draw_path(QPainter &p);               ///< 绘制路径（虚线+起终点+中间点）
    void draw_dimension_marks(QPainter &p);    ///< 绘制赛场尺寸标注

    /// @brief 绘制水平尺寸标注线。
    void draw_h_dimension(QPainter &p, qreal x1, qreal x2, qreal y, const QString &text);

    /// @brief 绘制垂直尺寸标注线。
    void draw_v_dimension(QPainter &p, qreal x, qreal y1, qreal y2, const QString &text);

    /// @brief 绘制同心圆标记（外圆+内圆+中心点）。
    void draw_concentric_circle(QPainter &p, const QPointF &center, qreal outer_r, qreal inner_r,
                                const QColor &outer_color, const QColor &inner_color);

    /// @brief 绘制5×5格子网格线。
    void draw_grid5(QPainter &p);              ///< 绘制5×5格子网格线
    void draw_task_code(QPainter &p);          ///< 绘制任务码显示

    // ==== 坐标转换与命中测试 ====

    /// @brief 赛场坐标 → 控件坐标。
    [[nodiscard]] QPointF map_to_widget(const QPointF &map_point) const noexcept;

    /// @brief 控件坐标 → 赛场坐标。
    [[nodiscard]] QPointF widget_to_map(const QPointF &widget_point) const noexcept;

    /// @brief 查找指定控件坐标处的障碍物索引。
    /// @return 障碍物索引，-1表示未命中
    [[nodiscard]] int find_obstacle_at(const QPointF &point) const;

    /// @brief 查找指定控件坐标处的启停区索引。
    /// @return 启停区索引（0或1），-1表示未命中
    [[nodiscard]] int find_start_zone_at(const QPointF &point) const;

    /// @brief 处理点击选择：优先选择启停区，其次标记障碍物。
    void handle_point_selection(const QPointF &pos);

    static constexpr qreal MAP_SIZE = gonxun::FIELD_SIZE_MM; ///< 赛场边长(mm)
    static constexpr qreal MARGIN = 0.0;                     ///< 绘制边距(px)
    static constexpr qreal MARGIN_TOP = 60.0;                ///< 顶部边距，给任务码留空间(px)

    QRectF map_rect_;               ///< 赛场在控件中的绘制矩形
    qreal scale_ = 1.0;             ///< 赛场坐标到控件坐标的缩放因子
    qreal scale_factor_ = 0.6;      ///< 地图全局缩放因子（用户可调）

    QVector<CourtZone> zones_;      ///< 启停区列表
    QVector<CourtCircle> buffer_circles_;    ///< 暂存区圆圈列表
    QVector<CourtCircle> process_circles_;   ///< 粗加工区圆圈列表
    QVector<ObstacleRect> obstacles_;        ///< 障碍物列表（15个）
    QVector<Grid5Cell> grid5_cells_;         ///< 5×5格子单元列表（25个）

    bool mark_mode_ = false;                ///< 是否处于障碍物标记模式
    bool start_zone_selectable_ = false;    ///< 启停区是否可选
    int selected_start_zone_ = -1;          ///< 已选启停区索引（-1=未选）

    QPointF robot_pos_{2250, 150};          ///< 机器人位置(mm)
    qreal robot_angle_ = 180.0;             ///< 机器人朝向角度(°)
    bool robot_visible_ = false;            ///< 机器人是否可见

    QString task_code_;                     ///< 任务码（如 "156+123+516+231"）

    QVector<QPointF> path_points_;          ///< 路径点列表(mm)
};
