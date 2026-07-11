#include "camerawidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QLabel>

CameraWidget::CameraWidget(QWidget *parent)
    : QWidget(parent)
    , m_isRunning(false)
    , m_cameraIndex(0)
    , m_frameWidth(640)
    , m_frameHeight(480)
    , m_frameCount(0)
    , m_fps(0)
    , m_lastFpsTime(0)
{
    setupUI();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &CameraWidget::updateFrame);
    m_timer->setInterval(33);
}

CameraWidget::~CameraWidget()
{
    stopCamera();
}

void CameraWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    m_displayLabel = new QLabel(this);
    m_displayLabel->setAlignment(Qt::AlignCenter);
    m_displayLabel->setMinimumSize(320, 240);
    m_displayLabel->setStyleSheet("QLabel { background-color: #222; border: 2px solid #555; border-radius: 4px; }");
    m_displayLabel->setText("摄像头未开启");
    m_displayLabel->setStyleSheet("QLabel { color: #888; background-color: #222; border: 2px solid #555; border-radius: 4px; font-size: 16px; }");
    mainLayout->addWidget(m_displayLabel, 1);

    QHBoxLayout *controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(10);

    m_startBtn = new QPushButton("开启", this);
    m_startBtn->setMinimumHeight(40);
    m_startBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border: none; border-radius: 4px; font-size: 14px; } QPushButton:hover { background-color: #45a049; } QPushButton:pressed { background-color: #3d8b40; }");
    connect(m_startBtn, &QPushButton::clicked, this, [this]() { startCamera(m_cameraIndex); });
    controlLayout->addWidget(m_startBtn);

    m_stopBtn = new QPushButton("停止", this);
    m_stopBtn->setMinimumHeight(40);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; border: none; border-radius: 4px; font-size: 14px; } QPushButton:hover { background-color: #da190b; } QPushButton:disabled { background-color: #ccc; }");
    connect(m_stopBtn, &QPushButton::clicked, this, &CameraWidget::stopCamera);
    controlLayout->addWidget(m_stopBtn);

    m_snapshotBtn = new QPushButton("截图", this);
    m_snapshotBtn->setMinimumHeight(40);
    m_snapshotBtn->setEnabled(false);
    m_snapshotBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border: none; border-radius: 4px; font-size: 14px; } QPushButton:hover { background-color: #0b7dda; } QPushButton:disabled { background-color: #ccc; }");
    connect(m_snapshotBtn, &QPushButton::clicked, this, &CameraWidget::saveSnapshot);
    controlLayout->addWidget(m_snapshotBtn);

    mainLayout->addLayout(controlLayout);

    QHBoxLayout *settingLayout = new QHBoxLayout();
    settingLayout->setSpacing(10);

    settingLayout->addWidget(new QLabel("摄像头:", this));
    m_cameraCombo = new QComboBox(this);
    m_cameraCombo->addItem("摄像头 0", 0);
    m_cameraCombo->addItem("摄像头 1", 1);
    m_cameraCombo->addItem("摄像头 2", 2);
    m_cameraCombo->setMinimumHeight(35);
    connect(m_cameraCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CameraWidget::onCameraIndexChanged);
    settingLayout->addWidget(m_cameraCombo, 1);

    mainLayout->addLayout(settingLayout);

    QHBoxLayout *exposureLayout = new QHBoxLayout();
    exposureLayout->addWidget(new QLabel("曝光:", this));
    m_exposureSlider = new QSlider(Qt::Horizontal, this);
    m_exposureSlider->setRange(-10, 10);
    m_exposureSlider->setValue(0);
    connect(m_exposureSlider, &QSlider::valueChanged, this, &CameraWidget::onExposureChanged);
    exposureLayout->addWidget(m_exposureSlider, 1);
    mainLayout->addLayout(exposureLayout);

    QHBoxLayout *statusLayout = new QHBoxLayout();
    m_fpsLabel = new QLabel("FPS: 0", this);
    m_fpsLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
    statusLayout->addWidget(m_fpsLabel);

    m_resolutionLabel = new QLabel("分辨率: --", this);
    m_resolutionLabel->setStyleSheet("color: #888;");
    statusLayout->addWidget(m_resolutionLabel, 0, Qt::AlignRight);

    mainLayout->addLayout(statusLayout);
}

void CameraWidget::startCamera(int index)
{
    if (m_isRunning) {
        stopCamera();
    }

    m_cameraIndex = index;

    if (!m_capture.open(index)) {
        emit cameraError(QString("无法打开摄像头 %1").arg(index));
        QMessageBox::warning(this, "错误", QString("无法打开摄像头 %1").arg(index));
        return;
    }

    m_capture.set(cv::CAP_PROP_FRAME_WIDTH, m_frameWidth);
    m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, m_frameHeight);

    m_frameWidth = m_capture.get(cv::CAP_PROP_FRAME_WIDTH);
    m_frameHeight = m_capture.get(cv::CAP_PROP_FRAME_HEIGHT);
    m_resolutionLabel->setText(QString("分辨率: %1x%2").arg(m_frameWidth).arg(m_frameHeight));

    m_isRunning = true;
    m_frameCount = 0;
    m_fps = 0;
    m_lastFpsTime = QDateTime::currentMSecsSinceEpoch();

    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_snapshotBtn->setEnabled(true);
    m_cameraCombo->setEnabled(false);

    m_timer->start();
}

void CameraWidget::stopCamera()
{
    m_timer->stop();
    m_capture.release();
    m_isRunning = false;

    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_snapshotBtn->setEnabled(false);
    m_cameraCombo->setEnabled(true);

    m_displayLabel->setText("摄像头已停止");
    m_displayLabel->setStyleSheet("QLabel { color: #888; background-color: #222; border: 2px solid #555; border-radius: 4px; font-size: 16px; }");
    m_fpsLabel->setText("FPS: 0");
}

void CameraWidget::setExposure(int value)
{
    if (m_isRunning && m_capture.isOpened()) {
#ifdef _WIN32
        m_capture.set(cv::CAP_PROP_EXPOSURE, value);
#else
        m_capture.set(cv::CAP_PROP_AUTO_EXPOSURE, 1);
        m_capture.set(cv::CAP_PROP_EXPOSURE, value * 10 + 50);
#endif
    }
}

void CameraWidget::setResolution(int width, int height)
{
    m_frameWidth = width;
    m_frameHeight = height;
    if (m_isRunning) {
        m_capture.set(cv::CAP_PROP_FRAME_WIDTH, width);
        m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    }
}

void CameraWidget::saveSnapshot()
{
    if (!m_isRunning || m_currentFrame.isNull()) return;

    QString fileName = QString("snapshot_%1.jpg").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString path = QDir::currentPath() + "/" + fileName;

    if (m_currentFrame.save(path)) {
        m_snapshotBtn->setText("已保存!");
        QTimer::singleShot(1000, m_snapshotBtn, [this]() { m_snapshotBtn->setText("截图"); });
    }
}

void CameraWidget::updateFrame()
{
    cv::Mat frame;
    if (m_capture.read(frame)) {
        emit frameCaptured(frame);

        m_currentFrame = matToQImage(frame);
        QPixmap pixmap = QPixmap::fromImage(m_currentFrame).scaled(
            m_displayLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        m_displayLabel->setPixmap(pixmap);
        m_displayLabel->setStyleSheet("QLabel { background-color: #222; border: 2px solid #555; border-radius: 4px; }");

        m_frameCount++;
        updateFPS();
    }
}

void CameraWidget::onCameraIndexChanged(int index)
{
    m_cameraIndex = m_cameraCombo->itemData(index).toInt();
}

void CameraWidget::onExposureChanged(int value)
{
    setExposure(value);
}

QImage CameraWidget::matToQImage(const cv::Mat &mat)
{
    if (mat.empty()) return QImage();

    if (mat.type() == CV_8UC1) {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return image.copy();
    } else if (mat.type() == CV_8UC3) {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return image.rgbSwapped();
    } else if (mat.type() == CV_8UC4) {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGBA8888);
        return image.copy();
    }

    return QImage();
}

void CameraWidget::updateFPS()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 delta = now - m_lastFpsTime;

    if (delta >= 1000) {
        m_fps = (double)m_frameCount * 1000.0 / delta;
        m_fpsLabel->setText(QString("FPS: %1").arg(m_fps, 0, 'f', 1));
        emit fpsChanged(m_fps);
        m_frameCount = 0;
        m_lastFpsTime = now;
    }
}

void CameraWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_isRunning && !m_currentFrame.isNull()) {
        QPixmap pixmap = QPixmap::fromImage(m_currentFrame).scaled(
            m_displayLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        m_displayLabel->setPixmap(pixmap);
    }
}
