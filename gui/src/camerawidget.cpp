// 引入自定义摄像头控件头文件
#include "camerawidget.h"
// 垂直布局、水平布局，用于搭建UI界面
#include <QVBoxLayout>
#include <QHBoxLayout>
// 时间类：用于FPS计算、截图文件时间戳命名
#include <QDateTime>
// 文件目录类：用于截图文件保存路径管理
#include <QDir>
// 弹窗提示框：摄像头打开失败异常提示
#include <QMessageBox>
// 文本标签控件：画面显示、状态文字展示
#include <QLabel>

/**
 * @brief 摄像头控件构造函数
 * @param parent 父窗口对象，用于Qt对象树内存管理
 * @note 初始化成员变量、搭建UI、创建帧刷新定时器
 */
CameraWidget::CameraWidget(QWidget *parent)
    : QWidget(parent)
    // 摄像头运行状态：默认未运行
    , m_isRunning(false)
    // 默认使用0号摄像头
    , m_cameraIndex(0)
    // 默认帧宽度640
    , m_frameWidth(640)
    // 默认帧高度480
    , m_frameHeight(480)
    // 帧计数器：用于统计FPS
    , m_frameCount(0)
    // 实时帧率
    , m_fps(0)
    // 上一次FPS统计的时间戳
    , m_lastFpsTime(0)
{
    // 初始化所有UI控件与布局
    setupUI();

    // 创建摄像头帧刷新定时器
    m_timer = new QTimer(this);
    // 定时器超时触发帧更新函数
    connect(m_timer, &QTimer::timeout, this, &CameraWidget::updateFrame);
    // 设置刷新间隔33ms，约30帧每秒(1000/33≈30)
    m_timer->setInterval(33);
}

/**
 * @brief 析构函数
 * @note 控件销毁时自动停止摄像头、释放资源
 */
CameraWidget::~CameraWidget()
{
    stopCamera();
}

/**
 * @brief 初始化UI界面函数
 * @details 搭建主布局、画面显示区域、控制按钮栏、摄像头选择、曝光调节、状态信息栏
 */
void CameraWidget::setupUI()
{
    // 主垂直布局：整体页面从上到下排列
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10); // 布局边距
    mainLayout->setSpacing(8); // 控件间距

    // ====================== 画面显示标签 ======================
    m_displayLabel = new QLabel(this);
    m_displayLabel->setAlignment(Qt::AlignCenter); // 内容居中
    m_displayLabel->setMinimumSize(320, 240); // 最小显示尺寸
    // 初始样式：深色背景、灰色边框
    m_displayLabel->setStyleSheet("QLabel { background-color: #222; border: 2px solid #555; border-radius: 4px; }");
    m_displayLabel->setText("摄像头未开启"); // 默认提示文字
    // 文字样式：灰色提示字体
    m_displayLabel->setStyleSheet("QLabel { color: #888; background-color: #222; border: 2px solid #555; border-radius: 4px; font-size: 16px; }");
    mainLayout->addWidget(m_displayLabel, 1); // 占满剩余空间

    // ====================== 控制按钮布局：开启/停止/截图 ======================
    QHBoxLayout *controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(10);

    // 开启摄像头按钮
    m_startBtn = new QPushButton("开启", this);
    m_startBtn->setMinimumHeight(40);
    // 绿色主题按钮样式、悬浮/按压状态样式
    m_startBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border: none; border-radius: 4px; font-size: 14px; } QPushButton:hover { background-color: #45a049; } QPushButton:pressed { background-color: #3d8b40; }");
    // 点击开启当前选中的摄像头
    connect(m_startBtn, &QPushButton::clicked, this, [this]() { startCamera(m_cameraIndex); });
    controlLayout->addWidget(m_startBtn);

    // 停止摄像头按钮
    m_stopBtn = new QPushButton("停止", this);
    m_stopBtn->setMinimumHeight(40);
    m_stopBtn->setEnabled(false); // 默认不可点击
    // 红色主题按钮、禁用样式
    m_stopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; border: none; border-radius: 4px; font-size: 14px; } QPushButton:hover { background-color: #da190b; } QPushButton:disabled { background-color: #ccc; }");
    connect(m_stopBtn, &QPushButton::clicked, this, &CameraWidget::stopCamera);
    controlLayout->addWidget(m_stopBtn);

    // 截图按钮
    m_snapshotBtn = new QPushButton("截图", this);
    m_snapshotBtn->setMinimumHeight(40);
    m_snapshotBtn->setEnabled(false); // 默认不可点击
    // 蓝色主题按钮、禁用样式
    m_snapshotBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border: none; border-radius: 4px; font-size: 14px; } QPushButton:hover { background-color: #0b7dda; } QPushButton:disabled { background-color: #ccc; }");
    connect(m_snapshotBtn, &QPushButton::clicked, this, &CameraWidget::saveSnapshot);
    controlLayout->addWidget(m_snapshotBtn);

    mainLayout->addLayout(controlLayout);

    // ====================== 摄像头选择下拉框布局 ======================
    QHBoxLayout *settingLayout = new QHBoxLayout();
    settingLayout->setSpacing(10);

    settingLayout->addWidget(new QLabel("摄像头:", this));
    m_cameraCombo = new QComboBox(this);
    // 添加3路摄像头选项，对应索引0/1/2
    m_cameraCombo->addItem("摄像头 0", 0);
    m_cameraCombo->addItem("摄像头 1", 1);
    m_cameraCombo->addItem("摄像头 2", 2);
    m_cameraCombo->setMinimumHeight(35);
    // 下拉框切换摄像头索引
    connect(m_cameraCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CameraWidget::onCameraIndexChanged);
    settingLayout->addWidget(m_cameraCombo, 1);

    mainLayout->addLayout(settingLayout);

    // ====================== 曝光调节滑块布局 ======================
    QHBoxLayout *exposureLayout = new QHBoxLayout();
    exposureLayout->addWidget(new QLabel("曝光:", this));
    m_exposureSlider = new QSlider(Qt::Horizontal, this);
    m_exposureSlider->setRange(-10, 10); // 曝光调节范围-10~10
    m_exposureSlider->setValue(0); // 默认居中0
    // 滑块数值变化实时更新曝光
    connect(m_exposureSlider, &QSlider::valueChanged, this, &CameraWidget::onExposureChanged);
    exposureLayout->addWidget(m_exposureSlider, 1);
    mainLayout->addLayout(exposureLayout);

    // ====================== 状态信息布局：FPS+分辨率 ======================
    QHBoxLayout *statusLayout = new QHBoxLayout();
    m_fpsLabel = new QLabel("FPS: 0", this);
    m_fpsLabel->setStyleSheet("color: #4CAF50; font-weight: bold;"); // 绿色FPS文字
    statusLayout->addWidget(m_fpsLabel);

    m_resolutionLabel = new QLabel("分辨率: --", this);
    m_resolutionLabel->setStyleSheet("color: #888;"); // 灰色分辨率文字
    statusLayout->addWidget(m_resolutionLabel, 0, Qt::AlignRight);

    mainLayout->addLayout(statusLayout);
}

/**
 * @brief 开启指定索引摄像头
 * @param index 摄像头设备索引(0/1/2)
 * @note 先停止已有摄像头，再初始化设备、设置分辨率、启动帧刷新
 */
void CameraWidget::startCamera(int index)
{
    // 如果摄像头正在运行，先停止旧设备
    if (m_isRunning) {
        stopCamera();
    }

    m_cameraIndex = index;

    // 打开指定摄像头设备
    if (!m_capture.open(index)) {
        // 打开失败：发送错误信号+弹窗提示
        emit cameraError(QString("无法打开摄像头 %1").arg(index));
        QMessageBox::warning(this, "错误", QString("无法打开摄像头 %1").arg(index));
        return;
    }

    // 设置摄像头预设分辨率
    m_capture.set(cv::CAP_PROP_FRAME_WIDTH, m_frameWidth);
    m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, m_frameHeight);

    // 读取摄像头实际生效的分辨率（部分设备不支持自定义分辨率）
    m_frameWidth = m_capture.get(cv::CAP_PROP_FRAME_WIDTH);
    m_frameHeight = m_capture.get(cv::CAP_PROP_FRAME_HEIGHT);
    m_resolutionLabel->setText(QString("分辨率: %1x%2").arg(m_frameWidth).arg(m_frameHeight));

    // 更新运行状态
    m_isRunning = true;
    m_frameCount = 0;
    m_fps = 0;
    m_lastFpsTime = QDateTime::currentMSecsSinceEpoch();

    // 更新按钮状态：切换可点击状态
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_snapshotBtn->setEnabled(true);
    m_cameraCombo->setEnabled(false);

    // 启动帧刷新定时器，开始实时预览
    m_timer->start();
}

/**
 * @brief 停止摄像头、释放设备资源
 * @note 停止定时器、释放摄像头句柄、重置UI状态
 */
void CameraWidget::stopCamera()
{
    m_timer->stop(); // 停止帧刷新
    m_capture.release(); // 释放摄像头设备
    m_isRunning = false;

    // 重置按钮状态
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_snapshotBtn->setEnabled(false);
    m_cameraCombo->setEnabled(true);

    // 重置画面显示与状态文字
    m_displayLabel->setText("摄像头已停止");
    m_displayLabel->setStyleSheet("QLabel { color: #888; background-color: #222; border: 2px solid #555; border-radius: 4px; font-size: 16px; }");
    m_fpsLabel->setText("FPS: 0");
}

/**
 * @brief 设置摄像头曝光参数
 * @param value 曝光值(-10 ~ 10)
 * @note 区分Windows和Linux平台不同曝光参数适配
 */
void CameraWidget::setExposure(int value)
{
    // 仅摄像头正常运行时生效
    if (m_isRunning && m_capture.isOpened()) {
#ifdef _WIN32
        // Windows平台直接设置曝光值
        m_capture.set(cv::CAP_PROP_EXPOSURE, value);
#else
        // Linux平台：先关闭自动曝光，再设置手动曝光值
        m_capture.set(cv::CAP_PROP_AUTO_EXPOSURE, 1);
        m_capture.set(cv::CAP_PROP_EXPOSURE, value * 10 + 50);
#endif
    }
}

/**
 * @brief 手动设置摄像头分辨率
 * @param width 帧宽度
 * @param height 帧高度
 */
void CameraWidget::setResolution(int width, int height)
{
    // 更新全局分辨率参数
    m_frameWidth = width;
    m_frameHeight = height;
    // 摄像头运行中则实时生效
    if (m_isRunning) {
        m_capture.set(cv::CAP_PROP_FRAME_WIDTH, width);
        m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    }
}

/**
 * @brief 截图保存函数
 * @note 以当前时间戳命名，保存为jpg图片到程序运行目录
 */
void CameraWidget::saveSnapshot()
{
    // 摄像头未运行/无画面则直接返回
    if (!m_isRunning || m_currentFrame.isNull()) return;

    // 生成时间戳文件名，避免重名
    QString fileName = QString("snapshot_%1.jpg").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString path = QDir::currentPath() + "/" + fileName;

    // 保存图片并提示状态
    if (m_currentFrame.save(path)) {
        m_snapshotBtn->setText("已保存!");
        // 1秒后恢复按钮文字
        QTimer::singleShot(1000, m_snapshotBtn, [this]() { m_snapshotBtn->setText("截图"); });
    }
}

/**
 * @brief 帧刷新槽函数：读取摄像头画面、渲染到UI
 * @note 定时器每33ms触发一次，读取OpenCV帧、转换为Qt图像、自适应缩放显示
 */
void CameraWidget::updateFrame()
{
    cv::Mat frame;
    // 成功读取一帧画面
    if (m_capture.read(frame)) {
        emit frameCaptured(frame); // 抛出原始帧信号，供外部使用

        // OpenCV Mat转为QImage，保存为当前帧
        m_currentFrame = matToQImage(frame);
        // 自适应缩放画面，保持比例、平滑渲染
        QPixmap pixmap = QPixmap::fromImage(m_currentFrame).scaled(
            m_displayLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        m_displayLabel->setPixmap(pixmap);
        // 清空文字样式，只显示画面
        m_displayLabel->setStyleSheet("QLabel { background-color: #222; border: 2px solid #555; border-radius: 4px; }");

        m_frameCount++; // 帧计数累加
        updateFPS(); // 更新帧率统计
    }
}

/**
 * @brief 摄像头下拉索引切换槽函数
 * @param index 下拉框选中索引
 * @note 更新待开启的摄像头编号
 */
void CameraWidget::onCameraIndexChanged(int index)
{
    m_cameraIndex = m_cameraCombo->itemData(index).toInt();
}

/**
 * @brief 曝光滑块数值变化槽函数
 * @param value 滑块当前值
 */
void CameraWidget::onExposureChanged(int value)
{
    setExposure(value);
}

/**
 * @brief OpenCV Mat 转 Qt QImage 工具函数
 * @param mat OpenCV原始图像矩阵
 * @return QImage 适配Qt的图像对象
 * @note 兼容灰度图、RGB彩色图、RGBA透明图，处理通道顺序差异
 */
QImage CameraWidget::matToQImage(const cv::Mat &mat)
{
    if (mat.empty()) return QImage();

    // 8位单通道灰度图
    if (mat.type() == CV_8UC1) {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return image.copy();
    }
    // 8位3通道彩色图(OpenCV默认BGR，需转换为RGB)
    else if (mat.type() == CV_8UC3) {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return image.rgbSwapped();
    }
    // 8位4通道透明彩色图
    else if (mat.type() == CV_8UC4) {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGBA8888);
        return image.copy();
    }

    return QImage();
}

/**
 * @brief 实时更新FPS帧率
 * @note 每1秒统计一次总帧数，计算平均帧率并更新UI
 */
void CameraWidget::updateFPS()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 delta = now - m_lastFpsTime;

    // 间隔满1秒更新一次FPS
    if (delta >= 1000) {
        // 计算平均帧率
        m_fps = (double)m_frameCount * 1000.0 / delta;
        // 保留1位小数显示
        m_fpsLabel->setText(QString("FPS: %1").arg(m_fps, 0, 'f', 1));
        emit fpsChanged(m_fps); // 抛出帧率变化信号
        // 重置计数与时间戳
        m_frameCount = 0;
        m_lastFpsTime = now;
    }
}

/**
 * @brief 窗口大小改变事件重写
 * @param event 窗口尺寸事件
 * @note 窗口缩放时，摄像头画面自适应窗口大小
 */
void CameraWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // 摄像头运行时实时适配窗口尺寸
    if (m_isRunning && !m_currentFrame.isNull()) {
        QPixmap pixmap = QPixmap::fromImage(m_currentFrame).scaled(
            m_displayLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        m_displayLabel->setPixmap(pixmap);
    }
}
