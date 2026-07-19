/**
 * @file courtmapwidget.cpp
 * @brief 场地地图控件实现文件
 * 
 * @details 本文件实现了智能物流搬运系统的核心可视化控件。
 *          核心功能：
 *          - 场地绘制：启停区、暂存区、粗加工区、障碍物、网格
 *          - 机器人显示：圆形机器人、方向箭头、轨迹路径
 *          - 用户交互：鼠标/触摸点击标记障碍物、选择启停区
 *          - 坐标系统：世界坐标（mm）与屏幕坐标（pixel）转换
 *          
 *          场地布局（2400x2400 mm）：
 *          - 启停区：右上角（2100-2400, 0-300）、右下角（2100-2400, 2100-2400）
 *          - 暂存区：左侧（0-300, 900-1500），3个圆形竖排
 *          - 粗加工区：底部（900-1500, 2100-2400），3个圆形横排
 *          - 障碍物：15个矩形区域，按照比赛规则分布
 *          
 *          绘图机制：
 *          - 重写 paintEvent() 实现自定义绘制
 *          - 使用 QPainter 绘制各种图形
 *          - 使用 QRadialGradient 绘制渐变效果
 *          - 使用 QPainterPath 绘制路径和箭头
 *          
 *          事件处理：
 *          - 鼠标事件：QMouseEvent（点击、移动）
 *          - 触摸事件：QTouchEvent（适配5/7寸触控屏）
 *          - 支持标记障碍物模式和选择启停区模式
 *          
 *          坐标转换：
 *          - 世界坐标（mm）→ 屏幕坐标：worldToScreen()
 *          - 屏幕坐标 → 世界坐标：screenToWorld()
 *          - 缩放因子：根据窗口尺寸动态计算
 *          
 * @see courtmapwidget.h 头文件定义
 * @see mainwindow.cpp 使用此控件的主窗口
 * @see touchhandler.h 触摸事件处理器
 * 
 * @author 工创赛2025智能物流搬运系统团队
 * @date 2024-01-15
 * @version 1.0.0
 * @history 2024-01-15 初始版本
 * @history 2024-02-10 新增触摸事件支持
 * 
 * @copyright 工创赛2025智能物流搬运系统
 */
#include "courtmapwidget.h"
#include <QPaintEvent>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QRadialGradient>
#include <QPainterPath>

/**
 * @brief CourtMapWidget 构造函数
 * 
 * @details 初始化场地地图控件，完成以下工作：
 *          1. 开启触摸事件支持
 *          2. 开启鼠标实时追踪
 *          3. 设置最小尺寸
 *          4. 初始化场地数据（启停区、暂存区、粗加工区）
 *          5. 初始化障碍物数据
 *          
 * @param parent 父窗口对象（默认 nullptr）
 */
CourtMapWidget::CourtMapWidget(QWidget *parent)
    : QWidget(parent)
{
    // 开启接收触屏事件，适配外接触摸显示屏
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    // 开启鼠标实时追踪，鼠标不按下也能捕获坐标
    setMouseTracking(true);
    // 设置画布最小宽高
    setMinimumSize(500, 500);
    // 初始化场地基础区域数据
    initMapData();
    // 初始化所有障碍物矩形数据
    initObstacles();
    // 初始化5×5网格格子数据
    initGrid5();
}

/**
 * @brief 初始化场地静态数据
 * 
 * @details 初始化启停区、暂存区、粗加工区的坐标和属性。
 *          - 启停区：2个，位于场地右侧
 *          - 暂存区：3个圆形，位于场地左侧竖排
 *          - 粗加工区：3个圆形，位于场地底部横排
 */
void CourtMapWidget::initMapData()
{
    // ==================== 启停区初始化 ====================
    // 启停区1：右上角（2100-2400, 0-300）
    // 启停区2：右下角（2100-2400, 2100-2400）
    m_zones = {
        {"启停区1", QRectF(2100, 0, 300, 300), QColor(0, 50, 200), false},
        {"启停区2", QRectF(2100, 2100, 300, 300), QColor(0, 50, 200), false}
    };

    // ==================== 暂存区初始化 ====================
    // 暂存区：场地左侧，3个同心圆竖排
    // 圆心x=75（区域中心），顶部起点y=1050
    // 间距：150mm
    m_bufferCircles.clear();
    for (int i = 0; i < 3; ++i) {
        m_bufferCircles.append({
            QPointF(75, 1050 + i * 150), 25, 20,
            QColor(20, 20, 20), QColor(255, 255, 255)
        });
    }

    // ==================== 粗加工区初始化 ====================
    // 粗加工区：场地底部，3个同心圆横排
    // 圆心y=2325（2400-150-25），整体居中
    // 间距：150mm
    m_processCircles.clear();
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
        { 0, 0, 550, 550},     { 550,  0, 450, 550},                           {1400,  0, 450, 550},
        { 0,  550, 550, 360},                          {1000,  550, 400, 450},                        {1850,  550, 550, 450},
                              { 550, 1000, 450, 400}, {1000, 1000, 400, 400}, {1400, 1000, 450, 400},
        { 0, 1490, 550, 360},                         {1000, 1400, 400, 450},                        {1850, 1400, 550, 450},
        { 0, 1850, 550, 550}, { 550, 1850, 360, 550},                         {1490, 1850, 360, 550}
    };

    m_obstacles.clear();
    // 遍历15个障碍物，存入结构体：序号、矩形、是否被标记
    for (int i = 0; i < 15; ++i) {
        m_obstacles.append({i, rects[i], false});
    }
}

/**
 * @brief 初始化5×5网格格子数据（手动定义坐标）
 * 
 * @details 手动定义25个格子的坐标范围，用于路径规划和可视化。
 *          网格坐标系统：
 *          - X轴：0-4（从右到左，0=右边界，4=左边界）
 *          - Y轴：0-4（从上到下，0=上边界，4=下边界）
 *          
 *          格子编号规则：
 *          - 从左上角开始，逐行编号（0-24）
 *          - 第0行：(0,0), (1,0), (2,0), (3,0), (4,0)
 *          - 第1行：(0,1), (1,1), (2,1), (3,1), (4,1)
 *          - ...
 *          - 第4行：(0,4), (1,4), (2,4), (3,4), (4,4)
 *          
 * @note 坐标定义参考场地区域布局和障碍物分布
 */
void CourtMapWidget::initGrid5()
{
    m_grid5Cells.clear();
    
    // ===== 手动定义25个格子的坐标（毫米） =====
    // 每个格子的 QRectF(x, y, width, height)
    // x: 左上角X坐标
    // y: 左上角Y坐标
    // width: 宽度
    // height: 高度
    
    // 第0行（Y=0，顶部）
    m_grid5Cells.append({0, 4, 0, QRectF(0, 0, 550, 550)});      // 格子0：(4,0) 左上角
    m_grid5Cells.append({1, 3, 0, QRectF(550, 0, 450, 550)});    // 格子1：(3,0)
    m_grid5Cells.append({2, 2, 0, QRectF(1000, 0, 400, 550)});    // 格子2：(2,0)
    m_grid5Cells.append({3, 1, 0, QRectF(1400, 0, 450, 550)});   // 格子3：(1,0)
    m_grid5Cells.append({4, 0, 0, QRectF(1850, 0, 550, 550)});   // 格子4：(0,0) 右上角
    
    // 第1行（Y=1）
    m_grid5Cells.append({5, 4, 1, QRectF(0, 550, 550, 360)});    // 格子5：(4,1)
    m_grid5Cells.append({6, 3, 1, QRectF(550, 550, 450, 450)});  // 格子6：(3,1)
    m_grid5Cells.append({7, 2, 1, QRectF(1000, 550, 400, 450)});  // 格子7：(2,1)
    m_grid5Cells.append({8, 1, 1, QRectF(1400, 550, 450, 450)}); // 格子8：(1,1)
    m_grid5Cells.append({9, 0, 1, QRectF(1850, 550, 550, 450)}); // 格子9：(0,1)
    // 第2行（Y=2，中间）
    m_grid5Cells.append({10, 4, 2, QRectF(0, 910, 550, 580)});   // 格子10：(4,2)
    m_grid5Cells.append({11, 3, 2, QRectF(550, 1000, 450, 400)}); // 格子11：(3,2)
    m_grid5Cells.append({12, 2, 2, QRectF(1000, 1000, 400, 400)}); // 格子12：(2,2) 中心
    m_grid5Cells.append({13, 1, 2, QRectF(1400, 1000, 450, 400)});// 格子13：(1,2)
    m_grid5Cells.append({14, 0, 2, QRectF(1850, 960, 550, 440)});// 格子14：(0,2)
    
    // 第3行（Y=3）
    m_grid5Cells.append({15, 4, 3, QRectF(0, 1490, 550, 360)});  // 格子15：(4,3)
    m_grid5Cells.append({16, 3, 3, QRectF(550, 1400, 450, 450)});// 格子16：(3,3)
    m_grid5Cells.append({17, 2, 3, QRectF(1000, 1400, 400, 450)});// 格子17：(2,3)
    m_grid5Cells.append({18, 1, 3, QRectF(1400, 1400, 450, 450)});// 格子18：(1,3)
    m_grid5Cells.append({19, 0, 3, QRectF(1850, 1400, 550, 450)});// 格子19：(0,3)
    // 第4行（Y=4，底部）
    m_grid5Cells.append({20, 4, 4, QRectF(0, 1850, 550, 550)});  // 格子20：(4,4) 左下角
    m_grid5Cells.append({21, 3, 4, QRectF(550, 1850, 360, 550)});// 格子21：(3,4)
    m_grid5Cells.append({22, 2, 4, QRectF(910, 1850, 580, 550)});// 格子22：(2,4)
    m_grid5Cells.append({23, 1, 4, QRectF(1490, 1850, 360, 550)});// 格子23：(1,4)
    m_grid5Cells.append({24, 0, 4, QRectF(1850, 1850, 550, 550)});// 格子24：(0,4) 右下角
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
    // 注意：关闭选择模式时不清空已选中的区域，保留用户的选择
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
    drawGrid5(p);             // 5×5网格线（新增）
    drawCenterBlocks(p);     // 中心四块方块障碍
    drawCenterCross(p);       // 中心十字虚线坐标轴
    drawRawMaterialArea(p);   // 原料渐变圆形区域
    drawStartStopZones(p);    // 启停出生选择区
    drawBufferArea(p);        // 暂存区同心圆
    drawRoughProcessArea(p);  // 粗加工区同心圆
    drawQRBoard(p);           // 侧边二维码板竖线+文字
    drawObstacles(p);         // 所有障碍物矩形（可标记变红）
    drawPath(p);              // 路径轨迹（A*规划结果）
    drawRobot(p);             // 机器人图标（选中启停区后显示）
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
            // 自动将机器人放置到所选启停区中心
            QRectF zoneRect = m_zones[idx].rect;
            m_robotPos = zoneRect.center();
            // 朝向场地中心
            m_robotAngle = (idx == 0) ? 225.0 : 315.0;  // 左下或左上方向
            m_robotVisible = true;
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

// 设置机器人位置并触发重绘
void CourtMapWidget::setRobotPos(const QPointF &pos, qreal angle)
{
    m_robotPos = pos;
    m_robotAngle = angle;
    m_robotVisible = true;
    update();
}

// 设置路径可视化
void CourtMapWidget::setPath(const QVector<QPointF> &points)
{
    m_pathPoints = points;
    update();
}

// 绘制机器人：俯视图，四驱麦轮底盘
void CourtMapWidget::drawRobot(QPainter &p)
{
    if (!m_robotVisible) return;

    p.save();

    // 移动到机器人位置并旋转
    QPointF center = mapToWidget(m_robotPos);
    p.translate(center);
    // Qt 旋转方向：正值=顺时针，我们的角度是逆时针正，所以取负
    p.rotate(-m_robotAngle);

    // 机器人尺寸（场地坐标转换为像素）
    // 真实机器人约 300x300mm
    qreal bodyW = 300 * m_scale;
    qreal bodyH = 300 * m_scale;
    qreal wheelW = 40 * m_scale;
    qreal wheelH = 80 * m_scale;

    // ===== 1. 机器人底盘（圆角矩形） =====
    QRectF bodyRect(-bodyW/2, -bodyH/2, bodyW, bodyH);
    p.setPen(QPen(QColor(50, 50, 50), 2));
    p.setBrush(QColor(241, 196, 15));  // 黄色底盘
    p.drawRoundedRect(bodyRect, 8, 8);

    // ===== 2. 四个麦克纳姆轮 =====
    p.setBrush(QColor(30, 30, 30));
    // 左前轮
    p.drawRoundedRect(QRectF(-bodyW/2 - wheelW/2, -bodyH/2 + 10*m_scale, wheelW, wheelH), 3, 3);
    // 右前轮
    p.drawRoundedRect(QRectF(bodyW/2 - wheelW/2, -bodyH/2 + 10*m_scale, wheelW, wheelH), 3, 3);
    // 左后轮
    p.drawRoundedRect(QRectF(-bodyW/2 - wheelW/2, bodyH/2 - 10*m_scale - wheelH, wheelW, wheelH), 3, 3);
    // 右后轮
    p.drawRoundedRect(QRectF(bodyW/2 - wheelW/2, bodyH/2 - 10*m_scale - wheelH, wheelW, wheelH), 3, 3);

    // ===== 3. 方向指示箭头（朝前） =====
    QPainterPath arrow;
    qreal arrowSize = bodyW * 0.25;
    arrow.moveTo(0, -bodyH/2 + 15*m_scale);           // 箭头尖
    arrow.lineTo(-arrowSize/2, -bodyH/2 + 15*m_scale + arrowSize);
    arrow.lineTo(arrowSize/2, -bodyH/2 + 15*m_scale + arrowSize);
    arrow.closeSubpath();
    p.setBrush(QColor(231, 76, 60));  // 红色箭头
    p.setPen(QPen(QColor(192, 57, 43), 1));
    p.drawPath(arrow);

    // ===== 4. 中心圆点 =====
    p.setBrush(QColor(50, 50, 50));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(0, 0), 5, 5);

    // ===== 5. 机器人标签 =====
    p.rotate(m_robotAngle);  // 反旋转，文字保持水平
    p.setPen(QColor(50, 50, 50));
    QFont font("Microsoft YaHei", 8);
    font.setBold(true);
    p.setFont(font);
    p.drawText(QRectF(-40, bodyH/2 + 5*m_scale, 80, 20), Qt::AlignCenter, "机器人");

    p.restore();
}

// 绘制路径轨迹
void CourtMapWidget::drawPath(QPainter &p)
{
    if (m_pathPoints.size() < 2) return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // 绘制路径线（虚线）
    QPen pathPen(QColor(41, 128, 185), 2, Qt::DashLine);
    p.setPen(pathPen);

    for (int i = 0; i < m_pathPoints.size() - 1; ++i) {
        QPointF p1 = mapToWidget(m_pathPoints[i]);
        QPointF p2 = mapToWidget(m_pathPoints[i + 1]);
        p.drawLine(p1, p2);
    }

    // 绘制路径起点（绿色圆）
    QPointF startPt = mapToWidget(m_pathPoints.first());
    p.setBrush(QColor(39, 174, 96));
    p.setPen(QPen(QColor(27, 120, 65), 2));
    p.drawEllipse(startPt, 6, 6);

    // 绘制路径终点（红色圆）
    QPointF endPt = mapToWidget(m_pathPoints.last());
    p.setBrush(QColor(231, 76, 60));
    p.setPen(QPen(QColor(192, 57, 43), 2));
    p.drawEllipse(endPt, 6, 6);

    // 绘制中间路径点（小蓝点）
    p.setBrush(QColor(52, 152, 219));
    p.setPen(Qt::NoPen);
    for (int i = 1; i < m_pathPoints.size() - 1; ++i) {
        QPointF pt = mapToWidget(m_pathPoints[i]);
        p.drawEllipse(pt, 3, 3);
    }

    p.restore();
}

/**
 * @brief 生成完整任务路径
 * 
 * @details 生成从启停区到各区域的完整任务流程路径。
 *          任务流程：启停区 → 二维码区 → 原料区 → 粗加工区 → 暂存区 → 启停区
 *          
 *          路径生成策略：
 *          1. 使用简化的点对点连接（直线）
 *          2. 后续可集成A*算法避开障碍物
 *          
 * @param startZoneIndex 启停区索引（0=右上角，1=右下角）
 * @param taskCode 任务码（例如"312"）
 * @return 完整路径点数组（场地坐标）
 */
QVector<QPointF> CourtMapWidget::generateFullMissionPath(int startZoneIndex, const QString& taskCode) const
{
    QVector<QPointF> fullPath;
    
    // ===== 场地各区域对应的格子坐标（5×5网格） =====
    // 注意：格子坐标(0,0)是右上角启停区1
    struct GridLocations {
        int startZone1[2] = {0, 0};        // 启停区1：格子(0,0)
        int startZone2[2] = {0, 4};        // 启停区2：格子(0,4)
        int qrZone[2] = {0, 2};            // 二维码区：格子(0,2)
        int materialZone[2] = {2, 0};      // 原料区：格子(2,0)
        int processSlots[3][2] = {         // 粗加工区：格子(1,4), (2,4), (3,4)
            {1, 4}, {2, 4}, {3, 4}
        };
        int bufferSlots[3][2] = {          // 暂存区：格子(4,1), (4,2), (4,3)
            {4, 1}, {4, 2}, {4, 3}
        };
    };
    
    GridLocations grid;
    
    // ===== 1. 起点：根据启停区索引选择 =====
    int startX = (startZoneIndex == 0) ? grid.startZone1[0] : grid.startZone2[0];
    int startY = (startZoneIndex == 0) ? grid.startZone1[1] : grid.startZone2[1];
    
    int currentX = startX;
    int currentY = startY;
    int currentAngle = 0;  // 初始朝向：向左（0°）
    
    // ===== 2. 前往二维码区 =====
    QVector<QPointF> path1 = generateBatchMovePath(currentX, currentY, grid.qrZone[0], grid.qrZone[1], currentAngle);
    fullPath.append(path1);
    currentX = grid.qrZone[0];
    currentY = grid.qrZone[1];
    // 计算当前朝向（根据最后一步移动）
    if (!path1.isEmpty()) {
        // 更新currentAngle（简化：假设X方向移动后朝向0°，Y方向移动后朝向90°）
        // TODO: 根据实际移动方向更新
    }
    
    // ===== 3. 前往原料区 =====
    QVector<QPointF> path2 = generateBatchMovePath(currentX, currentY, grid.materialZone[0], grid.materialZone[1], currentAngle);
    fullPath.append(path2);
    currentX = grid.materialZone[0];
    currentY = grid.materialZone[1];
    
    // ===== 4. 前往粗加工区（根据任务码） =====
    if (taskCode.length() >= 3) {
        for (int i = 0; i < 3; ++i) {
            int slotIndex = taskCode[i].digitValue() - 1;  // 转为0-indexed
            if (slotIndex >= 0 && slotIndex < 3) {
                // 先回原料区
                QVector<QPointF> pathBack = generateBatchMovePath(currentX, currentY, grid.materialZone[0], grid.materialZone[1], currentAngle);
                fullPath.append(pathBack);
                currentX = grid.materialZone[0];
                currentY = grid.materialZone[1];
                
                // 再去粗加工区
                QVector<QPointF> pathToProcess = generateBatchMovePath(currentX, currentY, grid.processSlots[slotIndex][0], grid.processSlots[slotIndex][1], currentAngle);
                fullPath.append(pathToProcess);
                currentX = grid.processSlots[slotIndex][0];
                currentY = grid.processSlots[slotIndex][1];
            }
        }
    }
    
    // ===== 5. 前往暂存区 =====
    QVector<QPointF> pathToBuffer = generateBatchMovePath(currentX, currentY, grid.bufferSlots[0][0], grid.bufferSlots[0][1], currentAngle);
    fullPath.append(pathToBuffer);
    currentX = grid.bufferSlots[0][0];
    currentY = grid.bufferSlots[0][1];
    
    // ===== 6. 返回启停区 =====
    QVector<QPointF> pathToStart = generateBatchMovePath(currentX, currentY, startX, startY, currentAngle);
    fullPath.append(pathToStart);
    
    return fullPath;
}

/**
 * @brief 生成批量移动路径（真实坐标）
 * 
 * @details 核心原则：
 *          1. 从一个格子中心到另一个格子中心，直线移动
 *          2. 先X方向移动（逐格），再Y方向移动（逐格）
 *          3. 只有在需要转向时才改变朝向
 *          4. 每个格子中心都是路径点
 *          
 * @param startGridX 起点格子X坐标（0-4）
 * @param startGridY 起点格子Y坐标（0-4）
 * @param goalGridX 终点格子X坐标（0-4）
 * @param goalGridY 终点格子Y坐标（0-4）
 * @param currentAngle 当前陀螺仪角度（0/90/180/270）
 * @return 真实坐标路径点数组（毫米）
 */
QVector<QPointF> CourtMapWidget::generateBatchMovePath(int startGridX, int startGridY,
                                                        int goalGridX, int goalGridY,
                                                        int currentAngle) const
{
    QVector<QPointF> pathPoints;
    
    // 计算移动向量
    int dx = goalGridX - startGridX;  // X方向格数
    int dy = goalGridY - startGridY;  // Y方向格数

    // ========== 方案1：批量移动（只添加起点、转向点、终点）==========
    // 核心原则：
    // 1. GUI显示格子中心作为路径点（可视化清晰）
    // 2. 机器人只需进入格子区域，不需要精确到达中心
    // 3. 不需要在格子里面微调
    // 4. 只在需要转向时添加路径点
    // 5. 检查路径上是否有障碍物

    // 步骤1：添加起点
    pathPoints.append(getCellCenter(startGridX, startGridY));

    // 步骤2：检查X方向路径上的障碍物
    if (dx != 0) {
        int direction = (dx > 0) ? 1 : -1;
        for (int x = startGridX + direction; x != goalGridX + direction; x += direction) {
            if (hasObstacleInCell(x, startGridY)) {
                // X方向有障碍物，返回空路径
                // TODO: 应该调用A*算法绕过障碍物
                return QVector<QPointF>();
            }
        }
        // X方向终点：(goalGridX, startGridY)
        pathPoints.append(getCellCenter(goalGridX, startGridY));
    }

    // 步骤3：检查Y方向路径上的障碍物
    if (dy != 0) {
        int direction = (dy > 0) ? 1 : -1;
        for (int y = startGridY + direction; y != goalGridY + direction; y += direction) {
            if (hasObstacleInCell(goalGridX, y)) {
                // Y方向有障碍物，返回空路径
                // TODO: 应该调用A*算法绕过障碍物
                return QVector<QPointF>();
            }
        }
        // Y方向终点：(goalGridX, goalGridY)
        pathPoints.append(getCellCenter(goalGridX, goalGridY));
    }

    return pathPoints;
}

/**
 * @brief 检测格子内是否有障碍物
 * 
 * @details 检查指定格子内是否有已标记的障碍物
 *          用于按需触发A*算法
 *          
 * @param gridX 格子X坐标（0-4）
 * @param gridY 格子Y坐标（0-4）
 * @return true=有障碍物，false=无障碍物
 */
bool CourtMapWidget::hasObstacleInCell(int gridX, int gridY) const
{
    // 查找对应的格子
    for (const auto& cell : m_grid5Cells) {
        if (cell.gridX == gridX && cell.gridY == gridY) {
            // 检查格子的矩形范围内是否有已标记的障碍物
            for (const auto& obstacle : m_obstacles) {
                if (obstacle.isMarked) {
                    // 检查障碍物是否与格子有交集（而不是完全包含）
                    if (cell.rect.intersects(obstacle.rect)) {
                        return true;
                    }
                }
            }
            return false;
        }
    }
    return false;
}

/**
 * @brief 获取格子的中心点坐标
 * 
 * @details 根据网格坐标计算真实场地坐标（毫米）
 *          
 * @param gridX 格子X坐标（0-4）
 * @param gridY 格子Y坐标（0-4）
 * @return 真实坐标（毫米）
 */
QPointF CourtMapWidget::getCellCenter(int gridX, int gridY) const
{
    // 查找对应的格子
    for (const auto& cell : m_grid5Cells) {
        if (cell.gridX == gridX && cell.gridY == gridY) {
            // 返回格子中心点
            return cell.rect.center();
        }
    }
    
    // 默认返回场地中心
    return QPointF(1200, 1200);
}

Grid5Cell CourtMapWidget::fieldToGrid5(int x, int y) const
{
    // 遍历所有格子，查找包含该点的格子
    for (const auto& cell : m_grid5Cells) {
        if (cell.rect.contains(x, y)) {
            return cell;  // 直接返回找到的格子
        }
    }
    
    // 如果点不在任何格子内，返回最近的格子
    Grid5Cell nearest = m_grid5Cells[12];  // 默认中心格子
    qreal minDist = 1e10;
    
    for (const auto& cell : m_grid5Cells) {
        QPointF center = cell.rect.center();
        qreal dx = x - center.x();
        qreal dy = y - center.y();
        qreal dist = dx * dx + dy * dy;  // 距离平方
        
        if (dist < minDist) {
            minDist = dist;
            nearest = cell;
        }
    }
    
    return nearest;
}

/**
 * @brief 绘制5×5网格线
 * 
 * @details 使用手动定义的25个格子坐标绘制网格线。
 *          每个格子的坐标在 initGrid5() 中手动定义。
 *          
 *          网格坐标系统：
 *          - X轴：从右到左，坐标0-4（0=右边界，4=左边界）
 *          - Y轴：从上到下，坐标0-4（0=上边界，4=下边界）
 *          
 *          网格用途：
 *          - 简化路径规划决策
 *          - 对应陀螺仪角度映射（0°向左，90°向下，180°向右，270°向上）
 */
void CourtMapWidget::drawGrid5(QPainter &p)
{
    p.save();
    
    // ===== 绘制网格线（明显的深灰色实线） =====
    QPen gridPen(QColor(80, 80, 80, 200), 2, Qt::SolidLine);  // 深灰色实线，宽度2
    p.setPen(gridPen);
    
    // ===== 使用手动定义的格子坐标绘制网格线 =====
    for (const auto &cell : m_grid5Cells) {
        // 绘制格子边界
        QRectF widgetRect = QRectF(
            mapToWidget(cell.rect.topLeft()),
            mapToWidget(cell.rect.bottomRight())
        );
        
        // 绘制格子的左边界和上边界（避免重复绘制）
        if (cell.gridX == 4) {  // 最左侧格子，绘制左边界
            p.drawLine(widgetRect.topLeft(), widgetRect.bottomLeft());
        }
        if (cell.gridY == 0) {  // 最顶部格子，绘制上边界
            p.drawLine(widgetRect.topLeft(), widgetRect.topRight());
        }
        
        // 绘制格子的右边界和下边界
        p.drawLine(widgetRect.topRight(), widgetRect.bottomRight());    // 右边界
        p.drawLine(widgetRect.bottomLeft(), widgetRect.bottomRight()); // 下边界
    }
    
    p.restore();
}