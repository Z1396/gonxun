#ifndef DATA_PANEL_WIDGET_H
#define DATA_PANEL_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QGroupBox>
#include <QGridLayout>
#include <QString>
#include <QPropertyAnimation>

class DataPanelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DataPanelWidget(QWidget *parent = nullptr);
    ~DataPanelWidget() = default;

public slots:
    void updateRobotPose(int xMm, int yMm, double thetaDeg);

    void updateTaskState(const QString& state);
    void updateTaskProgress(int cycle, int totalCycles, int picked, int placed, int totalMaterials);
    void updateTaskCode(const QString& code);

    void refreshAll();

signals:
    void dataRefreshRequested();

private:
    void setupUI();
    void setupStyles();
    QLabel* createLabel(const QString& text, const QString& style = "");
    QLabel* createValueLabel(const QString& text = "--", const QString& color = "#2c3e50");
    void flashStateLabel();

private:
    QTimer* m_refreshTimer = nullptr;
    QPropertyAnimation* m_stateFlashAnim = nullptr;

    QGroupBox* m_robotGroup = nullptr;
    QLabel* m_robotX = nullptr;
    QLabel* m_robotY = nullptr;
    QLabel* m_robotTheta = nullptr;

    QGroupBox* m_taskGroup = nullptr;
    QLabel* m_taskState = nullptr;
    QLabel* m_taskCode = nullptr;
    QLabel* m_taskCycle = nullptr;
    QLabel* m_taskMaterials = nullptr;

    QString m_labelStyle;
    QString m_valueStyle;
    QString m_groupStyle;
    QString m_highlightStyle;
    QString m_stateNormalStyle;
    QString m_stateFlashStyle;
};

#endif
