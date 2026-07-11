#ifndef CAMERAWIDGET_H
#define CAMERAWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QImage>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSlider>
#include <opencv2/opencv.hpp>

class CameraWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraWidget(QWidget *parent = nullptr);
    ~CameraWidget();

    bool isRunning() const { return m_isRunning; }
    int cameraIndex() const { return m_cameraIndex; }

signals:
    void frameCaptured(const cv::Mat &frame);
    void cameraError(const QString &error);
    void fpsChanged(double fps);

public slots:
    void startCamera(int index = 0);
    void stopCamera();
    void setExposure(int value);
    void setResolution(int width, int height);
    void saveSnapshot();

private slots:
    void updateFrame();
    void onCameraIndexChanged(int index);
    void onExposureChanged(int value);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUI();
    QImage matToQImage(const cv::Mat &mat);
    void updateFPS();

    cv::VideoCapture m_capture;
    QLabel *m_displayLabel;
    QPushButton *m_startBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_snapshotBtn;
    QComboBox *m_cameraCombo;
    QSlider *m_exposureSlider;
    QLabel *m_fpsLabel;
    QLabel *m_resolutionLabel;

    QTimer *m_timer;
    bool m_isRunning;
    int m_cameraIndex;
    int m_frameWidth;
    int m_frameHeight;

    int m_frameCount;
    double m_fps;
    qint64 m_lastFpsTime;
    QImage m_currentFrame;
};

#endif
