/// @file courtmapwidget.cpp
/// @brief 赛场地图控件实现，包含场地数据初始化、多层次绘制、坐标转换与交互处理。
///        场地布局为2400×2400mm的5×5格子图，包含启停区(2个)、15个可标记障碍物、
///        原料区(圆形托盘)、暂存区(左侧3圆)、粗加工区(底部3圆)、二维码板等。

#include "courtmapwidget.hpp"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTouchEvent>

/// @brief 构造地图控件：启用触摸事件与鼠标追踪，初始化场地数据。
CourtMapWidget::CourtMapWidget(QWidget *parent) noexcept
    : QWidget(parent)
{
    // 启用触摸事件与鼠标追踪
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    // 启用鼠标追踪
    setMouseTracking(true);
    // 设置控件最小尺寸
    setMinimumSize(500, 500);
    // 初始化地图数据
    init_map_data();
    // 初始化障碍物数据
    init_obstacles();
    // 初始化5×5格子网格
    init_grid5();
}

/// @brief 初始化赛场区域数据：2个启停区、3个暂存区圆圈、3个粗加工区圆圈。
void CourtMapWidget::init_map_data()
{
    zones_ = {
        {"启停区1", QRectF(2100, 0, 300, 300), QColor(0, 50, 200), false},
        {"启停区2", QRectF(2100, 2100, 300, 300), QColor(0, 50, 200), false}
    };

    // 暂存区3个同心圆标记（左侧垂直排列）
    buffer_circles_.clear();
    for (int i = 0; i < 3; ++i) {
        buffer_circles_.append({
            QPointF(75, 1050 + i * 150), 25, 20,
            QColor(20, 20, 20), QColor(255, 255, 255)
        });
    }

    // 粗加工区3个同心圆标记（底部水平排列）
    process_circles_.clear();
    for (int i = 0; i < 3; ++i) {
        process_circles_.append({
            QPointF(1050 + i * 150, 2325), 25, 20,
            QColor(20, 20, 20), QColor(255, 255, 255)
        });
    }
}

/// @brief 初始化15个可标记障碍物矩形，按5×3网格排列覆盖赛场各区域。
void CourtMapWidget::init_obstacles()
{
    QRectF rects[] = {
        { 0, 0, 550, 550},     { 550,  0, 450, 550},                           {1400,  0, 450, 550},
        { 0,  550, 550, 360},                          {1000,  550, 400, 450},                        {1850,  550, 550, 450},
                              { 550, 1000, 450, 400}, {1000, 1000, 400, 400}, {1400, 1000, 450, 400},
        { 0, 1490, 550, 360},                         {1000, 1400, 400, 450},                        {1850, 1400, 550, 450},
        { 0, 1850, 550, 550}, { 550, 1850, 360, 550},                         {1490, 1850, 360, 550}
    };

    obstacles_.clear();
    for (int i = 0; i < 15; ++i) {
        obstacles_.append({i, rects[i], false});
    }
}

/// @brief 初始化5×5格子网格，25个单元格覆盖整个赛场。
///        每个格子记录其赛场矩形区域与逻辑坐标(grid_x, grid_y)。
///        注意：格子Y轴方向与赛场Y轴一致（0=顶部，4=底部）。
void CourtMapWidget::init_grid5()
{
    grid5_cells_.clear();

    // 第0行（Y=0，顶部）
    grid5_cells_.append({0, 4, 0, QRectF(0, 0, 550, 550)});
    grid5_cells_.append({1, 3, 0, QRectF(550, 0, 450, 550)});
    grid5_cells_.append({2, 2, 0, QRectF(1000, 0, 400, 550)});
    grid5_cells_.append({3, 1, 0, QRectF(1400, 0, 450, 550)});
    grid5_cells_.append({4, 0, 0, QRectF(1850, 0, 550, 550)});

    // 第1行（Y=1）
    grid5_cells_.append({5, 4, 1, QRectF(0, 550, 550, 360)});
    grid5_cells_.append({6, 3, 1, QRectF(550, 550, 450, 450)});
    grid5_cells_.append({7, 2, 1, QRectF(1000, 550, 400, 450)});
    grid5_cells_.append({8, 1, 1, QRectF(1400, 550, 450, 450)});
    grid5_cells_.append({9, 0, 1, QRectF(1850, 550, 550, 450)});
    // 第2行（Y=2，中间）
    grid5_cells_.append({10, 4, 2, QRectF(0, 910, 550, 580)});
    grid5_cells_.append({11, 3, 2, QRectF(550, 1000, 450, 400)});
    grid5_cells_.append({12, 2, 2, QRectF(1000, 1000, 400, 400)});
    grid5_cells_.append({13, 1, 2, QRectF(1400, 1000, 450, 400)});
    grid5_cells_.append({14, 0, 2, QRectF(1850, 960, 550, 440)});

    // 第3行（Y=3）
    grid5_cells_.append({15, 4, 3, QRectF(0, 1490, 550, 360)});
    grid5_cells_.append({16, 3, 3, QRectF(550, 1400, 450, 450)});
    grid5_cells_.append({17, 2, 3, QRectF(1000, 1400, 400, 450)});
    grid5_cells_.append({18, 1, 3, QRectF(1400, 1400, 450, 450)});
    grid5_cells_.append({19, 0, 3, QRectF(1850, 1400, 550, 450)});
    // 第4行（Y=4，底部）
    grid5_cells_.append({20, 4, 4, QRectF(0, 1850, 550, 550)});
    grid5_cells_.append({21, 3, 4, QRectF(550, 1850, 360, 550)});
    grid5_cells_.append({22, 2, 4, QRectF(910, 1850, 580, 550)});
    grid5_cells_.append({23, 1, 4, QRectF(1490, 1850, 360, 550)});
    grid5_cells_.append({24, 0, 4, QRectF(1850, 1850, 550, 550)});
}

/// @brief 设置标记模式：开启时光标变为十字，关闭时恢复箭头。
void CourtMapWidget::set_mark_mode(bool enabled)
{
    // 更新标记模式状态
    mark_mode_ = enabled;
    // 更新鼠标光标
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);  
    update();
}

/// @brief 设置启停区可选模式，开启后启停区显示虚线边框提示。
void CourtMapWidget::set_start_zone_selectable(bool selectable)
{
    start_zone_selectable_ = selectable;
    update();
}

/// @brief 获取已选启停区名称，索引越界时返回空串。
QString CourtMapWidget::selected_start_zone_name() const
{
    return (selected_start_zone_ >= 0 && selected_start_zone_ < zones_.size())
           ? zones_[selected_start_zone_].name : QString();
}

/// @brief 统计已标记障碍物数量。
int CourtMapWidget::marked_count() const
{
    int count = 0;
    for (const auto &obs : obstacles_) {
        if (obs.is_marked) ++count;
    }
    return count;
}

/// @brief 清除所有障碍物标记状态。
void CourtMapWidget::clear_all_marks()
{
    for (auto &obs : obstacles_) obs.is_marked = false;
    update();
}

/// @brief 绘制事件：计算缩放因子与赛场绘制矩形，按层次绘制各元素。
void CourtMapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // 计算缩放：保持赛场正方形等比缩放，顶部留空间给任务码
    int w = width(), h = height();
    scale_ = qMin((w - 2*MARGIN) / MAP_SIZE, (h - MARGIN_TOP - MARGIN) / MAP_SIZE) * scale_factor_;
    qreal draw_w = MAP_SIZE * scale_, draw_h = MAP_SIZE * scale_;
    // 地图水平居中，垂直方向顶部留出 MARGIN_TOP 空间
    map_rect_ = QRectF((w - draw_w) / 2, MARGIN_TOP, draw_w, draw_h);

    // 按层次绘制各赛场元素
    draw_background(p);
    draw_outer_frame(p);
    draw_grid5(p);
    draw_center_blocks(p);
    draw_center_cross(p);
    draw_raw_material_area(p);
    draw_start_stop_zones(p);
    draw_buffer_area(p);
    draw_rough_process_area(p);
    draw_qr_board(p);
    draw_obstacles(p);
    draw_path(p);
    draw_robot(p);
    draw_dimension_marks(p);
    draw_task_code(p);
}

/// @brief 绘制白色控件背景与灰色赛场底色。
void CourtMapWidget::draw_background(QPainter &p)
{
    p.fillRect(rect(), Qt::white);
    p.fillRect(map_rect_, QColor(230, 230, 225));
}

/// @brief 绘制赛场蓝色粗外框。
void CourtMapWidget::draw_outer_frame(QPainter &p)
{
    p.setPen(QPen(QColor(0, 50, 200), 4));
    p.setBrush(Qt::NoBrush);
    p.drawRect(map_rect_);
}

/// @brief 绘制原料区：圆形托盘（渐变填充）+ 3个等距圆孔位 + 区域标签。
void CourtMapWidget::draw_raw_material_area(QPainter &p)
{
    QPointF center = map_to_widget(QPointF(1200, -70));
    qreal tray_r = 150 * scale_;

    // 渐变填充托盘
    QRadialGradient grad(center, tray_r);
    grad.setColorAt(0, QColor(255, 255, 255));
    grad.setColorAt(0.85, QColor(240, 240, 235));
    grad.setColorAt(1, QColor(180, 180, 170));

    p.setBrush(grad);
    p.setPen(QPen(QColor(100, 100, 90), 2));
    p.drawEllipse(center, tray_r, tray_r);

    // 3个等距圆孔位（120°间隔）
    qreal hole_dist = 100 * scale_;
    qreal hole_r = tray_r * 0.18;
    p.setBrush(QColor(60, 60, 55));
    p.setPen(Qt::NoPen);
    p.drawEllipse(center + QPointF(0, hole_dist), hole_r, hole_r);
    p.drawEllipse(center + QPointF(-hole_dist * 0.866, -hole_dist * 0.5), hole_r, hole_r);
    p.drawEllipse(center + QPointF(hole_dist * 0.866, -hole_dist * 0.5), hole_r, hole_r);

    // 区域标签
    p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    p.setPen(QColor(40, 40, 40));
    p.drawText(QRectF(map_rect_.x()-40, map_rect_.y()-22, map_rect_.width(), 20),
               Qt::AlignHCenter | Qt::AlignTop, "原料区");
}

/// @brief 绘制启停区：三种状态——选中（绿色+星号）、可选（虚线+提示）、默认（蓝色填充）。
void CourtMapWidget::draw_start_stop_zones(QPainter &p)
{
    p.setFont(QFont("Microsoft YaHei", 9, QFont::Bold));

    for (int i = 0; i < zones_.size(); ++i) {
        const auto &zone = zones_[i];
        QRectF r(map_to_widget(zone.rect.topLeft()), map_to_widget(zone.rect.bottomRight()));

        bool is_selected = (selected_start_zone_ == i);

        if (is_selected) {
            // 选中状态：绿色填充+白色粗边框+内框+星号标签+指示圆
            p.setBrush(QColor(0, 180, 80));
            p.setPen(QPen(Qt::white, 4));
            p.drawRect(r);
            p.setPen(QPen(QColor(0, 100, 50), 3));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r.adjusted(5, 5, -5, -5));

            p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, zone.name + "\n★已选中");

            p.setBrush(QColor(50, 220, 100));
            p.setPen(QPen(Qt::white, 2));
            p.drawEllipse(r.center() + QPointF(0, -r.height() * 0.35), 12, 12);
        }
        else if (start_zone_selectable_) {
            // 可选状态：半透明蓝+绿色虚线边框+提示文字
            p.setBrush(QColor(0, 50, 200, 150));
            p.setPen(QPen(QColor(0, 200, 100), 3, Qt::DashLine));
            p.drawRect(r);
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, zone.name + "\n点击选择");
        }
        else {
            // 默认状态：蓝色填充+深蓝细边框
            p.setBrush(zone.color);
            p.setPen(QPen(QColor(0, 30, 150), 2));
            p.drawRect(r);
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, zone.name);
        }
    }
}

/// @brief 绘制暂存区：灰色背景矩形+3个同心圆标记+竖排标签。
void CourtMapWidget::draw_buffer_area(QPainter &p)
{
    QRectF buffer_rect = QRectF(map_to_widget(QPointF(0, 910)),
                                map_to_widget(QPointF(150, 1490)));
    p.setBrush(QColor(220, 220, 215));
    p.setPen(QPen(QColor(150, 150, 145), 2));
    p.drawRect(buffer_rect);

    for (const auto &c : buffer_circles_) {
        QPointF center = map_to_widget(c.center);
        draw_concentric_circle(p, center, c.outer_radius * scale_, c.inner_radius * scale_,
                             c.outer_color, c.inner_color);
    }

    // 竖排标签（旋转-90°绘制）
    p.save();
    p.translate(map_to_widget(QPointF(70, 850)));
    p.rotate(-90);
    p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    p.setPen(QColor(40, 40, 40));
    p.drawText(QRectF(-100, -15, 200, 30), Qt::AlignCenter, "暂存区");
    p.restore();
}

/// @brief 绘制粗加工区：灰色背景矩形+3个同心圆标记+横排标签。
void CourtMapWidget::draw_rough_process_area(QPainter &p)
{
    QRectF process_rect = QRectF(map_to_widget(QPointF(910, 2250)),
                                map_to_widget(QPointF(1490, 2400)));
    p.setBrush(QColor(220, 220, 215));
    p.setPen(QPen(QColor(150, 150, 145), 2));
    p.drawRect(process_rect);

    for (const auto &c : process_circles_) {
        QPointF center = map_to_widget(c.center);
        draw_concentric_circle(p, center, c.outer_radius * scale_, c.inner_radius * scale_,
                             c.outer_color, c.inner_color);
    }

    p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    p.setPen(QColor(40, 40, 40));
    p.drawText(QRectF(map_rect_.x()-60, map_rect_.bottom() - 25,
                    map_rect_.width(), 20),
            Qt::AlignHCenter | Qt::AlignBottom, "粗加工区");
}

/// @brief 绘制4个中心方块（浅黄色），对应格子(1,1)(3,1)(1,3)(3,3)。
void CourtMapWidget::draw_center_blocks(QPainter &p)
{
    p.setBrush(QColor(248, 248, 200));
    p.setPen(QPen(QColor(200, 200, 150), 1.5));

    constexpr qreal blocks[4][4] = {
        { 550,  550, 450, 450},
        {1400,  550, 450, 450},
        { 550, 1400, 450, 450},
        {1400, 1400, 450, 450}
    };

    for (int i = 0; i < 4; ++i) {
        p.drawRect(QRectF(map_to_widget(QPointF(blocks[i][0], blocks[i][1])),
                          map_to_widget(QPointF(blocks[i][0] + blocks[i][2], blocks[i][1] + blocks[i][3]))));
    }
}

/// @brief 绘制中心十字虚线（赛场X/Y中轴）。
void CourtMapWidget::draw_center_cross(QPainter &p)
{
    p.setPen(QPen(QColor(120, 120, 120), 1.5, Qt::DashLine));
    p.drawLine(map_to_widget(QPointF(0, 1200)), map_to_widget(QPointF(2400, 1200)));
    p.drawLine(map_to_widget(QPointF(1200, 0)), map_to_widget(QPointF(1200, 2400)));
}

/// @brief 绘制二维码板：右侧竖线标记+旋转标签。
void CourtMapWidget::draw_qr_board(QPainter &p)
{
    p.setPen(QPen(QColor(80, 80, 80), 2));
    p.drawLine(map_to_widget(QPointF(2360, 1100)), map_to_widget(QPointF(2360, 1300)));

    p.save();
    p.translate(map_to_widget(QPointF(2375, 1200)));
    p.rotate(-90);
    p.setFont(QFont("Microsoft YaHei", 9, QFont::Bold));
    p.setPen(QColor(50, 50, 50));
    p.drawText(QRectF(-100, 15, 200, 30), Qt::AlignCenter, "二维码板");
    p.restore();
}

/// @brief 绘制障碍物：已标记显示红色半透明填充+白色"障碍"文字，
///        标记模式下未标记的显示红色虚线边框。
void CourtMapWidget::draw_obstacles(QPainter &p)
{
    for (const auto &obs : obstacles_) {
        QRectF r(map_to_widget(obs.rect.topLeft()), map_to_widget(obs.rect.bottomRight()));

        if (obs.is_marked) {
            p.setBrush(QColor(220, 30, 30, 180));
            p.setPen(QPen(QColor(180, 20, 20), 4));
            p.drawRect(r);
            p.setFont(QFont("Arial", qMax(8, int(14 * scale_)), QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, "障碍");
        }
        else if (mark_mode_) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(220, 40, 40), 2));
            p.drawRect(r);
        }
    }
}

/// @brief 绘制赛场尺寸标注线（水平/垂直标尺）。
void CourtMapWidget::draw_dimension_marks(QPainter &p)
{
    p.setFont(QFont("Arial", 8));
    p.setPen(QColor(60, 60, 60));

    draw_h_dimension(p, 0, 150, -30, "150");
    draw_h_dimension(p, 1100, 1300, -30, "1100-1300");
    draw_h_dimension(p, 2100, 2400, -30, "300");

    draw_h_dimension(p, 0, 150, MAP_SIZE + 30, "150");
    draw_h_dimension(p, 0, 2400, MAP_SIZE + 60, "2400");

    draw_v_dimension(p, -30, 0, 300, "300");
    draw_v_dimension(p, -30, 350, 930, "580");

    draw_v_dimension(p, 2430, 0, 300, "300");
    draw_v_dimension(p, 2430, 1100, 1300, "1100-1300");
}

/// @brief 绘制水平尺寸标注：水平线+两端竖线+居中文字。
void CourtMapWidget::draw_h_dimension(QPainter &p, qreal x1, qreal x2, qreal y, const QString &text)
{
    QPointF p1 = map_to_widget(QPointF(x1, y)), p2 = map_to_widget(QPointF(x2, y));

    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.drawLine(p1.x(), p1.y(), p2.x(), p2.y());
    p.drawLine(p1.x(), p1.y() - 6, p1.x(), p1.y() + 6);
    p.drawLine(p2.x(), p2.y() - 6, p2.x(), p2.y() + 6);

    if (!text.isEmpty()) {
        p.drawText(QRectF(p1.x(), p1.y() - 12, p2.x() - p1.x(), 16), Qt::AlignCenter, text);
    }
}

/// @brief 绘制垂直尺寸标注：垂直线+两端横线+右侧文字。
void CourtMapWidget::draw_v_dimension(QPainter &p, qreal x, qreal y1, qreal y2, const QString &text)
{
    QPointF p1 = map_to_widget(QPointF(x, y1)), p2 = map_to_widget(QPointF(x, y2));

    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.drawLine(p1.x(), p1.y(), p2.x(), p2.y());
    p.drawLine(p1.x() - 6, p1.y(), p1.x() + 6, p1.y());
    p.drawLine(p2.x() - 6, p2.y(), p2.x() + 6, p2.y());

    if (!text.isEmpty()) {
        p.drawText(QRectF(p1.x() - 35, (p1.y() + p2.y()) / 2 - 8, 30, 16), Qt::AlignRight | Qt::AlignVCenter, text);
    }
}

/// @brief 绘制同心圆标记：外圆→内圆→中心点，用于暂存区/粗加工区圆孔。
void CourtMapWidget::draw_concentric_circle(QPainter &p, const QPointF &center, qreal outer_r, qreal inner_r,
                                           const QColor &outer_color, const QColor &inner_color)
{
    p.setBrush(outer_color);
    p.setPen(QPen(QColor(30, 30, 30), 1));
    p.drawEllipse(center, outer_r, outer_r);

    p.setBrush(inner_color);
    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.drawEllipse(center, inner_r, inner_r);

    p.setBrush(QColor(20, 20, 20));
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, inner_r * 0.3, inner_r * 0.3);
}

/// @brief 赛场坐标→控件坐标：应用赛场矩形偏移与缩放。
QPointF CourtMapWidget::map_to_widget(const QPointF &map_point) const noexcept
{
    return QPointF(map_rect_.x() + map_point.x() * scale_, map_rect_.y() + map_point.y() * scale_);
}

/// @brief 控件坐标→赛场坐标：逆缩放与逆偏移。scale_=0时返回原点。
QPointF CourtMapWidget::widget_to_map(const QPointF &widget_point) const noexcept
{
    return (scale_ > 0) ? QPointF((widget_point.x() - map_rect_.x()) / scale_,
                                    (widget_point.y() - map_rect_.y()) / scale_) : QPointF();
}

/// @brief 查找控件坐标处的障碍物索引，转换为赛场坐标后逐个判定contains。
/// @return 障碍物索引(0-14)，-1表示未命中
int CourtMapWidget::find_obstacle_at(const QPointF &point) const
{
    QPointF mp = widget_to_map(point);
    for (int i = 0; i < obstacles_.size(); ++i) {
        if (obstacles_[i].rect.contains(mp)) return i;
    }
    return -1;
}

/// @brief 查找控件坐标处的启停区索引。
/// @return 启停区索引(0或1)，-1表示未命中
int CourtMapWidget::find_start_zone_at(const QPointF &point) const
{
    QPointF mp = widget_to_map(point);
    for (int i = 0; i < zones_.size(); ++i) {
        if (zones_[i].rect.contains(mp)) return i;
    }
    return -1;
}

/// @brief 处理点击选择：优先处理启停区选择，其次处理障碍物标记切换。
///        选择启停区时同时初始化机器人位置与朝向。
void CourtMapWidget::handle_point_selection(const QPointF &pos)
{
    // 启停区选择优先
    if (start_zone_selectable_ && !mark_mode_) {
        int idx = find_start_zone_at(pos);
        if (idx >= 0) {
            selected_start_zone_ = idx;
            QRectF zone_rect = zones_[idx].rect;
            robot_pos_ = zone_rect.center();
            robot_angle_ = 90.0;  // 90°=向下
            robot_visible_ = true;
            emit start_zone_selected(idx, zones_[idx].name);
            update();
            return;
        }
    }

    // 障碍物标记切换
    if (mark_mode_) {
        int idx = find_obstacle_at(pos);
        if (idx >= 0) {
            obstacles_[idx].is_marked = !obstacles_[idx].is_marked;
            emit obstacle_toggled(idx, obstacles_[idx].is_marked);
            update();
        }
    }
}

/// @brief 鼠标释放事件：委托给handle_point_selection()处理。
void CourtMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    handle_point_selection(event->pos());
    QWidget::mouseReleaseEvent(event);
}

/// @brief 窗口大小变更事件：触发重绘以更新缩放与布局。
void CourtMapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

/// @brief 通用事件拦截：处理TouchEnd事件以支持触摸屏交互。
bool CourtMapWidget::event(QEvent *event)
{
    if (event->type() == QEvent::TouchEnd) {
        QTouchEvent *te = static_cast<QTouchEvent*>(event);
        if (te && !te->touchPoints().isEmpty()) {
            handle_point_selection(te->touchPoints().first().pos());
        }
        return true;
    }
    return QWidget::event(event);
}

/// @brief 设置机器人位置与朝向，自动设为可见并触发重绘。
void CourtMapWidget::set_robot_pos(const QPointF &pos, qreal angle)
{
    robot_pos_ = pos;
    robot_angle_ = angle;
    robot_visible_ = true;
    update();
}

/// @brief 设置路径点序列并触发重绘。
void CourtMapWidget::set_path(const QVector<QPointF> &points)
{
    path_points_ = points;
    update();
}

/// @brief 绘制机器人：黄色圆角方体+四轮+红色方向箭头+中心点+标签。
///        机器人以当前位置为中心旋转绘制，标签始终正向显示。
void CourtMapWidget::draw_robot(QPainter &p)
{
    if (!robot_visible_) return;

    p.save();

    QPointF center = map_to_widget(robot_pos_);
    p.translate(center);
    p.rotate(-robot_angle_);  // 逆时针：0=左, 90=下, 180=右, 270=上

    // 机器人方体
    qreal body_w = 300 * scale_;
    qreal body_h = 300 * scale_;
    qreal wheel_w = 40 * scale_;
    qreal wheel_h = 80 * scale_;

    QRectF body_rect(-body_w/2, -body_h/2, body_w, body_h);
    p.setPen(QPen(QColor(50, 50, 50), 2));
    p.setBrush(QColor(241, 196, 15));
    p.drawRoundedRect(body_rect, 8, 8);

    // 四轮
    p.setBrush(QColor(30, 30, 30));
    p.drawRoundedRect(QRectF(-body_w/2 - wheel_w/2, -body_h/2 + 10*scale_, wheel_w, wheel_h), 3, 3);
    p.drawRoundedRect(QRectF(body_w/2 - wheel_w/2, -body_h/2 + 10*scale_, wheel_w, wheel_h), 3, 3);
    p.drawRoundedRect(QRectF(-body_w/2 - wheel_w/2, body_h/2 - 10*scale_ - wheel_h, wheel_w, wheel_h), 3, 3);
    p.drawRoundedRect(QRectF(body_w/2 - wheel_w/2, body_h/2 - 10*scale_ - wheel_h, wheel_w, wheel_h), 3, 3);

    // 方向箭头（默认指向左，rotate 后：0°=左, 90°=下, 180°=右, 270°=上）
    QPainterPath arrow;
    qreal arrow_size = body_w * 0.25;
    arrow.moveTo(-body_w/2 + 15*scale_, 0);
    arrow.lineTo(-body_w/2 + 15*scale_ + arrow_size, -arrow_size/2);
    arrow.lineTo(-body_w/2 + 15*scale_ + arrow_size, arrow_size/2);
    arrow.closeSubpath();
    p.setBrush(QColor(231, 76, 60));
    p.setPen(QPen(QColor(192, 57, 43), 1));
    p.drawPath(arrow);

    // 中心点
    p.setBrush(QColor(50, 50, 50));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(0, 0), 5, 5);

    // "机器人"标签（反向旋转以保持正向）
    p.rotate(robot_angle_);
    p.setPen(QColor(50, 50, 50));
    QFont font("Microsoft YaHei", 8);
    font.setBold(true);
    p.setFont(font);
    p.drawText(QRectF(-40, body_h/2 + 5*scale_, 80, 20), Qt::AlignCenter, "机器人");

    p.restore();
}

/// @brief 绘制路径：蓝色虚线连线+绿色起点圆+红色终点圆+蓝色中间点。
void CourtMapWidget::draw_path(QPainter &p)
{
    if (path_points_.size() < 2) return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // 虚线路径线
    QPen path_pen(QColor(41, 128, 185), 2, Qt::DashLine);
    p.setPen(path_pen);

    for (int i = 0; i < path_points_.size() - 1; ++i) {
        QPointF p1 = map_to_widget(path_points_[i]);
        QPointF p2 = map_to_widget(path_points_[i + 1]);
        p.drawLine(p1, p2);
    }

    // 起点标记（绿色）
    QPointF start_pt = map_to_widget(path_points_.first());
    p.setBrush(QColor(39, 174, 96));
    p.setPen(QPen(QColor(27, 120, 65), 2));
    p.drawEllipse(start_pt, 6, 6);

    // 终点标记（红色）
    QPointF end_pt = map_to_widget(path_points_.last());
    p.setBrush(QColor(231, 76, 60));
    p.setPen(QPen(QColor(192, 57, 43), 2));
    p.drawEllipse(end_pt, 6, 6);

    // 中间点标记（蓝色小圆）
    p.setBrush(QColor(52, 152, 219));
    p.setPen(Qt::NoPen);
    for (int i = 1; i < path_points_.size() - 1; ++i) {
        QPointF pt = map_to_widget(path_points_[i]);
        p.drawEllipse(pt, 3, 3);
    }

    p.restore();
}

/// @brief 查询指定格子是否存在障碍物。
///        固定障碍物：中心黄色方块(1,1)(3,1)(1,3)(3,3)和启停区(0,0)(0,4)。
///        用户标记障碍物：通过obstacle_to_grid映射表查询。
/// @param grid_x 格子X坐标(0-4)
/// @param grid_y 格子Y坐标(0-4)
/// @return true 存在障碍物
bool CourtMapWidget::has_obstacle_in_cell(int grid_x, int grid_y) const
{
    // 黄色固定障碍物：(3,1), (1,1), (3,3), (1,3)
    if ((grid_x == 3 && grid_y == 1) || (grid_x == 1 && grid_y == 1) ||
        (grid_x == 3 && grid_y == 3) || (grid_x == 1 && grid_y == 3)) {
        return true;
    }

    // 启停区：(0,0), (0,4)
    if (grid_x == 0 && (grid_y == 0 || grid_y == 4)) {
        return true;
    }

    // 用户标记障碍物：15个障碍物矩形到格子坐标的映射表
    static const int obstacle_to_grid[15][2] = {
        {4,0}, {3,0}, {1,0},
        {4,1}, {2,1}, {0,1},
        {3,2}, {2,2}, {1,2},
        {4,3}, {2,3}, {0,3},
        {4,4}, {3,4}, {1,4}
    };

    for (int i = 0; i < 15; ++i) {
        if (obstacles_[i].is_marked &&
            obstacle_to_grid[i][0] == grid_x &&
            obstacle_to_grid[i][1] == grid_y) {
            return true;
        }
    }

    return false;
}

/// @brief 获取指定格子的中心坐标（赛场坐标系）。
/// @param grid_x 格子X坐标(0-4)
/// @param grid_y 格子Y坐标(0-4)
/// @return 格子矩形中心点(mm)，未找到时返回(1200,1200)即赛场中心
QPointF CourtMapWidget::get_cell_center(int grid_x, int grid_y) const
{
    for (const auto& cell : grid5_cells_) {
        if (cell.grid_x == grid_x && cell.grid_y == grid_y) {
            return cell.rect.center();
        }
    }
    return QPointF(1200, 1200);
}

/// @brief 将赛场坐标转换为5×5格子单元。
///        先精确查找包含该点的格子，若不在任何格子内则返回距离中心最近的格子。
/// @param x 赛场X坐标(mm)
/// @param y 赛场Y坐标(mm)
/// @return 匹配的Grid5Cell
Grid5Cell CourtMapWidget::field_to_grid5(int x, int y) const
{
    // 精确查找：点在格子矩形内
    for (const auto& cell : grid5_cells_) {
        if (cell.rect.contains(x, y)) {
            return cell;
        }
    }

    // 模糊查找：返回距离最近的格子（按中心点距离平方）
    Grid5Cell nearest = grid5_cells_[12];
    qreal min_dist = 1e10;

    for (const auto& cell : grid5_cells_) {
        QPointF center = cell.rect.center();
        qreal dx = x - center.x();
        qreal dy = y - center.y();
        qreal dist = dx * dx + dy * dy;

        if (dist < min_dist) {
            min_dist = dist;
            nearest = cell;
        }
    }

    return nearest;
}

/// @brief 绘制5×5格子网格线：每个格子绘制右边界和下边界，
///        最左列额外绘制左边界，最顶行额外绘制上边界。
void CourtMapWidget::draw_grid5(QPainter &p)
{
    p.save();

    QPen grid_pen(QColor(80, 80, 80, 200), 2, Qt::SolidLine);
    p.setPen(grid_pen);

    for (const auto &cell : grid5_cells_) {
        QRectF widget_rect = QRectF(
            map_to_widget(cell.rect.topLeft()),
            map_to_widget(cell.rect.bottomRight())
        );

        // 最左列格子绘制左边界
        if (cell.grid_x == 4) {
            p.drawLine(widget_rect.topLeft(), widget_rect.bottomLeft());
        }
        // 最顶行格子绘制上边界
        if (cell.grid_y == 0) {
            p.drawLine(widget_rect.topLeft(), widget_rect.topRight());
        }

        // 每个格子绘制右边界和下边界
        p.drawLine(widget_rect.topRight(), widget_rect.bottomRight());
        p.drawLine(widget_rect.bottomLeft(), widget_rect.bottomRight());
    }

    p.restore();
}

/// @brief 设置任务码，触发重绘。
void CourtMapWidget::set_task_code(const QString& task_code)
{
    task_code_ = task_code;
    update();
}

/// @brief 绘制任务码显示（地图顶部居中）。
///        字体高度≥12mm（约36pt），白底黑字。
void CourtMapWidget::draw_task_code(QPainter& p)
{
    if (task_code_.isEmpty()) return;

    // 字体设置：40pt粗体（字体高度约14mm，满足≥12mm要求）
    QFont font("Microsoft YaHei", 42, QFont::Bold);
    p.setFont(font);

    // 计算文本位置：控件顶部居中
    QFontMetrics fm(font);
    QRect text_rect = fm.boundingRect(task_code_);
    int text_width = text_rect.width();
    int text_height = text_rect.height();

    // 位置：控件顶部居中，留少量边距
    int x = static_cast<int>(((width() - text_width) / 2) - 0);
    int y = -30;  // 顶部边距5px

    // 绘制白色背景
    QRect bg_rect(x - 10, y - 5, text_width + 20, text_height + 10);
    p.fillRect(bg_rect, Qt::white);

    // 绘制黑色文本
    p.setPen(Qt::black);
    p.drawText(x, y + text_height - 5, task_code_);
}
