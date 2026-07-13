/**
 * Jetson Nano 性能优化模块
 * 对应 Python: vision/jetson_optimizer.py
 */
#pragma once

#include <atomic>
#include <string>
#include <thread>

/** 优化参数结构体 */
struct OptimParams {
    int imgsz;
    int batchSize;
    int workers;
    bool half;
};

/** 检测是否在 Jetson 平台上运行 */
bool isJetsonPlatform();

/** 获取 GPU 内存信息 (Jetson 共享内存) */
struct MemoryInfo {
    int usedMb;
    int totalMb;
    int freeMb;
    double usagePercent;
};
MemoryInfo getGpuMemoryInfo();

/** 获取 GPU 当前频率 (MHz) */
double getGpuFrequency();

/** 获取优化参数 */
OptimParams optimizeForJetson();

/** 开启 Jetson 最大性能模式 */
bool enablePerformanceMode();

/** 关闭 Jetson 性能模式 */
bool disablePerformanceMode();

/** 打印系统状态信息 */
void printSystemStatus();

/** Jetson 性能监控器 */
class JetsonPerformanceMonitor {
public:
    explicit JetsonPerformanceMonitor(int interval = 5);
    ~JetsonPerformanceMonitor();

    void start();
    void stop();

private:
    void monitorLoop();
    int m_interval;
    std::atomic<bool> m_running;
    std::thread m_thread;
};
