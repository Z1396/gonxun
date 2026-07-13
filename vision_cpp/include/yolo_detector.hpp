/**
 * YOLOv8 TensorRT 检测模块 (C++ 版本)
 * 对应 Python: vision/yolo_tensorrt_detector.py
 *
 * 工作流程:
 * 1. ONNX → TensorRT Engine (在 Jetson 上转换一次)
 * 2. 使用 TensorRT C++ API 执行推理
 *
 * 使用前需要:
 * 1. 导出 ONNX: from ultralytics import YOLO; YOLO('best.pt').export(format='onnx')
 * 2. 转换 Engine: trtexec --onnx=best.onnx --saveEngine=best.engine --fp16
 */
#pragma once

#include <opencv2/opencv.hpp>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <climits>

#ifdef TENSORRT_AVAILABLE
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime_api.h>
#endif

/** 单个检测结果 */
struct Detection {
    cv::Rect bbox;           // 边界框
    float confidence;        // 置信度
    int classId;             // 类别ID
    std::string className;   // 类别名

    cv::Point center() const {
        return {bbox.x + bbox.width / 2, bbox.y + bbox.height / 2};
    }

    cv::Size size() const {
        return {bbox.width, bbox.height};
    }
};

#ifdef TENSORRT_AVAILABLE
/**
 * TensorRT Logger 类
 */
class TensorRTLogger : public nvinfer1::ILogger {
public:
    void log(nvinfer1::ILogger::Severity severity, const char* msg) noexcept override;
};
#endif

/**
 * YOLOv8 TensorRT 检测器
 */
class YOLOv8Detector {
public:
    /**
     * @param enginePath TensorRT Engine 文件路径 (.engine)
     * @param imgsz 推理图像尺寸
     * @param confThreshold 置信度阈值
     */
    explicit YOLOv8Detector(const std::string& enginePath = "",
                            int imgsz = 320,
                            float confThreshold = 0.5f);
    ~YOLOv8Detector();

    /** 对单帧图像进行目标检测 */
    std::vector<Detection> detect(const cv::Mat& img);

    /**
     * 兼容 ColorDetector 接口，返回指定颜色目标的中心点
     * @param color 颜色名: "red"/"blue"/"green"/"yellow"/"black"/"light_blue"
     * @return 中心点坐标; 无目标返回 std::nullopt
     */
    std::optional<cv::Point> detectCenter(const cv::Mat& img,
                                          const std::string& color,
                                          int minArea = 0,
                                          int maxArea = INT_MAX);

    /** 模型预热，避免首帧延迟 */
    void warmup();

    /** 模型是否可用 */
    bool isAvailable() const { return m_available; }

private:
#ifdef TENSORRT_AVAILABLE
    /** 预处理图像 */
    void preprocess(const cv::Mat& img, float* inputBuffer);

    /** 后处理输出 */
    std::vector<Detection> postprocess(const float* outputBuffer,
                                       int origWidth, int origHeight);

    /** 分配 CUDA 内存 */
    void allocateBuffers();

    /** 释放 CUDA 内存 */
    void freeBuffers();
#endif

    bool m_available = false;
    int m_imgsz = 320;
    float m_confThreshold = 0.5f;
    std::string m_enginePath;

#ifdef TENSORRT_AVAILABLE
    // TensorRT 组件
    TensorRTLogger m_logger;
    nvinfer1::IRuntime* m_runtime = nullptr;
    nvinfer1::ICudaEngine* m_engine = nullptr;
    nvinfer1::IExecutionContext* m_context = nullptr;

    // CUDA 缓冲区
    void* m_inputBuffer = nullptr;    // GPU 输入
    void* m_outputBuffer = nullptr;   // GPU 输出
    float* m_inputHost = nullptr;     // CPU 输入
    float* m_outputHost = nullptr;    // CPU 输出

    // 输入输出信息
    std::string m_inputName;
    std::string m_outputName;
    int m_inputSize = 0;
    int m_outputSize = 0;
#endif

    // 类别名称映射
    std::unordered_map<int, std::string> m_classNames = {
        {0, "red_block"}, {1, "blue_block"}, {2, "green_block"},
        {3, "yellow_block"}, {4, "black_block"}, {5, "light_blue_block"}
    };

    // 颜色名 -> 类别名 映射
    std::unordered_map<std::string, std::string> m_colorMap = {
        {"red", "red_block"}, {"blue", "blue_block"}, {"green", "green_block"},
        {"yellow", "yellow_block"}, {"black", "black_block"}, {"light_blue", "light_blue_block"}
    };
};