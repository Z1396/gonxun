#include "ring_detector.hpp"

#include <algorithm>
#include <vector>
#include <tuple>
#include <string>

/**
 * @brief 赛事得分计算函数
 * @param ring_id 圆环编号 1~6
 * @param material_fallen 物料是否成功落入环内
 * @return int 对应得分，无效情况返回0分
 * 赛事计分规则（从外到内分值递减）：
 * 1号环 = 15分 | 2号环 = 10分 | 3号环 = 7分
 * 4号环 = 5分  | 5号环 = 3分  | 6号环 = 1分
 */
int calcPlacementScore(int ring_id, bool material_fallen) {
    // 物料未掉落 / 环号非法，得0分
    if (material_fallen || ring_id < 1 || ring_id > 6) return 0;
    // 数组下标0占位不用，1~6对应环号分数
    static const int scores[] = {0, 15, 10, 7, 5, 3, 1};
    return scores[ring_id];
}

/**
 * @brief 高精度三环检测器 构造函数
 * 初始化整套图像处理、霍夫圆超参，专为微小、模糊、暗光三环场景优化
 */
ThreeRingDetector::ThreeRingDetector() {
    // 腐蚀迭代次数：弱化细小噪点、纹理干扰
    _erodeIter = 2;
    // 膨胀卷积核：7*7 大面积填充孔洞，闭合圆环轮廓
    _dilateKernel = cv::Mat::ones(7, 7, CV_8UC1);
    _dilateIter = 1;

    // CLAHE 自适应直方图均衡参数（解决暗光、阴影画面）
    _claheClip = 5.0;       // 对比度限制阈值，防止过度增强噪点
    _claheGrid = cv::Size(8, 8); // 分块均衡网格大小

    // 形态学梯度核：椭圆形结构元素，提取圆环边缘轮廓
    _gradKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));

    // 高斯模糊参数：强模糊降噪，平滑画面杂点
    _gaussKsize = cv::Size(7, 7);
    _gaussSigma = 3.0;

    // 二值化阈值：筛选高梯度边缘，过滤弱纹理
    _threshVal = 70;
    // 二值后模糊核：细化圆环轮廓
    _threshGaussKsize = cv::Size(9, 9);

    // ========== 高精度霍夫圆 ALT 模式参数（三环核心） ==========
    _houghDp = 1.5;                 // 累加器分辨率
    _houghMinDist = 50.0;           // 圆心最小间距，避免重复检测
    _houghParam1 = 100.0;           // Canny边缘高阈值
    _houghParam2 = 0.95;            // 圆拟合置信度（极高，严格筛选真圆）
    _houghMinR = 15;                // 最小圆环半径
    _houghMaxR = 50;                // 最大圆环半径
}

/**
 * @brief 三环检测主函数
 * @param img 输入原图，会直接绘制检测结果
 * @return 成功返回6位数组 [x1,y1,x2,y2,x3,y3]，失败返回nullopt
 * 强制约束：必须恰好检测到3个圆，否则判定识别失败
 */
std::optional<std::array<int, 6>> ThreeRingDetector::detect(cv::Mat& img) {
    if (img.empty()) return std::nullopt;

    // ==================== 图像预处理流水线 ====================
    // 1. BGR转HSV：分离色彩与亮度，抗光线干扰更强
    cv::Mat hsv, erodeHsv, dilated, gray;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);

    // 2. 腐蚀操作：3*3核，消除细小噪点、毛刺
    cv::Mat erodeKernel = cv::Mat::ones(3, 3, CV_8UC1);
    cv::erode(hsv, erodeHsv, erodeKernel, cv::Point(-1, -1), _erodeIter);

    // 3. 膨胀操作：填充圆环轮廓断裂处，闭合圆形边缘
    cv::dilate(erodeHsv, dilated, _dilateKernel, cv::Point(-1, -1), _dilateIter);

    // 4. 转回灰度图，用于后续边缘提取
    cv::cvtColor(dilated, gray, cv::COLOR_BGR2GRAY);

    // 5. CLAHE 自适应直方图均衡：提亮暗部、压制过曝，适配赛场光线不均
    cv::Mat equalized;
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(_claheClip, _claheGrid);
    clahe->apply(gray, equalized);

    // ==================== 边缘强化处理 ====================
    cv::Mat gradient, blurred, scaled, thresholded;
    // 6. 形态学梯度：原图-腐蚀图，精准提取物体边缘轮廓
    cv::morphologyEx(equalized, gradient, cv::MORPH_GRADIENT, _gradKernel);

    // 7. 高斯模糊：平滑边缘噪点
    cv::GaussianBlur(gradient, blurred, _gaussKsize, _gaussSigma, _gaussSigma);

    // 8. 像素增益放大：放大边缘对比度，弱化背景
    cv::convertScaleAbs(blurred, scaled, 4.0, 0.0);

    // 9. 二次模糊：进一步降噪
    cv::GaussianBlur(scaled, scaled, _gaussKsize, _gaussSigma, _gaussSigma);

    // 10. 二值化：边缘变白、背景变黑，得到纯净轮廓图
    cv::threshold(scaled, thresholded, _threshVal, 255, cv::THRESH_BINARY);

    // 11. 最终平滑：优化二值化锯齿
    cv::GaussianBlur(thresholded, thresholded, _threshGaussKsize, _gaussSigma, _gaussSigma);

    // ==================== 霍夫圆检测（高精度ALT模式） ====================
    cv::Mat circles;
    // HOUGH_GRADIENT_ALT：OpenCV最新高精度圆检测，适合残缺、模糊圆环
    cv::HoughCircles(thresholded, circles, cv::HOUGH_GRADIENT_ALT,
                     _houghDp, _houghMinDist, _houghParam1, _houghParam2,
                     _houghMinR, _houghMaxR);

    // 严格校验：必须恰好3个圆环，多一个少一个都判定识别失败
    if (circles.empty() || circles.cols != 3) return std::nullopt;

    // 收集3个圆环的圆心坐标
    std::vector<std::pair<int, int>> pts;
    for (int i = 0; i < circles.cols; i++) {
        cv::Vec3f c = circles.at<cv::Vec3f>(0, i);
        int cx = cvRound(c[0]); // 圆心x
        int cy = cvRound(c[1]); // 圆心y
        int r = cvRound(c[2]);  // 圆半径

        // 可视化绘制：红色圆环边框 + 蓝色圆心点
        cv::circle(img, cv::Point(cx, cy), r, cv::Scalar(0, 0, 255), 2);
        cv::circle(img, cv::Point(cx, cy), 2, cv::Scalar(255, 0, 0), 2);

        pts.emplace_back(cx, cy);
    }

    // 坐标排序：先按x升序、再按y升序，和Python原版排序逻辑完全对齐
    // 保证每次识别的三个环顺序固定，不会乱序
    std::sort(pts.begin(), pts.end());

    // 打包输出固定格式数组：[x1,y1,x2,y2,x3,y3]
    std::array<int, 6> result;
    for (int i = 0; i < 3; i++) {
        result[i * 2] = pts[i].first;
        result[i * 2 + 1] = pts[i].second;
    }
    return result;
}

/**
 * @brief 六环得分区检测器 构造函数
 * 常规圆检测参数，适配场地6个大小不一的得分圆环
 */
SixRingDetector::SixRingDetector() {
    _blurKsize = cv::Size(5, 5);    // 基础降噪模糊核
    _cannyLow = 50.0;               // Canny边缘检测低阈值
    _cannyHigh = 150.0;             // Canny边缘检测高阈值
    _houghDp = 1.0;
    _houghMinDist = 40.0;
    _houghParam1 = 100.0;
    _houghParam2 = 30.0;            // 常规宽松阈值，适配场地圆环
    _houghMinR = 20;                // 最小得分环半径
    _houghMaxR = 120;               // 最大得分环半径
}

/**
 * @brief 六环检测主函数
 * @param img 输入原图，直接绘制圆环、编号
 * @return map<环号, (x,y)> 返回1~6号圆环圆心坐标
 */
std::map<int, std::pair<int, int>> SixRingDetector::detect(cv::Mat& img) {
    std::map<int, std::pair<int, int>> result;
    if (img.empty()) return result;

    // ==================== 基础预处理 ====================
    cv::Mat gray, blurred, edges;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);          // 转灰度
    cv::GaussianBlur(gray, blurred, _blurKsize, 0);      // 高斯降噪
    cv::Canny(blurred, edges, _cannyLow, _cannyHigh);    // Canny提取边缘

    // ==================== 标准霍夫圆检测 ====================
    cv::Mat circles;
    cv::HoughCircles(edges, circles, cv::HOUGH_GRADIENT,
                     _houghDp, _houghMinDist, _houghParam1, _houghParam2,
                     _houghMinR, _houghMaxR);

    // 无圆环直接返回空
    if (circles.empty()) return result;

    // 存储所有检测到的圆：(x,y,r)
    std::vector<std::tuple<int, int, int>> detected;
    for (int i = 0; i < circles.cols; i++) {
        cv::Vec3f c = circles.at<cv::Vec3f>(0, i);
        detected.emplace_back(static_cast<int>(c[0]), static_cast<int>(c[1]),
                              static_cast<int>(c[2]));
    }

    // 按圆心X坐标从左到右排序（对应场地1~6号环从左至右分布）
    std::sort(detected.begin(), detected.end(),
        [](const std::tuple<int, int, int>& a, const std::tuple<int, int, int>& b) {
            return std::get<0>(a) < std::get<0>(b);
        });

    // 不足6个环不识别，保证完整性
    if (detected.size() < 6) return result;

    // 取前6个圆环，自动编号1~6，存入map并可视化
    for (int i = 0; i < 6; i++) {
        int x = std::get<0>(detected[i]);
        int y = std::get<1>(detected[i]);
        int r = std::get<2>(detected[i]);

        // 键：环号(1~6)，值：圆心坐标
        result[i + 1] = std::make_pair(x, y);

        // 绘制紫色圆环边框
        cv::circle(img, cv::Point(x, y), r, cv::Scalar(255, 0, 255), 2);
        // 绘制环号文字标注
        cv::putText(img, std::to_string(i + 1), cv::Point(x - 5, y + 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 0, 255), 2);
    }

    return result;
}
