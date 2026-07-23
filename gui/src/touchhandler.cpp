/// @file touchhandler.cpp
/// @brief 触摸事件处理器实现，包含鼠标/触摸事件到内部TouchPoint的映射、
///        定时器驱动的长按/双击判定，以及双指捏合/旋转检测。

#include "touchhandler.hpp"

#include <QDateTime>
#include <QTouchEvent>
#include <cmath>

/// @brief 构造手势处理器：初始化定时器与默认阈值参数。
TouchHandler::TouchHandler(QObject *parent) noexcept
    : QObject(parent)
    , current_gesture_(NoGesture)
    , tap_radius_(20)
    , tap_delay_(200)
    , long_press_duration_(800)
    , swipe_distance_(50)
    , double_tap_interval_(300)
    , is_long_press_(false)
    , is_double_tap_(false)
{
    // 长按判定定时器：超时即触发长按手势
    long_press_timer_ = new QTimer(this);
    long_press_timer_->setSingleShot(true);
    long_press_timer_->setInterval(long_press_duration_);
    connect(long_press_timer_, &QTimer::timeout, this, &TouchHandler::on_long_press_timer);

    // 单击判定定时器：双击间隔内无第二次点击则确认单击
    tap_timer_ = new QTimer(this);
    tap_timer_->setSingleShot(true);
    tap_timer_->setInterval(double_tap_interval_);
    connect(tap_timer_, &QTimer::timeout, this, &TouchHandler::on_tap_timer);

    last_tap_time_ = 0;
}

TouchHandler::~TouchHandler()
{
}

/// @brief 处理触摸事件：根据TouchBegin/Update/End分发到对应处理函数。
/// @param event 触摸事件
/// @return true 事件已处理
bool TouchHandler::process_touch_event(QTouchEvent *event)
{
    if (!event) return false;

    switch (event->type()) {
    case QEvent::TouchBegin:
        handle_touch_begin(event->touchPoints());
        return true;
    case QEvent::TouchUpdate:
        handle_touch_update(event->touchPoints());
        return true;
    case QEvent::TouchEnd:
        handle_touch_end(event->touchPoints());
        return true;
    default:
        break;
    }
    return false;
}

/// @brief 处理鼠标事件：将鼠标Press/Move/Release映射为单指触摸。
///        Press记录起点并启动长按定时器；Release时根据距离判定点击/双击/滑动。
/// @param event 鼠标事件
/// @return true 事件已处理
bool TouchHandler::process_mouse_event(QMouseEvent *event)
{
    if (!event) return false;

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        // 记录按下位置，启动长按定时器
        TouchPoint tp;
        tp.id = 0;
        tp.start_pos = event->pos();
        tp.current_pos = event->pos();
        tp.start_time = now;

        touch_points_.clear();
        touch_points_.append(tp);

        current_gesture_ = NoGesture;
        is_long_press_ = false;

        long_press_timer_->start();

        emit touch_pressed(event->pos(), 0);
        return true;
    }
    case QEvent::MouseMove: {
        // 更新位置，移动超阈值则取消长按
        if (touch_points_.size() > 0) {
            touch_points_[0].current_pos = event->pos();
            emit touch_moved(event->pos(), 0);

            if (distance(touch_points_[0].start_pos, touch_points_[0].current_pos) > tap_radius_) {
                long_press_timer_->stop();
            }
        }
        return true;
    }
    case QEvent::MouseButtonRelease: {
        if (touch_points_.size() > 0) {
            long_press_timer_->stop();
            emit touch_released(event->pos(), 0);

            if (!is_long_press_) {
                QPointF start = touch_points_[0].start_pos;
                QPointF end = event->pos();
                qreal dist = distance(start, end);

                if (dist < tap_radius_) {
                    // 移动距离小→判定为点击，检查双击
                    qint64 time_delta = now - last_tap_time_;
                    if (time_delta < double_tap_interval_ && distance(last_tap_pos_, end) < tap_radius_) {
                        // 双击：位置接近且时间间隔短
                        is_double_tap_ = true;
                        current_gesture_ = DoubleTap;
                        emit double_tap_detected(event->pos());
                        emit gesture_detected(DoubleTap);
                        tap_timer_->stop();
                    } else {
                        // 可能是单击，等待双击超时确认
                        last_tap_pos_ = end;
                        last_tap_time_ = now;
                        tap_timer_->start();
                    }
                }
                else if (dist > swipe_distance_) {
                    // 移动距离大→判定为滑动
                    GestureType swipe = detect_swipe(start, end);
                    current_gesture_ = swipe;
                    emit swipe_detected(swipe);
                    emit gesture_detected(swipe);
                }
            }

            touch_points_.clear();
        }
        return true;
    }
    default:
        break;
    }
    return false;
}

/// @brief 长按定时器回调：若仍有触摸点，判定为长按手势。
void TouchHandler::on_long_press_timer()
{
    if (touch_points_.size() > 0) {
        is_long_press_ = true;
        current_gesture_ = LongPress;
        emit long_press_detected(touch_points_[0].current_pos.toPoint());
        emit gesture_detected(LongPress);
    }
}

/// @brief 点击定时器回调：超时且无双击则确认为单击。
void TouchHandler::on_tap_timer()
{
    if (!is_double_tap_ && touch_points_.isEmpty()) {
        current_gesture_ = Tap;
        emit tap_detected(last_tap_pos_.toPoint());
        emit gesture_detected(Tap);
    }
    is_double_tap_ = false;
}

/// @brief 处理触摸开始：记录所有触摸点，单指时启动长按定时器。
void TouchHandler::handle_touch_begin(const QList<QTouchEvent::TouchPoint> &points)
{
    touch_points_.clear();
    current_gesture_ = NoGesture;
    is_long_press_ = false;
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (int i = 0; i < points.size(); ++i) {
        TouchPoint tp;
        tp.id = points[i].id();
        tp.start_pos = points[i].pos();
        tp.current_pos = points[i].pos();
        tp.start_time = now;
        touch_points_.append(tp);
        emit touch_pressed(points[i].pos().toPoint(), tp.id);
    }

    // 单指触摸启动长按判定
    if (touch_points_.size() == 1) {
        long_press_timer_->start();
    }
}

/// @brief 处理触摸更新：更新各触摸点位置，单指移动超阈值取消长按，
///        双指时调用detect_gesture()检测捏合/旋转。
void TouchHandler::handle_touch_update(const QList<QTouchEvent::TouchPoint> &points)
{
    // 按ID匹配更新位置
    for (int i = 0; i < points.size(); ++i) {
        for (int j = 0; j < touch_points_.size(); ++j) {
            if (touch_points_[j].id == points[i].id()) {
                touch_points_[j].current_pos = points[i].pos();
                emit touch_moved(points[i].pos().toPoint(), touch_points_[j].id);
                break;
            }
        }
    }

    // 单指：移动超阈值取消长按
    if (touch_points_.size() == 1) {
        if (distance(touch_points_[0].start_pos, touch_points_[0].current_pos) > tap_radius_) {
            long_press_timer_->stop();
        }
    }

    // 双指：检测捏合/旋转
    if (touch_points_.size() == 2) {
        detect_gesture();
    }
}

/// @brief 处理触摸结束：停止长按定时器，单指释放时判定点击/双击/滑动。
void TouchHandler::handle_touch_end(const QList<QTouchEvent::TouchPoint> &points)
{
    long_press_timer_->stop();

    for (int i = 0; i < points.size(); ++i) {
        emit touch_released(points[i].pos().toPoint(), points[i].id());
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 单指释放且非长按：根据移动距离判定手势
    if (touch_points_.size() == 1 && !is_long_press_) {
        QPointF start = touch_points_[0].start_pos;
        QPointF end = touch_points_[0].current_pos;
        qreal dist = distance(start, end);

        if (dist < tap_radius_) {
            // 点击/双击判定（同鼠标处理逻辑）
            qint64 time_delta = now - last_tap_time_;
            if (time_delta < double_tap_interval_ && distance(last_tap_pos_, end) < tap_radius_) {
                is_double_tap_ = true;
                current_gesture_ = DoubleTap;
                emit double_tap_detected(end.toPoint());
                emit gesture_detected(DoubleTap);
                tap_timer_->stop();
            }
            else {
                last_tap_pos_ = end;
                last_tap_time_ = now;
                tap_timer_->start();
            }
        }
        else if (dist > swipe_distance_) {
            // 滑动判定
            GestureType swipe = detect_swipe(start, end);
            current_gesture_ = swipe;
            emit swipe_detected(swipe);
            emit gesture_detected(swipe);
        }
    }

    touch_points_.clear();
}

/// @brief 双指手势检测：根据两指距离变化率判定PinchIn/Out，
///        根据两指连线角度变化判定Rotate。
///        捏合阈值：比例<0.85或>1.15；旋转阈值：角度变化>15°。
void TouchHandler::detect_gesture()
{
    if (touch_points_.size() != 2) return;

    QPointF p1_start = touch_points_[0].start_pos;
    QPointF p2_start = touch_points_[1].start_pos;
    QPointF p1_current = touch_points_[0].current_pos;
    QPointF p2_current = touch_points_[1].current_pos;

    // 捏合判定：两指距离比例
    qreal start_dist = distance(p1_start, p2_start);
    qreal current_dist = distance(p1_current, p2_current);

    if (start_dist > 0) {
        qreal scale = current_dist / start_dist;
        if (scale < 0.85) {
            current_gesture_ = PinchIn;
            emit pinch_detected(scale);
            emit gesture_detected(PinchIn);
        }
        else if (scale > 1.15) {
            current_gesture_ = PinchOut;
            emit pinch_detected(scale);
            emit gesture_detected(PinchOut);
        }
    }

    // 旋转判定：两指连线角度变化
    qreal start_angle = angle_between(p1_start, p2_start);
    qreal current_angle = angle_between(p1_current, p2_current);
    qreal angle_delta = current_angle - start_angle;

    if (fabs(angle_delta) > 15.0) {
        current_gesture_ = Rotate;
        emit rotate_detected(angle_delta);
        emit gesture_detected(Rotate);
    }
}

/// @brief 判定滑动方向：取位移分量较大的轴方向。
/// @param start 起始位置
/// @param end 结束位置
/// @return 滑动方向枚举
TouchHandler::GestureType TouchHandler::detect_swipe(const QPointF &start, const QPointF &end)
{
    qreal dx = end.x() - start.x();
    qreal dy = end.y() - start.y();

    if (fabs(dx) > fabs(dy)) {
        return dx > 0 ? SwipeRight : SwipeLeft;
    }
    else {
        return dy > 0 ? SwipeDown : SwipeUp;
    }
}

/// @brief 计算两点间欧氏距离。
qreal TouchHandler::distance(const QPointF &p1, const QPointF &p2) const noexcept
{
    qreal dx = p2.x() - p1.x();
    qreal dy = p2.y() - p1.y();
    return sqrt(dx * dx + dy * dy);
}

/// @brief 计算两点连线与X轴正方向的夹角(°)，使用atan2确保全象限正确。
qreal TouchHandler::angle_between(const QPointF &p1, const QPointF &p2) const noexcept
{
    qreal dx = p2.x() - p1.x();
    qreal dy = p2.y() - p1.y();
    return atan2(dy, dx) * 180.0 / M_PI;
}
