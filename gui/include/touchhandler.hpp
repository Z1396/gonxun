/// @file touchhandler.hpp
/// @brief 鼠标/触摸屏手势统一识别处理类。
///        支持单击、双击、长按、四方向滑动、捏合缩放与旋转手势。
///        通过定时器判定长按与双击，通过距离阈值区分点击与滑动，
///        通过双指距离/角度变化检测捏合与旋转。

#pragma once

#include <QList>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QTimer>
#include <QTouchEvent>

/// @brief 鼠标/触摸屏手势统一识别处理器。
///
/// 将 QTouchEvent 和 QMouseEvent 统一抽象为内部 TouchPoint 列表，
/// 根据移动距离、时间间隔、双指几何关系识别以下手势：
/// - Tap / DoubleTap / LongPress（单指）
/// - SwipeLeft/Right/Up/Down（单指滑动）
/// - PinchIn/PinchOut / Rotate（双指）
/// 手势阈值可通过setter配置，适用于不同屏幕尺寸。
class TouchHandler : public QObject
{
    Q_OBJECT

public:
    /// @brief 手势类型枚举，覆盖单指与双指手势。
    enum GestureType {
        NoGesture,      ///< 无手势
        Tap,            ///< 单击
        DoubleTap,      ///< 双击
        LongPress,      ///< 长按
        SwipeLeft,      ///< 左滑
        SwipeRight,     ///< 右滑
        SwipeUp,        ///< 上滑
        SwipeDown,      ///< 下滑
        PinchIn,        ///< 捏合缩小
        PinchOut,       ///< 张开放大
        Rotate          ///< 双指旋转
    };
    Q_ENUM(GestureType)

    /// @brief 构造手势处理器，初始化定时器与默认阈值。
    /// @param parent 父对象
    explicit TouchHandler(QObject *parent = nullptr) noexcept;
    ~TouchHandler() override;

    // ==== 可配置手势阈值 ====

    /// @brief 设置点击判定半径，移动距离小于此值视为点击。
    /// @param radius 像素半径，默认20
    void set_tap_radius(int radius) noexcept { tap_radius_ = radius; }

    /// @brief 设置点击延迟（当前未直接使用，保留扩展）。
    /// @param ms 毫秒数，默认200
    void set_tap_delay(int ms) noexcept { tap_delay_ = ms; }

    /// @brief 设置长按触发时长。
    /// @param ms 毫秒数，默认800
    void set_long_press_duration(int ms) noexcept { long_press_duration_ = ms; }

    /// @brief 设置滑动判定最小距离。
    /// @param dist 像素距离，默认50
    void set_swipe_distance(int dist) noexcept { swipe_distance_ = dist; }

    /// @brief 设置双击最大间隔时间。
    /// @param ms 毫秒数，默认300
    void set_double_tap_interval(int ms) noexcept { double_tap_interval_ = ms; }

    // ==== 事件入口 ====

    /// @brief 处理触摸事件，根据类型分发到begin/update/end处理。
    /// @param event 触摸事件指针
    /// @return true 事件已处理
    bool process_touch_event(QTouchEvent *event);

    /// @brief 处理鼠标事件，将鼠标操作映射为等价的单指触摸。
    /// @param event 鼠标事件指针
    /// @return true 事件已处理
    bool process_mouse_event(QMouseEvent *event);

    // ==== 状态查询 ====

    /// @brief 获取当前触摸点数量。
    /// @return 触摸点数量
    [[nodiscard]] int touch_point_count() const noexcept { return touch_points_.size(); }

    /// @brief 获取最近识别的手势类型。
    /// @return 当前手势类型
    [[nodiscard]] GestureType current_gesture() const noexcept { return current_gesture_; }

signals:
    /// @brief 触摸按下时发射。
    /// @param point 按下位置
    /// @param id 触摸点ID
    void touch_pressed(const QPoint &point, int id = 0);

    /// @brief 触摸释放时发射。
    /// @param point 释放位置
    /// @param id 触摸点ID
    void touch_released(const QPoint &point, int id = 0);

    /// @brief 触摸移动时发射。
    /// @param point 当前位置
    /// @param id 触摸点ID
    void touch_moved(const QPoint &point, int id = 0);

    /// @brief 任何手势被识别时发射（通用手势信号）。
    /// @param gesture 识别到的手势类型
    void gesture_detected(GestureType gesture);

    /// @brief 单击手势识别时发射。
    /// @param point 点击位置
    void tap_detected(const QPoint &point);

    /// @brief 双击手势识别时发射。
    /// @param point 双击位置
    void double_tap_detected(const QPoint &point);

    /// @brief 长按手势识别时发射（按住超时即触发，无需释放）。
    /// @param point 长按位置
    void long_press_detected(const QPoint &point);

    /// @brief 滑动手势识别时发射。
    /// @param direction 滑动方向（SwipeLeft/Right/Up/Down）
    void swipe_detected(GestureType direction);

    /// @brief 捏合手势识别时发射。
    /// @param scale 缩放比例（<1缩小，>1放大）
    void pinch_detected(qreal scale);

    /// @brief 旋转手势识别时发射。
    /// @param angle 旋转角度变化量(°)
    void rotate_detected(qreal angle);

private slots:
    /// @brief 长按定时器回调，超时则判定为长按手势。
    void on_long_press_timer();

    /// @brief 点击定时器回调，超时且无双击则判定为单击手势。
    void on_tap_timer();

private:
    /// @brief 内部触摸点数据，记录起始位置、当前位置与时间戳。
    struct TouchPoint {
        int id;                 ///< 触摸点ID
        QPointF start_pos;     ///< 按下时的起始位置
        QPointF current_pos;   ///< 当前位置
        qint64 start_time;     ///< 按下时刻的毫秒时间戳
    };

    /// @brief 处理触摸开始事件，记录触摸点并启动长按定时器。
    void handle_touch_begin(const QList<QTouchEvent::TouchPoint> &points);

    /// @brief 处理触摸更新事件，更新位置并在双指时检测捏合/旋转。
    void handle_touch_update(const QList<QTouchEvent::TouchPoint> &points);

    /// @brief 处理触摸结束事件，根据移动距离与时间判定点击/双击/滑动。
    void handle_touch_end(const QList<QTouchEvent::TouchPoint> &points);

    /// @brief 双指手势检测，根据两指距离变化判定捏合，角度变化判定旋转。
    void detect_gesture();

    /// @brief 根据起止点判定滑动方向。
    /// @param start 起始位置
    /// @param end 结束位置
    /// @return 滑动方向（SwipeLeft/Right/Up/Down）
    [[nodiscard]] GestureType detect_swipe(const QPointF &start, const QPointF &end);

    /// @brief 计算两点间欧氏距离。
    [[nodiscard]] qreal distance(const QPointF &p1, const QPointF &p2) const noexcept;

    /// @brief 计算两点连线与X轴的夹角(°)。
    [[nodiscard]] qreal angle_between(const QPointF &p1, const QPointF &p2) const noexcept;

    // ==== 成员变量 ====
    QList<TouchPoint> touch_points_;    ///< 当前活跃触摸点列表
    GestureType current_gesture_;       ///< 最近识别的手势类型

    int tap_radius_;            ///< 点击判定半径(px)
    int tap_delay_;             ///< 点击延迟(ms)
    int long_press_duration_;   ///< 长按时长阈值(ms)
    int swipe_distance_;        ///< 滑动距离阈值(px)
    int double_tap_interval_;   ///< 双击间隔阈值(ms)

    QTimer *long_press_timer_;  ///< 长按判定定时器
    QTimer *tap_timer_;         ///< 单击判定定时器（等待双击超时）

    QPointF last_tap_pos_;      ///< 上次点击位置
    qint64 last_tap_time_;      ///< 上次点击时刻(ms)

    bool is_long_press_;        ///< 当前是否为长按状态
    bool is_double_tap_;        ///< 当前是否为双击状态
};
