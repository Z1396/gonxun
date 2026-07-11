#include "touchhandler.h"
#include <QDateTime>
#include <QTouchEvent>
#include <math.h>

TouchHandler::TouchHandler(QObject *parent)
    : QObject(parent)
    , m_currentGesture(NoGesture)
    , m_tapRadius(20)
    , m_tapDelay(200)
    , m_longPressDuration(800)
    , m_swipeDistance(50)
    , m_doubleTapInterval(300)
    , m_isLongPress(false)
    , m_isDoubleTap(false)
{
    m_longPressTimer = new QTimer(this);
    m_longPressTimer->setSingleShot(true);
    m_longPressTimer->setInterval(m_longPressDuration);
    connect(m_longPressTimer, &QTimer::timeout, this, &TouchHandler::onLongPressTimer);

    m_tapTimer = new QTimer(this);
    m_tapTimer->setSingleShot(true);
    m_tapTimer->setInterval(m_doubleTapInterval);
    connect(m_tapTimer, &QTimer::timeout, this, &TouchHandler::onTapTimer);

    m_lastTapTime = 0;
}

TouchHandler::~TouchHandler()
{
}

bool TouchHandler::processTouchEvent(QTouchEvent *event)
{
    if (!event) return false;

    switch (event->type()) {
    case QEvent::TouchBegin:
        handleTouchBegin(event->touchPoints());
        return true;
    case QEvent::TouchUpdate:
        handleTouchUpdate(event->touchPoints());
        return true;
    case QEvent::TouchEnd:
        handleTouchEnd(event->touchPoints());
        return true;
    default:
        break;
    }
    return false;
}

bool TouchHandler::processMouseEvent(QMouseEvent *event)
{
    if (!event) return false;

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        TouchPoint tp;
        tp.id = 0;
        tp.startPos = event->pos();
        tp.currentPos = event->pos();
        tp.startTime = now;
        m_touchPoints.clear();
        m_touchPoints.append(tp);
        m_currentGesture = NoGesture;
        m_isLongPress = false;
        m_longPressTimer->start();
        emit touchPressed(event->pos(), 0);
        return true;
    }
    case QEvent::MouseMove: {
        if (m_touchPoints.size() > 0) {
            m_touchPoints[0].currentPos = event->pos();
            emit touchMoved(event->pos(), 0);

            if (distance(m_touchPoints[0].startPos, m_touchPoints[0].currentPos) > m_tapRadius) {
                m_longPressTimer->stop();
            }
        }
        return true;
    }
    case QEvent::MouseButtonRelease: {
        if (m_touchPoints.size() > 0) {
            m_longPressTimer->stop();
            emit touchReleased(event->pos(), 0);

            if (!m_isLongPress) {
                QPointF start = m_touchPoints[0].startPos;
                QPointF end = event->pos();
                qreal dist = distance(start, end);

                if (dist < m_tapRadius) {
                    qint64 timeDelta = now - m_lastTapTime;
                    if (timeDelta < m_doubleTapInterval && distance(m_lastTapPos, end) < m_tapRadius) {
                        m_isDoubleTap = true;
                        m_currentGesture = DoubleTap;
                        emit doubleTapDetected(event->pos());
                        emit gestureDetected(DoubleTap);
                        m_tapTimer->stop();
                    } else {
                        m_lastTapPos = end;
                        m_lastTapTime = now;
                        m_tapTimer->start();
                    }
                } else if (dist > m_swipeDistance) {
                    GestureType swipe = detectSwipe(start, end);
                    m_currentGesture = swipe;
                    emit swipeDetected(swipe);
                    emit gestureDetected(swipe);
                }
            }

            m_touchPoints.clear();
        }
        return true;
    }
    default:
        break;
    }
    return false;
}

void TouchHandler::onLongPressTimer()
{
    if (m_touchPoints.size() > 0) {
        m_isLongPress = true;
        m_currentGesture = LongPress;
        emit longPressDetected(m_touchPoints[0].currentPos.toPoint());
        emit gestureDetected(LongPress);
    }
}

void TouchHandler::onTapTimer()
{
    if (!m_isDoubleTap && m_touchPoints.isEmpty()) {
        m_currentGesture = Tap;
        emit tapDetected(m_lastTapPos.toPoint());
        emit gestureDetected(Tap);
    }
    m_isDoubleTap = false;
}

void TouchHandler::handleTouchBegin(const QList<QTouchEvent::TouchPoint> &points)
{
    m_touchPoints.clear();
    m_currentGesture = NoGesture;
    m_isLongPress = false;
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (int i = 0; i < points.size(); ++i) {
        TouchPoint tp;
        tp.id = points[i].id();
        tp.startPos = points[i].pos();
        tp.currentPos = points[i].pos();
        tp.startTime = now;
        m_touchPoints.append(tp);
        emit touchPressed(points[i].pos().toPoint(), tp.id);
    }

    if (m_touchPoints.size() == 1) {
        m_longPressTimer->start();
    }
}

void TouchHandler::handleTouchUpdate(const QList<QTouchEvent::TouchPoint> &points)
{
    for (int i = 0; i < points.size(); ++i) {
        for (int j = 0; j < m_touchPoints.size(); ++j) {
            if (m_touchPoints[j].id == points[i].id()) {
                m_touchPoints[j].currentPos = points[i].pos();
                emit touchMoved(points[i].pos().toPoint(), m_touchPoints[j].id);
                break;
            }
        }
    }

    if (m_touchPoints.size() == 1) {
        if (distance(m_touchPoints[0].startPos, m_touchPoints[0].currentPos) > m_tapRadius) {
            m_longPressTimer->stop();
        }
    }

    if (m_touchPoints.size() == 2) {
        detectGesture();
    }
}

void TouchHandler::handleTouchEnd(const QList<QTouchEvent::TouchPoint> &points)
{
    m_longPressTimer->stop();

    for (int i = 0; i < points.size(); ++i) {
        emit touchReleased(points[i].pos().toPoint(), points[i].id());
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (m_touchPoints.size() == 1 && !m_isLongPress) {
        QPointF start = m_touchPoints[0].startPos;
        QPointF end = m_touchPoints[0].currentPos;
        qreal dist = distance(start, end);

        if (dist < m_tapRadius) {
            qint64 timeDelta = now - m_lastTapTime;
            if (timeDelta < m_doubleTapInterval && distance(m_lastTapPos, end) < m_tapRadius) {
                m_isDoubleTap = true;
                m_currentGesture = DoubleTap;
                emit doubleTapDetected(end.toPoint());
                emit gestureDetected(DoubleTap);
                m_tapTimer->stop();
            } else {
                m_lastTapPos = end;
                m_lastTapTime = now;
                m_tapTimer->start();
            }
        } else if (dist > m_swipeDistance) {
            GestureType swipe = detectSwipe(start, end);
            m_currentGesture = swipe;
            emit swipeDetected(swipe);
            emit gestureDetected(swipe);
        }
    }

    m_touchPoints.clear();
}

void TouchHandler::detectGesture()
{
    if (m_touchPoints.size() != 2) return;

    QPointF p1Start = m_touchPoints[0].startPos;
    QPointF p2Start = m_touchPoints[1].startPos;
    QPointF p1Current = m_touchPoints[0].currentPos;
    QPointF p2Current = m_touchPoints[1].currentPos;

    qreal startDist = distance(p1Start, p2Start);
    qreal currentDist = distance(p1Current, p2Current);

    if (startDist > 0) {
        qreal scale = currentDist / startDist;
        if (scale < 0.85) {
            m_currentGesture = PinchIn;
            emit pinchDetected(scale);
            emit gestureDetected(PinchIn);
        } else if (scale > 1.15) {
            m_currentGesture = PinchOut;
            emit pinchDetected(scale);
            emit gestureDetected(PinchOut);
        }
    }

    qreal startAngle = angleBetween(p1Start, p2Start);
    qreal currentAngle = angleBetween(p1Current, p2Current);
    qreal angleDelta = currentAngle - startAngle;

    if (fabs(angleDelta) > 15.0) {
        m_currentGesture = Rotate;
        emit rotateDetected(angleDelta);
        emit gestureDetected(Rotate);
    }
}

TouchHandler::GestureType TouchHandler::detectSwipe(const QPointF &start, const QPointF &end)
{
    qreal dx = end.x() - start.x();
    qreal dy = end.y() - start.y();

    if (fabs(dx) > fabs(dy)) {
        return dx > 0 ? SwipeRight : SwipeLeft;
    } else {
        return dy > 0 ? SwipeDown : SwipeUp;
    }
}

qreal TouchHandler::distance(const QPointF &p1, const QPointF &p2) const
{
    qreal dx = p2.x() - p1.x();
    qreal dy = p2.y() - p1.y();
    return sqrt(dx * dx + dy * dy);
}

qreal TouchHandler::angleBetween(const QPointF &p1, const QPointF &p2) const
{
    qreal dx = p2.x() - p1.x();
    qreal dy = p2.y() - p1.y();
    return atan2(dy, dx) * 180.0 / M_PI;
}
