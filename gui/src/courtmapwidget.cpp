// 引入自身头文件，包含类定义、结构体、成员变量、信号声明
#include "courtmapwidget.h"
// Qt绘图事件基类，窗口重绘时触发paintEvent
#include <QPaintEvent>
// 鼠标点击/移动事件
#include <QMouseEvent>
// 触摸屏触摸事件（适配5/7  寸触控屏）
#include <QTouchEvent>
// 径向渐变，用于原料区、机器人圆形渐变填充
#include <QRadialGradient>

// 构造函数：画布初始化
CourtMapWidget::CourtMapWidget(QWidget *parent)
    : QWidget(parent)
{
    // 开启接收触屏事件，适配外接触摸显示屏
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    // 开启鼠标实时追踪，鼠标不按下也能捕获坐标（可选拓展悬浮高亮）
    setMouseTracking(true);
    // 设置画布最小宽高，窗口缩到再小也不会小于500*500像素
    setMinimumSize(500, 500);
    // 初始化场地基础区域数据（启停区、暂存圆、粗加工圆）
    initMapData();
    // 初始化所有障碍物矩形数据
    initObstacles();
}

// 初始化场地静态数据：启停区、暂存圆形点位、粗加工圆形点位
void CourtMapWidget::initMapData()
{
    // 清空启停区数组，填入两个出生区域：名称、场地矩形、填充色、是否选中标记
    m_zones = {
        {"启停区1", QRectF(2100, 0, 300, 300), QColor(0, 50, 200), false},
        {"启停区2", QRectF(2100, 2100, 300, 300), QColor(0, 50, 200), false}
    };

    m_bufferCircles.clear();
    // 暂存区：场地左侧，3个同心圆竖排
    // 整体区域宽300高580，外圆半径80，三圆均匀分布（间距50）
    // 圆心x=150（区域中心），顶部起点y=350
    for (int i = 0; i < 3; ++i) {
        m_bufferCircles.append({
            QPointF(75, 1050 + i * 150), 25, 20,
            QColor(20, 20, 20), QColor(255, 255, 255)
        });
    }

    m_processCircles.clear();
    // 粗加工区：场地底部，3个同心圆横排
    // 底部边距150，外圆半径80，三圆均匀分布（间距50）
    // 圆心y=2170（2400-150-80），整体居中
    for (int i = 0; i < 3; ++i) {
        m_processCircles.append({
            QPointF(1050 + i * 150, 2325), 25, 20,
            QColor(20, 20, 20), QColor(255, 255, 255)
        });
    }
}

// 初始化全部障碍物矩形集合
void CourtMapWidget::initObstacles()
{
    // 场地内所有障碍物的世界坐标矩形数组
    QRectF rects[] = {
        { 200, 100, 300, 300},  { 570,  100, 300, 300},                         {1490,  100, 300, 300},
        { 200,  610, 300, 300},                         {1020,  610, 300, 300},                        {1890,  610, 300, 300},
                                { 570, 1000, 300, 300}, {1050, 1000, 300, 300}, {1490, 1000, 300, 300},
        { 200, 1450, 300, 300},                         {1050, 1450, 300, 300},                        {1890, 1450, 300, 300},
        { 200, 1900, 300, 300}, { 570, 1900, 300, 300},                         {1490, 1900, 300, 300}
    };

    m_obstacles.clear();
    // 遍历15个障碍物，存入结构体：序号、矩形、是否被标记
    for (int i = 0; i < 15; ++i) {
        m_obstacles.append({i, rects[i], false});
    }
}

// 切换标记模式：开启后点击障碍物可红标标记
void CourtMapWidget::setMarkMode(bool enabled)
{
    m_markMode = enabled;
    // 标记模式鼠标变为十字准星，普通模式恢复箭头
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
    update(); // 重绘画布，刷新障碍物显示样式
}

// 控制是否允许点击选择启停出生区
void CourtMapWidget::setStartZoneSelectable(bool selectable)
{
    m_startZoneSelectable = selectable;
    // 关闭选择功能时，清空已选中区域下标
    if (!selectable) m_selectedStartZone = -1;
    update(); // 刷新启停区绘制样式
}

// 获取当前选中启停区的文字名称，无选中返回空字符串
QString CourtMapWidget::selectedStartZoneName() const
{
    return (m_selectedStartZone >= 0 && m_selectedStartZone < m_zones.size())
           ? m_zones[m_selectedStartZone].name : QString();
}

// 统计当前被标记的障碍物总数量，供状态栏显示
int CourtMapWidget::markedCount() const
{
    int count = 0;
    for (const auto &obs : m_obstacles) {
        if (obs.isMarked) ++count;
    }
    return count;
}

// 返回所有已标记障碍物完整数据，可传给上层做路径规划、仿真
QVector<ObstacleRect> CourtMapWidget::getMarkedObstacles() const
{
    QVector<ObstacleRect> result;
    for (const auto &obs : m_obstacles) {
        if (obs.isMarked) result.append(obs);
    }
    return result;
}

// 清空全部障碍物标记状态
void CourtMapWidget::clearAllMarks()
{
    for (auto &obs : m_obstacles) obs.isMarked = false;
    update(); // 刷新画布
}

// 核心绘图事件：窗口大小变化/调用update()自动执行，分层绘制整张场地
void CourtMapWidget::paintEvent(QPaintEvent *event)
{
    // 未使用事件参数，消除编译警告
    Q_UNUSED(event)
    // 创建画笔对象，所有绘制操作依托p完成
    QPainter p(this);
    // 开启抗锯齿，线条、圆形、文字无锯齿
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    int w = width(), h = height();
    // 计算缩放系数m_scale：MAP_SIZE=场地世界总尺寸，四周留MARGIN边距
    // 取宽高缩放最小值，保证场地完整显示不拉伸变形
    m_scale = qMin((w - 2*MARGIN) / MAP_SIZE, (h - 2*MARGIN) / MAP_SIZE);
    qreal drawW = MAP_SIZE * m_scale, drawH = MAP_SIZE * m_scale;
    // 计算场地画布在控件内居中的矩形区域
    m_mapRect = QRectF((w - drawW) / 2, (h - drawH) / 2, drawW, drawH);

    // 分层绘制：由底层到上层，顺序不能乱（上层覆盖下层）
    drawBackground(p);        // 背景底色
    drawOuterFrame(p);        // 场地外边框
    drawCenterBlocks(p);     // 中心四块方块障碍
    drawCenterCross(p);       // 中心十字虚线坐标轴
    drawRawMaterialArea(p);   // 原料渐变圆形区域
    drawStartStopZones(p);    // 启停出生选择区
    drawBufferArea(p);        // 暂存区同心圆
    drawRoughProcessArea(p);  // 粗加工区同心圆
    drawQRBoard(p);           // 侧边二维码板竖线+文字
    drawObstacles(p);         // 所有障碍物矩形（可标记变红）
    drawDimensionMarks(p);    // 尺寸标注线与文字
}

// 绘制背景：控件整体白底 + 场地浅灰底色
void CourtMapWidget::drawBackground(QPainter &p)
{
    p.fillRect(rect(), Qt::white);
    p.fillRect(m_mapRect, QColor(230, 230, 225));
}

// 绘制场地外围蓝色粗边框
void CourtMapWidget::drawOuterFrame(QPainter &p)
{
    p.setPen(QPen(QColor(0, 50, 200), 4));
    p.setBrush(Qt::NoBrush); // 只描边不填充
    p.drawRect(m_mapRect);
}

// 绘制原料区径向渐变大圆，内部三个小孔、中心小圆
void CourtMapWidget::drawRawMaterialArea(QPainter &p)
{
    // 世界坐标转控件像素坐标：顶部中心
    // 托盘半径150mm，部分超出场地边界，只有约80像素在地图内
    QPointF center = mapToWidget(QPointF(1200, -70));
    qreal trayR = 150 * m_scale;   // 托盘（大圆）半径：150mm

    // 径向渐变：中心白→浅灰→深灰边缘
    QRadialGradient grad(center, trayR);
    grad.setColorAt(0, QColor(255, 255, 255));
    grad.setColorAt(0.85, QColor(240, 240, 235));
    grad.setColorAt(1, QColor(180, 180, 170));

    p.setBrush(grad);
    p.setPen(QPen(QColor(100, 100, 90), 2));
    p.drawEllipse(center, trayR, trayR);

    // 绘制内部三个物块：均匀120度分布，围成圆圈半径100mm
    qreal holeDist = 100 * m_scale;  // 物块距离中心的半径：100mm
    qreal holeR = trayR * 0.18;       // 物块自身半径
    p.setBrush(QColor(60, 60, 55));
    p.setPen(Qt::NoPen);
    // 三个物块均匀分布：90°（下）、210°（左上）、330°（右上）
    p.drawEllipse(center + QPointF(0, holeDist), holeR, holeR);                          // 90° 下方
    p.drawEllipse(center + QPointF(-holeDist * 0.866, -holeDist * 0.5), holeR, holeR);   // 210° 左上
    p.drawEllipse(center + QPointF(holeDist * 0.866, -holeDist * 0.5), holeR, holeR);    // 330° 右上

    // 原料区顶部文字
    p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    p.setPen(QColor(40, 40, 40));
    p.drawText(QRectF(m_mapRect.x()-40, m_mapRect.y()-22, m_mapRect.width(), 20),
               Qt::AlignHCenter | Qt::AlignTop, "原料区");
}

// 绘制启停出生区，分三种状态：已选中 / 可点击未选 / 不可选择普通显示
void CourtMapWidget::drawStartStopZones(QPainter &p)
{
    p.setFont(QFont("Microsoft YaHei", 9, QFont::Bold));

    for (int i = 0; i < m_zones.size(); ++i) {
        const auto &zone = m_zones[i];
        // 世界矩形转画布像素矩形
        QRectF r(mapToWidget(zone.rect.topLeft()), mapToWidget(zone.rect.bottomRight()));

        bool isSelected = (m_selectedStartZone == i);

        // 状态1：当前被选中，绿色填充+白色边框，带★标记
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

            // 中心白色小圆标记
            p.setBrush(QColor(50, 220, 100));
            p.setPen(QPen(Qt::white, 2));
            p.drawEllipse(r.center() + QPointF(0, -r.height() * 0.35), 12, 12);
        }
        // 状态2：允许点击选择，蓝色半透明+绿色虚线边框，提示点击
        else if (m_startZoneSelectable) {
            p.setBrush(QColor(0, 50, 200, 150));
            p.setPen(QPen(QColor(0, 200, 100), 3, Qt::DashLine));
            p.drawRect(r);
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, zone.name + "\n点击选择");
        }
        // 状态3：普通静态展示，纯色填充无交互提示
        else {
            p.setBrush(zone.color);
            p.setPen(QPen(QColor(0, 30, 150), 2));
            p.drawRect(r);
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, zone.name);
        }
    }
}

// 绘制全部暂存区同心圆，左侧竖排文字标注
void CourtMapWidget::drawBufferArea(QPainter &p)
{
    // 暂存区长方形区域：宽150，高580（包含三个圆）
    QRectF bufferRect = QRectF(mapToWidget(QPointF(0, 910)), 
                               mapToWidget(QPointF(150, 1490)));
    p.setBrush(QColor(220, 220, 215));
    p.setPen(QPen(QColor(150, 150, 145), 2));
    p.drawRect(bufferRect);

    // 绘制三个同心圆
    for (const auto &c : m_bufferCircles) {
        QPointF center = mapToWidget(c.center);
        drawConcentricCircle(p, center, c.outerRadius * m_scale, c.innerRadius * m_scale,
                             c.outerColor, c.innerColor);
    }

    // 保存绘图状态，平移旋转绘制竖排文字（在同心圆右侧）
    p.save();
    p.translate(mapToWidget(QPointF(70, 850)));
    p.rotate(-90);
    p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    p.setPen(QColor(40, 40, 40));
    p.drawText(QRectF(-100, -15, 200, 30), Qt::AlignCenter, "暂存区");
    p.restore(); // 恢复平移旋转状态，不影响后续绘制
}

// 绘制粗加工区同心圆，底部文字标注
void CourtMapWidget::drawRoughProcessArea(QPainter &p)
{
    // 粗加工区长方形区域：宽580，高150（包含三个圆）
    QRectF processRect = QRectF(mapToWidget(QPointF(910, 2250)), 
                                mapToWidget(QPointF(1490, 2400)));
    p.setBrush(QColor(220, 220, 215));
    p.setPen(QPen(QColor(150, 150, 145), 2));
    p.drawRect(processRect);

    // 绘制三个同心圆
    for (const auto &c : m_processCircles) {
        QPointF center = mapToWidget(c.center);
        drawConcentricCircle(p, center, c.outerRadius * m_scale, c.innerRadius * m_scale,
                             c.outerColor, c.innerColor);
    }

    p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    p.setPen(QColor(40, 40, 40));
    p.drawText(QRectF(m_mapRect.x()-60, m_mapRect.bottom() - 25, 
                    m_mapRect.width(), 20),
            Qt::AlignHCenter | Qt::AlignBottom, "粗加工区");
}

// 绘制场地中心四块方形障碍物
void CourtMapWidget::drawCenterBlocks(QPainter &p)
{
    p.setBrush(QColor(248, 248, 200));
    p.setPen(QPen(QColor(200, 200, 150), 1.5));

    // 四块方块世界坐标 (x, y, width, height)，每块 450×450
    constexpr qreal blocks[4][4] = {
        { 550,  550, 450, 450},  // 左上方块
        {1400,  550, 450, 450},  // 右上方块
        { 550, 1400, 450, 450},  // 左下方块
        {1400, 1400, 450, 450}   // 右下方块
    };

    for (int i = 0; i < 4; ++i) {
        // 坐标转换后绘制矩形
        p.drawRect(QRectF(mapToWidget(QPointF(blocks[i][0], blocks[i][1])),
                          mapToWidget(QPointF(blocks[i][0] + blocks[i][2], blocks[i][1] + blocks[i][3]))));
    }
}

// 绘制场地中心横竖虚线十字坐标轴
void CourtMapWidget::drawCenterCross(QPainter &p)
{
    p.setPen(QPen(QColor(120, 120, 120), 1.5, Qt::DashLine));
    p.drawLine(mapToWidget(QPointF(0, 1200)), mapToWidget(QPointF(2400, 1200)));
    p.drawLine(mapToWidget(QPointF(1200, 0)), mapToWidget(QPointF(1200, 2400)));
}

// 绘制右侧二维码竖线+竖向文字
void CourtMapWidget::drawQRBoard(QPainter &p)
{
    // 二维码板在右侧，纵向范围约1100-1300
    p.setPen(QPen(QColor(80, 80, 80), 2));
    p.drawLine(mapToWidget(QPointF(2360, 1100)), mapToWidget(QPointF(2360, 1300)));

    p.save();
    p.translate(mapToWidget(QPointF(2375, 1200)));
    p.rotate(-90);
    p.setFont(QFont("Microsoft YaHei", 9, QFont::Bold));
    p.setPen(QColor(50, 50, 50));
    p.drawText(QRectF(-100, 15, 200, 30), Qt::AlignCenter, "二维码板");
    p.restore();
}

// 绘制所有障碍物：标记状态红色填充，标记模式只描红边
void CourtMapWidget::drawObstacles(QPainter &p)
{
    for (const auto &obs : m_obstacles) {
        QRectF r(mapToWidget(obs.rect.topLeft()), mapToWidget(obs.rect.bottomRight()));

        // 已标记：半透红填充、粗红边框、白色“障碍”文字
        if (obs.isMarked) {
            p.setBrush(QColor(220, 30, 30, 180));
            p.setPen(QPen(QColor(180, 20, 20), 4));
            p.drawRect(r);
            p.setFont(QFont("Arial", qMax(8, int(14 * m_scale)), QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, "障碍");
        }
        // 开启标记模式未选中：仅红色细边框，无填充
        else if (m_markMode) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(220, 40, 40), 2));
            p.drawRect(r);
        }
    }
}

// 绘制场地长宽尺寸标注线与文字
void CourtMapWidget::drawDimensionMarks(QPainter &p)
{
    p.setFont(QFont("Arial", 8));
    p.setPen(QColor(60, 60, 60));

    // 顶部水平尺寸线
    drawHDimension(p, 0, 150, -30, "150");
    drawHDimension(p, 1100, 1300, -30, "1100-1300");
    drawHDimension(p, 2100, 2400, -30, "300");

    // 底部水平尺寸线
    drawHDimension(p, 0, 150, MAP_SIZE + 30, "150");
    drawHDimension(p, 0, 2400, MAP_SIZE + 60, "2400");

    // 左侧垂直尺寸线
    drawVDimension(p, -30, 0, 300, "300");
    drawVDimension(p, -30, 350, 930, "580");

    // 右侧垂直尺寸线
    drawVDimension(p, 2430, 0, 300, "300");
    drawVDimension(p, 2430, 1100, 1300, "1100-1300");
}

// 绘制水平尺寸标注辅助函数：线段+两端短竖线+中间文字
void CourtMapWidget::drawHDimension(QPainter &p, qreal x1, qreal x2, qreal y, const QString &text)
{
    QPointF p1 = mapToWidget(QPointF(x1, y)), p2 = mapToWidget(QPointF(x2, y));

    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.drawLine(p1.x(), p1.y(), p2.x(), p2.y());
    // 两端刻度短线
    p.drawLine(p1.x(), p1.y() - 6, p1.x(), p1.y() + 6);
    p.drawLine(p2.x(), p2.y() - 6, p2.x(), p2.y() + 6);

    if (!text.isEmpty()) {
        p.drawText(QRectF(p1.x(), p1.y() - 12, p2.x() - p1.x(), 16), Qt::AlignCenter, text);
    }
}

// 绘制垂直尺寸标注辅助函数：竖线+两端横线+侧边文字
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

// 通用同心圆绘制工具：外圈+内圈+中心小黑点
void CourtMapWidget::drawConcentricCircle(QPainter &p, const QPointF &center, qreal outerR, qreal innerR,
                                           const QColor &outerColor, const QColor &innerColor)
{
    // 外圈大圆
    p.setBrush(outerColor);
    p.setPen(QPen(QColor(30, 30, 30), 1));
    p.drawEllipse(center, outerR, outerR);

    // 内圈小圆
    p.setBrush(innerColor);
    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.drawEllipse(center, innerR, innerR);

    // 中心黑点
    p.setBrush(QColor(20, 20, 20));
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, innerR * 0.3, innerR * 0.3);
}

// 世界场地坐标 → 控件屏幕像素坐标（核心坐标转换）
QPointF CourtMapWidget::mapToWidget(const QPointF &mapPoint) const
{
    return QPointF(m_mapRect.x() + mapPoint.x() * m_scale, m_mapRect.y() + mapPoint.y() * m_scale);
}

// 控件屏幕像素坐标 → 场地世界坐标（鼠标/触屏点击时反向换算）
QPointF CourtMapWidget::widgetToMap(const QPointF &widgetPoint) const
{
    // 防止缩放为0除零崩溃
    return (m_scale > 0) ? QPointF((widgetPoint.x() - m_mapRect.x()) / m_scale,
                                    (widgetPoint.y() - m_mapRect.y()) / m_scale) : QPointF();
}

// 根据点击像素，查找点击到的障碍物下标，无返回-1
int CourtMapWidget::findObstacleAt(const QPointF &point) const
{
    QPointF mp = widgetToMap(point);
    for (int i = 0; i < m_obstacles.size(); ++i) {
        if (m_obstacles[i].rect.contains(mp)) return i;
    }
    return -1;
}

// 根据点击像素，查找点击到的启停区下标，无返回-1
int CourtMapWidget::findStartZoneAt(const QPointF &point) const
{
    QPointF mp = widgetToMap(point);
    for (int i = 0; i < m_zones.size(); ++i) {
        if (m_zones[i].rect.contains(mp)) return i;
    }
    return -1;
}

// 统一处理点击逻辑：区分选启停区 / 标记障碍物
void CourtMapWidget::handlePointSelection(const QPointF &pos)
{
    // 允许选区且不在标记模式：点击启停区则选中并发射信号
    if (m_startZoneSelectable && !m_markMode) {
        int idx = findStartZoneAt(pos);
        if (idx >= 0) {
            m_selectedStartZone = idx;
            emit startZoneSelected(idx, m_zones[idx].name);
            update();
            return;
        }
    }

    // 标记模式：点击障碍物切换标记状态，发射信号通知上层
    if (m_markMode) {
        int idx = findObstacleAt(pos);
        if (idx >= 0) {
            m_obstacles[idx].isMarked = !m_obstacles[idx].isMarked;
            emit obstacleToggled(idx, m_obstacles[idx].isMarked);
            update();
        }
    }
}

// 鼠标松开事件：触发点击判定逻辑
void CourtMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    handlePointSelection(event->pos());
    QWidget::mouseReleaseEvent(event);
}

// 窗口大小改变自动重绘画布，自适应缩放
void CourtMapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

// 全局事件分发：拦截触屏抬起事件，兼容触摸屏点击
bool CourtMapWidget::event(QEvent *event)
{
    if (event->type() == QEvent::TouchEnd) {
        QTouchEvent *te = static_cast<QTouchEvent*>(event);
        if (te && !te->touchPoints().isEmpty()) {
            // 取第一根触摸点坐标执行点击逻辑
            handlePointSelection(te->touchPoints().first().pos());
        }
        return true;
    }
    // 其他事件交给Qt默认处理
    return QWidget::event(event);
}