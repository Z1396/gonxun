#ifndef CAMERAWIDGET_H
#define CAMERAWIDGET_H

// Qt基础窗口控件基类，所有自定义界面组件都继承它
#include <QWidget>
// 用于显示视频画面、图片的文本/图像标签控件
#include <QLabel>
// 定时器：定时刷新摄像头画面（核心！视频靠定时器刷屏）
#include <QTimer>
// Qt图像格式类，用于承载画面、渲染到界面
#include <QImage>
// 图片画布、贴图类，用于缩放显示画面
#include <QPixmap>
// 按钮控件：启动、停止、截图按钮
#include <QPushButton>
// 垂直布局：上下排布UI控件
#include <QVBoxLayout>
// 水平布局：左右排布UI控件
#include <QHBoxLayout>
// 下拉框：选择摄像头设备索引
#include <QComboBox>
// 滑动条：调节曝光参数
#include <QSlider>
// OpenCV完整头文件，用于读取摄像头、解析图像帧
#include <opencv2/opencv.hpp>

// 自定义摄像头视频组件类
// 功能：集成摄像头打开、关闭、画面刷新、曝光调节、分辨率设置、截图、FPS统计
// 可直接嵌入你的Qt主界面，作为视频显示窗口
class CameraWidget : public QWidget
{
    // Qt宏：必须写！开启信号槽机制、反射机制
    Q_OBJECT

public:
    // 构造函数：默认无父窗口，可挂载到任意界面
    explicit CameraWidget(QWidget *parent = nullptr);
    // 析构函数：释放摄像头资源、定时器、UI内存
    ~CameraWidget();

    // 外部接口：查询摄像头是否正在运行
    bool isRunning() const { return m_isRunning; }
    // 外部接口：获取当前打开的摄像头索引号
    int cameraIndex() const { return m_cameraIndex; }

// 信号：向外发送数据（给主窗口、业务逻辑使用）
signals:
    // 每一帧画面捕获完成后，发送OpenCV原始Mat帧
    // 用途：传给Python通信模块、识别算法模块做图像处理
    void frameCaptured(const cv::Mat &frame);
    // 摄像头异常信号：打不开、断开、报错时发送错误信息
    void cameraError(const QString &error);
    // 实时FPS帧率更新信号，向外推送当前帧率
    void fpsChanged(double fps);

// 外部可调用槽函数：供外部按钮、代码调用
public slots:
    // 启动摄像头，支持指定设备索引号，默认0号摄像头
    void startCamera(int index = 0);
    // 关闭摄像头、停止画面刷新
    void stopCamera();
    // 设置摄像头曝光参数
    void setExposure(int value);
    // 手动设置摄像头分辨率（宽、高）
    void setResolution(int width, int height);
    // 保存当前画面为截图
    void saveSnapshot();

// 内部私有槽函数：组件内部自己使用
private slots:
    // 定时器绑定的核心函数：定时读取摄像头帧、刷新UI画面
    void updateFrame();
    // 下拉框切换摄像头设备触发
    void onCameraIndexChanged(int index);
    // 曝光滑动条数值改变触发
    void onExposureChanged(int value);

// 重写Qt原生窗口事件
protected:
    // 窗口大小改变时自动适配视频画面缩放
    void resizeEvent(QResizeEvent *event) override;

// 内部私有方法 & 私有成员变量
private:
    // 初始化所有UI控件、布局、按钮、下拉框
    void setupUI();
    // 工具函数：OpenCV Mat 转 Qt QImage（跨格式渲染核心）
    QImage matToQImage(const cv::Mat &mat);
    // 统计、更新实时FPS帧率
    void updateFPS();

    // OpenCV摄像头捕获对象，负责底层读取视频流
    cv::VideoCapture m_capture;

    // UI控件指针
    QLabel *m_displayLabel;        // 视频画面显示画布
    QPushButton *m_startBtn;       // 启动摄像头按钮
    QPushButton *m_stopBtn;        // 停止摄像头按钮
    QPushButton *m_snapshotBtn;    // 截图按钮
    QComboBox *m_cameraCombo;      // 摄像头选择下拉框
    QSlider *m_exposureSlider;     // 曝光调节滑动条
    QLabel *m_fpsLabel;            // 帧率显示文本
    QLabel *m_resolutionLabel;     // 分辨率显示文本

    QTimer *m_timer;               // 画面刷新定时器（视频帧率核心）
    bool m_isRunning;              // 摄像头运行状态标记
    int m_cameraIndex;             // 当前摄像头设备编号
    int m_frameWidth;              // 视频帧宽度
    int m_frameHeight;             // 视频帧高度

    int m_frameCount;              // 一秒内帧计数，用于算FPS
    double m_fps;                  // 当前实时帧率
    qint64 m_lastFpsTime;          // 上一次计算FPS的时间戳
    QImage m_currentFrame;         // 当前最新的一帧Qt图像
};

#endif