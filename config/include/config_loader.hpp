/**
 * 配置加载器
 * 从 YAML 文件加载系统配置参数
 */
#pragma once

#include <string>
#include <memory>

// 前向声明，避免头文件依赖
namespace YAML {
    class Node;
}

namespace gonxun {

/**
 * 配置结构体
 * 包含所有可配置参数
 */
struct Config {
    // 日志配置
    struct {
        std::string level{"INFO"};
        std::string format{"%(asctime)s [%(levelname)s] %(message)s"};
    } logging;

    // 串口配置
    struct {
        std::string port{"/dev/ttyCH341USB0"};
        int baudrate{115200};
        double timeout{0.05};
        bool mock{true};
        bool mock_cycle{true};
    } serial;

    // 摄像头配置
    struct {
        struct {
            int index{1};
            int width{640};
            int height{480};
        } main;
        struct {
            int index{2};
            int width{640};
            int height{480};
        } qr;
    } camera;

    // 颜色识别配置
    struct {
        int min_area{2000};
        int dock_min_area{3000};
    } color_detection;

    // 卡尔曼滤波配置
    struct {
        double Q{1e-5};
        double R{1e-2};
    } kalman_filter;

    // 场地配置
    struct {
        int size{2400};
        double pixel_per_mm{0.22};
        struct {
            int width{400};
            int center{1200};
            int start{1000};
            int end{1400};
        } lane;
    } field;

    // YOLO 配置
    struct {
        std::string model_path;
        std::string torchscript_path;
        double conf_threshold{0.5};
        double iou_threshold{0.45};
        int imgsz{640};
        bool half{false};
    } yolo;

    // 系统配置
    struct {
        std::string version{"3.0.0"};
        std::string name{"GonxunSystem"};
        std::string description;
    } system;
};

/**
 * 配置加载器类
 * 单例模式，全局共享配置
 */
class ConfigLoader {
public:
    /**
     * 获取单例实例
     */
    static ConfigLoader& instance();

    /**
     * 从文件加载配置
     * @param path YAML 配置文件路径
     * @return 是否加载成功
     */
    bool load(const std::string& path);

    /**
     * 获取当前配置
     */
    const Config& config() const { return config_; }

    /**
     * 检查配置是否已加载
     */
    bool isLoaded() const { return loaded_; }

private:
    ConfigLoader() = default;
    ~ConfigLoader() = default;
    ConfigLoader(const ConfigLoader&) = delete;
    ConfigLoader& operator=(const ConfigLoader&) = delete;

    // 解析 YAML 节点
    void parseLogging(const YAML::Node& node);
    void parseSerial(const YAML::Node& node);
    void parseCamera(const YAML::Node& node);
    void parseColorDetection(const YAML::Node& node);
    void parseKalmanFilter(const YAML::Node& node);
    void parseField(const YAML::Node& node);
    void parseYolo(const YAML::Node& node);
    void parseSystem(const YAML::Node& node);

    Config config_;
    bool loaded_{false};
};

} // namespace gonxun