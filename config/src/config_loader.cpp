/**
 * @file config_loader.cpp
 * @brief 配置加载器实现。
 *
 * 使用 yaml-cpp 库逐节解析 YAML 配置文件，填充 Config 全局结构体。
 * 采用模块化解析设计，每个业务模块独立解析，代码解耦易维护。
 * 当 YAML_CPP_AVAILABLE 宏未定义时，禁用YAML解析逻辑，
 * load() 直接返回false，所有业务配置项保持结构体默认初始值，实现无依赖兼容。
 * 
 * @design 核心设计思想
 * 1. 单例模式：全局唯一配置加载器，保证全局配置统一
 * 2. 模块化解析：各业务配置独立parse函数，新增配置无需改动主加载逻辑
 * 3. 容错降级：文件不存在、格式错误、库未安装均不崩溃，兜底默认配置
 * 4. 按需赋值：YAML缺失字段不覆盖，保留结构体默认值
 * 5. 宏隔离编译：实现带库/无库双模式编译，适配不同编译环境
 */

#include "config_loader.hpp"

#include <fstream>    // 文件流读写，用于加载本地YAML配置文件
#include <iostream>   // 标准日志输出，打印配置加载状态、错误信息

// 条件编译：仅定义YAML_CPP_AVAILABLE宏时，引入yaml-cpp库头文件
// 未定义该宏时，完全剥离yaml-cpp依赖，适配极简编译环境
#ifdef YAML_CPP_AVAILABLE
#include <yaml-cpp/yaml.h>
#endif

// 项目命名空间，隔离全局作用域，避免全局命名冲突
namespace gonxun {

/**
 * @brief 获取配置加载器全局单例
 * @return ConfigLoader& 全局唯一加载器实例引用
 * @note 局部静态变量单例：线程安全(C++11及以上)、懒加载、自动析构
 * @design 全局唯一配置入口，项目任意位置通过ConfigLoader::instance()访问加载器
 */
ConfigLoader& ConfigLoader::instance() {
    // 局部静态变量：首次调用时初始化，程序生命周期内唯一
    static ConfigLoader inst;
    return inst; 
}

/**
 * @brief 从 YAML 文件加载并解析全量业务配置
 * @param path YAML配置文件的绝对/相对路径
 * @return true 配置文件存在且解析完全成功
 * @return false 库未启用/文件异常/解析失败，使用默认配置
 * 
 * @details 执行流程：
 * 1. 检测YAML库是否可用，不可用直接降级默认配置
 * 2. 加载本地YAML文件为内存节点树
 * 3. 遍历所有业务模块，按需解析存在的配置节点
 * 4. 任意解析异常均捕获，不崩溃程序，自动兜底默认配置
 * 
 * @feature 容错特性：
 * - 配置文件缺失：捕获异常，使用默认配置
 * - 配置字段缺失：跳过解析，保留默认值
 * - 配置格式错误：捕获异常，使用默认配置
 * - 字段类型不匹配：抛出异常，兜底默认配置
 */
bool ConfigLoader::load(const std::string& path) {
    // 条件编译：启用yaml-cpp库时执行解析逻辑
#ifdef YAML_CPP_AVAILABLE
    try {
        // 加载YAML文件至内存，生成层级节点树
        // 自动校验文件合法性、格式规范性
        YAML::Node root = YAML::LoadFile(path);

        // 模块化按需解析：仅解析配置文件中存在的节点，缺失节点直接跳过
        // 遵循【有则覆盖，无则默认】原则，保证配置兼容性
        if (root["logging"])         parse_logging(root["logging"]);
        if (root["serial"])          parse_serial(root["serial"]);
        if (root["motion"])          parse_motion(root["motion"]);
        if (root["camera"])          parse_camera(root["camera"]);
        if (root["color_detection"]) parse_color_detection(root["color_detection"]);
        if (root["kalman_filter"])   parse_kalman_filter(root["kalman_filter"]);
        if (root["field"])           parse_field(root["field"]);
        if (root["yolo"])            parse_yolo(root["yolo"]);
        if (root["system"])          parse_system(root["system"]);

        // 标记配置已加载完成
        loaded_ = true;
        std::cout << "[Config] 配置加载成功: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        // 捕获所有解析、文件、格式异常，统一容错处理
        std::cerr << "[Config] 配置加载失败: " << e.what() << std::endl;
        std::cerr << "[Config] 使用默认配置" << std::endl;
        return false;
    }
#else
    // 未启用yaml-cpp库，直接降级默认配置，无文件解析动作
    std::cout << "[Config] yaml-cpp 未安装，使用默认配置" << std::endl;
    return false;
#endif
}

// 仅启用yaml-cpp库时，编译所有解析成员函数
#ifdef YAML_CPP_AVAILABLE

/**
 * @brief 解析日志模块配置
 * @param node YAML中logging节点
 * @field level 日志输出等级
 * @field format 日志输出格式
 */
void ConfigLoader::parse_logging(const YAML::Node& node) {
    // 字段存在则覆盖配置，不存在保留默认值
    if (node["level"]) config_.logging.level = node["level"].as<std::string>();
    if (node["format"]) config_.logging.format = node["format"].as<std::string>();
}

/**
 * @brief 解析串口通信模块配置
 * @param node YAML中serial节点
 * @field port 串口设备路径/名称
 * @field baudrate 串口波特率
 * @field timeout 串口读写超时时间
 * @field mock 串口模拟开关（虚拟串口调试）
 * @field mock_cycle 模拟数据循环输出开关
 */
void ConfigLoader::parse_serial(const YAML::Node& node) {
    if (node["port"]) config_.serial.port = node["port"].as<std::string>();
    if (node["baudrate"]) config_.serial.baudrate = node["baudrate"].as<int>();
    if (node["timeout"]) config_.serial.timeout = node["timeout"].as<double>();
    if (node["mock"]) config_.serial.mock = node["mock"].as<bool>();
    if (node["mock_cycle"]) config_.serial.mock_cycle = node["mock_cycle"].as<bool>();
}

/**
 * @brief 解析运动控制模块配置
 * @param node YAML中motion节点
 * @brief 机器人/设备运动参数配置，控制移动速度、加速度、超时策略
 * @field default_speed 默认运动速度
 * @field default_accel 默认运动加速度
 * @field max_retries 指令最大重试次数
 * @field command_timeout 运动指令超时时间
 * @field steps_per_grid 单网格对应步进数
 * @field grid_size_mm 单网格物理尺寸(毫米)
 */
void ConfigLoader::parse_motion(const YAML::Node& node) {
    if (node["default_speed"]) config_.motion.default_speed = node["default_speed"].as<int>();
    if (node["default_accel"]) config_.motion.default_accel = node["default_accel"].as<int>();
    if (node["max_retries"]) config_.motion.max_retries = node["max_retries"].as<int>();
    if (node["command_timeout"]) config_.motion.command_timeout = node["command_timeout"].as<int>();
    if (node["steps_per_grid"]) config_.motion.steps_per_grid = node["steps_per_grid"].as<int>();
    if (node["grid_size_mm"]) config_.motion.grid_size_mm = node["grid_size_mm"].as<int>();
}

/**
 * @brief 解析相机模块配置
 * @param node YAML中camera节点
 * @brief 支持双相机配置：主相机+QR识别相机，每个相机独立参数
 * @subnode main 主视觉相机配置（画面采集、视觉检测）
 * @subnode qr QR码识别专用相机配置
 * @field index 相机设备索引号
 * @field width 图像采集宽度
 * @field height 图像采集高度
 * @field buffer_size 缓冲区大小
 * @field auto_exposure 自动曝光
 * @field exposure 曝光值
 * @field gain 增益值
 */
void ConfigLoader::parse_camera(const YAML::Node& node) {
    // 解析主相机配置
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
    // 解析QR相机配置
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

/**
 * @brief 解析颜色检测模块配置
 * @param node YAML中color_detection节点
 * @field min_area 有效色块最小面积（过滤噪点）
 * @field dock_min_area 对接区域色块最小面积（精准对接判定）
 */
void ConfigLoader::parse_color_detection(const YAML::Node& node) {
    if (node["min_area"]) config_.color_detection.min_area = node["min_area"].as<int>();
    if (node["dock_min_area"]) config_.color_detection.dock_min_area = node["dock_min_area"].as<int>();
}

/**
 * @brief 解析卡尔曼滤波算法配置
 * @param node YAML中kalman_filter节点
 * @brief 用于传感器数据降噪、姿态位置平滑
 * @field Q 过程噪声协方差（系统噪声）
 * @field R 观测噪声协方差（传感器噪声）
 */
void ConfigLoader::parse_kalman_filter(const YAML::Node& node) {
    if (node["Q"]) config_.kalman_filter.Q = node["Q"].as<double>();
    if (node["R"]) config_.kalman_filter.R = node["R"].as<double>();
}

/**
 * @brief 解析场地环境配置
 * @param node YAML中field节点
 * @brief 机器人运行场地参数，包含场地尺寸、像素比例尺、车道参数
 * @subnode lane 车道细分配置
 * @field size 场地整体尺寸
 * @field pixel_per_mm 每毫米对应像素（视觉测距换算）
 * @field lane.width 车道宽度
 * @field lane.center 车道中心偏移量
 * @field lane.start 车道起始坐标
 * @field lane.end 车道终止坐标
 */
void ConfigLoader::parse_field(const YAML::Node& node) {
    if (node["size"]) config_.field.size = node["size"].as<int>();
    if (node["pixel_per_mm"]) config_.field.pixel_per_mm = node["pixel_per_mm"].as<double>();
    // 解析嵌套车道配置
    if (node["lane"]) {
        auto& lane = node["lane"];
        if (lane["width"]) config_.field.lane.width = lane["width"].as<int>();
        if (lane["center"]) config_.field.lane.center = lane["center"].as<int>();
        if (lane["start"]) config_.field.lane.start = lane["start"].as<int>();
        if (lane["end"]) config_.field.lane.end = lane["end"].as<int>();
    }
}

/**
 * @brief 解析YOLO目标检测模型配置
 * @param node YAML中yolo节点
 * @brief 深度学习推理参数配置，控制模型加载与推理精度
 * @field model_path YOLO原始模型文件路径
 * @field torchscript_path 推理模型脚本路径
 * @field conf_threshold 置信度阈值（过滤低置信度目标）
 * @field iou_threshold 交并比阈值（非极大值抑制）
 * @field imgsz 推理输入图像尺寸
 * @field half 是否开启半精度推理（加速、降显存）
 */
void ConfigLoader::parse_yolo(const YAML::Node& node) {
    if (node["model_path"]) config_.yolo.model_path = node["model_path"].as<std::string>();
    if (node["torchscript_path"]) config_.yolo.torchscript_path = node["torchscript_path"].as<std::string>();
    if (node["conf_threshold"]) config_.yolo.conf_threshold = node["conf_threshold"].as<double>();
    if (node["iou_threshold"]) config_.yolo.iou_threshold = node["iou_threshold"].as<double>();
    if (node["imgsz"]) config_.yolo.imgsz = node["imgsz"].as<int>();
    if (node["half"]) config_.yolo.half = node["half"].as<bool>();
}

/**
 * @brief 解析系统基础信息配置
 * @param node YAML中system节点
 * @brief 项目版本、名称、描述等基础标识信息
 * @field version 程序版本号
 * @field name 项目名称
 * @field description 项目功能描述
 */
void ConfigLoader::parse_system(const YAML::Node& node) {
    if (node["version"]) config_.system.version = node["version"].as<std::string>();
    if (node["name"]) config_.system.name = node["name"].as<std::string>();
    if (node["description"]) config_.system.description = node["description"].as<std::string>();
}

#endif // YAML_CPP_AVAILABLE

} // namespace gonxun
