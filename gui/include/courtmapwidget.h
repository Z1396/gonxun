#ifndef COURTMAPWIDGET_H
#define COURTMAPWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QVector>
#include <QString>
#include <QPointF>
#include <QRectF>

struct CourtZone {
    QString name;
    QRectF rect;
    QColor color;
    bool isSelected;
};

struct CourtCircle {
    QPointF center;
    qreal outerRadius;
    qreal innerRadius;
    QColor outerColor;
    QColor innerColor;
};

struct ObstacleRect {
    int id;
    QRectF rect;
    bool isMarked;
};

class CourtMapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CourtMapWidget(QWidget *parent = nullptr);
    ~CourtMapWidget() = default;

    void setMarkMode(bool enabled);
    bool isMarkMode() const { return m_markMode; }

    void setStartZoneSelectable(bool selectable);
    bool isStartZoneSelectable() const { return m_startZoneSelectable; }

    int selectedStartZone() const { return m_selectedStartZone; }
    QString selectedStartZoneName() const;

    int markedCount() const;
    QVector<ObstacleRect> getMarkedObstacles() const;
    void clearAllMarks();

signals:
    void obstacleToggled(int id, bool marked);
    void startZoneSelected(int zoneIndex, const QString &zoneName);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;

private:
    void initMapData();
    void initObstacles();

    void drawBackground(QPainter &p);
    void drawOuterFrame(QPainter &p);
    void drawRawMaterialArea(QPainter &p);
    void drawStartStopZones(QPainter &p);
    void drawBufferArea(QPainter &p);
    void drawRoughProcessArea(QPainter &p);
    void drawCenterBlocks(QPainter &p);
    void drawCenterCross(QPainter &p);
    void drawQRBoard(QPainter &p);
    void drawProgressLabel(QPainter &p);
    void drawRobot(QPainter &p);
    void drawObstacles(QPainter &p);
    void drawDimensionMarks(QPainter &p);

    void drawHDimension(QPainter &p, qreal x1, qreal x2, qreal y, const QString &text);
    void drawVDimension(QPainter &p, qreal x, qreal y1, qreal y2, const QString &text);
    void drawConcentricCircle(QPainter &p, const QPointF &center, qreal outerR, qreal innerR,
                              const QColor &outerColor, const QColor &innerColor);

    QPointF mapToWidget(const QPointF &mapPoint) const;
    QPointF widgetToMap(const QPointF &widgetPoint) const;
    int findObstacleAt(const QPointF &point) const;
    int findStartZoneAt(const QPointF &point) const;
    void handlePointSelection(const QPointF &pos);

    static constexpr qreal MAP_SIZE = 2400.0;
    static constexpr qreal MARGIN = 80.0;

    QRectF m_mapRect;
    qreal m_scale = 1.0;

    QVector<CourtZone> m_zones;
    QVector<CourtCircle> m_bufferCircles;
    QVector<CourtCircle> m_processCircles;
    QVector<ObstacleRect> m_obstacles;

    bool m_markMode = false;
    bool m_startZoneSelectable = false;
    int m_selectedStartZone = -1;
};

#endif