#include "courtmapwidget.h"
#include <QPaintEvent>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QRadialGradient>

CourtMapWidget::CourtMapWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    setMouseTracking(true);
    setMinimumSize(500, 500);
    initMapData();
    initObstacles();
}

void CourtMapWidget::initMapData()
{
    m_zones = {
        {"启停区1", QRectF(2100, 0, 300, 300), QColor(0, 50, 200), false},
        {"启停区2", QRectF(2100, 2100, 300, 300), QColor(0, 50, 200), false}
    };

    m_bufferCircles.clear();
    for (int i = 0; i < 3; ++i) {
        m_bufferCircles.append({
            QPointF(250, 670 + i * 230), 80, 40,
            QColor(20, 20, 20), QColor(255, 255, 255)
        });
    }

    m_processCircles.clear();
    for (int i = 0; i < 3; ++i) {
        m_processCircles.append({
            QPointF(900 + i * 300, 2200), 80, 40,
            QColor(20, 20, 20), QColor(255, 255, 255)
        });
    }
}

void CourtMapWidget::initObstacles()
{
    QRectF rects[] = {
        { 520,  300, 260, 220}, {1420,  300, 260, 220},
        { 350,  680, 240, 200}, {1020,  680, 240, 200}, {1730,  680, 240, 200},
        { 650, 1000, 220, 180}, {1050, 1000, 220, 180}, {1450, 1000, 220, 180},
        { 350, 1400, 240, 200}, {1050, 1400, 220, 200}, {1730, 1400, 240, 200},
        { 650, 1900, 220, 180}, {1350, 1900, 260, 220}
    };

    m_obstacles.clear();
    for (int i = 0; i < 13; ++i) {
        m_obstacles.append({i, rects[i], false});
    }
}

void CourtMapWidget::setMarkMode(bool enabled)
{
    m_markMode = enabled;
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void CourtMapWidget::setStartZoneSelectable(bool selectable)
{
    m_startZoneSelectable = selectable;
    if (!selectable) m_selectedStartZone = -1;
    update();
}

QString CourtMapWidget::selectedStartZoneName() const
{
    return (m_selectedStartZone >= 0 && m_selectedStartZone < m_zones.size())
           ? m_zones[m_selectedStartZone].name : QString();
}

int CourtMapWidget::markedCount() const
{
    int count = 0;
    for (const auto &obs : m_obstacles) {
        if (obs.isMarked) ++count;
    }
    return count;
}

QVector<ObstacleRect> CourtMapWidget::getMarkedObstacles() const
{
    QVector<ObstacleRect> result;
    for (const auto &obs : m_obstacles) {
        if (obs.isMarked) result.append(obs);
    }
    return result;
}

void CourtMapWidget::clearAllMarks()
{
    for (auto &obs : m_obstacles) obs.isMarked = false;
    update();
}

void CourtMapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    int w = width(), h = height();
    m_scale = qMin((w - 2*MARGIN) / MAP_SIZE, (h - 2*MARGIN) / MAP_SIZE);
    qreal drawW = MAP_SIZE * m_scale, drawH = MAP_SIZE * m_scale;
    m_mapRect = QRectF((w - drawW) / 2, (h - drawH) / 2, drawW, drawH);

    drawBackground(p);
    drawOuterFrame(p);
    drawCenterBlocks(p);
    drawCenterCross(p);
    drawRawMaterialArea(p);
    drawStartStopZones(p);
    drawBufferArea(p);
    drawRoughProcessArea(p);
    drawQRBoard(p);
    drawProgressLabel(p);
    drawObstacles(p);
    drawRobot(p);
    drawDimensionMarks(p);
}

void CourtMapWidget::drawBackground(QPainter &p)
{
    p.fillRect(rect(), Qt::white);
    p.fillRect(m_mapRect, QColor(230, 230, 225));
}

void CourtMapWidget::drawOuterFrame(QPainter &p)
{
    p.setPen(QPen(QColor(0, 50, 200), 4));
    p.setBrush(Qt::NoBrush);
    p.drawRect(m_mapRect);
}

void CourtMapWidget::drawRawMaterialArea(QPainter &p)
{
    QPointF center = mapToWidget(QPointF(1200, 220));
    qreal r = 100 * m_scale;

    QRadialGradient grad(center, r);
    grad.setColorAt(0, QColor(255, 255, 255));
    grad.setColorAt(0.85, QColor(240, 240, 235));
    grad.setColorAt(1, QColor(180, 180, 170));

    p.setBrush(grad);
    p.setPen(QPen(QColor(100, 100, 90), 2));
    p.drawEllipse(center, r, r);

    qreal holeR = r * 0.18, holeDist = r * 0.62;
    p.setBrush(QColor(60, 60, 55));
    p.setPen(Qt::NoPen);
    p.drawEllipse(center + QPointF(-holeDist, 0), holeR, holeR);
    p.drawEllipse(center + QPointF(holeDist, 0), holeR, holeR);
    p.drawEllipse(center + QPointF(0, holeDist), holeR, holeR);

    p.setBrush(QColor(200, 200, 190));
    p.setPen(QPen(QColor(150, 150, 140), 1));
    p.drawEllipse(center, r * 0.28, r * 0.28);

    p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    p.setPen(QColor(40, 40, 40));
    p.drawText(QRectF(m_mapRect.x(), m_mapRect.y() + r * 0.15, m_mapRect.width(), 20),
               Qt::AlignHCenter | Qt::AlignTop, "原料区");
}

void CourtMapWidget::drawStartStopZones(QPainter &p)
{
    p.setFont(QFont("Microsoft YaHei", 9, QFont::Bold));

    for (int i = 0; i < m_zones.size(); ++i) {
        const auto &zone = m_zones[i];
        QRectF r(mapToWidget(zone.rect.topLeft()), mapToWidget(zone.rect.bottomRight()));

        bool isSelected = (m_selectedStartZone == i);

        if (isSelected) {
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
        } else if (m_startZoneSelectable) {
            p.setBrush(QColor(0, 50, 200, 150));
            p.setPen(QPen(QColor(0, 200, 100), 3, Qt::DashLine));
            p.drawRect(r);
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, zone.name + "\n点击选择");
        } else {
            p.setBrush(zone.color);
            p.setPen(QPen(QColor(0, 30, 150), 2));
            p.drawRect(r);
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, zone.name);
        }
    }
}

void CourtMapWidget::drawBufferArea(QPainter &p)
{
    for (const auto &c : m_bufferCircles) {
        QPointF center = mapToWidget(c.center);
        drawConcentricCircle(p, center, c.outerRadius * m_scale, c.innerRadius * m_scale,
                             c.outerColor, c.innerColor);
    }

    p.save();
    p.translate(mapToWidget(QPointF(70, 900)));
    p.rotate(-90);
    p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    p.setPen(QColor(40, 40, 40));
    p.drawText(QRectF(-100, -15, 200, 30), Qt::AlignCenter, "暂存区");
    p.restore();
}

void CourtMapWidget::drawRoughProcessArea(QPainter &p)
{
    for (const auto &c : m_processCircles) {
        QPointF center = mapToWidget(c.center);
        drawConcentricCircle(p, center, c.outerRadius * m_scale, c.innerRadius * m_scale,
                             c.outerColor, c.innerColor);
    }

    p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    p.setPen(QColor(40, 40, 40));
    p.drawText(QRectF(mapToWidget(QPointF(1000, 2050)), mapToWidget(QPointF(1400, 2150))),
               Qt::AlignHCenter | Qt::AlignBottom, "粗加工区");
}

void CourtMapWidget::drawCenterBlocks(QPainter &p)
{
    p.setBrush(QColor(248, 248, 200));
    p.setPen(QPen(QColor(200, 200, 150), 1.5));

    constexpr qreal bw = 450, offset = 400;
    constexpr qreal blocks[4][4] = {
        {1200 - offset - bw, 1200 - offset - bw, bw, bw},
        {1200 + offset,       1200 - offset - bw, bw, bw},
        {1200 - offset - bw, 1200 + offset,       bw, bw},
        {1200 + offset,       1200 + offset,       bw, bw}
    };

    for (int i = 0; i < 4; ++i) {
        p.drawRect(QRectF(mapToWidget(QPointF(blocks[i][0], blocks[i][1])),
                          mapToWidget(QPointF(blocks[i][0] + blocks[i][2], blocks[i][1] + blocks[i][3]))));
    }
}

void CourtMapWidget::drawCenterCross(QPainter &p)
{
    p.setPen(QPen(QColor(120, 120, 120), 1.5, Qt::DashLine));
    p.drawLine(mapToWidget(QPointF(0, 1200)), mapToWidget(QPointF(2400, 1200)));
    p.drawLine(mapToWidget(QPointF(1200, 0)), mapToWidget(QPointF(1200, 2400)));
}

void CourtMapWidget::drawQRBoard(QPainter &p)
{
    p.setPen(QPen(QColor(80, 80, 80), 2));
    p.drawLine(mapToWidget(QPointF(2360, 450)), mapToWidget(QPointF(2360, 1950)));

    p.save();
    p.translate(mapToWidget(QPointF(2370, 1200)));
    p.rotate(-90);
    p.setFont(QFont("Microsoft YaHei", 9, QFont::Bold));
    p.setPen(QColor(50, 50, 50));
    p.drawText(QRectF(-100, -15, 200, 30), Qt::AlignCenter, "二维码板");
    p.restore();
}

void CourtMapWidget::drawProgressLabel(QPainter &p)
{
    QPointF center = mapToWidget(QPointF(1200, 1200));
    qreal barW = 180 * m_scale, barH = 50 * m_scale;
    QRectF barRect(center.x() - barW/2, center.y() - barH/2, barW, barH);

    p.setBrush(QColor(70, 70, 70, 230));
    p.setPen(QPen(QColor(50, 50, 50), 2));
    p.drawRoundedRect(barRect, barH * 0.3, barH * 0.3);

    p.setFont(QFont("Arial", qMax(10, int(20 * m_scale)), QFont::Bold));
    p.setPen(Qt::white);
    p.drawText(barRect, Qt::AlignCenter, "81%");
}

void CourtMapWidget::drawObstacles(QPainter &p)
{
    for (const auto &obs : m_obstacles) {
        QRectF r(mapToWidget(obs.rect.topLeft()), mapToWidget(obs.rect.bottomRight()));

        if (obs.isMarked) {
            p.setBrush(QColor(220, 30, 30, 180));
            p.setPen(QPen(QColor(180, 20, 20), 4));
            p.drawRect(r);
            p.setFont(QFont("Arial", qMax(8, int(14 * m_scale)), QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, "障碍");
        } else if (m_markMode) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(220, 40, 40), 2));
            p.drawRect(r);
        }
    }
}

void CourtMapWidget::drawRobot(QPainter &p)
{
    QPointF pos = mapToWidget(QPointF(1200, 1200));
    qreal r = qBound(15.0, 40 * m_scale, 50.0);

    QRadialGradient grad(pos, r);
    grad.setColorAt(0, QColor(255, 90, 90));
    grad.setColorAt(0.6, QColor(220, 40, 40));
    grad.setColorAt(1, QColor(150, 20, 20));

    p.setBrush(grad);
    p.setPen(QPen(QColor(120, 10, 10), 2));
    p.drawEllipse(pos, r, r);
    p.setPen(QPen(Qt::white, 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(pos, r * 0.45, r * 0.45);
}

void CourtMapWidget::drawDimensionMarks(QPainter &p)
{
    p.setFont(QFont("Arial", 8));
    p.setPen(QColor(60, 60, 60));

    drawHDimension(p, 0, 150, -40, "150");
    drawHDimension(p, 2100, 2400, -40, "300");
    drawHDimension(p, 0, 2400, MAP_SIZE + 40, "2400");

    drawVDimension(p, -60, 76, 300, "75.85");
    drawVDimension(p, -60, 540, 1120, "580");
    drawVDimension(p, 2440, 0, 300, "300");

    p.drawText(QRectF(mapToWidget(QPointF(1650, -15)), QSizeF(200, 18)), Qt::AlignLeft, "1100-1300");
}

void CourtMapWidget::drawHDimension(QPainter &p, qreal x1, qreal x2, qreal y, const QString &text)
{
    QPointF p1 = mapToWidget(QPointF(x1, y)), p2 = mapToWidget(QPointF(x2, y));

    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.drawLine(p1.x(), p1.y(), p2.x(), p2.y());
    p.drawLine(p1.x(), p1.y() - 6, p1.x(), p1.y() + 6);
    p.drawLine(p2.x(), p2.y() - 6, p2.x(), p2.y() + 6);

    if (!text.isEmpty()) {
        p.drawText(QRectF(p1.x(), p1.y() - 12, p2.x() - p1.x(), 16), Qt::AlignCenter, text);
    }
}

void CourtMapWidget::drawVDimension(QPainter &p, qreal x, qreal y1, qreal y2, const QString &text)
{
    QPointF p1 = mapToWidget(QPointF(x, y1)), p2 = mapToWidget(QPointF(x, y2));

    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.drawLine(p1.x(), p1.y(), p2.x(), p2.y());
    p.drawLine(p1.x() - 6, p1.y(), p1.x() + 6, p1.y());
    p.drawLine(p2.x() - 6, p2.y(), p2.x() + 6, p2.y());

    if (!text.isEmpty()) {
        p.drawText(QRectF(p1.x() - 35, (p1.y() + p2.y()) / 2 - 8, 30, 16), Qt::AlignRight | Qt::AlignVCenter, text);
    }
}

void CourtMapWidget::drawConcentricCircle(QPainter &p, const QPointF &center, qreal outerR, qreal innerR,
                                           const QColor &outerColor, const QColor &innerColor)
{
    p.setBrush(outerColor);
    p.setPen(QPen(QColor(30, 30, 30), 1));
    p.drawEllipse(center, outerR, outerR);

    p.setBrush(innerColor);
    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.drawEllipse(center, innerR, innerR);

    p.setBrush(QColor(20, 20, 20));
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, innerR * 0.3, innerR * 0.3);
}

QPointF CourtMapWidget::mapToWidget(const QPointF &mapPoint) const
{
    return QPointF(m_mapRect.x() + mapPoint.x() * m_scale, m_mapRect.y() + mapPoint.y() * m_scale);
}

QPointF CourtMapWidget::widgetToMap(const QPointF &widgetPoint) const
{
    return (m_scale > 0) ? QPointF((widgetPoint.x() - m_mapRect.x()) / m_scale,
                                    (widgetPoint.y() - m_mapRect.y()) / m_scale) : QPointF();
}

int CourtMapWidget::findObstacleAt(const QPointF &point) const
{
    QPointF mp = widgetToMap(point);
    for (int i = 0; i < m_obstacles.size(); ++i) {
        if (m_obstacles[i].rect.contains(mp)) return i;
    }
    return -1;
}

int CourtMapWidget::findStartZoneAt(const QPointF &point) const
{
    QPointF mp = widgetToMap(point);
    for (int i = 0; i < m_zones.size(); ++i) {
        if (m_zones[i].rect.contains(mp)) return i;
    }
    return -1;
}

void CourtMapWidget::handlePointSelection(const QPointF &pos)
{
    if (m_startZoneSelectable && !m_markMode) {
        int idx = findStartZoneAt(pos);
        if (idx >= 0) {
            m_selectedStartZone = idx;
            emit startZoneSelected(idx, m_zones[idx].name);
            update();
            return;
        }
    }

    if (m_markMode) {
        int idx = findObstacleAt(pos);
        if (idx >= 0) {
            m_obstacles[idx].isMarked = !m_obstacles[idx].isMarked;
            emit obstacleToggled(idx, m_obstacles[idx].isMarked);
            update();
        }
    }
}

void CourtMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    handlePointSelection(event->pos());
    QWidget::mouseReleaseEvent(event);
}

void CourtMapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

bool CourtMapWidget::event(QEvent *event)
{
    if (event->type() == QEvent::TouchEnd) {
        QTouchEvent *te = static_cast<QTouchEvent*>(event);
        if (te && !te->touchPoints().isEmpty()) {
            handlePointSelection(te->touchPoints().first().pos());
        }
        return true;
    }
    return QWidget::event(event);
}