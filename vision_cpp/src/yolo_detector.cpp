/**
 * YOLOv8 TensorRT 检测模块实现
 * 对应 Python: vision/yolo_tensorrt_detector.py
 */
#include "yolo_detector.hpp"
#include "config.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>

#ifdef TENSORRT_AVAILABLE
// TensorRT Logger 实现
void TensorRTLogger::log(nvinfer1::ILogger::Severity severity, const char* msg) noexcept {
    if (severity <= nvinfer1::ILogger::Severity::kWARNING) {
        std::cout << "[TensorRT] " << msg << std::endl;
    }
}
#endif

YOLOv8Detector::YOLOv8Detector(const std::string& enginePath,
                               int imgsz,
                               float confThreshold)
    : m_imgsz(imgsz), m_confThreshold(confThreshold) {

#ifdef TENSORRT_AVAILABLE
    // 确定模型路径
    std::string path = enginePath;
    if (path.empty()) {
        path = config::YOLO_TS_MODEL_PATH;
        size_t pos = path.rfind('.');
        if (pos != std::string::npos) {
            path = path.substr(0, pos) + ".engine";
        }
    }
    m_enginePath = path;

    try {
        // 读取 engine 文件
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "无法打开 TensorRT Engine: " << path << std::endl;
            std::cerr << "请先转换模型: trtexec --onnx=best.onnx --saveEngine=best.engine --fp16" << std::endl;
            return;
        }

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> engineData(size);
        file.read(engineData.data(), size);
        file.close();

        // 创建 runtime 和 engine
        m_runtime = nvinfer1::createInferRuntime(m_logger);
        if (!m_runtime) {
            std::cerr << "创建 TensorRT Runtime 失败" << std::endl;
            return;
        }

        m_engine = m_runtime->deserializeCudaEngine(engineData.data(), size);
        if (!m_engine) {
            std::cerr << "反序列化 TensorRT Engine 失败" << std::endl;
            return;
        }

        m_context = m_engine->createExecutionContext();
        if (!m_context) {
            std::cerr << "创建 TensorRT Context 失败" << std::endl;
            return;
        }

        // 获取输入输出信息
        for (int i = 0; i < m_engine->getNbBindings(); ++i) {
            const char* name = m_engine->getBindingName(i);
            nvinfer1::Dims dims = m_engine->getBindingDimensions(i);
            size_t vol = 1;
            for (int j = 0; j < dims.nbDims; ++j) {
                vol *= dims.d[j];
            }

            if (m_engine->bindingIsInput(i)) {
                m_inputName = name;
                m_inputSize = static_cast<int>(vol);
                std::cout << "输入: " << name << ", 大小: " << vol << std::endl;
            } else {
                m_outputName = name;
                m_outputSize = static_cast<int>(vol);
                std::cout << "输出: " << name << ", 大小: " << vol << std::endl;
            }
        }

        allocateBuffers();
        m_available = true;
        std::cout << "TensorRT Engine 加载成功: " << path << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "TensorRT 初始化异常: " << e.what() << std::endl;
    }
#else
    m_available = false;
    std::cerr << "[警告] TensorRT 未安装，YOLO 检测功能不可用" << std::endl;
    std::cerr << "请在 Jetson Nano 上编译此程序以启用 TensorRT" << std::endl;
#endif
}

YOLOv8Detector::~YOLOv8Detector() {
#ifdef TENSORRT_AVAILABLE
    freeBuffers();

    if (m_context) {
        m_context->destroy();
        m_context = nullptr;
    }
    if (m_engine) {
        m_engine->destroy();
        m_engine = nullptr;
    }
    if (m_runtime) {
        m_runtime->destroy();
        m_runtime = nullptr;
    }
#endif
}

#ifdef TENSORRT_AVAILABLE
void YOLOv8Detector::allocateBuffers() {
    cudaMalloc(&m_inputBuffer, m_inputSize * sizeof(float));
    cudaMalloc(&m_outputBuffer, m_outputSize * sizeof(float));
    m_inputHost = new float[m_inputSize];
    m_outputHost = new float[m_outputSize];
}

void YOLOv8Detector::freeBuffers() {
    if (m_inputBuffer) { cudaFree(m_inputBuffer); m_inputBuffer = nullptr; }
    if (m_outputBuffer) { cudaFree(m_outputBuffer); m_outputBuffer = nullptr; }
    delete[] m_inputHost; m_inputHost = nullptr;
    delete[] m_outputHost; m_outputHost = nullptr;
}

void YOLOv8Detector::preprocess(const cv::Mat& img, float* inputBuffer) {
    cv::Mat resized;
    cv::resize(img, resized, cv::Size(m_imgsz, m_imgsz), 0, 0, cv::INTER_LINEAR);

    for (int c = 0; c < 3; ++c) {
        for (int h = 0; h < m_imgsz; ++h) {
            for (int w = 0; w < m_imgsz; ++w) {
                float val = static_cast<float>(resized.at<cv::Vec3b>(h, w)[2 - c]) / 255.0f;
                inputBuffer[c * m_imgsz * m_imgsz + h * m_imgsz + w] = val;
            }
        }
    }
}

std::vector<Detection> YOLOv8Detector::postprocess(const float* outputBuffer,
                                                    int origWidth, int origHeight) {
    std::vector<Detection> detections;
    int numBoxes = m_outputSize / 6;
    float scaleX = static_cast<float>(origWidth) / m_imgsz;
    float scaleY = static_cast<float>(origHeight) / m_imgsz;

    for (int i = 0; i < numBoxes; ++i) {
        float x = outputBuffer[i * 6 + 0];
        float y = outputBuffer[i * 6 + 1];
        float w = outputBuffer[i * 6 + 2];
        float h = outputBuffer[i * 6 + 3];
        float conf = outputBuffer[i * 6 + 4];
        int classId = static_cast<int>(outputBuffer[i * 6 + 5]);

        if (conf < m_confThreshold) continue;

        int x1 = static_cast<int>((x - w / 2) * scaleX);
        int y1 = static_cast<int>((y - h / 2) * scaleY);
        int width = static_cast<int>(w * scaleX);
        int height = static_cast<int>(h * scaleY);

        Detection det;
        det.bbox = cv::Rect(x1, y1, width, height);
        det.confidence = conf;
        det.classId = classId;
        auto it = m_classNames.find(classId);
        det.className = (it != m_classNames.end()) ? it->second : std::to_string(classId);
        detections.push_back(det);
    }
    return detections;
}
#endif

std::vector<Detection> YOLOv8Detector::detect(const cv::Mat& img) {
    if (!m_available || img.empty()) return {};

#ifdef TENSORRT_AVAILABLE
    preprocess(img, m_inputHost);
    cudaMemcpy(m_inputBuffer, m_inputHost, m_inputSize * sizeof(float), cudaMemcpyHostToDevice);
    void* bindings[2] = {m_inputBuffer, m_outputBuffer};
    m_context->executeV2(bindings);
    cudaMemcpy(m_outputHost, m_outputBuffer, m_outputSize * sizeof(float), cudaMemcpyDeviceToHost);
    return postprocess(m_outputHost, img.cols, img.rows);
#else
    return {};
#endif
}

std::optional<cv::Point> YOLOv8Detector::detectCenter(const cv::Mat& img,
                                                       const std::string& color,
                                                       int minArea,
                                                       int maxArea) {
    if (!m_available || img.empty()) return std::nullopt;

#ifdef TENSORRT_AVAILABLE
    auto it = m_colorMap.find(color);
    if (it == m_colorMap.end()) return std::nullopt;

    std::string targetClass = it->second;
    auto detections = detect(img);

    const Detection* bestDet = nullptr;
    int bestArea = 0;
    for (const auto& det : detections) {
        if (det.className != targetClass) continue;
        int area = det.size().area();
        if (area >= minArea && area <= maxArea && area > bestArea) {
            bestArea = area;
            bestDet = &det;
        }
    }

    if (bestDet == nullptr) return std::nullopt;
    return bestDet->center();
#else
    return std::nullopt;
#endif
}

void YOLOv8Detector::warmup() {
#ifdef TENSORRT_AVAILABLE
    if (!m_available) return;
    cv::Mat dummy(m_imgsz, m_imgsz, CV_8UC3, cv::Scalar(0, 0, 0));
    detect(dummy);
    std::cout << "TensorRT 模型预热完成" << std::endl;
#endif
}