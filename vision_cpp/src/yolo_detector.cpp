/**
 * @file yolo_detector.cpp
 * @brief YOLOv8 TensorRT 检测模块实现
 *
 * 实现 TensorRT Engine 加载、CUDA 推理和后处理。
 * 编译时需定义 TENSORRT_AVAILABLE 宏；未定义时
 * 所有方法返回空结果，is_available() 为 false。
 */
#include "yolo_detector.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>

#ifdef TENSORRT_AVAILABLE
/**
 * @brief TensorRT 日志回调，仅输出 WARNING 及以上级别
 * @param severity 日志级别
 * @param msg 日志消息
 */
void TensorRTLogger::log(nvinfer1::ILogger::Severity severity, const char* msg) noexcept {
    if (severity <= nvinfer1::ILogger::Severity::kWARNING) {
        std::cout << "[TensorRT] " << msg << std::endl;
    }
}
#endif

/**
 * @brief 构造函数，加载 TensorRT Engine 文件
 * @note 加载流程: 读文件 → createInferRuntime → deserializeCudaEngine
 *       → createExecutionContext → 解析 binding → 分配缓冲区
 *       任一步骤失败则 available_ 保持 false
 */
YOLOv8Detector::YOLOv8Detector(const std::string& engine_path,
                               int imgsz,
                               float conf_threshold)
    : imgsz_(imgsz), conf_threshold_(conf_threshold) {

#ifdef TENSORRT_AVAILABLE
    // 推导 Engine 路径: 若为空，从 TorchScript 路径替换扩展名
    std::string path = engine_path;
    if (path.empty()) {
        path = config::YOLO_TS_MODEL_PATH;
        size_t pos = path.rfind('.');
        if (pos != std::string::npos) {
            path = path.substr(0, pos) + ".engine";
        }
    }
    engine_path_ = path;

    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "无法打开 TensorRT Engine: " << path << std::endl;
            std::cerr << "请先转换模型: trtexec --onnx=best.onnx --saveEngine=best.engine --fp16" << std::endl;
            return;
        }

        // 读取整个 Engine 文件到内存
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> engine_data(size);
        file.read(engine_data.data(), size);
        file.close();

        // 创建 Runtime 并反序列化 Engine
        runtime_ = nvinfer1::createInferRuntime(logger_);
        if (!runtime_) {
            std::cerr << "创建 TensorRT Runtime 失败" << std::endl;
            return;
        }

        engine_ = runtime_->deserializeCudaEngine(engine_data.data(), size);
        if (!engine_) {
            std::cerr << "反序列化 TensorRT Engine 失败" << std::endl;
            return;
        }

        context_ = engine_->createExecutionContext();
        if (!context_) {
            std::cerr << "创建 TensorRT Context 失败" << std::endl;
            return;
        }

        // 解析输入/输出 binding 名称和尺寸
        for (int i = 0; i < engine_->getNbBindings(); ++i) {
            const char* name = engine_->getBindingName(i);
            nvinfer1::Dims dims = engine_->getBindingDimensions(i);

            size_t vol = 1;
            for (int j = 0; j < dims.nbDims; ++j) {
                vol *= dims.d[j];
            }

            if (engine_->bindingIsInput(i)) {
                input_name_ = name;
                input_size_ = static_cast<int>(vol);
                std::cout << "输入: " << name << ", 大小: " << vol << std::endl;
            } else {
                output_name_ = name;
                output_size_ = static_cast<int>(vol);
                std::cout << "输出: " << name << ", 大小: " << vol << std::endl;
            }
        }

        allocate_buffers();

        available_ = true;
        std::cout << "TensorRT Engine 加载成功: " << path << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "TensorRT 初始化异常: " << e.what() << std::endl;
    }
#else
    available_ = false;
    std::cerr << "[警告] TensorRT 未安装，YOLO 检测功能不可用" << std::endl;
    std::cerr << "请在 Jetson Nano 上编译此程序以启用 TensorRT" << std::endl;
#endif
}

/** @brief 析构，释放 TensorRT 资源和 CUDA 缓冲区 */
YOLOv8Detector::~YOLOv8Detector() {
#ifdef TENSORRT_AVAILABLE
    free_buffers();

    if (context_) {
        context_->destroy();
        context_ = nullptr;
    }
    if (engine_) {
        engine_->destroy();
        engine_ = nullptr;
    }
    if (runtime_) {
        runtime_->destroy();
        runtime_ = nullptr;
    }
#endif
}

#ifdef TENSORRT_AVAILABLE
/** @brief 分配 CUDA 设备端和主机端缓冲区 */
void YOLOv8Detector::allocate_buffers() {
    cudaMalloc(&input_buffer_, input_size_ * sizeof(float));
    cudaMalloc(&output_buffer_, output_size_ * sizeof(float));

    input_host_ = new float[input_size_];
    output_host_ = new float[output_size_];
}

/** @brief 释放 CUDA 和主机端缓冲区 */
void YOLOv8Detector::free_buffers() {
    if (input_buffer_) { cudaFree(input_buffer_); input_buffer_ = nullptr; }
    if (output_buffer_) { cudaFree(output_buffer_); output_buffer_ = nullptr; }

    delete[] input_host_; input_host_ = nullptr;
    delete[] output_host_; output_host_ = nullptr;
}

/**
 * @brief 预处理: resize → BGR→RGB → 归一化 [0,1]，写入 NCHW 缓冲区
 * @param img 原始 BGR 图像
 * @param input_buffer 主机端输入缓冲区 (NCHW 排列)
 */
void YOLOv8Detector::preprocess(const cv::Mat& img, float* input_buffer) {
    cv::Mat resized;
    cv::resize(img, resized, cv::Size(imgsz_, imgsz_), 0, 0, cv::INTER_LINEAR);

    // BGR→RGB, 归一化到 [0,1], NCHW 排列
    for (int c = 0; c < 3; ++c) {
        for (int h = 0; h < imgsz_; ++h) {
            for (int w = 0; w < imgsz_; ++w) {
                float val = static_cast<float>(resized.at<cv::Vec3b>(h, w)[2 - c]) / 255.0f;
                input_buffer[c * imgsz_ * imgsz_ + h * imgsz_ + w] = val;
            }
        }
    }
}

/**
 * @brief 后处理: 解析输出缓冲区，过滤低置信度框，缩放回原图坐标
 * @param output_buffer 主机端输出缓冲区，每个检测框6个浮点 (cx,cy,w,h,conf,class_id)
 * @param orig_width 原图宽度
 * @param orig_height 原图高度
 * @return 检测结果列表
 */
std::vector<Detection> YOLOv8Detector::postprocess(const float* output_buffer,
                                                    int orig_width, int orig_height) {
    std::vector<Detection> detections;

    int num_boxes = output_size_ / 6;

    float scale_x = static_cast<float>(orig_width) / imgsz_;
    float scale_y = static_cast<float>(orig_height) / imgsz_;

    for (int i = 0; i < num_boxes; ++i) {
        float x = output_buffer[i * 6 + 0];
        float y = output_buffer[i * 6 + 1];
        float w = output_buffer[i * 6 + 2];
        float h = output_buffer[i * 6 + 3];
        float conf = output_buffer[i * 6 + 4];
        int class_id = static_cast<int>(output_buffer[i * 6 + 5]);

        if (conf < conf_threshold_) continue;

        // 中心坐标转左上角，缩放回原图尺寸
        int x1 = static_cast<int>((x - w / 2) * scale_x);
        int y1 = static_cast<int>((y - h / 2) * scale_y);
        int width = static_cast<int>(w * scale_x);
        int height = static_cast<int>(h * scale_y);

        Detection det;
        det.bbox = cv::Rect(x1, y1, width, height);
        det.confidence = conf;
        det.class_id = class_id;

        auto it = class_names_.find(class_id);
        det.class_name = (it != class_names_.end()) ? it->second : std::to_string(class_id);

        detections.push_back(det);
    }

    return detections;
}
#endif

/**
 * @brief 对图像执行目标检测
 * @param img 输入 BGR 图像
 * @return 检测结果列表；不可用或图像为空返回空
 * @note 推理流程: preprocess → H2D → executeV2 → D2H → postprocess
 */
std::vector<Detection> YOLOv8Detector::detect(const cv::Mat& img) {
    if (!available_ || img.empty()) return {};

#ifdef TENSORRT_AVAILABLE
    preprocess(img, input_host_);

    // 主机→设备
    cudaMemcpy(input_buffer_, input_host_, input_size_ * sizeof(float), cudaMemcpyHostToDevice);

    // 执行推理
    void* bindings[2] = {input_buffer_, output_buffer_};
    context_->executeV2(bindings);

    // 设备→主机
    cudaMemcpy(output_host_, output_buffer_, output_size_ * sizeof(float), cudaMemcpyDeviceToHost);

    return postprocess(output_host_, img.cols, img.rows);
#else
    return {};
#endif
}

/**
 * @brief 兼容接口: 返回指定颜色面积最大目标的中心点
 * @param img 输入 BGR 图像
 * @param color 颜色简称
 * @param min_area 最小面积阈值
 * @param max_area 最大面积阈值
 * @return 目标中心点；无匹配返回 std::nullopt
 */
std::optional<cv::Point> YOLOv8Detector::detect_center(const cv::Mat& img,
                                                       const std::string& color,
                                                       int min_area,
                                                       int max_area) {
    if (!available_ || img.empty()) return std::nullopt;

#ifdef TENSORRT_AVAILABLE
    auto it = color_map_.find(color);
    if (it == color_map_.end()) return std::nullopt;

    std::string target_class = it->second;
    auto detections = detect(img);

    // 在面积范围内找面积最大的匹配目标
    const Detection* best_det = nullptr;
    int best_area = 0;

    for (const auto& det : detections) {
        if (det.class_name != target_class) continue;

        int area = det.size().area();

        if (area >= min_area && area <= max_area && area > best_area) {
            best_area = area;
            best_det = &det;
        }
    }

    if (best_det == nullptr) return std::nullopt;
    return best_det->center();
#else
    return std::nullopt;
#endif
}

/**
 * @brief 用全黑图像预热模型，消除首次推理的 CUDA 初始化延迟
 */
void YOLOv8Detector::warmup() {
#ifdef TENSORRT_AVAILABLE
    if (!available_) return;

    cv::Mat dummy(imgsz_, imgsz_, CV_8UC3, cv::Scalar(0, 0, 0));
    detect(dummy);

    std::cout << "TensorRT 模型预热完成" << std::endl;
#endif
}
