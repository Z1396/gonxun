#ifndef TOUCHHANDLER_H
#define TOUCHHANDLER_H

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QList>
#include <QTimer>
#include <QTouchEvent>

class TouchHandler : public QObject
{
    Q_OBJECT

public:
    enum GestureType {
        NoGesture,
        Tap,
        DoubleTap,
        LongPress,
        SwipeLeft,
        SwipeRight,
        SwipeUp,
        SwipeDown,
        PinchIn,
        PinchOut,
        Rotate
    };
    Q_ENUM(GestureType)

    explicit TouchHandler(QObject *parent = nullptr);
    ~TouchHandler();

    void setTapRadius(int radius) { m_tapRadius = radius; }
    void setTapDelay(int ms) { m_tapDelay = ms; }
    void setLongPressDuration(int ms) { m_longPressDuration = ms; }
    void setSwipeDistance(int dist) { m_swipeDistance = dist; }
    void setDoubleTapInterval(int ms) { m_doubleTapInterval = ms; }

    bool processTouchEvent(QTouchEvent *event);
    bool processMouseEvent(QMouseEvent *event);

    int touchPointCount() const { return m_touchPoints.size(); }
    GestureType currentGesture() const { return m_currentGesture; }

signals:
    void touchPressed(const QPoint &point, int id = 0);
    void touchReleased(const QPoint &point, int id = 0);
    void touchMoved(const QPoint &point, int id = 0);
    void gestureDetected(GestureType gesture);
    void tapDetected(const QPoint &point);
    void doubleTapDetected(const QPoint &point);
    void longPressDetected(const QPoint &point);
    void swipeDetected(GestureType direction);
    void pinchDetected(qreal scale);
    void rotateDetected(qreal angle);

private slots:
    void onLongPressTimer();
    void onTapTimer();

private:
    struct TouchPoint {
        int id;
        QPointF startPos;
        QPointF currentPos;
        qint64 startTime;
    };

    void handleTouchBegin(const QList<QTouchEvent::TouchPoint> &points);
    void handleTouchUpdate(const QList<QTouchEvent::TouchPoint> &points);
    void handleTouchEnd(const QList<QTouchEvent::TouchPoint> &points);
    void detectGesture();
    GestureType detectSwipe(const QPointF &start, const QPointF &end);
    qreal distance(const QPointF &p1, const QPointF &p2) const;
    qreal angleBetween(const QPointF &p1, const QPointF &p2) const;

    QList<TouchPoint> m_touchPoints;
    GestureType m_currentGesture;

    int m_tapRadius;
    int m_tapDelay;
    int m_longPressDuration;
    int m_swipeDistance;
    int m_doubleTapInterval;

    QTimer *m_longPressTimer;
    QTimer *m_tapTimer;
    QPointF m_lastTapPos;
    qint64 m_lastTapTime;
    bool m_isLongPress;
    bool m_isDoubleTap;
};

#endif
