/**
 * @file yolo_detector.hpp
 * @brief YOLOv8 TensorRT 检测模块
 *
 * 基于 TensorRT 加速的 YOLOv8 目标检测器，用于物料颜色分类。
 * 编译时需定义 TENSORRT_AVAILABLE 宏以启用 TensorRT 后端；
 * 未定义时所有检测方法返回空结果，available() 为 false。
 * 支持 6 类物料: red/blue/green/yellow/black/light_blue
 */
#pragma once

#include <climits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <opencv2/opencv.hpp>

#ifdef TENSORRT_AVAILABLE
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime_api.h>
#endif

/**
 * @brief 单个检测结果
 */
struct Detection {
    cv::Rect bbox;          ///< 边界框 (x, y, width, height)
    float confidence;       ///< 置信度 [0, 1]
    int class_id;           ///< 类别 ID (0-5)
    std::string class_name; ///< 类别名称，如 "red_block"

    /** @brief 计算边界框中心点 */
    [[nodiscard]] cv::Point center() const noexcept {
        return {bbox.x + bbox.width / 2, bbox.y + bbox.height / 2};
    }

    /** @brief 计算边界框尺寸 */
    [[nodiscard]] cv::Size size() const noexcept {
        return {bbox.width, bbox.height};
    }
};

#ifdef TENSORRT_AVAILABLE
/**
 * @brief TensorRT 日志回调，仅输出 WARNING 及以上级别
 */
class TensorRTLogger : public nvinfer1::ILogger {
public:
    void log(nvinfer1::ILogger::Severity severity, const char* msg) noexcept override;
};
#endif

/**
 * @brief YOLOv8 TensorRT 检测器
 *
 * 加载 TensorRT Engine 文件并执行推理。支持批量检测和
 * 按颜色名查找最大目标中心的兼容接口。
 * @note 依赖 TensorRT 库，未安装时 is_available() 返回 false
 */
class YOLOv8Detector {
public:
    /**
     * @brief 构造函数，加载 TensorRT Engine
     * @param engine_path Engine 文件路径；为空时从 TorchScript 路径推导 .engine 路径
     * @param imgsz 推理输入图像尺寸 (px)
     * @param conf_threshold 置信度阈值，低于此值的检测框被过滤
     */
    explicit YOLOv8Detector(const std::string& engine_path = "",
                            int imgsz = 320,
                            float conf_threshold = 0.5f);
    ~YOLOv8Detector();

    /**
     * @brief 对图像执行目标检测
     * @param img 输入 BGR 图像
     * @return 检测结果列表；不可用时返回空
     */
    [[nodiscard]] std::vector<Detection> detect(const cv::Mat& img);

    /**
     * @brief 兼容 ColorDetector 接口，返回指定颜色目标的中心点
     * @param img 输入 BGR 图像
     * @param color 颜色名: red/blue/green/yellow/black/light_blue
     * @param min_area 最小面积阈值 (px²)
     * @param max_area 最大面积阈值 (px²)
     * @return 面积最大的目标中心点；无目标返回 std::nullopt
     */
    [[nodiscard]] std::optional<cv::Point> detect_center(const cv::Mat& img,
                                                         const std::string& color,
                                                         int min_area = 0,
                                                         int max_area = INT_MAX);

    /** @brief 用全黑图像预热模型，消除首次推理延迟 */
    void warmup();

    /** @brief TensorRT Engine 是否可用 */
    [[nodiscard]] bool is_available() const noexcept { return available_; }

private:
#ifdef TENSORRT_AVAILABLE
    /**
     * @brief 预处理：resize + BGR→RGB + 归一化到 [0,1]，写入 NCHW 输入缓冲区
     * @param img 原始图像
     * @param input_buffer 主机端输入缓冲区
     */
    void preprocess(const cv::Mat& img, float* input_buffer);
    /**
     * @brief 后处理：解析输出缓冲区，过滤低置信度框，缩放回原图坐标
     * @param output_buffer 主机端输出缓冲区
     * @param orig_width 原图宽度
     * @param orig_height 原图高度
     * @return 检测结果列表
     */
    [[nodiscard]] std::vector<Detection> postprocess(const float* output_buffer,
                                                     int orig_width, int orig_height);
    /** @brief 分配 CUDA 设备端和主机端输入输出缓冲区 */
    void allocate_buffers();
    /** @brief 释放 CUDA 和主机端缓冲区 */
    void free_buffers();
#endif

    bool available_ = false;     ///< Engine 是否成功加载
    int imgsz_ = 320;            ///< 推理图像尺寸
    float conf_threshold_ = 0.5f;///< 置信度阈值
    std::string engine_path_;    ///< Engine 文件路径

#ifdef TENSORRT_AVAILABLE
    TensorRTLogger logger_;                        ///< TensorRT 日志回调
    nvinfer1::IRuntime* runtime_ = nullptr;        ///< TensorRT Runtime
    nvinfer1::ICudaEngine* engine_ = nullptr;      ///< TensorRT Engine
    nvinfer1::IExecutionContext* context_ = nullptr;///< TensorRT 推理上下文

    void* input_buffer_ = nullptr;   ///< CUDA 设备端输入缓冲区
    void* output_buffer_ = nullptr;  ///< CUDA 设备端输出缓冲区
    float* input_host_ = nullptr;    ///< 主机端输入缓冲区
    float* output_host_ = nullptr;   ///< 主机端输出缓冲区

    std::string input_name_;   ///< 输入 binding 名称
    std::string output_name_;  ///< 输出 binding 名称
    int input_size_ = 0;       ///< 输入元素数
    int output_size_ = 0;      ///< 输出元素数
#endif

    ///< 类别 ID → 类别名映射
    std::unordered_map<int, std::string> class_names_ = {
        {0, "red_block"}, {1, "blue_block"}, {2, "green_block"},
        {3, "yellow_block"}, {4, "black_block"}, {5, "light_blue_block"}
    };

    ///< 颜色简称 → 类别名映射，用于 detect_center 接口
    std::unordered_map<std::string, std::string> color_map_ = {
        {"red", "red_block"}, {"blue", "blue_block"}, {"green", "green_block"},
        {"yellow", "yellow_block"}, {"black", "black_block"}, {"light_blue", "light_blue_block"}
    };
};
