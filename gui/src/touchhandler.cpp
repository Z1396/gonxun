/**
 * @file touchhandler.cpp
 * @brief 触摸事件处理器实现文件
 * 
 * @details 本文件实现了触摸屏手势识别和处理功能。
 *          核心功能：
 *          - 手势识别：单击、双击、长按、滑动、缩放
 *          - 定时器机制：长按计时、双击判定窗口
 *          - 参数配置：点击半径、长按时长、滑动距离、双击间隔
 *          
 *          手势判定规则：
 *          - 单击：触摸按下后移动距离 < tapRadius，抬起时判定
 *          - 双击：两次单击间隔 < doubleTapInterval，位置相近
 *          - 长按：触摸按下后保持不动 > longPressDuration
 *          - 滑动：触摸按下后移动距离 > swipeDistance
 *          - 缩放：双指距离变化判定
 *          
 *          定时器机制：
 *          - m_longPressTimer：长按计时器（单次触发）
 *          - m_tapTimer：双击判定窗口定时器（单次触发）
 *          
 *          使用方式：
 *          @code
 *          TouchHandler* handler = new TouchHandler(this);
 *          connect(handler, &TouchHandler::singleTap, [](QPointF pos) {
 *              qDebug() << "Single tap at" << pos;
 *          });
 *          // 在 touchEvent() 中调用
 *          handler->processTouchEvent(event);
 *          @endcode
 *          
 *          注意事项：
 *          - 适配5/7寸触控屏，参数已优化
 *          - 支持多指触摸（缩放手势）
 *          - 定时器使用 Qt::PreciseTimer 提高精度
 *          
 * @see touchhandler.h 头文件定义
 * @see courtmapwidget.cpp 使用此处理器的地图控件
 * 
 * @author 工创赛2025智能物流搬运系统团队
 * @date 2024-01-15
 * @version 1.0.0
 * @history 2024-01-15 初始版本
 * @history 2024-02-10 新增缩放手势支持
 * 
 * @copyright 工创赛2025智能物流搬运系统
 */
#include "touchhandler.h"
#include <QDateTime>
#include <QTouchEvent>
#include <math.h>

/**
 * @brief 构造函数：初始化手势处理器、定时器、默认参数与状态
 * @param parent 父对象，用于Qt对象树内存管理
 */
TouchHandler::TouchHandler(QObject *parent)
    : QObject(parent)
    // 初始化当前手势状态：无手势
    , m_currentGesture(NoGesture)
    // 点击判定半径：触摸/鼠标移动小于该值判定为点击，而非滑动
    , m_tapRadius(20)
    // 点击延迟阈值（备用参数）
    , m_tapDelay(200)
    // 长按判定时长：按压超过800ms判定为长按
    , m_longPressDuration(800)
    // 滑动判定距离：移动距离超过50px判定为滑动手势
    , m_swipeDistance(50)
    // 双击间隔阈值：两次点击间隔小于300ms判定为双击
    , m_doubleTapInterval(300)
    // 长按状态标记：默认未触发长按
    , m_isLongPress(false)
    // 双击状态标记：默认未触发双击
    , m_isDoubleTap(false)
{
    // ====================== 长按定时器初始化 ======================
    m_longPressTimer = new QTimer(this);
    // 设置为单次触发：只计时一次，不会循环触发
    m_longPressTimer->setSingleShot(true);
    // 设置长按定时时长800ms
    m_longPressTimer->setInterval(m_longPressDuration);
    // 绑定定时器超时信号：时长结束触发长按槽函数
    connect(m_longPressTimer, &QTimer::timeout, this, &TouchHandler::onLongPressTimer);

    // ====================== 双击判定定时器初始化 ======================
    m_tapTimer = new QTimer(this);
    // 单次触发：用于等待双击判定窗口期
    m_tapTimer->setSingleShot(true);
    // 双击判定窗口期300ms
    m_tapTimer->setInterval(m_doubleTapInterval);
    // 定时结束后判定：窗口期内无第二次点击则判定为单击
    connect(m_tapTimer, &QTimer::timeout, this, &TouchHandler::onTapTimer);

    // 初始化上一次点击的时间戳（毫秒级时间）
    m_lastTapTime = 0;
}

/**
 * @brief 析构函数
 * @note 定时器由this为父对象，Qt自动回收内存，无需手动释放
 */
TouchHandler::~TouchHandler()
{
}

/**
 * @brief 统一处理原生触摸屏触摸事件
 * @param event Qt触摸事件对象
 * @return bool 事件是否被当前处理器处理（true=已处理）
 */
bool TouchHandler::processTouchEvent(QTouchEvent *event)
{
    // 空指针防护
    if (!event) return false;

    // 根据触摸事件类型分发处理
    switch (event->type()) {
    // 触摸按下（手指接触屏幕）
    case QEvent::TouchBegin:
        handleTouchBegin(event->touchPoints());
        return true;
    // 触摸移动（手指在屏幕滑动）
    case QEvent::TouchUpdate:
        handleTouchUpdate(event->touchPoints());
        return true;
    // 触摸抬起（手指离开屏幕）
    case QEvent::TouchEnd:
        handleTouchEnd(event->touchPoints());
        return true;
    // 其他触摸事件忽略
    default:
        break;
    }
    return false;
}

/**
 * @brief 兼容处理鼠标事件（用鼠标模拟触控手势）
 * @param event 鼠标事件对象
 * @return bool 事件是否被处理
 * @note 适配无触屏设备，鼠标左键按压=触摸按下，移动=触摸滑动，松开=触摸抬起
 */
bool TouchHandler::processMouseEvent(QMouseEvent *event)
{
    if (!event) return false;

    // 获取当前系统时间戳（毫秒），用于时间间隔计算
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    switch (event->type()) {
    // 鼠标左键按下：模拟触摸按下
    case QEvent::MouseButtonPress: {
        // 自定义触摸点结构体，存储鼠标触控信息
        TouchPoint tp;
        tp.id = 0; // 鼠标固定单点ID=0
        tp.startPos = event->pos(); // 按压起始坐标
        tp.currentPos = event->pos(); // 当前坐标
        tp.startTime = now; // 按压时间

        // 清空历史触控点，保存当前鼠标触控点
        m_touchPoints.clear();
        m_touchPoints.append(tp);

        // 重置手势状态、长按标记
        m_currentGesture = NoGesture;
        m_isLongPress = false;

        // 启动长按定时器，开始计时
        m_longPressTimer->start();

        // 抛出按压信号，对外通知按下事件
        emit touchPressed(event->pos(), 0);
        return true;
    }
    // 鼠标移动：模拟触摸滑动
    case QEvent::MouseMove: {
        // 存在有效触控点时更新坐标
        if (m_touchPoints.size() > 0) {
            m_touchPoints[0].currentPos = event->pos();
            // 抛出移动信号
            emit touchMoved(event->pos(), 0);

            // 若移动距离超过点击半径，判定为滑动，终止长按判定
            if (distance(m_touchPoints[0].startPos, m_touchPoints[0].currentPos) > m_tapRadius) {
                m_longPressTimer->stop();
            }
        }
        return true;
    }
    // 鼠标左键松开：模拟触摸抬起，核心手势判定逻辑
    case QEvent::MouseButtonRelease: {
        if (m_touchPoints.size() > 0) {
            // 停止长按计时
            m_longPressTimer->stop();
            // 抛出抬起信号
            emit touchReleased(event->pos(), 0);

            // 未触发长按，判定为点击/滑动手势
            if (!m_isLongPress) {
                // 获取按压起始坐标和抬起结束坐标
                QPointF start = m_touchPoints[0].startPos;
                QPointF end = event->pos();
                // 计算按压全程移动距离
                qreal dist = distance(start, end);

                // 距离小于点击半径：判定为点击类手势（单击/双击）
                if (dist < m_tapRadius) {
                    // 计算与上一次点击的时间间隔
                    qint64 timeDelta = now - m_lastTapTime;
                    // 满足双击条件：时间间隔达标 + 两次点击位置基本一致
                    if (timeDelta < m_doubleTapInterval && distance(m_lastTapPos, end) < m_tapRadius) {
                        m_isDoubleTap = true;
                        m_currentGesture = DoubleTap;
                        // 抛出双击信号
                        emit doubleTapDetected(event->pos());
                        emit gestureDetected(DoubleTap);
                        // 停止单击定时器，避免触发单击
                        m_tapTimer->stop();
                    } else {
                        // 不满足双击：记录本次点击信息，启动单击定时器
                        m_lastTapPos = end;
                        m_lastTapTime = now;
                        m_tapTimer->start();
                    }
                }
                // 移动距离超过滑动阈值：判定为滑动手势
                else if (dist > m_swipeDistance) {
                    // 识别滑动方向
                    GestureType swipe = detectSwipe(start, end);
                    m_currentGesture = swipe;
                    // 抛出滑动信号
                    emit swipeDetected(swipe);
                    emit gestureDetected(swipe);
                }
            }

            // 清空触控点，结束本次手势
            m_touchPoints.clear();
        }
        return true;
    }
    // 其他鼠标事件忽略
    default:
        break;
    }
    return false;
}

/**
 * @brief 长按定时器超时槽函数：触发长按手势
 * @note 定时器计时结束且触控点未松开，判定为长按
 */
void TouchHandler::onLongPressTimer()
{
    // 存在有效触控点，说明持续按压未松开
    if (m_touchPoints.size() > 0) {
        m_isLongPress = true; // 标记长按触发
        m_currentGesture = LongPress; // 更新当前手势
        // 抛出长按信号，携带长按坐标
        emit longPressDetected(m_touchPoints[0].currentPos.toPoint());
        emit gestureDetected(LongPress);
    }
}

/**
 * @brief 单击定时器超时槽函数：判定为单击手势
 * @note 双击窗口期结束，无第二次点击则触发单击
 */
void TouchHandler::onTapTimer()
{
    // 未触发双击、且手势已结束，判定为普通单击
    if (!m_isDoubleTap && m_touchPoints.isEmpty()) {
        m_currentGesture = Tap;
        // 抛出单击信号
        emit tapDetected(m_lastTapPos.toPoint());
        emit gestureDetected(Tap);
    }
    // 重置双击标记，等待下一次手势
    m_isDoubleTap = false;
}

/**
 * @brief 处理触摸按下事件（真实触屏）
 * @param points 屏幕按压的所有触摸点（支持多指触控）
 */
void TouchHandler::handleTouchBegin(const QList<QTouchEvent::TouchPoint> &points)
{
    // 清空历史触控数据，重置状态
    m_touchPoints.clear();
    m_currentGesture = NoGesture;
    m_isLongPress = false;
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 遍历所有触摸点（支持多指同时按下）
    for (int i = 0; i < points.size(); ++i) {
        TouchPoint tp;
        tp.id = points[i].id(); // 触摸点唯一ID（区分多指）
        tp.startPos = points[i].pos(); // 按压起始坐标
        tp.currentPos = points[i].pos(); // 当前坐标
        tp.startTime = now; // 按压时间
        m_touchPoints.append(tp);
        // 逐点抛出按压信号
        emit touchPressed(points[i].pos().toPoint(), tp.id);
    }

    // 单指按压：启动长按定时器（多指不触发长按）
    if (m_touchPoints.size() == 1) {
        m_longPressTimer->start();
    }
}

/**
 * @brief 处理触摸移动事件（真实触屏滑动）
 * @param points 当前所有移动的触摸点
 */
void TouchHandler::handleTouchUpdate(const QList<QTouchEvent::TouchPoint> &points)
{
    // 更新所有触摸点的实时坐标
    for (int i = 0; i < points.size(); ++i) {
        for (int j = 0; j < m_touchPoints.size(); ++j) {
            // 根据ID匹配对应触摸点，更新坐标
            if (m_touchPoints[j].id == points[i].id()) {
                m_touchPoints[j].currentPos = points[i].pos();
                emit touchMoved(points[i].pos().toPoint(), m_touchPoints[j].id);
                break;
            }
        }
    }

    // 单指滑动：位移超过点击半径，取消长按判定
    if (m_touchPoints.size() == 1) {
        if (distance(m_touchPoints[0].startPos, m_touchPoints[0].currentPos) > m_tapRadius) {
            m_longPressTimer->stop();
        }
    }

    // 双指触控：触发双指手势检测（缩放、旋转）
    if (m_touchPoints.size() == 2) {
        detectGesture();
    }
}

/**
 * @brief 处理触摸抬起事件（真实触屏松手）
 * @param points 抬起的触摸点
 */
void TouchHandler::handleTouchEnd(const QList<QTouchEvent::TouchPoint> &points)
{
    // 停止长按计时
    m_longPressTimer->stop();

    // 逐点抛出抬起信号
    for (int i = 0; i < points.size(); ++i) {
        emit touchReleased(points[i].pos().toPoint(), points[i].id());
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 单指手势结束、未触发长按：判定单击/双击/滑动
    if (m_touchPoints.size() == 1 && !m_isLongPress) {
        QPointF start = m_touchPoints[0].startPos;
        QPointF end = m_touchPoints[0].currentPos;
        qreal dist = distance(start, end);

        // 点击类手势
        if (dist < m_tapRadius) {
            qint64 timeDelta = now - m_lastTapTime;
            // 双击判定
            if (timeDelta < m_doubleTapInterval && distance(m_lastTapPos, end) < m_tapRadius) {
                m_isDoubleTap = true;
                m_currentGesture = DoubleTap;
                emit doubleTapDetected(end.toPoint());
                emit gestureDetected(DoubleTap);
                m_tapTimer->stop();
            }
            // 等待双击窗口期，判定单击
            else {
                m_lastTapPos = end;
                m_lastTapTime = now;
                m_tapTimer->start();
            }
        }
        // 滑动手势判定
        else if (dist > m_swipeDistance) {
            GestureType swipe = detectSwipe(start, end);
            m_currentGesture = swipe;
            emit swipeDetected(swipe);
            emit gestureDetected(swipe);
        }
    }

    // 清空本次手势数据
    m_touchPoints.clear();
}

/**
 * @brief 双指手势检测核心函数：识别缩放、旋转手势
 * @note 仅双指触控时生效
 */
void TouchHandler::detectGesture()
{
    // 非双指直接返回
    if (m_touchPoints.size() != 2) return;

    // 获取双指起始、当前坐标
    QPointF p1Start = m_touchPoints[0].startPos;
    QPointF p2Start = m_touchPoints[1].startPos;
    QPointF p1Current = m_touchPoints[0].currentPos;
    QPointF p2Current = m_touchPoints[1].currentPos;

    // 计算双指初始间距、当前间距
    qreal startDist = distance(p1Start, p2Start);
    qreal currentDist = distance(p1Current, p2Current);

    // 缩放手势判定（避免除0）
    if (startDist > 0) {
        // 计算缩放比例：当前间距/初始间距
        qreal scale = currentDist / startDist;
        // 比例小于0.85：双指缩小
        if (scale < 0.85) {
            m_currentGesture = PinchIn;
            emit pinchDetected(scale);
            emit gestureDetected(PinchIn);
        }
        // 比例大于1.15：双指放大
        else if (scale > 1.15) {
            m_currentGesture = PinchOut;
            emit pinchDetected(scale);
            emit gestureDetected(PinchOut);
        }
    }

    // 旋转手势判定
    qreal startAngle = angleBetween(p1Start, p2Start); // 初始夹角
    qreal currentAngle = angleBetween(p1Current, p2Current); // 当前夹角
    qreal angleDelta = currentAngle - startAngle; // 角度变化量

    // 角度变化超过15度，判定为旋转
    if (fabs(angleDelta) > 15.0) {
        m_currentGesture = Rotate;
        emit rotateDetected(angleDelta);
        emit gestureDetected(Rotate);
    }
}

/**
 * @brief 识别滑动手势方向
 * @param start 按压起始坐标
 * @param end 抬起结束坐标
 * @return GestureType 上下左右滑动类型
 * @note 优先判断横竖方向：水平位移大=左右滑，垂直位移大=上下滑
 */
TouchHandler::GestureType TouchHandler::detectSwipe(const QPointF &start, const QPointF &end)
{
    // 计算X、Y轴位移差值
    qreal dx = end.x() - start.x();
    qreal dy = end.y() - start.y();

    // 水平位移大于垂直：左右滑动
    if (fabs(dx) > fabs(dy)) {
        return dx > 0 ? SwipeRight : SwipeLeft;
    }
    // 垂直位移大于水平：上下滑动
    else {
        return dy > 0 ? SwipeDown : SwipeUp;
    }
}

/**
 * @brief 计算两点之间直线距离
 * @param p1 坐标点1
 * @param p2 坐标点2
 * @return qreal 两点直线像素距离
 */
qreal TouchHandler::distance(const QPointF &p1, const QPointF &p2) const
{
    qreal dx = p2.x() - p1.x();
    qreal dy = p2.y() - p1.y();
    // 勾股定理计算直线距离
    return sqrt(dx * dx + dy * dy);
}

/**
 * @brief 计算两点连线与X轴正方向的夹角（角度制）
 * @param p1 起点
 * @param p2 终点
 * @return qreal 夹角角度（-180~180）
 */
qreal TouchHandler::angleBetween(const QPointF &p1, const QPointF &p2) const
{
    qreal dx = p2.x() - p1.x();
    qreal dy = p2.y() - p1.y();
    // atan2返回弧度，转换为角度
    return atan2(dy, dx) * 180.0 / M_PI;
}
