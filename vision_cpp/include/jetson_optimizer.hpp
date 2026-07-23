/**
 * @file jetson_optimizer.hpp
 * @brief Jetson Nano 性能优化模块
 *
 * 提供 Jetson 平台检测、GPU 内存/频率监控、性能模式切换、
 * 自适应推理参数优化等功能。非 Jetson 平台上所有函数
 * 返回安全默认值或 false。
 *
 * 性能模式通过 jetson_clocks 工具开启/恢复，
 * 需 sudo 权限。
 */
#pragma once

#include <atomic>
#include <string>
#include <thread>

/**
 * @brief 优化参数
 */
struct OptimParams {
    int imgsz;      ///< 推理图像尺寸 (px)，Jetson 上建议 320
    int batch_size; ///< 批量大小，Jetson 上建议 1
    int workers;    ///< 工作线程数，Jetson 上建议 0（主线程推理）
    bool half;      ///< 是否使用 FP16 半精度
};

/**
 * @brief GPU 内存信息
 */
struct MemoryInfo {
    int used_mb;        ///< 已用内存 (MB)
    int total_mb;       ///< 总内存 (MB)
    int free_mb;        ///< 空闲内存 (MB)
    double usage_percent; ///< 使用率 (%)
};

/**
 * @brief 检测当前是否为 Jetson 平台
 * @return 是 Jetson 返回 true
 */
[[nodiscard]] bool is_jetson_platform();

/**
 * @brief 获取 GPU 内存使用信息（通过 tegrastats 解析）
 * @return MemoryInfo 结构；非 Jetson 返回全0
 */
[[nodiscard]] MemoryInfo get_gpu_memory_info();

/**
 * @brief 获取 GPU 当前频率
 * @return 频率 (MHz)；非 Jetson 或读取失败返回 0
 */
[[nodiscard]] double get_gpu_frequency();

/**
 * @brief 根据平台和内存状况生成优化参数
 * @return 优化参数；Jetson 4GB 返回 {320,1,0,true}，其他返回 {640,8,0,false}
 */
[[nodiscard]] OptimParams optimize_for_jetson();

/**
 * @brief 开启 Jetson 最大性能模式 (sudo jetson_clocks)
 * @return 成功返回 true；非 Jetson 或执行失败返回 false
 */
[[nodiscard]] bool enable_performance_mode();

/**
 * @brief 恢复 Jetson 默认性能模式 (sudo jetson_clocks --restore)
 * @return 成功返回 true；非 Jetson 或执行失败返回 false
 */
[[nodiscard]] bool disable_performance_mode();

/**
 * @brief 打印 Jetson 系统状态（内存/GPU频率/CPU温度）到标准输出
 */
void print_system_status();

/**
 * @brief Jetson 性能后台监控器
 *
 * 定期检查 GPU 内存使用率，超过 90% 时输出警告。
 */
class JetsonPerformanceMonitor {
public:
    /**
     * @brief 构造函数
     * @param interval 监控间隔 (秒)，默认 5
     */
    explicit JetsonPerformanceMonitor(int interval = 5);
    ~JetsonPerformanceMonitor();

    /** @brief 启动监控线程；非 Jetson 平台输出警告后返回 */
    void start();
    /** @brief 停止监控线程 */
    void stop();

private:
    /** @brief 监控循环：定期检查内存使用率并报警 */
    void monitor_loop();
    int interval_;                ///< 监控间隔 (秒)
    std::atomic<bool> running_;   ///< 线程运行标志
    std::thread thread_;          ///< 监控线程
};
