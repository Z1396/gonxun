/**
 * Jetson Nano 性能优化模块实现
 * 对应 Python: vision/jetson_optimizer.py
 */
#include "jetson_optimizer.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <chrono>

bool isJetsonPlatform() {
    std::ifstream f("/etc/nv_tegra_release");
    return f.good();
}

MemoryInfo getGpuMemoryInfo() {
    MemoryInfo info{0, 0, 0, 0.0};
    if (!isJetsonPlatform()) return info;

    // 使用 tegrastats 获取内存信息
    FILE* pipe = popen("tegrastats --interval 1", "r");
    if (!pipe) return info;

    char buffer[512];
    if (fgets(buffer, sizeof(buffer), pipe)) {
        std::string output(buffer);
        // 示例: RAM 2048/3964MB (lfb 2x4MB) CPU ...
        auto ramPos = output.find("RAM ");
        if (ramPos != std::string::npos) {
            auto mbPos = output.find("MB", ramPos);
            if (mbPos != std::string::npos) {
                std::string parts = output.substr(ramPos + 4, mbPos - ramPos - 4);
                auto slashPos = parts.find('/');
                if (slashPos != std::string::npos) {
                    info.usedMb = std::atoi(parts.substr(0, slashPos).c_str());
                    info.totalMb = std::atoi(parts.substr(slashPos + 1).c_str());
                    info.freeMb = info.totalMb - info.usedMb;
                    info.usagePercent = info.totalMb > 0
                        ? static_cast<double>(info.usedMb) / info.totalMb * 100 : 0;
                }
            }
        }
    }
    pclose(pipe);
    return info;
}

double getGpuFrequency() {
    if (!isJetsonPlatform()) return 0.0;
    const char* freqPath = "/sys/devices/17000000.gp10b/devfreq/17000000.gp10b/cur_freq";
    std::ifstream f(freqPath);
    if (f.is_open()) {
        std::string line;
        std::getline(f, line);
        long freqHz = std::atol(line.c_str());
        return freqHz / 1e6;
    }
    return 0.0;
}

OptimParams optimizeForJetson() {
    if (!isJetsonPlatform()) {
        return {640, 8, 0, false};
    }

    auto memInfo = getGpuMemoryInfo();
    if (memInfo.totalMb > 0 && memInfo.totalMb <= 4096) {
        if (memInfo.usagePercent > 80) {
            std::cerr << "[Jetson] 内存使用率过高: " << memInfo.usagePercent << "%" << std::endl;
        }
        return {320, 1, 0, true};
    }
    return {320, 1, 0, true};
}

bool enablePerformanceMode() {
    if (!isJetsonPlatform()) return false;
    int ret = system("sudo jetson_clocks");
    if (ret == 0) {
        std::cout << "[Jetson] 已开启最大性能模式" << std::endl;
        return true;
    }
    return false;
}

bool disablePerformanceMode() {
    if (!isJetsonPlatform()) return false;
    int ret = system("sudo jetson_clocks --restore");
    if (ret == 0) {
        std::cout << "[Jetson] 已恢复默认性能模式" << std::endl;
        return true;
    }
    return false;
}

void printSystemStatus() {
    if (!isJetsonPlatform()) {
        std::cout << "非 Jetson 平台" << std::endl;
        return;
    }

    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "Jetson 系统状态\n";
    std::cout << std::string(50, '=') << "\n";

    auto memInfo = getGpuMemoryInfo();
    std::cout << "\n内存使用:\n";
    std::cout << "  已用: " << memInfo.usedMb << " MB\n";
    std::cout << "  总量: " << memInfo.totalMb << " MB\n";
    std::cout << "  空闲: " << memInfo.freeMb << " MB\n";
    std::cout << "  使用率: " << memInfo.usagePercent << "%\n";

    double gpuFreq = getGpuFrequency();
    if (gpuFreq > 0) {
        std::cout << "\nGPU 频率: " << static_cast<int>(gpuFreq) << " MHz\n";
    }

    // CPU 温度
    std::ifstream tempFile("/sys/devices/virtual/thermal/thermal_zone0/temp");
    if (tempFile.is_open()) {
        std::string tempStr;
        std::getline(tempFile, tempStr);
        double temp = std::atof(tempStr.c_str()) / 1000.0;
        std::cout << "CPU 温度: " << temp << "°C\n";
    }

    std::cout << std::string(50, '=') << "\n";
}

JetsonPerformanceMonitor::JetsonPerformanceMonitor(int interval)
    : m_interval(interval), m_running(false) {}

JetsonPerformanceMonitor::~JetsonPerformanceMonitor() {
    stop();
}

void JetsonPerformanceMonitor::start() {
    if (!isJetsonPlatform()) {
        std::cerr << "非 Jetson 平台，监控不可用" << std::endl;
        return;
    }
    m_running = true;
    m_thread = std::thread(&JetsonPerformanceMonitor::monitorLoop, this);
    std::cout << "性能监控已启动（间隔 " << m_interval << "s）" << std::endl;
}

void JetsonPerformanceMonitor::stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void JetsonPerformanceMonitor::monitorLoop() {
    while (m_running) {
        auto memInfo = getGpuMemoryInfo();
        if (memInfo.usagePercent > 90) {
            std::cerr << "[警告] 内存使用率过高: " << memInfo.usagePercent << "%" << std::endl;
            std::cerr << "建议: 降低 batch_size 或 imgsz" << std::endl;
        }
        // sleep
        for (int i = 0; i < m_interval * 10 && m_running; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
