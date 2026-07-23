/**
 * @file config_loader.hpp
 * @brief 配置加载器，从 YAML 文件加载系统配置。
 *
 * 定义 Config 结构体涵盖日志、串口、相机、颜色检测、卡尔曼滤波、
 * 场地、YOLO 模型和系统信息等全部配置项，并提供单例 ConfigLoader
 * 从 YAML 文件逐节解析填充。当 yaml-cpp 不可用时使用默认值。
 */
#pragma once

#include <memory>
#include <string>

namespace YAML {
    class Node;
}

namespace gonxun {

/**
 * @brief 系统配置结构体，涵盖所有可配置子系统。
 *
 * 各子结构体成员均带有默认值，YAML 中未指定的项保持默认。
 */
struct Config {
    /// 日志配置
    struct {
        std::string level{"INFO"};                                        ///< 日志级别（DEBUG/INFO/WARNING/ERROR）
        std::string format{"%(asctime)s [%(levelname)s] %(message)s"};    ///< 日志格式字符串
    } logging;

    /// 串口通信配置
    struct {
        std::string port{"/dev/ttyCH341USB0"};  ///< 串口设备路径
        int baudrate{115200};                    ///< 波特率
        double timeout{0.05};                    ///< 读写超时（秒）
        bool mock{true};                         ///< 是否模拟串口（调试用）
        bool mock_cycle{true};                   ///< 是否模拟串口循环
    } serial;

    /// 运动控制配置
    struct {
        int default_speed{300};      ///< 默认移动速度 (mm/s)
        int default_accel{500};      ///< 默认加速度 (mm/s²)
        int max_retries{3};          ///< 超时重试次数
        int command_timeout{500};    ///< 指令超时时间 (ms)
        int steps_per_grid{480};     ///< 每格步数脉冲数
        int grid_size_mm{480};       ///< 每格物理尺寸 (mm)
    } motion;

    /// 相机配置
    struct {
        /// 主相机配置
        struct {
            int index{1};        ///< 设备索引号
            int width{640};      ///< 分辨率宽度
            int height{480};     ///< 分辨率高度
        } main;
        /// QR 扫码相机配置
        struct {
            int index{2};        ///< 设备索引号
            int width{640};      ///< 分辨率宽度
            int height{480};     ///< 分辨率高度
        } qr;
    } camera;

    /// 颜色检测配置
    struct {
        int min_area{2000};      ///< 最小检测面积（像素）
        int dock_min_area{3000}; ///< 对接区域最小面积（像素）
    } color_detection;

    /// 卡尔曼滤波参数
    struct {
        double Q{1e-5};          ///< 过程噪声协方差
        double R{1e-2};          ///< 测量噪声协方差
    } kalman_filter;

    /// 场地配置
    struct {
        int size{2400};           ///< 场地边长（mm）
        double pixel_per_mm{0.22};///< 像素/毫米 比率
        /// 车道配置
        struct {
            int width{400};       ///< 车道宽度（像素）
            int center{1200};     ///< 车道中心位置（像素）
            int start{1000};      ///< 车道起始位置（像素）
            int end{1400};        ///< 车道结束位置（像素）
        } lane;
    } field;

    /// YOLO 目标检测模型配置
    struct {
        std::string model_path;           ///< ONNX 模型文件路径
        std::string torchscript_path;     ///< TorchScript 模型文件路径
        double conf_threshold{0.5};       ///< 置信度阈值
        double iou_threshold{0.45};       ///< NMS IoU 阈值
        int imgsz{640};                   ///< 推理输入图像尺寸
        bool half{false};                 ///< 是否使用 FP16 半精度推理
    } yolo;

    /// 系统信息
    struct {
        std::string version{"3.0.0"};     ///< 系统版本号
        std::string name{"GonxunSystem"}; ///< 系统名称
        std::string description;          ///< 系统描述
    } system;
};

/**
 * @brief 配置加载器单例，从 YAML 文件加载 Config。
 *
 * 使用 Meyer's Singleton 保证全局唯一实例。
 * 当 yaml-cpp 库不可用时，load() 返回 false，使用默认配置。
 */
class ConfigLoader {
public:
    /**
     * @brief 获取单例实例。
     * @return ConfigLoader 的全局唯一引用
     */
    [[nodiscard]] static ConfigLoader& instance();

    /**
     * @brief 从 YAML 文件加载配置。
     * @param path YAML 配置文件路径
     * @return true 加载成功，false 加载失败（使用默认配置）
     */
    [[nodiscard]] bool load(const std::string& path);

    /// 获取当前配置的常量引用
    [[nodiscard]] const Config& config() const { return config_; }

    /// 判断配置是否已成功加载
    [[nodiscard]] bool is_loaded() const { return loaded_; }

private:
    ConfigLoader() = default;
    ~ConfigLoader() = default;
    ConfigLoader(const ConfigLoader&) = delete;
    ConfigLoader& operator=(const ConfigLoader&) = delete;

    /// 解析日志配置节
    void parse_logging(const YAML::Node& node);
    /// 解析串口配置节
    void parse_serial(const YAML::Node& node);
    /// 解析运动控制配置节
    void parse_motion(const YAML::Node& node);
    /// 解析相机配置节
    void parse_camera(const YAML::Node& node);
    /// 解析颜色检测配置节
    void parse_color_detection(const YAML::Node& node);
    /// 解析卡尔曼滤波配置节
    void parse_kalman_filter(const YAML::Node& node);
    /// 解析场地配置节
    void parse_field(const YAML::Node& node);
    /// 解析 YOLO 配置节
    void parse_yolo(const YAML::Node& node);
    /// 解析系统信息配置节
    void parse_system(const YAML::Node& node);

    Config config_;          ///< 当前配置
    bool loaded_{false};     ///< 是否已成功加载
};

} // namespace gonxun
