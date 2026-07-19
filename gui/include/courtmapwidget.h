#ifndef COURTMAPWIDGET_H
#define COURTMAPWIDGET_H

// Qt自定义控件基类，所有绘图界面都继承此类
#include <QWidget>
// Qt绘图核心类：画线、矩形、圆、文字、填充全部靠它
#include <QPainter>
// 动态数组容器，存储所有区域、障碍物、点位
#include <QVector>
// 字符串类：存储区域名称、标注文字
#include <QString>
// 浮点坐标点：精准地图点位、机器人坐标
#include <QPointF>
// 浮点矩形：精准区域范围、障碍物矩形
#include <QRectF>

/**
 * @brief 场地区域结构体
 * @desc 用于存储赛场内所有矩形功能区
 */
struct CourtZone {
    QString name;        // 区域名称（起点区/缓冲区/加工区等）
    QRectF rect;         // 区域矩形坐标范围（地图坐标系）
    QColor color;        // 区域绘制颜色
    bool isSelected;     // 是否被用户选中（点击高亮）
};

/**
 * @brief 场地同心圆结构体
 * @desc 用于绘制赛场中心同心圆、缓冲区环形区域
 */
struct CourtCircle {
    QPointF center;      // 圆心坐标
    qreal outerRadius;   // 外圆半径
    qreal innerRadius;   // 内圆半径
    QColor outerColor;   // 外环颜色
    QColor innerColor;   // 内环填充颜色
};

/**
 * @brief 障碍物矩形结构体
 * @desc 可点击标记的障碍物方块，用于地图标定、避障路径规划
 */
struct ObstacleRect {
    int id;              // 障碍物唯一编号
    QRectF rect;         // 障碍物矩形范围
    bool isMarked;       // 是否被人工标记（勾选/选中）
};

/**
 * @brief 5×5网格格子结构体
 * @desc 手动定义的网格格子，用于简化路径规划
 */
struct Grid5Cell {
    int id;              // 格子唯一编号（0-24，从左上角开始，逐行编号）
    int gridX;           // 网格X坐标（0-4，从右到左）
    int gridY;           // 网格Y坐标（0-4，从上到下）
    QRectF rect;         // 格子在场地中的矩形范围（毫米）
};

/**
 * @brief CourtMapWidget 赛场地图绘制控件
 * @功能：
 * 1、完整绘制比赛场地 2400*2400 标准场地
 * 2、绘制所有功能区、障碍物、中心图案、尺寸标注
 * 3、支持鼠标/触摸屏点击选起点、标记障碍物
 * 4、自动缩放适配窗口大小
 * 5、完全兼容鼠标 + 触摸屏（你不用改任何点击代码）
 * 6、可对外输出标记障碍物、选中起点区域，给路径规划/Python算法使用
 */
class CourtMapWidget : public QWidget
{
    // Qt信号槽必需宏，开启事件、信号、反射机制
    Q_OBJECT

public:
    // 构造函数：可嵌入任意父窗口
    explicit CourtMapWidget(QWidget *parent = nullptr);
    // 默认析构，无需手动释放内存
    ~CourtMapWidget() = default;

    /**
     * @brief 设置障碍物标记模式开关
     * @param enabled true=点击可标记障碍物，false=普通浏览模式
     */
    void setMarkMode(bool enabled);
    // 获取当前是否处于障碍物标记模式
    bool isMarkMode() const { return m_markMode; }

    /**
     * @brief 开启/关闭起点区域选择功能
     */
    void setStartZoneSelectable(bool selectable);
    // 获取是否允许选择起点区域
    bool isStartZoneSelectable() const { return m_startZoneSelectable; }

    // 获取当前选中的起点区域下标
    int selectedStartZone() const { return m_selectedStartZone; }
    // 获取当前选中的起点区域名称
    QString selectedStartZoneName() const;

    // 获取当前已标记的障碍物总数
    int markedCount() const;
    // 获取所有被标记的障碍物数据（传给算法/路径规划）
    QVector<ObstacleRect> getMarkedObstacles() const;
    // 清空所有障碍物标记
    void clearAllMarks();

    /**
     * @brief 设置机器人位置（场地坐标，毫米）
     * @param pos 机器人在场地坐标系中的位置
     * @param angle 机器人朝向角度（度，0=东，逆时针正）
     */
    void setRobotPos(const QPointF &pos, qreal angle = 0.0);

    /**
     * @brief 设置机器人是否可见
     */
    void setRobotVisible(bool visible) { m_robotVisible = visible; update(); }

    /**
     * @brief 获取机器人当前位置
     */
    QPointF robotPos() const { return m_robotPos; }
    qreal robotAngle() const { return m_robotAngle; }
    bool isRobotVisible() const { return m_robotVisible; }

    /**
     * @brief 设置路径可视化（用于显示 A* 规划的路径）
     * @param points 路径点数组（场地坐标）
     */
    void setPath(const QVector<QPointF> &points);
    void clearPath() { m_pathPoints.clear(); update(); }

    /**
     * @brief 生成完整任务路径（从启停区到各区域的完整流程）
     * @param startZoneIndex 启停区索引（0=右上角，1=右下角）
     * @param taskCode 任务码（例如"312"）
     * @return 完整路径点数组（场地坐标）
     */
    QVector<QPointF> generateFullMissionPath(int startZoneIndex, const QString& taskCode) const;
    
    /**
     * @brief 批量移动命令结构体
     * @details 用于描述机器人的批量移动指令
     */
    struct BatchMoveCommand {
        int targetGridX;        // 目标格子X坐标（0-4）
        int targetGridY;        // 目标格子Y坐标（0-4）
        int turnAngle;          // 转向角度（0/90/180/270）
        int moveCount;          // 移动格数
        std::string direction;  // 移动方向描述
    };
    
    /**
     * @brief 生成批量移动路径（真实坐标）
     * @param startGridX 起点格子X坐标（0-4）
     * @param startGridY 起点格子Y坐标（0-4）
     * @param goalGridX 终点格子X坐标（0-4）
     * @param goalGridY 终点格子Y坐标（0-4）
     * @param currentAngle 当前陀螺仪角度（0/90/180/270）
     * @return 真实坐标路径点数组（毫米）
     */
    QVector<QPointF> generateBatchMovePath(int startGridX, int startGridY, 
                                            int goalGridX, int goalGridY,
                                            int currentAngle) const;
    
    /**
     * @brief 检测格子内是否有障碍物
     * @param gridX 格子X坐标（0-4）
     * @param gridY 格子Y坐标（0-4）
     * @return true=有障碍物，false=无障碍物
     */
    bool hasObstacleInCell(int gridX, int gridY) const;
    
    /**
     * @brief 获取格子的中心点坐标
     * @param gridX 格子X坐标（0-4）
     * @param gridY 格子Y坐标（0-4）
     * @return 真实坐标（毫米）
     */
    QPointF getCellCenter(int gridX, int gridY) const;

    /**
     * @brief 将毫米坐标转换为格子坐标（5×5网格）
     * @param x 毫米坐标X
     * @param y 毫米坐标Y
     * @return 格子坐标（gridX, gridY都在0-4范围内）
     */
    Grid5Cell fieldToGrid5(int x, int y) const;

signals:
    /**
     * @brief 障碍物标记状态改变信号
     * @param id 障碍物编号
     * @param marked true=标记，false=取消标记
     * 可连接给底层/算法，实时更新避障列表
     */
    void obstacleToggled(int id, bool marked);

    /**
     * @brief 起点区域被选中信号
     * @param zoneIndex 区域下标
     * @param zoneName 区域名称
     * 用于通知业务逻辑切换机器人起始点位
     */
    void startZoneSelected(int zoneIndex, const QString &zoneName);

protected:
    // 【重写Qt绘图事件】所有地图绘制全部在这里执行
    void paintEvent(QPaintEvent *event) override;

    // 【重写鼠标释放事件】处理点击选区域、标记障碍物
    void mouseReleaseEvent(QMouseEvent *event) override;

    // 【重写窗口大小变化事件】窗口缩放，地图自动适配
    void resizeEvent(QResizeEvent *event) override;

    // 【重写总事件】自动适配触摸屏、鼠标事件统一分发
    bool event(QEvent *event) override;

private:
    // 初始化所有场地区域数据
    void initMapData();
    // 初始化所有障碍物方块数据
    void initObstacles();
    // 初始化5×5网格格子数据（手动定义坐标）
    void initGrid5();

    // 绘制场地背景
    void drawBackground(QPainter &p);
    // 绘制场地最外边框
    void drawOuterFrame(QPainter &p);
    // 绘制原材料区
    void drawRawMaterialArea(QPainter &p);
    // 绘制起点/停机区域
    void drawStartStopZones(QPainter &p);
    // 绘制缓冲区环形区域
    void drawBufferArea(QPainter &p);
    // 绘制粗加工区域
    void drawRoughProcessArea(QPainter &p);
    // 绘制场地中心障碍方块
    void drawCenterBlocks(QPainter &p);
    // 绘制场地中心十字标线
    void drawCenterCross(QPainter &p);
    // 绘制二维码标定板位置
    void drawQRBoard(QPainter &p);
    // 绘制所有可点击障碍物
    void drawObstacles(QPainter &p);
    // 绘制机器人（四驱麦轮，俯视图）
    void drawRobot(QPainter &p);
    // 绘制路径轨迹
    void drawPath(QPainter &p);
    // 绘制场地尺寸标注（长/宽刻度）
    void drawDimensionMarks(QPainter &p);

    // 绘制水平尺寸标注
    void drawHDimension(QPainter &p, qreal x1, qreal x2, qreal y, const QString &text);
    // 绘制垂直尺寸标注
    void drawVDimension(QPainter &p, qreal x, qreal y1, qreal y2, const QString &text);
    // 绘制同心圆通用方法（缓冲区、中心环复用）
    void drawConcentricCircle(QPainter &p, const QPointF &center, qreal outerR, qreal innerR,
                              const QColor &outerColor, const QColor &innerColor);
    // 绘制5×5网格线
    void drawGrid5(QPainter &p);

    /**
     * @brief 坐标转换：真实场地坐标 → 窗口像素坐标
     * 2400mm真实物理坐标转成UI缩放后的像素坐标
     */
    QPointF mapToWidget(const QPointF &mapPoint) const;

    /**
     * @brief 坐标转换：窗口像素坐标 → 真实场地坐标
     * 点击屏幕位置反推真实物理坐标，给算法路径规划
     */
    QPointF widgetToMap(const QPointF &widgetPoint) const;

    // 根据点击位置查找命中的障碍物ID
    int findObstacleAt(const QPointF &point) const;
    // 根据点击位置查找命中的起点区域
    int findStartZoneAt(const QPointF &point) const;
    // 统一处理点击选择逻辑（障碍物标记/起点选择）
    void handlePointSelection(const QPointF &pos);

    // 比赛场地标准尺寸：2400mm * 2400mm
    static constexpr qreal MAP_SIZE = 2400.0;
    // 绘图边距，防止贴边显示
    static constexpr qreal MARGIN = 80.0;

    QRectF m_mapRect;                     // 地图在控件内的绘制区域
    qreal m_scale = 1.0;                  // 当前缩放比例（自适应窗口）

    QVector<CourtZone> m_zones;           // 所有功能区域数组
    QVector<CourtCircle> m_bufferCircles; // 缓冲区圆环数据
    QVector<CourtCircle> m_processCircles;// 加工区圆环数据
    QVector<ObstacleRect> m_obstacles;    // 所有障碍物数据
    QVector<Grid5Cell> m_grid5Cells;      // 5×5网格格子数据（25个格子）

    bool m_markMode = false;              // 障碍物标记模式开关
    bool m_startZoneSelectable = false;   // 起点选择模式开关
    int m_selectedStartZone = -1;         // 当前选中起点下标，-1代表未选中

    // 机器人状态
    QPointF m_robotPos{2250, 150};        // 机器人位置（场地坐标，毫米）
    qreal m_robotAngle = 180.0;           // 机器人朝向（度，0=东，逆时针正）
    bool m_robotVisible = false;          // 机器人是否可见

    // 路径可视化
    QVector<QPointF> m_pathPoints;        // 路径点数组（场地坐标）
};

#endif