/**
 * 配置加载器实现
 */

#include "config_loader.hpp"
#include <iostream>
#include <fstream>

#ifdef YAML_CPP_AVAILABLE
#include <yaml-cpp/yaml.h>
#endif

namespace gonxun {

ConfigLoader& ConfigLoader::instance() {
    static ConfigLoader instance;
    return instance;
}

bool ConfigLoader::load(const std::string& path) {
#ifdef YAML_CPP_AVAILABLE
    try {
        YAML::Node root = YAML::LoadFile(path);

        if (root["logging"]) parseLogging(root["logging"]);
        if (root["serial"]) parseSerial(root["serial"]);
        if (root["camera"]) parseCamera(root["camera"]);
        if (root["color_detection"]) parseColorDetection(root["color_detection"]);
        if (root["kalman_filter"]) parseKalmanFilter(root["kalman_filter"]);
        if (root["field"]) parseField(root["field"]);
        if (root["yolo"]) parseYolo(root["yolo"]);
        if (root["system"]) parseSystem(root["system"]);

        loaded_ = true;
        std::cout << "[Config] 配置加载成功: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Config] 配置加载失败: " << e.what() << std::endl;
        std::cerr << "[Config] 使用默认配置" << std::endl;
        return false;
    }
#else
    std::cout << "[Config] yaml-cpp 未安装，使用默认配置" << std::endl;
    return false;
#endif
}

#ifdef YAML_CPP_AVAILABLE
void ConfigLoader::parseLogging(const YAML::Node& node) {
    if (node["level"]) config_.logging.level = node["level"].as<std::string>();
    if (node["format"]) config_.logging.format = node["format"].as<std::string>();
}

void ConfigLoader::parseSerial(const YAML::Node& node) {
    if (node["port"]) config_.serial.port = node["port"].as<std::string>();
    if (node["baudrate"]) config_.serial.baudrate = node["baudrate"].as<int>();
    if (node["timeout"]) config_.serial.timeout = node["timeout"].as<double>();
    if (node["mock"]) config_.serial.mock = node["mock"].as<bool>();
    if (node["mock_cycle"]) config_.serial.mock_cycle = node["mock_cycle"].as<bool>();
}

void ConfigLoader::parseCamera(const YAML::Node& node) {
    if (node["main"]) {
        auto& main = node["main"];
        if (main["index"]) config_.camera.main.index = main["index"].as<int>();
        if (main["width"]) config_.camera.main.width = main["width"].as<int>();
        if (main["height"]) config_.camera.main.height = main["height"].as<int>();
    }
    if (node["qr"]) {
        auto& qr = node["qr"];
        if (qr["index"]) config_.camera.qr.index = qr["index"].as<int>();
        if (qr["width"]) config_.camera.qr.width = qr["width"].as<int>();
        if (qr["height"]) config_.camera.qr.height = qr["height"].as<int>();
    }
}

void ConfigLoader::parseColorDetection(const YAML::Node& node) {
    if (node["min_area"]) config_.color_detection.min_area = node["min_area"].as<int>();
    if (node["dock_min_area"]) config_.color_detection.dock_min_area = node["dock_min_area"].as<int>();
}

void ConfigLoader::parseKalmanFilter(const YAML::Node& node) {
    if (node["Q"]) config_.kalman_filter.Q = node["Q"].as<double>();
    if (node["R"]) config_.kalman_filter.R = node["R"].as<double>();
}

void ConfigLoader::parseField(const YAML::Node& node) {
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

void ConfigLoader::parseYolo(const YAML::Node& node) {
    if (node["model_path"]) config_.yolo.model_path = node["model_path"].as<std::string>();
    if (node["torchscript_path"]) config_.yolo.torchscript_path = node["torchscript_path"].as<std::string>();
    if (node["conf_threshold"]) config_.yolo.conf_threshold = node["conf_threshold"].as<double>();
    if (node["iou_threshold"]) config_.yolo.iou_threshold = node["iou_threshold"].as<double>();
    if (node["imgsz"]) config_.yolo.imgsz = node["imgsz"].as<int>();
    if (node["half"]) config_.yolo.half = node["half"].as<bool>();
}

void ConfigLoader::parseSystem(const YAML::Node& node) {
    if (node["version"]) config_.system.version = node["version"].as<std::string>();
    if (node["name"]) config_.system.name = node["name"].as<std::string>();
    if (node["description"]) config_.system.description = node["description"].as<std::string>();
}
#endif

} // namespace gonxun