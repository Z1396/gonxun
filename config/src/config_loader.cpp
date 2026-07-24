/**
 * @file config_loader.cpp
 * @brief 配置加载器实现。
 *
 * 使用 yaml-cpp 库逐节解析 YAML 配置文件，填充 Config 全局结构体。
 * 采用模块化解析设计，每个业务模块独立解析，代码解耦易维护。
 * 当 YAML_CPP_AVAILABLE 宏未定义时，禁用YAML解析逻辑，
 * init() 直接返回错误，所有业务配置项保持结构体默认初始值，实现无依赖兼容。
 */

#include "config_loader.hpp"

#include <fstream>
#include <iostream>

#ifdef YAML_CPP_AVAILABLE
#include <yaml-cpp/yaml.h>
#endif

namespace gonxun {

ConfigLoader& ConfigLoader::instance() {
    static ConfigLoader inst;
    return inst;
}

ExpectedVoid ConfigLoader::init(const std::string& path) noexcept {
#ifdef YAML_CPP_AVAILABLE
    try {
        YAML::Node root = YAML::LoadFile(path);

        ConfigLoader& inst = instance();

        if (root["logging"])         inst.parse_logging(root["logging"]);
        if (root["serial"])          inst.parse_serial(root["serial"]);
        if (root["motion"])          inst.parse_motion(root["motion"]);
        if (root["camera"])          inst.parse_camera(root["camera"]);
        if (root["color_detection"]) inst.parse_color_detection(root["color_detection"]);
        if (root["kalman_filter"])   inst.parse_kalman_filter(root["kalman_filter"]);
        if (root["field"])           inst.parse_field(root["field"]);
        if (root["yolo"])            inst.parse_yolo(root["yolo"]);
        if (root["system"])          inst.parse_system(root["system"]);

        std::cout << "[Config] 配置加载成功: " << path << std::endl;
        return {};
    } catch (const std::exception& e) {
        return std::string("[Config] 配置加载失败: " + std::string(e.what()));
    }
#else
    std::cout << "[Config] yaml-cpp 未安装，使用默认配置" << std::endl;
    return {};
#endif
}

#ifdef YAML_CPP_AVAILABLE

void ConfigLoader::parse_logging(const YAML::Node& node) {
    if (node["level"]) config_.logging.level = node["level"].as<std::string>();
    if (node["format"]) config_.logging.format = node["format"].as<std::string>();
}

void ConfigLoader::parse_serial(const YAML::Node& node) {
    if (node["port"]) config_.serial.port = node["port"].as<std::string>();
    if (node["baudrate"]) config_.serial.baudrate = node["baudrate"].as<int>();
    if (node["timeout"]) config_.serial.timeout = node["timeout"].as<double>();
    if (node["mock"]) config_.serial.mock = node["mock"].as<bool>();
    if (node["mock_cycle"]) config_.serial.mock_cycle = node["mock_cycle"].as<bool>();
}

void ConfigLoader::parse_motion(const YAML::Node& node) {
    if (node["default_speed"]) config_.motion.default_speed = node["default_speed"].as<int>();
    if (node["default_accel"]) config_.motion.default_accel = node["default_accel"].as<int>();
    if (node["max_retries"]) config_.motion.max_retries = node["max_retries"].as<int>();
    if (node["command_timeout"]) config_.motion.command_timeout = node["command_timeout"].as<int>();
    if (node["steps_per_grid"]) config_.motion.steps_per_grid = node["steps_per_grid"].as<int>();
    if (node["grid_size_mm"]) config_.motion.grid_size_mm = node["grid_size_mm"].as<int>();
}

void ConfigLoader::parse_camera(const YAML::Node& node) {
    if (node["main"]) {
        auto& main = node["main"];
        if (main["index"]) config_.camera.main.index = main["index"].as<int>();
        if (main["width"]) config_.camera.main.width = main["width"].as<int>();
        if (main["height"]) config_.camera.main.height = main["height"].as<int>();
        if (main["buffer_size"]) config_.camera.main.buffer_size = main["buffer_size"].as<int>();
        if (main["auto_exposure"]) config_.camera.main.auto_exposure = main["auto_exposure"].as<int>();
        if (main["exposure"]) config_.camera.main.exposure = main["exposure"].as<int>();
        if (main["gain"]) config_.camera.main.gain = main["gain"].as<int>();
    }
    if (node["qr"]) {
        auto& qr = node["qr"];
        if (qr["index"]) config_.camera.qr.index = qr["index"].as<int>();
        if (qr["width"]) config_.camera.qr.width = qr["width"].as<int>();
        if (qr["height"]) config_.camera.qr.height = qr["height"].as<int>();
        if (qr["buffer_size"]) config_.camera.qr.buffer_size = qr["buffer_size"].as<int>();
        if (qr["auto_exposure"]) config_.camera.qr.auto_exposure = qr["auto_exposure"].as<int>();
        if (qr["exposure"]) config_.camera.qr.exposure = qr["exposure"].as<int>();
        if (qr["gain"]) config_.camera.qr.gain = qr["gain"].as<int>();
    }
}

void ConfigLoader::parse_color_detection(const YAML::Node& node) {
    if (node["min_area"]) config_.color_detection.min_area = node["min_area"].as<int>();
    if (node["dock_min_area"]) config_.color_detection.dock_min_area = node["dock_min_area"].as<int>();
}

void ConfigLoader::parse_kalman_filter(const YAML::Node& node) {
    if (node["Q"]) config_.kalman_filter.Q = node["Q"].as<double>();
    if (node["R"]) config_.kalman_filter.R = node["R"].as<double>();
}

void ConfigLoader::parse_field(const YAML::Node& node) {
    if (node["size"]) config_.field.size = node["size"].as<int>();
    if (node["pixel_per_mm"]) config_.field.pixel_per_mm = node["pixel_per_mm"].as<double>();
    if (node["lane"]) {
        auto& lane = node["lane"];
        if (lane["width"]) config_.field.lane.width = lane["width"].as<int>();
        if (lane["center"]) config_.field.lane.center = lane["center"].as<int>();
        if (lane["start"]) config_.field.lane.start = lane["start"].as<int>();
        if (lane["end"]) config_.field.lane.end = lane["end"].as<int>();
    }
}

void ConfigLoader::parse_yolo(const YAML::Node& node) {
    if (node["model_path"]) config_.yolo.model_path = node["model_path"].as<std::string>();
    if (node["torchscript_path"]) config_.yolo.torchscript_path = node["torchscript_path"].as<std::string>();
    if (node["conf_threshold"]) config_.yolo.conf_threshold = node["conf_threshold"].as<double>();
    if (node["iou_threshold"]) config_.yolo.iou_threshold = node["iou_threshold"].as<double>();
    if (node["imgsz"]) config_.yolo.imgsz = node["imgsz"].as<int>();
    if (node["half"]) config_.yolo.half = node["half"].as<bool>();
}

void ConfigLoader::parse_system(const YAML::Node& node) {
    if (node["version"]) config_.system.version = node["version"].as<std::string>();
    if (node["name"]) config_.system.name = node["name"].as<std::string>();
    if (node["description"]) config_.system.description = node["description"].as<std::string>();
}

#endif // YAML_CPP_AVAILABLE

} // namespace gonxun