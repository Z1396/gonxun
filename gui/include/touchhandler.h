// 头文件保护宏，防止重复包含造成类重定义编译报错
#ifndef TOUCHHANDLER_H
// 定义标识，配对上方#ifndef
#define TOUCHHANDLER_H

// Qt信号槽基类，所有能发射信号、定义槽的类必须继承QObject
#include <QObject>
// 整数坐标点（输出给上层UI用）
#include <QPoint>
// 浮点坐标点（触摸高精度原始坐标，计算手势距离/角度）
#include <QPointF>
// 容器，存储多点触摸所有触点信息
#include <QList>
// 定时器：用于识别长按、双击间隔
#include <QTimer>
// Qt原生触摸屏触摸事件封装类
#include <QTouchEvent>

/**
 * @brief 鼠标/触摸屏手势统一识别处理类
 * 支持单点：单击、双击、长按、上下左右滑动
 * 支持双指：放大(PinchOut)、缩小(PinchIn)、旋转(Rotate)
 * 统一封装鼠标事件与电容触摸事件，上层绘图控件只需要接收信号，不用区分鼠标/触屏
 */
class TouchHandler : public QObject
{
    // Qt元对象宏，必须存在才能启用信号槽、Q_ENUM反射
    Q_OBJECT

public:
    /**
     * @brief 所有手势类型枚举
     * Q_ENUM宏将枚举注册到Qt元系统，支持信号传参、打印字符串调试
     */
    enum GestureType {
        NoGesture,      // 无有效手势
        Tap,            // 单击（轻点）
        DoubleTap,      // 双击
        LongPress,      // 长按
        SwipeLeft,      // 左滑
        SwipeRight,     // 右滑
        SwipeUp,        // 上滑
        SwipeDown,      // 下滑
        PinchIn,        // 双指捏合缩小
        PinchOut,       // 双指张开放大
        Rotate          // 双指旋转
    };
    Q_ENUM(GestureType)

    /**
     * @brief 构造函数
     * @param parent 父对象，Qt自动内存回收
     */
    explicit TouchHandler(QObject *parent = nullptr);
    // 析构函数
    ~TouchHandler();

    // ====================== 可配置手势阈值接口 ======================
    /**
     * @brief 设置点击判定半径：按下抬起偏移小于该像素才算单击，否则判定滑动
     */
    void setTapRadius(int radius) { m_tapRadius = radius; }
    /**
     * @brief 单击延时预留（双击判定等待时长）
     */
    void setTapDelay(int ms) { m_tapDelay = ms; }
    /**
     * @brief 长按触发毫秒阈值，按住超过该时间判定长按
     */
    void setLongPressDuration(int ms) { m_longPressDuration = ms; }
    /**
     * @brief 滑动最小移动距离，超过才判定为滑动手势
     */
    void setSwipeDistance(int dist) { m_swipeDistance = dist; }
    /**
     * @brief 双击间隔阈值，两次点击间隔小于该值判定双击
     */
    void setDoubleTapInterval(int ms) { m_doubleTapInterval = ms; }

    // ====================== 事件入口处理函数 ======================
    /**
     * @brief 处理原生触摸屏QTouchEvent事件
     * @param event Qt触摸事件对象
     * @return true 事件已处理，可拦截不再向下传递
     */
    bool processTouchEvent(QTouchEvent *event);
    /**
     * @brief 处理鼠标事件，将鼠标模拟为单触点触摸
     * @param event Qt鼠标事件
     * @return true 事件已处理
     */
    bool processMouseEvent(QMouseEvent *event);

    // ====================== 状态查询只读接口 ======================
    /**
     * @brief 获取当前屏幕触点数量（0/1/2多指）
     */
    int touchPointCount() const { return m_touchPoints.size(); }
    /**
     * @brief 获取当前识别到的手势类型
     */
    GestureType currentGesture() const { return m_currentGesture; }

signals:
    // 基础触点原始事件信号（按下/移动/抬起）
    /**
     * @brief 触点按下
     * @param point 触点屏幕坐标
     * @param id 触点ID，鼠标固定为0，触屏多指区分不同id
     */
    void touchPressed(const QPoint &point, int id = 0);
    /**
     * @brief 触点抬起
     */
    void touchReleased(const QPoint &point, int id = 0);
    /**
     * @brief 触点滑动移动
     */
    void touchMoved(const QPoint &point, int id = 0);

    // 通用手势总信号，任何手势触发都会发送，统一接收处理
    void gestureDetected(GestureType gesture);

    // 各类手势独立细分信号，按需绑定
    void tapDetected(const QPoint &point);                // 单击触发
    void doubleTapDetected(const QPoint &point);          // 双击触发
    void longPressDetected(const QPoint &point);          // 长按触发
    void swipeDetected(GestureType direction);            // 滑动，参数为滑动方向枚举
    void pinchDetected(qreal scale);                      // 双指缩放，scale>1放大 <1缩小
    void rotateDetected(qreal angle);                    // 双指旋转，返回旋转角度(度)

private slots:
    /**
     * @brief 长按定时器超时槽，计时到判定长按手势
     */
    void onLongPressTimer();
    /**
     * @brief 双击等待定时器超时槽：超时未第二次点击，则判定为普通单击
     */
    void onTapTimer();

private:
    /**
     * @brief 单个触摸触点数据结构体
     * 保存触点起始、实时坐标与按下时间戳，用于手势计算
     */
    struct TouchPoint {
        int id;                 // 触点唯一标识id
        QPointF startPos;       // 触点按下时起始浮点坐标
        QPointF currentPos;     // 当前实时浮点坐标
        qint64 startTime;      // 按下时毫秒时间戳
    };

    // ====================== 内部私有处理函数 ======================
    /**
     * @brief 触点按下事件统一处理
     */
    void handleTouchBegin(const QList<QTouchEvent::TouchPoint> &points);
    /**
     * @brief 触点滑动移动统一处理
     */
    void handleTouchUpdate(const QList<QTouchEvent::TouchPoint> &points);
    /**
     * @brief 触点抬起统一处理
     */
    void handleTouchEnd(const QList<QTouchEvent::TouchPoint> &points);
    /**
     * @brief 双指手势检测（缩放、旋转）
     */
    void detectGesture();
    /**
     * @brief 根据起止点计算滑动方向枚举
     */
    GestureType detectSwipe(const QPointF &start, const QPointF &end);
    /**
     * @brief 计算两点之间欧氏距离
     */
    qreal distance(const QPointF &p1, const QPointF &p2) const;
    /**
     * @brief 两点连线相对X轴夹角（角度制），用于旋转计算
     */
    qreal angleBetween(const QPointF &p1, const QPointF &p2) const;

    // ====================== 私有成员变量 ======================
    QList<TouchPoint> m_touchPoints;       // 当前所有活动触点缓存列表
    GestureType m_currentGesture;          // 当前识别到的手势

    // 手势判定阈值参数
    int m_tapRadius;                       // 点击最大偏移像素
    int m_tapDelay;                        // 单击等待延时（备用）
    int m_longPressDuration;               // 长按毫秒阈值
    int m_swipeDistance;                   // 滑动最小位移
    int m_doubleTapInterval;               // 双击最大间隔

    QTimer *m_longPressTimer;              // 长按倒计时定时器
    QTimer *m_tapTimer;                   // 双击等待定时器

    QPointF m_lastTapPos;                  // 上一次单击坐标，用于双击位置校验
    qint64 m_lastTapTime;                  // 上一次单击时间戳

    bool m_isLongPress;                    // 当前是否已触发长按标记
    bool m_isDoubleTap;                    // 当前是否判定为双击标记
};

// 头文件结束保护
#endif