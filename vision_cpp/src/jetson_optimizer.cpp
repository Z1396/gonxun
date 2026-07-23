/**
 * @file jetson_optimizer.cpp
 * @brief Jetson Nano 性能优化模块实现
 *
 * 通过 tegrastats 解析 GPU 内存，通过 sysfs 读取 GPU 频率和 CPU 温度，
 * 通过 jetson_clocks 切换性能模式。非 Jetson 平台所有函数返回安全默认值。
 */
#include "jetson_optimizer.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

/**
 * @brief 检测是否为 Jetson 平台
 * @return /etc/nv_tegra_release 存在返回 true
 */
bool is_jetson_platform() {
    std::ifstream f("/etc/nv_tegra_release");
    return f.good();
}

/**
 * @brief 获取 GPU 内存信息
 * @return MemoryInfo 结构；非 Jetson 返回全0
 * @note 通过 tegrastats 输出解析 "RAM XXX/YYY MB" 行
 */
MemoryInfo get_gpu_memory_info() {
    MemoryInfo info{0, 0, 0, 0.0};

    if (!is_jetson_platform()) return info;

    FILE* pipe = popen("tegrastats --interval 1", "r");
    if (!pipe) return info;

    char buffer[512];
    if (fgets(buffer, sizeof(buffer), pipe)) {
        std::string output(buffer);

        // 解析 "RAM XXX/YYY MB" 格式
        auto ram_pos = output.find("RAM ");
        if (ram_pos != std::string::npos) {
            auto mb_pos = output.find("MB", ram_pos);
            if (mb_pos != std::string::npos) {
                std::string parts = output.substr(ram_pos + 4, mb_pos - ram_pos - 4);
                auto slash_pos = parts.find('/');

                if (slash_pos != std::string::npos) {
                    info.used_mb = std::atoi(parts.substr(0, slash_pos).c_str());
                    info.total_mb = std::atoi(parts.substr(slash_pos + 1).c_str());
                    info.free_mb = info.total_mb - info.used_mb;
                    info.usage_percent = info.total_mb > 0
                        ? static_cast<double>(info.used_mb) / info.total_mb * 100 : 0;
                }
            }
        }
    }

    pclose(pipe);
    return info;
}

/**
 * @brief 获取 GPU 当前频率
 * @return 频率 (MHz)；非 Jetson 或读取失败返回 0
 * @note 读取 /sys/devices/17000000.gp10b/devfreq/.../cur_freq
 */
double get_gpu_frequency() {
    if (!is_jetson_platform()) return 0.0;

    const char* freq_path = "/sys/devices/17000000.gp10b/devfreq/17000000.gp10b/cur_freq";

    std::ifstream f(freq_path);
    if (f.is_open()) {
        std::string line;
        std::getline(f, line);

        long freq_hz = std::atol(line.c_str());
        return freq_hz / 1e6;  // Hz → MHz
    }

    return 0.0;
}

/**
 * @brief 根据平台和内存状况生成优化参数
 * @return Jetson 4GB: {320,1,0,true}；其他: {640,8,0,false}
 * @note 内存使用率>80% 时输出警告
 */
OptimParams optimize_for_jetson() {
    if (!is_jetson_platform()) {
        return {640, 8, 0, false};
    }

    auto mem_info = get_gpu_memory_info();

    if (mem_info.total_mb > 0 && mem_info.total_mb <= 4096) {
        if (mem_info.usage_percent > 80) {
            std::cerr << "[Jetson] 内存使用率过高: " << mem_info.usage_percent << "%" << std::endl;
        }
        return {320, 1, 0, true};
    }

    return {320, 1, 0, true};
}

/**
 * @brief 开启最大性能模式
 * @return 成功返回 true
 * @note 执行 sudo jetson_clocks，需 sudo 免密配置
 */
bool enable_performance_mode() {
    if (!is_jetson_platform()) return false;

    int ret = system("sudo jetson_clocks");

    if (ret == 0) {
        std::cout << "[Jetson] 已开启最大性能模式" << std::endl;
        return true;
    }

    return false;
}

/**
 * @brief 恢复默认性能模式
 * @return 成功返回 true
 * @note 执行 sudo jetson_clocks --restore
 */
bool disable_performance_mode() {
    if (!is_jetson_platform()) return false;

    int ret = system("sudo jetson_clocks --restore");

    if (ret == 0) {
        std::cout << "[Jetson] 已恢复默认性能模式" << std::endl;
        return true;
    }

    return false;
}

/**
 * @brief 打印 Jetson 系统状态到标准输出
 * @note 输出: 内存使用、GPU 频率、CPU 温度
 */
void print_system_status() {
    if (!is_jetson_platform()) {
        std::cout << "非 Jetson 平台" << std::endl;
        return;
    }

    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "Jetson 系统状态\n";
    std::cout << std::string(50, '=') << "\n";

    auto mem_info = get_gpu_memory_info();
    std::cout << "\n内存使用:\n";
    std::cout << "  已用: " << mem_info.used_mb << " MB\n";
    std::cout << "  总量: " << mem_info.total_mb << " MB\n";
    std::cout << "  空闲: " << mem_info.free_mb << " MB\n";
    std::cout << "  使用率: " << mem_info.usage_percent << "%\n";

    double gpu_freq = get_gpu_frequency();
    if (gpu_freq > 0) {
        std::cout << "\nGPU 频率: " << static_cast<int>(gpu_freq) << " MHz\n";
    }

    // CPU 温度从 thermal_zone0 读取，单位 mC → °C
    std::ifstream temp_file("/sys/devices/virtual/thermal/thermal_zone0/temp");
    if (temp_file.is_open()) {
        std::string temp_str;
        std::getline(temp_file, temp_str);

        double temp = std::atof(temp_str.c_str()) / 1000.0;
        std::cout << "CPU 温度: " << temp << "°C\n";
    }

    std::cout << std::string(50, '=') << "\n";
}

/**
 * @brief 构造函数
 * @param interval 监控间隔 (秒)
 */
JetsonPerformanceMonitor::JetsonPerformanceMonitor(int interval)
    : interval_(interval), running_(false) {}

/** @brief 析构，停止监控线程 */
JetsonPerformanceMonitor::~JetsonPerformanceMonitor() {
    stop();
}

/**
 * @brief 启动监控线程
 * @note 非 Jetson 平台输出警告后返回
 */
void JetsonPerformanceMonitor::start() {
    if (!is_jetson_platform()) {
        std::cerr << "非 Jetson 平台，监控不可用" << std::endl;
        return;
    }

    running_ = true;
    thread_ = std::thread(&JetsonPerformanceMonitor::monitor_loop, this);

    std::cout << "性能监控已启动（间隔 " << interval_ << "s）" << std::endl;
}

/** @brief 停止监控线程 */
void JetsonPerformanceMonitor::stop() {
    running_ = false;

    if (thread_.joinable()) {
        thread_.join();
    }
}

/**
 * @brief 监控循环
 * @note 每 interval_ 秒检查一次内存使用率，>90% 时输出警告
 */
void JetsonPerformanceMonitor::monitor_loop() {
    while (running_) {
        auto mem_info = get_gpu_memory_info();

        if (mem_info.usage_percent > 90) {
            std::cerr << "[警告] 内存使用率过高: " << mem_info.usage_percent << "%" << std::endl;
            std::cerr << "建议: 降低 batch_size 或 imgsz" << std::endl;
        }

        // 分段 sleep 以便及时响应 stop()
        for (int i = 0; i < interval_ * 10 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
