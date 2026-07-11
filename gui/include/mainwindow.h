#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include "courtmapwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;

private slots:
    void onMarkButtonClicked();
    void onObstacleToggled(int id, bool marked);
    void onSelectStartZoneClicked();
    void onStartZoneSelected(int zoneIndex, const QString &zoneName);

private:
    void updateStatus();

    CourtMapWidget *m_courtMap = nullptr;
    QPushButton *m_markBtn = nullptr;
    QPushButton *m_selectStartBtn = nullptr;
    QLabel *m_statusLabel = nullptr;
};

#endif